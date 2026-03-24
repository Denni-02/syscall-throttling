#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include "registry_data.h"

// Informazioni del modulo
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Dennis/0365494");
MODULE_DESCRIPTION("Syscall Throttling LKM - Advanced Operating Systems");
MODULE_VERSION("0.1");

// Dichiarazione funzioni esterne char_device.c
extern int init_char_device(void);
extern void cleanup_char_device(void);

// Funzione chiamata al caricamento del modulo (insmod)
static int __init core_init(void) {
    int ret;
    int max_calls_retrieved = 0;
    int result;
    printk(KERN_INFO "[Syscall_Throttling] Modulo Caricato con successo.\n");

    ret = init_char_device();
    if (ret < 0) return ret;

    // --- TEST DUMMY ---------------------------------------------
    // printk(KERN_INFO "[Syscall_Throttling] Esecuzione Test Dummy Liste...\n");
    // add_rule(1000, "spammer", 2, 5);
    // add_rule(-1, "hacker_tool", 0, 50);
    // debug_print_rules();
    // ---------------------------

    // --- TEST DUMMY ---------------------------------------------
    printk(KERN_INFO "[Syscall_Throttling] Esecuzione Test Dummy Spinlock...\n");
    add_rule(1000, "spammer", 2, 5);
    
    result = is_throttled(1000, "un_processo_finto", 2, &max_calls_retrieved);
    
    if (result) {
        printk(KERN_INFO "[Syscall_Throttling] TEST SUPERATO: Trovata regola per UID 1000. MAX è %d\n", max_calls_retrieved);
    } else {
        printk(KERN_ERR "[Syscall_Throttling] TEST FALLITO: Regola non trovata!\n");
    }
    // ---------------------------

    return 0; // caricamento completato senza errori
}

// Funzione chiamata allo scaricamento del modulo (rmmod)
static void __exit core_exit(void) {
    cleanup_char_device();

    /* Nota: Qui abbiamo attualmente un MEMORY LEAK. 
     * I nodi allocati nel Test Dummy non vengono liberati con kfree(). 
     * Risolveremo questo problema architetturale
     * con la funzione di Garbage Collection. */

    printk(KERN_INFO "[Syscall_Throttling] Modulo Scaricato con successo.\n");
}

// Registrazione delle macro di init ed exit
module_init(core_init);
module_exit(core_exit);