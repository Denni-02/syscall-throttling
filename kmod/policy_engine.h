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
 * start_policy_engine() - Inizializza e avvia l'orologio di sistema (Kernel Timer in Softirq).
 * Return: 0 in caso di successo.
*/
int start_policy_engine(void);

/**
 * stop_policy_engine() - Arresta il timer di sistema in sicurezza.
 * Utilizza del_timer_sync() per attendere la fine di eventuali callback in esecuzione.
*/
void stop_policy_engine(void);

/**
 * wait_for_zero_wrappers() - Sospende l'unload finché i thread non escono dall'hook.
*/
void wait_for_zero_wrappers(void);

extern int global_monitor_state;

#endif /* POLICY_ENGINE_H */