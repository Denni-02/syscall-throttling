/**
 * Implementazione del metodo Kprobes per la discovery.
 * Progetto: Syscall Throttling LKM
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kprobes.h>
#include "kprobes_discovery.h"

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
 * kprobes_find_syscall_table() - Risolve l'indirizzo della Syscall Table (Metodo Kprobes).
 * 
 * Sfrutta l'infrastruttura di debug (Kprobes) per estrarre l'indirizzo
 * di kallsyms_lookup_name, smonta immediatamente la sonda per evitare
 * overhead prestazionale, e utilizza la funzione appena sbloccata
 * per individuare la vera sys_call_table.
 * 
 * Return: L'indirizzo di memoria della sys_call_table, o 0 in caso di fallimento.
*/
unsigned long kprobes_find_syscall_table(void) {
    kallsyms_lookup_name_t kallsyms_lookup_name_ptr = NULL;
    unsigned long table_addr;
    int ret;

    ret = register_kprobe(&kp);
    if (ret < 0) {
        printk(KERN_ERR "[Syscall_Throttling] Registrazione Kprobe fallita (%d)\n", ret);
        return 0;
    }

    kallsyms_lookup_name_ptr = (kallsyms_lookup_name_t) kp.addr;
    unregister_kprobe(&kp);

    if (!kallsyms_lookup_name_ptr) {
        printk(KERN_ERR "[Syscall_Throttling] Impossibile trovare kallsyms_lookup_name\n");
        return 0;
    }

    table_addr = kallsyms_lookup_name_ptr("sys_call_table");
    return table_addr;
}

#endif /* USE_KPROBES_DISCOVERY */