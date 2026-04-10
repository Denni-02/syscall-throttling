/**
 * Header interno del Kernel Space.
 * Espone esclusivamente le API per l'inizializzazione e la pulizia
 * del sottosistema di intercettazione delle system call,
 * nascondendo l'algoritmo di discovery sottostante.
*/

#ifndef SYS_INTERCEPTOR_H
#define SYS_INTERCEPTOR_H

/**
 * init_interceptor() - Avvia la procedura di discovery.
 * Return: 0 in caso di successo, un codice di errore negativo altrimenti.
*/
int init_interceptor(void);

/**
 * cleanup_interceptor() - Esegue la pulizia allo scaricamento del modulo.
*/
void cleanup_interceptor(void);

/**
 * hook_specific_syscall() - Inietta l'hook su una syscall specifica.
 * Implementa il Reference Counting per evitare sovrascritture.
 * @syscall_num: Il numero della system call da intercettare.
 * Return: 0 in caso di successo.
*/
int hook_specific_syscall(int syscall_num);

/**
 * unhook_specific_syscall() - Rimuove l'hook da una syscall specifica.
 * Ripristina il puntatore originale solo se il Reference Count scende a 0.
 * @syscall_num: Il numero della system call da ripristinare.
*/
void unhook_specific_syscall(int syscall_num);

#endif // SYS_INTERCEPTOR_H