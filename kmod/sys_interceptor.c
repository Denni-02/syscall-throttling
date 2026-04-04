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
#include <linux/version.h>
#include "sys_interceptor.h"
#include "policy_engine.h"

// Puntatore globale che ospiterà l'indirizzo della tabella trovata 
unsigned long **sys_call_table_ptr = NULL;

// Puntatore per salvare la syscall originale e ripristinarla allo scaricamento
unsigned long *original_ni_syscall = NULL;

/* ========================================================================= *
 * IL REFERENCE MONITOR (TRAMPOLINO DI PASS-THROUGH)                         *
 * ========================================================================= */

/* Definiamo il tipo di puntatore a funzione per le syscall moderne (che usano pt_regs) */
typedef asmlinkage long (*syscall_wrapper_t)(const struct pt_regs *regs);

asmlinkage long my_dummy_syscall(const struct pt_regs *regs) {
    syscall_wrapper_t original_sys_call;

    /* 1. Applichiamo la policy di Throttling 
     * (Passiamo 134 hardcodato per ora, ma lo renderemo dinamico a breve) 
     */
    enforce_syscall_policy(134);

    /* 2. Recuperiamo il puntatore alla syscall originale che avevamo salvato in Fase 3 */
    original_sys_call = (syscall_wrapper_t)original_ni_syscall;

    /* 3. Esecuzione Reale (Pass-Through): 
     * Lasciamo che il kernel esegua la vera operazione richiesta dallo User Space
     * e restituiamo il suo esito originale, in modo che l'utente non si accorga di nulla.
     */
    return original_sys_call(regs);
}

/* ========================================================================= *
 * DISPATCHER OVERRIDE (KERNEL >= 5.15)                       *
 * ========================================================================= */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
#define INST_LEN 5
static char jump_inst[INST_LEN];
static unsigned long x64_sys_call_addr = 0;
static char original_dispatcher_code[INST_LEN]; 

static struct kprobe kp_dispatcher = { .symbol_name = "x64_sys_call" };

/* Ripristina l'uso della tabella in RAM bypassando il dispatcher hardcoded */
static inline void dispatcher_override_call(struct pt_regs *regs, unsigned int nr) {
    asm volatile("mov (%1, %0, 8), %%rax\n\t"
                 "jmp __x86_indirect_thunk_rax\n\t"
                 :
                 : "r"((long)nr), "r"(sys_call_table_ptr)
                 : "rax");
}

static int prepare_dispatcher_override(void) {
    int offset;
    
    // Troviamo il dispatcher
    if (register_kprobe(&kp_dispatcher)) {
        printk(KERN_ERR "[Syscall_Throttling] Errore: x64_sys_call non trovato!\n");
        return -1;
    }
    x64_sys_call_addr = (unsigned long)kp_dispatcher.addr;
    unregister_kprobe(&kp_dispatcher);

    // Salviamo le prime 5 istruzioni originali per ripristinarle a fine esecuzione 
    memcpy(original_dispatcher_code, (void *)x64_sys_call_addr, INST_LEN);

    // Prepariamo l'istruzione assembly di salto (Opcode 0xE9 = JMP rel32) 
    jump_inst[0] = 0xE9;
    offset = (unsigned long)dispatcher_override_call - x64_sys_call_addr - INST_LEN;
    memcpy(jump_inst + 1, &offset, sizeof(int));

    return 0;
}
#endif

/* ========================================================================= *
 * ALGORITMO DI DISCOVERY                              
 * ========================================================================= */

#if USE_KPROBES_DISCOVERY == 1

// Importiamo la libreria Kprobes 
#include "kprobes_discovery.h"

static unsigned long get_syscall_table_address(void) {
    return kprobes_find_syscall_table();
}

#else

// Importiamo la libreria privata per lo scanner hardware 
#include "mmu_scanner.h"

static unsigned long get_syscall_table_address(void) {
    return scan_for_syscall_table();
}

#endif /* USE_KPROBES_DISCOVERY */

/* ========================================================================= *
 * GESTIONE PROTEZIONI HARDWARE (CR0 & CR4)                                  *
 * ========================================================================= */

unsigned long cr0, cr4;

static inline void write_cr0_forced(unsigned long val) {
    unsigned long __force_order;
    asm volatile("mov %0, %%cr0" : "+r"(val), "+m"(__force_order));
}

static inline void protect_memory(void) {
    write_cr0_forced(cr0);
}

static inline void unprotect_memory(void) {
    write_cr0_forced(cr0 & ~X86_CR0_WP);
}

static inline void write_cr4_forced(unsigned long val) {
    unsigned long __force_order;
    asm volatile("mov %0, %%cr4" : "+r"(val), "+m"(__force_order));
}

static inline void conditional_cet_disable(void) {
#ifdef X86_CR4_CET
    if (cr4 & X86_CR4_CET)
        write_cr4_forced(cr4 & ~X86_CR4_CET);
#endif
}

static inline void conditional_cet_enable(void) {
#ifdef X86_CR4_CET
    if (cr4 & X86_CR4_CET)
        write_cr4_forced(cr4);
#endif
}

static inline void begin_syscall_table_hack(void) {
    preempt_disable();
    cr0 = read_cr0();
    cr4 = __read_cr4(); 
    conditional_cet_disable();
    unprotect_memory();
}

static inline void end_syscall_table_hack(void) {
    protect_memory();
    conditional_cet_enable();
    preempt_enable();
}

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

    /* === INIZIO ATTACCO === */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
    printk(KERN_INFO "[Syscall_Throttling] Kernel >= 5.15 rilevato: Preparazione Dispatcher Override...\n");
    if (prepare_dispatcher_override() < 0) return -1;
#endif

    printk(KERN_INFO "[Syscall_Throttling] Disattivazione protezione CR0 e iniezione dell'hook...\n");
    begin_syscall_table_hack();
    
    original_ni_syscall = (unsigned long *)sys_call_table_ptr[134];
    sys_call_table_ptr[134] = (unsigned long *)my_dummy_syscall;
    
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
    /* Scriviamo brutalmente il salto (JMP) dentro il codice eseguibile del kernel! */
    memcpy((unsigned char *)x64_sys_call_addr, jump_inst, INST_LEN);
#endif

    end_syscall_table_hack();
    printk(KERN_INFO "[Syscall_Throttling] Hook installato con successo.\n");
    /* === FINE ATTACCO === */

    return 0;
}

void cleanup_interceptor(void) {
    if (sys_call_table_ptr && original_ni_syscall) {
        printk(KERN_INFO "[Syscall_Throttling] Ripristino della Syscall Table originale...\n");
        begin_syscall_table_hack();
        
        // Ripristiniamo la tabella 
        sys_call_table_ptr[134] = original_ni_syscall;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
        // Rimuoviamo il salto (JMP) e ripristiniamo le istruzioni originali 
        if (x64_sys_call_addr) {
            printk(KERN_INFO "[Syscall_Throttling] Ripristino del dispatcher x64_sys_call...\n");
            memcpy((unsigned char *)x64_sys_call_addr, original_dispatcher_code, INST_LEN);
        }
#endif
        end_syscall_table_hack();
        printk(KERN_INFO "[Syscall_Throttling] Intercettore smontato. Sistema sicuro.\n");
    } else {
        printk(KERN_INFO "[Syscall_Throttling] Nessun hook da ripristinare.\n");
    }
}