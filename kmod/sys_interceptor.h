/**
 * Header interno del Kernel Space.
 * Espone esclusivamente le API per l'inizializzazione e la pulizia
 * del sottosistema di intercettazione delle system call,
 * nascondendo l'algoritmo di discovery sottostante.
*/

#ifndef SYS_INTERCEPTOR_H
#define SYS_INTERCEPTOR_H

/**
 * init_interceptor() - Avvia la procedura di discovery e hooking.
 * Esegue il discovery della sys_call_table (tramite Kprobes o Memory Scanner
 * a seconda del flag di compilazione) e inietta la system call fittizia.
 *
 * Return: 0 in caso di successo, un codice di errore negativo altrimenti.
 */
int init_interceptor(void);

/**
 * cleanup_interceptor() - Ripristina la sys_call_table originale.
 * Rimuove l'hook disabilitando temporaneamente la protezione hardware in
 * scrittura e ripristina il puntatore originale, garantendo che lo scaricamento
 * del modulo non causi Kernel Panic alle successive chiamate di sistema.
 */
void cleanup_interceptor(void);

#endif // SYS_INTERCEPTOR_H