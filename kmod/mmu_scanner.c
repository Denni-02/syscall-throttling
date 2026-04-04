/**
 * Motore di scansione hardware della memoria (Brute-Force).
 * Implementa le routine di attraversamento delle Page Tables (PML4, PUD, PMD, PTE)
 * e il pattern matching per trovare la firma della sys_call_table in RAM.
 * 
 * La logica è stata estratta e riadattata a partire dal codice fornito 
 * nel materiale didattico del corso (autore: Prof. Francesco Quaglia).
 */

#include <linux/kernel.h>
#include <linux/mm.h>
#include <asm/page.h>
#include <asm/io.h>
#include <linux/pgtable.h>
#include "mmu_scanner.h"

/* ================== COSTANTI E MASCHERE HARDWARE ================== */
#define ADDRESS_MASK        0xfffffffffffff000ULL
#define PT_ADDRESS_MASK     0x7ffffffffffff000ULL
#define VALID               0x1
#define LH_MAPPING          0x80
#define NO_MAP              (-1)

// Limiti della memoria virtuale x86_64 in cui risiede il kernel
#define START       0xffffffff00000000ULL
#define MAX_ADDR    0xfffffffffff00000ULL

// Indici noti delle syscall non implementate (sys_ni_syscall)
#define FIRST_NI_SYSCALL    134
#define SECOND_NI_SYSCALL   174
#define THIRD_NI_SYSCALL    182 
#define FOURTH_NI_SYSCALL   183
#define FIFTH_NI_SYSCALL    214 
#define SIXTH_NI_SYSCALL    215 
#define SEVENTH_NI_SYSCALL  236 

// Macro per l'estrazione degli indici delle Page Table (9 bit per livello)
#define PML4(addr) (((long long)(addr) >> 39) & 0x1ff)
#define PDP(addr)  (((long long)(addr) >> 30) & 0x1ff)
#define PDE(addr)  (((long long)(addr) >> 21) & 0x1ff)
#define PTE(addr)  (((long long)(addr) >> 12) & 0x1ff)

/* ================== LETTURA PAGE TABLES (VTPMO) ================== */

// Legge il registro CR3 (radice delle Page Table)
static inline unsigned long _read_cr3(void) {
    unsigned long val;
    asm volatile("mov %%cr3,%0" : "=r" (val) : );
    return val;
}

#define PAGE_TABLE_ADDRESS phys_to_virt(_read_cr3() & ADDRESS_MASK)

/**
 * sys_vtpmo() - Traduce un indirizzo virtuale esplorando la MMU.
 * Verifica che ogni livello dell'albero sia allocato in RAM fisica (bit VALID).
 */
static int sys_vtpmo(unsigned long vaddr) {
    pud_t *pdp;
    pmd_t *pde;
    pte_t *pte;
    pgd_t *pml4;

    unsigned long frame_addr;

    // 1. PML4 (Page Map Level 4)
    pml4 = (pgd_t *)PAGE_TABLE_ADDRESS;
    if (!(pgd_val(pml4[PML4(vaddr)]) & VALID)) {
        return NO_MAP;
    }

    // 2. PDP (Page Directory Pointer) 
    pdp = (pud_t *)__va(pgd_val(pml4[PML4(vaddr)]) & PT_ADDRESS_MASK);
    if (!(pud_val(pdp[PDP(vaddr)]) & VALID)) {
        return NO_MAP;
    }

    // 3. PDE (Page Directory Entry) 
    pde = (pmd_t *)__va(pud_val(pdp[PDP(vaddr)]) & PT_ADDRESS_MASK);
    if (!(pmd_val(pde[PDE(vaddr)]) & VALID)) {
        return NO_MAP;
    }

    // Controllo per le Huge Pages (2MB). Il kernel ci risiede spesso. 
    if (pmd_val(pde[PDE(vaddr)]) & LH_MAPPING) {
        frame_addr = pmd_val(pde[PDE(vaddr)]) & PT_ADDRESS_MASK;
        return (frame_addr >> 12); /* Ritorna il frame number */
    }

    // 4. PTE (Page Table Entry) 
    pte = (pte_t *)__va(pmd_val(pde[PDE(vaddr)]) & PT_ADDRESS_MASK);
    if (!(pte_val(pte[PTE(vaddr)]) & VALID)) {
        return NO_MAP;
    }

    frame_addr = pte_val(pte[PTE(vaddr)]) & PT_ADDRESS_MASK;
    return (frame_addr >> 12);
}

