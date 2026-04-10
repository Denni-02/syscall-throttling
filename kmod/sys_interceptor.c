/**
 * Sottosistema di intercettazione delle chiamate di sistema.
 * Implementa la discovery dinamica della sys_call_table e il dirottamento
 * (hooking) del flusso di esecuzione verso il Reference Monitor.
 * Supporta due algoritmi di discovery selezionabili a tempo di 
 * compilazione: Kprobes e MMU Scanner.
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kprobes.h>
#include <linux/version.h>
#include "sys_interceptor.h"
#include "policy_engine.h"

#define MAX_SYSCALLS 512 // syscall supportate in x86_64

// Array che conta quante regole stanno monitorando una syscall
static int syscall_refcount[MAX_SYSCALLS] = {0};

// Array per salvare i puntatori originali
static void *original_syscall_ptrs[MAX_SYSCALLS] = {NULL};

// Lock per evitare Race Condition se aggiungiamo regole contemporaneamente
static DEFINE_SPINLOCK(hook_lock);

// Puntatore globale che ospiterà l'indirizzo della tabella trovata 
unsigned long **sys_call_table_ptr = NULL;

// Puntatore per salvare la syscall originale e ripristinarla allo scaricamento
unsigned long *original_ni_syscall = NULL;

/* ========================================================================= *
 * REFERENCE MONITOR                                                         *
 * ========================================================================= */

// Definiamo il tipo di puntatore a funzione per le syscall
typedef asmlinkage long (*syscall_wrapper_t)(const struct pt_regs *regs);

asmlinkage long my_dummy_syscall(const struct pt_regs *regs) {
    syscall_wrapper_t original_sys_call;
    int dynamic_syscall_num;

    // 1. Estrazione dinamica del numero della syscall dall'hardware (x86_64)
    dynamic_syscall_num = (int)regs->orig_ax;
    if (dynamic_syscall_num < 0 || dynamic_syscall_num >= MAX_SYSCALLS) {
        printk(KERN_ERR "[Syscall_Throttling] Numero syscall fuori range: %d\n", dynamic_syscall_num);
        return -ENOSYS;
    }

    // 2. Applichiamo la policy passando il numero reale appena estratto
    enforce_syscall_policy(dynamic_syscall_num);

    // 3. Recuperiamo il puntatore alla syscall originale 
    original_sys_call = (syscall_wrapper_t)original_syscall_ptrs[dynamic_syscall_num];
    if (!original_sys_call) {
        printk(KERN_ERR "[Syscall_Throttling] Puntatore originale NULL per la syscall %d!\n", dynamic_syscall_num);
        return -ENOSYS;
    }

    // 4. Esecuzione Reale: 
    return original_sys_call(regs);
}

/* ========================================================================= *
 * DISPATCHER OVERRIDE (KERNEL >= 5.15)                                      *
 * ========================================================================= */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
#define INST_LEN 5
static char jump_inst[INST_LEN];
static unsigned long x64_sys_call_addr = 0;
static char original_dispatcher_code[INST_LEN]; 

static struct kprobe kp_dispatcher = { .symbol_name = "x64_sys_call" };

// Ripristina l'uso della tabella in RAM bypassando il dispatcher hardcoded
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
 * ALGORITMO DI DISCOVERY                                                    *
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
 * INTERFACCIA PUBBLICA DEL SOTTOSISTEMA                                     *  
 * ========================================================================= */

