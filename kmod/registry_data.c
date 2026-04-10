/**
 * Motore del database interno al kernel. 
 * Gestisce l'allocazione della memoria Ring 0, la memorizzazione delle regole 
 * e la sincronizzazione multi-core (RCU o Spinlock Globale).
*/

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/string.h>
#include <linux/spinlock.h>
#include <linux/rculist.h>  
#include <linux/rcupdate.h>
#include "registry_data.h"
#include "sys_interceptor.h"

/**
 * struct throttling_rule - Nodo della lista per le regole
 * @uid:         User ID da monitorare (-1 indica nessun filtro sull'utente)
 * @comm:        Nome del task da limitare (stringa vuota indica nessun filtro)
 * @syscall_num: Numero della system call bersaglio
 * @max_calls:   Soglia massima di chiamate permesse in 1 secondo
 * @current_calls: Contatore atomico thread-safe delle invocazioni correnti
 * @list:        Struttura kernel standard per l'ancoraggio alla doubly-linked list
*/
struct throttling_rule {
    int uid;
    char comm[MAX_COMM_LEN];
    int syscall_num;
    int max_calls;
    atomic_t current_calls;
    struct list_head list; 
};

// Inizializzazione della lista delle regole
static LIST_HEAD(rules_list);

// Lock globale utilizzato per serializzare gli Scrittori
DEFINE_SPINLOCK(registry_lock);

// Alloca e inserisce una nuova regola alla lista
int add_rule(int uid, const char *comm, int syscall_num, int max_calls) {
    struct throttling_rule *new_rule;

    // kmalloc() fuori dal lock: se la RAM scarseggia, il kernel può mettere
    // questo thread a dormire. Dormire con uno spinlock chiuso = Deadlock
    new_rule = kmalloc(sizeof(struct throttling_rule), GFP_KERNEL);
    if (!new_rule) {
        printk(KERN_ERR "[Syscall_Throttling] Errore: kmalloc fallita.\n");
        return -ENOMEM;
    }

    // Popolamento della nuova regola
    new_rule->uid = uid;
    new_rule->syscall_num = syscall_num;
    new_rule->max_calls = max_calls;
    atomic_set(&new_rule->current_calls, 0);
    if (comm != NULL) {
        strncpy(new_rule->comm, comm, MAX_COMM_LEN - 1);
        new_rule->comm[MAX_COMM_LEN - 1] = '\0'; 
    } else {
        new_rule->comm[0] = '\0';
    }

    /* --- INIZIO SEZIONE CRITICA (Scrittore) --- */
    spin_lock(&registry_lock);
#if USE_SPINLOCK
    list_add_tail(&new_rule->list, &rules_list);
#else
    // Pubblica il nodo in modo sicuro per i lettori RCU concorrenti
    list_add_tail_rcu(&new_rule->list, &rules_list);
#endif
    spin_unlock(&registry_lock);
    /* --- FINE SEZIONE CRITICA --- */

    printk(KERN_INFO "[Syscall_Throttling] Regola salvata in RAM: Syscall %d, MAX %d\n", 
           syscall_num, max_calls);

    if (hook_specific_syscall(syscall_num) < 0) { // Agganciamo l'hook
        printk(KERN_ERR "[Syscall_Throttling] Errore nell'hooking dinamico della syscall %d\n", syscall_num);
    }
           
    return 0;
}

// Rimuove in modo sicuro una regola
int remove_rule(int syscall_num) {
    struct throttling_rule *cursor, *tmp;
    int removed = 0;

    spin_lock(&registry_lock); // Blocca altri scrittori
    list_for_each_entry_safe(cursor, tmp, &rules_list, list) {
        if (cursor->syscall_num == syscall_num) {
#if USE_SPINLOCK
            list_del(&cursor->list);
            spin_unlock(&registry_lock);
            unhook_specific_syscall(syscall_num); // Rimuovi hook
            kfree(cursor); // Niente RCU, possiamo deallocare subito
#else
            // Sgancia il nodo dalla lista (Invisibile ai NUOVI lettori)
            list_del_rcu(&cursor->list);
            spin_unlock(&registry_lock);

            unhook_specific_syscall(syscall_num); // Rimuovi hook

            // GRACE PERIOD: Blocca questo thread finché i VECCHI lettori
            // che stanno ancora analizzando 'cursor' non chiamano rcu_read_unlock()
            synchronize_rcu();
            
            // Ora è sicuro deallocare la memoria
            kfree(cursor);
#endif
            removed = 1;
            break;
        }
    }
    if (!removed) spin_unlock(&registry_lock);
    
    return removed ? 0 : -ENOENT;
}