/* ================== MOTORE DI SCANSIONE E PATTERN MATCHING ================== */

/**
 * good_area() - Controllo euristico sui falsi positivi.
 * Verifica che gli indici precedenti alla prima sys_ni_syscall non 
 * contengano tutti lo stesso puntatore.
 */
static int good_area(unsigned long *addr) {
    int i;
    for(i = 1; i < FIRST_NI_SYSCALL; i++) {
        if(addr[i] == addr[FIRST_NI_SYSCALL]) goto bad_area;
    }   
    return 1;
bad_area:
    return 0;
}

/**
 * validate_page() - Cerca la firma della sys_call_table in una singola pagina.
 * Cerca il pattern ripetuto degli indici sys_ni_syscall.
 * Return: L'indirizzo esadecimale della tabella, o 0 se non trovata.
 */
static unsigned long validate_page(unsigned long page) {
    int i = 0;
    unsigned long new_page = page;
    unsigned long *addr;

    // Scorre la pagina a passi di 8 byte (la dimensione di un puntatore a 64 bit) 
    for(i = 0; i < PAGE_SIZE; i += sizeof(void*)) {
        
        // Calcola dove finirebbe teoricamente la tabella partendo da questo offset 
        new_page = page + i + SEVENTH_NI_SYSCALL * sizeof(void*);
            
        // Se la tabella sfora nella pagina successiva, ci assicuriamo che esista fisicamente in RAM 
        if ( ((page + PAGE_SIZE) == (new_page & ADDRESS_MASK)) && sys_vtpmo(new_page) == NO_MAP ) {
            break;
        }

        addr = (unsigned long *)(page + i);

        // Le euristiche di pattern matching viste nel corso
        if ( ((addr[FIRST_NI_SYSCALL] & 0x3) == 0) &&
             (addr[FIRST_NI_SYSCALL] != 0x0) &&
             (addr[FIRST_NI_SYSCALL] > 0xffffffff00000000ULL) &&
             (addr[FIRST_NI_SYSCALL] == addr[SECOND_NI_SYSCALL]) &&
             (addr[FIRST_NI_SYSCALL] == addr[THIRD_NI_SYSCALL]) &&
             (addr[FIRST_NI_SYSCALL] == addr[FOURTH_NI_SYSCALL]) &&
             (addr[FIRST_NI_SYSCALL] == addr[FIFTH_NI_SYSCALL]) &&
             (addr[FIRST_NI_SYSCALL] == addr[SIXTH_NI_SYSCALL]) &&
             (addr[FIRST_NI_SYSCALL] == addr[SEVENTH_NI_SYSCALL]) &&
             good_area(addr) ) 
        {
            // Tabella trovata! Restituiamo direttamente l'indirizzo pulito 
            return (unsigned long)addr;
        }
    }
    return 0;
}

/**
 * scan_for_syscall_table() - Entry point della libreria (ex syscall_table_finder).
 * Attraversa la memoria virtuale e chiama il validatore.
 * Return: L'indirizzo esadecimale, o 0 in caso di fallimento totale.
 */
unsigned long scan_for_syscall_table(void) {
    unsigned long k;
    unsigned long found_addr = 0;

    // Scansiona la RAM saltando di pagina in pagina (4096 byte) 
    for(k = START; k < MAX_ADDR; k += 4096) {
        
        // Prima di leggere la pagina, chiediamo all'hardware se esiste fisicamente in RAM 
        if (sys_vtpmo(k) != NO_MAP) {
            
            // Esiste! Cerchiamo il pattern 
            found_addr = validate_page(k);
            if (found_addr != 0) {
                return found_addr; // Trovata, interrompiamo la scansione 
            }
        }
    }
    
    return 0;
}