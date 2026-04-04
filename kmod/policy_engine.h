/**
 * Motore di Throttling (Monitor).
 * Gestisce l'identificazione del chiamante, l'accounting e la sospensione.
*/

#ifndef POLICY_ENGINE_H
#define POLICY_ENGINE_H

#include <linux/ptrace.h> 

/**
 * enforce_syscall_policy() - Entry point per l'intercettazione.
 * @syscall_num: Il numero della system call intercettata.
 * Verifica se il chiamante corrente è soggetto a limitazioni e, 
 * in caso affermativo, applica l'accounting e l'eventuale throttling.
*/
void enforce_syscall_policy(int syscall_num);

/**
 * start_policy_engine() - Crea e avvia il demone del tempo (Kthread).
 * Return: 0 in caso di successo, un codice di errore negativo se fallisce.
*/
int start_policy_engine(void);

/**
 * stop_policy_engine() - Invia il segnale di terminazione al demone e attende
 * la sua chiusura sicura in fase di scaricamento del modulo.
*/
void stop_policy_engine(void);

#endif /* POLICY_ENGINE_H */