int is_throttled(int uid, const char *comm, int syscall_num, int *out_max_calls) {
    struct throttling_rule *cursor;
    int should_block = 0; // 0 = Lascia passare, 1 = Congela il thread
    int current_count = 0;

#if USE_SPINLOCK
    spin_lock(&registry_lock);
    list_for_each_entry(cursor, &rules_list, list) {
#else
    // Disabilita la preemption, indica l'inizio della lettura RCU
    rcu_read_lock(); 
    list_for_each_entry_rcu(cursor, &rules_list, list) {
#endif
        if (cursor->syscall_num == syscall_num || cursor->syscall_num == -1) {
            if ((cursor->uid != -1 && cursor->uid == uid) || 
                (strlen(cursor->comm) > 0 && strncmp(cursor->comm, comm, MAX_COMM_LEN) == 0)) {
                
                *out_max_calls = cursor->max_calls;
                
                // Incrementiamo il contatore e leggiamo il nuovo valore in modo atomico
                current_count = atomic_inc_return(&cursor->current_calls);
                
                // Se il contatore supera il limite, alziamo il flag di blocco
                if (current_count > cursor->max_calls) {
                    should_block = 1;
                }
                break; // Trovata la regola, usciamo dal ciclo 
            }
        }
    }
#if USE_SPINLOCK
    spin_unlock(&registry_lock);
#else
    rcu_read_unlock(); // Fine lettura RCU
#endif

    return should_block;

}

// Deallocazione in fase di scaricamento del modulo (rmmod)
void cleanup_registry(void) {
    struct throttling_rule *cursor, *tmp;
    int count = 0;
    
    printk(KERN_INFO "[Syscall_Throttling] Avvio Garbage Collection del registro...\n");

    spin_lock(&registry_lock);
    
    // list_for_each_entry_safe ci permette di cancellare i nodi mentre iteriamo
    list_for_each_entry_safe(cursor, tmp, &rules_list, list) {
        // Disaccoppiamo il nodo
        list_del(&cursor->list);

        unhook_specific_syscall(cursor->syscall_num);
        
        // Grazie al Reference Counting del Kernel è garantito che non ci sono più lettori attivi, 
        // possiamo deallocare direttamente la memoria senza usare synchronize_rcu()
        kfree(cursor);
        count++;
    }
    
    spin_unlock(&registry_lock);
    printk(KERN_INFO "[Syscall_Throttling] Garbage Collection completata. Liberati %d nodi.\n", count);
}

// Funzione di debug per verificare l'attraversamento
void debug_print_rules(void) {
    struct throttling_rule *cursor;
    
    printk(KERN_INFO "[Syscall_Throttling] --- Lettura Database Regole ---\n");
    
#if USE_SPINLOCK
    spin_lock(&registry_lock);
    list_for_each_entry(cursor, &rules_list, list) {
#else
    rcu_read_lock();
    list_for_each_entry_rcu(cursor, &rules_list, list) {
#endif
        printk(KERN_INFO "[Syscall_Throttling] Regola -> UID: %d, Comm: '%s', Syscall: %d, MAX: %d\n",
               cursor->uid, cursor->comm, cursor->syscall_num, cursor->max_calls);
    }

#if USE_SPINLOCK
    spin_unlock(&registry_lock);
#else
    rcu_read_unlock();
#endif
}

// Resetta tutti i contatori atomici per la nuova finestra temporale
void reset_all_counters(void) {
    struct throttling_rule *cursor;

    // Usiamo la lettura lock-free o lo spinlock in base al Makefile
#if USE_SPINLOCK
    spin_lock(&registry_lock);
    list_for_each_entry(cursor, &rules_list, list) {
        atomic_set(&cursor->current_calls, 0);
    }
    spin_unlock(&registry_lock);
#else
    rcu_read_lock();
    list_for_each_entry_rcu(cursor, &rules_list, list) {
        atomic_set(&cursor->current_calls, 0);
    }
    rcu_read_unlock();
#endif
}