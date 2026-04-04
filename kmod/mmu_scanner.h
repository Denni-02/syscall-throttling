/**
 * Libreria interna per l'analisi hardware della memoria.
 * Espone l'algoritmo brute-force per la localizzazione della 
 * sys_call_table aggirando le protezioni di esportazione.
*/

#ifndef MMU_SCANNER_H
#define MMU_SCANNER_H

/**
 * scan_for_syscall_table() - Esegue lo scan della RAM.
 * Attraversa la Memory Management Unit (MMU) per cercare
 * la tabella delle chiamate di sistema in modo sicuro,
 * evitando Page Fault in Ring 0.
 *
 * Return: L'indirizzo esadecimale della tabella, o 0 se non trovata.
*/
unsigned long scan_for_syscall_table(void);

#endif // MMU_SCANNER_H