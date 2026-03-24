#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/string.h>
#include <linux/spinlock.h>
#include "registry_data.h"

// Interruttore: 1 = Spinlock, 0 = RCU
#define USE_SPINLOCK 1

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

#if USE_SPINLOCK
    DEFINE_SPINLOCK(registry_lock);
#endif

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

#if USE_SPINLOCK
    spin_lock(&registry_lock);
#endif
    // -- INIZIO SEZIONE CRITICA (Aggiunta nuova regola alla lista) --
    list_add_tail(&new_rule->list, &rules_list);
    // -- FINE SEZIONE CRITICA --
#if USE_SPINLOCK
    spin_unlock(&registry_lock);
#endif

    printk(KERN_INFO "[Syscall_Throttling] Regola salvata in RAM: Syscall %d, MAX %d\n", 
           syscall_num, max_calls);
           
    return 0;
}

int is_throttled(int uid, const char *comm, int syscall_num, int *out_max_calls) {
    struct throttling_rule *cursor;
    int found = 0;

#if USE_SPINLOCK
    spin_lock(&registry_lock);
#endif
    // -- INIZIO SEZIONE CRITICA (Lettura regole)
    list_for_each_entry(cursor, &rules_list, list) {
        // Verifica corrispondenza syscall (o -1 per intercettarle tutte)
        if (cursor->syscall_num == syscall_num || cursor->syscall_num == -1) {
            
            // Verifica corrispondenza UID
            if (cursor->uid != -1 && cursor->uid == uid) {
                *out_max_calls = cursor->max_calls;
                found = 1;
                break;
            }
            
            // Verifica corrispondenza Nome Programma
            if (strlen(cursor->comm) > 0 && strncmp(cursor->comm, comm, MAX_COMM_LEN) == 0) {
                *out_max_calls = cursor->max_calls;
                found = 1;
                break;
            }
        }
    }
    // -- FINE SEZIONE CRITICA --
#if USE_SPINLOCK
    spin_unlock(&registry_lock);
#endif
    
    return found;
}

// Funzione di debug per verificare l'attraversamento
void debug_print_rules(void) {
    struct throttling_rule *cursor;
    
    printk(KERN_INFO "[Syscall_Throttling] --- Lettura Database Regole ---\n");
    
#if USE_SPINLOCK
    spin_lock(&registry_lock);
#endif
    list_for_each_entry(cursor, &rules_list, list) {
        printk(KERN_INFO "[Syscall_Throttling] Regola -> UID: %d, Comm: '%s', Syscall: %d, MAX: %d\n",
               cursor->uid, cursor->comm, cursor->syscall_num, cursor->max_calls);
    }
#if USE_SPINLOCK
    spin_unlock(&registry_lock);
#endif
}