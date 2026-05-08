/**
 * Libreria interna per la risoluzione dei simboli kernel.
 * Sfrutta le API Kprobes per aggirare le restrizioni di esportazione
 * dei kernel >= 5.7 e localizzare la sys_call_table.
*/

#ifndef KPROBES_DISCOVERY_H
#define KPROBES_DISCOVERY_H

/**
 * kprobes_find_syscall_table() - Estrae l'indirizzo della tabella.
 * Return: L'indirizzo esadecimale della sys_call_table, o 0 in caso di errore.
*/
unsigned long kprobes_find_syscall_table(void);

#endif