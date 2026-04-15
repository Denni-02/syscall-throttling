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
 * Viene invocata periodicamente dal demone di sistema (Kthread). Attraversa
 * il database e resetta a zero il contatore atomico `current_calls` di tutte
 * le regole attive, permettendo ai thread di riprendere le invocazioni.
*/
void reset_all_counters(void);

/**
 * update_peak_delay() - Registra il ritardo massimo subìto
 * @syscall_num: La syscall bersaglio
 * @delay_cycles: I cicli di clock
 * @victim_uid: UID del processo che ha subìto il blocco
 * @victim_comm: Nome del processo che ha subìto il blocco
 */
void update_peak_delay(int syscall_num, unsigned long long delay_cycles, int victim_uid, const char *victim_comm);

/**
 * update_thread_stats() - Aggiorna il picco di thread bloccati simultaneamente
 * @syscall_num: La syscall bersaglio
 * @current_blocked_now: Il numero di thread attualmente in Wait Queue
 * 
 * Viene chiamata dal policy_engine ogni volta che un thread sta per essere
 * addormentato. Garantisce l'integrità dei dati tramite spinlock globale.
 */
void update_thread_stats(int syscall_num, int current_blocked_now);

/**
 * get_rule_stats() - Estrae le statistiche di una specifica regola
 * @syscall_num: La system call da cercare
 * @out_stats: Puntatore alla struttura da popolare per lo User Space
 * * Return: 0 in caso di successo, -ENOENT se la regola non esiste.
 */
int get_rule_stats(int syscall_num, struct stats_payload *out_stats);

/**
 * get_active_rules() - Estrae un'istantanea delle regole attualmente attive
 * @out_list: Puntatore alla struttura payload da popolare
 *
 * Attraversa il database in RAM protetto da spinlock e copia i metadati
 * delle regole all'interno dell'array fornito, fino a MAX_RULES_EXPORT.
 * Utilizzata per l'esportazione sicura verso lo User Space tramite IOCTL.
 */
void get_active_rules(struct list_payload *out_list);

void debug_print_rules(void); // Utility di stampa

#endif // REGISTRY_DATA_H