int init_interceptor(void) {
    unsigned long *syscall_array;
    printk(KERN_INFO "[Syscall_Throttling] Avvio discovery della Syscall Table...\n");
    
    // Invochiamo la funzione che, in base al Makefile, sarà Kprobes o lo Scanner
    sys_call_table_ptr = (unsigned long **)get_syscall_table_address();
    
    if (!sys_call_table_ptr) {
        printk(KERN_ERR "[Syscall_Throttling] Errore critico: sys_call_table non trovata!\n");
        return -1;
    }

    /* 
     * Security Audit: Usiamo %px esclusivamente in fase di sviluppo per bypassare
     * l'hashing dei puntatori (KASLR) e stampare l'indirizzo esadecimale puro.
     * Questo ci permetterà di verificare visivamente il successo dell'attacco.
    */
    printk(KERN_INFO "[Syscall_Throttling] Syscall Table trovata con successo all'indirizzo: %px\n", 
           (void *)sys_call_table_ptr);

    syscall_array = (unsigned long *)sys_call_table_ptr;

    // Fallback
    original_ni_syscall = (void *)syscall_array[134];

/* Ripristiniamo il bypass delle difese per Kernel >= 5.15 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
    printk(KERN_INFO "[Syscall_Throttling] Kernel >= 5.15 rilevato: Preparazione Dispatcher Override...\n");
    if (prepare_dispatcher_override() < 0) {
        return -1;
    }

    begin_syscall_table_hack();
    memcpy((void *)x64_sys_call_addr, jump_inst, INST_LEN);
    end_syscall_table_hack();

    printk(KERN_INFO "[Syscall_Throttling] Dispatcher Override iniettato. Il kernel ora legge dalla RAM.\n");

#endif

    return 0;
}

void cleanup_interceptor(void) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
    // Ripristiniamo il dispatcher originale per evitare Kernel Panic allo scaricamento 
    if (x64_sys_call_addr) {
        begin_syscall_table_hack();
        memcpy((void *)x64_sys_call_addr, original_dispatcher_code, INST_LEN);
        end_syscall_table_hack();
        printk(KERN_INFO "[Syscall_Throttling] Dispatcher Override rimosso.\n");
    }
#endif
    printk(KERN_INFO "[Syscall_Throttling] Intercettore smontato in sicurezza.\n");
}

int hook_specific_syscall(int syscall_num) {
    unsigned long *syscall_array;

    if (syscall_num < 0 || syscall_num >= MAX_SYSCALLS) return -EINVAL;
    if (!sys_call_table_ptr) return -EFAULT;

    syscall_array = (unsigned long *)sys_call_table_ptr;

    spin_lock(&hook_lock);

    // Se è la prima regola su questa syscall, procediamo con l'iniezione
    if (syscall_refcount[syscall_num] == 0) {
        // Salviamo il puntatore originale
        original_syscall_ptrs[syscall_num] = (void *)syscall_array[syscall_num];

        // Disattivazione CR0/CR4 e iniezione del nostro hook
        begin_syscall_table_hack();
        syscall_array[syscall_num] = (unsigned long)my_dummy_syscall;
        end_syscall_table_hack();

        printk(KERN_INFO "[Syscall_Throttling] Hook FISICO installato sulla Syscall %d\n", syscall_num);
    } else {
        printk(KERN_INFO "[Syscall_Throttling] Syscall %d già hookata. Incremento solo il contatore.\n", syscall_num);
    }

    // Incrementiamo i riferimenti
    syscall_refcount[syscall_num]++;
    spin_unlock(&hook_lock);

    return 0;
}

void unhook_specific_syscall(int syscall_num) {
    unsigned long *syscall_array;

    if (syscall_num < 0 || syscall_num >= MAX_SYSCALLS) return;
    if (!sys_call_table_ptr) return;

    syscall_array = (unsigned long *)sys_call_table_ptr;

    spin_lock(&hook_lock);

    if (syscall_refcount[syscall_num] > 0) {
        syscall_refcount[syscall_num]--;

        // Se nessuna regola monitora più questa syscall, ripristiniamo il kernel
        if (syscall_refcount[syscall_num] == 0) {
            
            begin_syscall_table_hack();
            syscall_array[syscall_num] = (unsigned long)original_syscall_ptrs[syscall_num];
            end_syscall_table_hack();

            original_syscall_ptrs[syscall_num] = NULL;
            
            printk(KERN_INFO "[Syscall_Throttling] Hook FISICO rimosso dalla Syscall %d\n", syscall_num);
        } else {
            printk(KERN_INFO "[Syscall_Throttling] Regola rimossa, ma la Syscall %d rimane hookata (%d regole attive).\n", 
                   syscall_num, syscall_refcount[syscall_num]);
        }
    }
    spin_unlock(&hook_lock);
}