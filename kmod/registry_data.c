#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/string.h>
#include <linux/spinlock.h>
#include <linux/rculist.h>  
#include <linux/rcupdate.h>
#include "registry_data.h"

// Struttura dati per una regola
struct throttling_rule {
    int uid;
    char comm[MAX_COMM_LEN];
    int syscall_num;
    int max_calls;
    struct list_head list; 
};

// Inizializzazione della lista delle regole
static LIST_HEAD(rules_list);

DEFINE_SPINLOCK(registry_lock);

// Funzione per aggiungere una regola alla lista
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
    if (comm != NULL) {
        strncpy(new_rule->comm, comm, MAX_COMM_LEN - 1);
        new_rule->comm[MAX_COMM_LEN - 1] = '\0'; 
    } else {
        new_rule->comm[0] = '\0';
    }

    // --- INIZIO SEZIONE SCRITTURA ---
    spin_lock(&registry_lock);
#if USE_SPINLOCK
    list_add_tail(&new_rule->list, &rules_list);
#else
    // Pubblica il nodo in modo sicuro per i lettori RCU concorrenti
    list_add_tail_rcu(&new_rule->list, &rules_list);
#endif
    spin_unlock(&registry_lock);
    // --- FINE SEZIONE SCRITTURA ---

    printk(KERN_INFO "[Syscall_Throttling] Regola salvata in RAM: Syscall %d, MAX %d\n", 
           syscall_num, max_calls);
           
    return 0;
}

int is_throttled(int uid, const char *comm, int syscall_num, int *out_max_calls) {
    struct throttling_rule *cursor;
    int found = 0;

#if USE_SPINLOCK
    spin_lock(&registry_lock);
    list_for_each_entry(cursor, &rules_list, list) {
#else
    // Disabilita la preemption, indica l'inizio della lettura RCU. ZERO Lock.
    rcu_read_lock(); 
    list_for_each_entry_rcu(cursor, &rules_list, list) {
#endif
        if (cursor->syscall_num == syscall_num || cursor->syscall_num == -1) {
            if (cursor->uid != -1 && cursor->uid == uid) {
                *out_max_calls = cursor->max_calls;
                found = 1;
                break;
            }
            if (strlen(cursor->comm) > 0 && strncmp(cursor->comm, comm, MAX_COMM_LEN) == 0) {
                *out_max_calls = cursor->max_calls;
                found = 1;
                break;
            }
        }
    }
#if USE_SPINLOCK
    spin_unlock(&registry_lock);
#else
    rcu_read_unlock(); // Fine lettura RCU
#endif

    return found;
}

// Funzione critica per illustrare il Grace Period
int remove_rule(int syscall_num) {
    struct throttling_rule *cursor, *tmp;
    int removed = 0;

    spin_lock(&registry_lock); // Blocca altri scrittori
    list_for_each_entry_safe(cursor, tmp, &rules_list, list) {
        if (cursor->syscall_num == syscall_num) {
#if USE_SPINLOCK
            list_del(&cursor->list);
            spin_unlock(&registry_lock);
            kfree(cursor); // Niente RCU, possiamo deallocare subito
#else
            // Sgancia il nodo dalla lista (Invisibile ai NUOVI lettori)
            list_del_rcu(&cursor->list);
            spin_unlock(&registry_lock);
            
            // GRACE PERIOD: Blocca questo thread finché i VECCHI lettori
            // che stanno ancora analizzando 'cursor' non chiamano rcu_read_unlock()
            synchronize_rcu();
            
            // Ora è matematicamente sicuro deallocare la memoria
            kfree(cursor);
#endif
            removed = 1;
            break;
        }
    }
    if (!removed) spin_unlock(&registry_lock);
    
    return removed ? 0 : -ENOENT;
}

// Da chiamare in fase di scaricamento del modulo (rmmod)
void cleanup_registry(void) {
    struct throttling_rule *cursor, *tmp;
    int count = 0;
    
    printk(KERN_INFO "[Syscall_Throttling] Avvio Garbage Collection del registro...\n");

    spin_lock(&registry_lock);
    
    // list_for_each_entry_safe ci permette di cancellare i nodi mentre iteriamo
    list_for_each_entry_safe(cursor, tmp, &rules_list, list) {
        // Disaccoppiamo il nodo
        list_del(&cursor->list);
        
        // Visto che il modulo si sta scaricando e non ci sono più lettori attivi, 
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