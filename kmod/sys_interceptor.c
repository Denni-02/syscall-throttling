/**
 * Sottosistema di intercettazione delle chiamate di sistema.
 * Implementa la discovery dinamica della sys_call_table e il dirottamento
 * (hooking) del flusso di esecuzione verso il Reference Monitor.
 * Il file supporta due algoritmi di discovery selezionabili a tempo di 
 * compilazione: Kprobes e MMU Scanner.
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kprobes.h>
#include "sys_interceptor.h"

// Puntatore globale che ospiterà l'indirizzo della tabella trovata 
unsigned long **sys_call_table_ptr = NULL;

/* ========================================================================= *
 * ALGORITMO DI DISCOVERY                              
 * ========================================================================= */

#if USE_KPROBES_DISCOVERY == 1

/** 
 * Dichiariamo un puntatore a funzione che ha la stessa firma
 * della funzione nascosta che vogliamo catturare. 
*/
typedef unsigned long (*kallsyms_lookup_name_t)(const char *name);

/** 
 * Prepariamo la sonda dandogli solo il nome del simbolo che ci interessa.
*/
static struct kprobe kp = {
    .symbol_name = "kallsyms_lookup_name"
};

/**
 * get_syscall_table_address() - Risolve l'indirizzo della Syscall Table (Metodo Kprobes).
 * 
 * Sfrutta l'infrastruttura di debug (Kprobes) per estrarre l'indirizzo
 * di kallsyms_lookup_name, smonta immediatamente la sonda per evitare
 * overhead prestazionale, e utilizza la funzione appena sbloccata
 * per individuare la vera sys_call_table.
 * 
 * Return: L'indirizzo di memoria della sys_call_table, o 0 in caso di fallimento.
 */
static unsigned long get_syscall_table_address(void) {
    kallsyms_lookup_name_t kallsyms_lookup_name_ptr = NULL;
    unsigned long table_addr;
    int ret;

    // Chiamiamo register_kprobe(). Il kernel risolve l'indirizzo e lo salva in kp.addr
    ret = register_kprobe(&kp);
    if (ret < 0) {
        printk(KERN_ERR "[Syscall_Throttling] Registrazione Kprobe fallita (%d)\n", ret);
        return 0;
    }

    // Rubiamo l'indirizzo (castandolo al nostro puntatore a funzione)
    kallsyms_lookup_name_ptr = (kallsyms_lookup_name_t) kp.addr;

    // Rimuoviamo immediatamente la sonda per non rallentare il sistema
    unregister_kprobe(&kp);

    if (!kallsyms_lookup_name_ptr) {
        printk(KERN_ERR "[Syscall_Throttling] Impossibile trovare kallsyms_lookup_name\n");
        return 0;
    }

    // Usiamo la funzione sbloccata per chiedere dove si trova la sys_call_table!
    table_addr = kallsyms_lookup_name_ptr("sys_call_table");
    
    return table_addr;
}

#else

// Qui andrà il codice con la brute-force
static unsigned long get_syscall_table_address(void) {
    printk(KERN_INFO "[Syscall_Throttling] Scanner hardware non ancora implementato.\n");
    return 0;
}

#endif /* USE_KPROBES_DISCOVERY */

/* ========================================================================= *
 * INTERFACCIA PUBBLICA DEL SOTTOSISTEMA               
 * ========================================================================= */

int init_interceptor(void) {
    printk(KERN_INFO "[Syscall_Throttling] Avvio discovery della Syscall Table...\n");
    
    // Invochiamo la funzione che, in base al Makefile, sarà Kprobes o lo Scanner
    sys_call_table_ptr = (unsigned long **)get_syscall_table_address();
    
    if (!sys_call_table_ptr) {
        printk(KERN_ERR "[Syscall_Throttling] Errore critico: sys_call_table non trovata!\n");
        return -1;
    }

    /** 
     * Security Audit: Usiamo %px esclusivamente in fase di sviluppo per bypassare
     * l'hashing dei puntatori (KASLR) e stampare l'indirizzo esadecimale puro.
     * Questo ci permetterà di verificare visivamente il successo dell'attacco.
     */
    printk(KERN_INFO "[Syscall_Throttling] Syscall Table trovata con successo all'indirizzo: %px\n", 
           (void *)sys_call_table_ptr);
    
    return 0;
}

void cleanup_interceptor(void) {
    printk(KERN_INFO "[Syscall_Throttling] Intercettore smontato (Nessun hook ancora attivo).\n");
}