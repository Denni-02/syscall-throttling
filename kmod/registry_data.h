/**
 * Header interno del Kernel Space. 
 * Espone le API del Reference Monitor per l'aggiunta, la rimozione, 
 * la validazione e la Garbage Collection delle regole.
*/

#ifndef REGISTRY_DATA_H
#define REGISTRY_DATA_H

#include <linux/types.h>
#include "../include/defender_api.h"

/**
 * add_rule() - Inserisce una nuova regola di throttling nel database kernel
 * @uid:         ID dell'utente target (-1 per ignorare questo filtro)
 * @comm:        Nome del processo target (stringa vuota per ignorare)
 * @syscall_num: Numero della system call da monitorare
 * @max_calls:   Limite massimo di chiamate consentite al secondo
 *
 * Alloca dinamicamente (tramite kmalloc) un nuovo nodo per la regola e lo 
 * inserisce nella lista collegata.
 *
 * Return: 0 in caso di successo, -ENOMEM se l'allocazione di memoria fallisce.
 */
int add_rule(int uid, const char *comm, int syscall_num, int max_calls);

/**
 * remove_rule() - Rimuove una regola esistente dal database
 * @syscall_num: Numero della system call associata alla regola da eliminare
 *
 * Sgancia il nodo dalla lista protetta.
 * Return: 0 se la regola è stata rimossa, -ENOENT se non è stata trovata.
 */
int remove_rule(int syscall_num);

/**
 * is_throttled() - Esegue il conteggio e valuta la policy di blocco
 * @uid:           UID del processo chiamante (current_uid)
 * @comm:          Nome del processo chiamante (current->comm)
 * @syscall_num:   Numero della system call intercettata
 * @out_max_calls: Puntatore per restituire per riferimento il limite MAX
 *
 * Attraversa il database in RAM. Se trova una regola corrispondente, 
 * incrementa in modo atomico (lock-free) il contatore delle chiamate.
 * 
 * Return: 1 se il limite MAX è stato superato (il thread DEVE essere sospeso), 
 * 0 se il processo può procedere (nessuna regola trovata, o limite non superato).
*/
int is_throttled(int uid, const char *comm, int syscall_num, int *out_max_calls);

/**
 * cleanup_registry() - Garbage Collection per lo scaricamento del modulo
 * Da invocare esclusivamente all'interno della module_exit. Svuota la lista 
 * e chiama kfree() su tutti i nodi allocati per prevenire Memory Leak. 
*/
void cleanup_registry(void);

/**
 * reset_all_counters() - Azzeramento asincrono per la nuova finestra temporale
 *
 * Viene invocata periodicamente dal demone di sistema (Kthread). Attraversa
 * il database e resetta a zero il contatore atomico `current_calls` di tutte
 * le regole attive, permettendo ai thread di riprendere le invocazioni.
*/
void reset_all_counters(void);

void debug_print_rules(void); // Utility di stampa

#endif // REGISTRY_DATA_H