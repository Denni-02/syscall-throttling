/**
 * Entry point del Modulo Kernel. 
 * Contiene le macro e le routine principali di inizializzazione (insmod) 
 * e di deallocazione sicura e pulizia (rmmod).
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include "registry_data.h"
#include "sys_interceptor.h"
#include "policy_engine.h"

// Metadati del modulo
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Dennis/0365494");
MODULE_DESCRIPTION("Syscall Throttling LKM - Advanced Operating Systems");
MODULE_VERSION("0.1");

// Dichiarazione funzioni esterne char_device.c
extern int init_char_device(void);
extern void cleanup_char_device(void);

/**
 * core_init() - Entry point eseguito al caricamento del modulo (insmod).
 * Return: 0 in caso di successo, codice di errore negativo in caso di fault.
*/
static int __init core_init(void) {
    int ret;
    printk(KERN_INFO "[Syscall_Throttling] Modulo Caricato con successo.\n");

    // 1. Registra il device nel VFS
    ret = init_char_device(); 
    if (ret < 0) return ret;

    // 2. Trova la sys_call_table e prepara il Dispatcher Override
    ret = init_interceptor();
    if (ret < 0) {
        cleanup_char_device(); // Rollback di sicurezza se fallisce
        return ret;
    }

    // 3. Arma il Kernel Timer e imposta global_monitor_state=1
    ret = start_policy_engine();
    if (ret < 0) {
        // Rollback a cascata
        cleanup_interceptor(); 
        cleanup_char_device();
        return ret;
    }

    printk(KERN_INFO "[Syscall_Throttling] Modulo in attesa di comandi dallo User Space.\n");

    return 0; // Caricamento completato senza errori
}

/**
 * core_exit() - Routine di cleanup eseguita allo scaricamento (rmmod).
*/
static void __exit core_exit(void) {

    stop_policy_engine(); // Imposta monitor a OFF e sveglia tutti i thread bloccati.
    cleanup_interceptor(); // Ripristina Dispatcher Override e sys_call_table
    wait_for_zero_wrappers(); // Aspetta che tutti i thread già dentro il codice del modulo completino
    cleanup_char_device(); // Deregistra il device
    cleanup_registry(); // Libera la memoria delle regole

    printk(KERN_INFO "[Syscall_Throttling] Modulo Scaricato con successo.\n");
}

// Registrazione delle macro di init ed exit
module_init(core_init);
module_exit(core_exit);