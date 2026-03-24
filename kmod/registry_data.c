#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/string.h>
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

// Funzione per aggiungere una regola alla lista
int add_rule(int uid, const char *comm, int syscall_num, int max_calls) {
    struct throttling_rule *new_rule;
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

    // Aggiunta della nuova regola alla lista
    list_add_tail(&new_rule->list, &rules_list);

    printk(KERN_INFO "[Syscall_Throttling] Regola salvata in RAM: Syscall %d, MAX %d\n", 
           syscall_num, max_calls);
           
    return 0;
}

// Funzione di debug per verificare l'attraversamento
void debug_print_rules(void) {
    struct throttling_rule *cursor;
    
    printk(KERN_INFO "[Syscall_Throttling] --- Lettura Database Regole ---\n");
    
    // Macro del kernel per iterare in modo sicuro sulla lista
    list_for_each_entry(cursor, &rules_list, list) {
        printk(KERN_INFO "[Syscall_Throttling] Regola -> UID: %d, Comm: '%s', Syscall: %d, MAX: %d\n",
               cursor->uid, cursor->comm, cursor->syscall_num, cursor->max_calls);
    }
}