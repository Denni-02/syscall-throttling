/**
 * Header condiviso tra User Space e Kernel Space. 
 * Definisce le strutture dati e i Magic Number per la comunicazione sicura tramite IOCTL.
*/

#ifndef DEFENDER_API_H
#define DEFENDER_API_H

#include <linux/ioctl.h>

#define MAX_COMM_LEN 16 // Lunghezza massima del nome di un processo in Linux (TASK_COMM_LEN)


/**
 * struct config_data - Payload per la configurazione del Reference Monitor
 * @target_uid:  User ID del thread da sottoporre a throttling (-1 per tutti)
 * @comm:        Nome dell'eseguibile (Task Command) da limitare
 * @syscall_num: Numero identificativo della System Call da intercettare
 * @max_calls:   Soglia massima (MAX) di invocazioni consentite al secondo
 * Questa struttura viene passata dal Ring 3 (User Space) al Ring 0 (Kernel Space). 
*/
struct config_data {
    int target_uid;
    char comm[MAX_COMM_LEN];
    int syscall_num;
    int max_calls;
};

/**
 * struct stats_payload - Payload per l'estrazione delle statistiche
 * @syscall_num:          (IN) Numero della system call di cui si richiedono i dati
 * @peak_delay:           (OUT) Ritardo massimo registrato (in cicli di clock)
 * @peak_victim_uid:      (OUT) User ID del processo che ha subito il ritardo massimo
 * @peak_victim_comm:     (OUT) Nome dell'eseguibile che ha subito il ritardo massimo
 * @peak_threads_blocked: (OUT) Numero massimo di thread bloccati simultaneamente
 * Lo User Space compila @syscall_num e la invia al Ring 0. 
 * Il Kernel legge il numero, estrae i dati dal database e
 * sovrascrive i campi (OUT) prima di rimandarla al Ring 3.
*/
struct stats_payload {
    int syscall_num;
    unsigned long long peak_delay;
    int peak_victim_uid;
    char peak_victim_comm[MAX_COMM_LEN];
    int peak_threads_blocked;
};

/**
 * SET_THROTTLING_RULE - Magic Number per la system call IOCTL
 * Utilizziamo la macro _IOW (Ioctl Out/Write) per codificare:
 * direzione del flusso dati (User -> Kernel);
 * magic number ('T' per Throttling) che identifica il modulo;
 * numero sequenziale del comando (1);
 * dimensione del payload (sizeof(struct config_data)).
*/
#define SET_THROTTLING_RULE _IOW('T', 1, struct config_data)

/**
 * IOCTL_GET_STATS - Magic Number per la lettura delle statistiche
 * Utilizziamo la macro _IOWR (Ioctl Read/Write) per codificare uno scambio 
 * bidirezionale: lo User Space scrive la richiesta specificando la syscall, 
 * il Kernel Space risponde popolando la medesima struttura struct stats_payload.
*/
#define IOCTL_GET_STATS _IOWR('T', 2, struct stats_payload)

/**
 * IOCTL_TOGGLE_MONITOR - Magic Number per l'accensione/spegnimento del monitor
 * Utilizziamo la macro _IOW (Ioctl Write) per inviare un semplice intero (1 o 0)
 * dal Ring 3 al Ring 0, permettendo di abilitare o disabilitare globalmente
 * il motore di policy senza dover scaricare il modulo.
*/
#define IOCTL_TOGGLE_MONITOR _IOW('T', 3, int)

#endif // DEFENDER_API_H