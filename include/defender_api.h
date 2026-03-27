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
 * SET_THROTTLING_RULE - Magic Number per la system call IOCTL
 * Utilizziamo la macro _IOW (Ioctl Out/Write) per codificare:
 * direzione del flusso dati (User -> Kernel);
 * magic number ('T' per Throttling) che identifica il modulo;
 * numero sequenziale del comando (1);
 * dimensione del payload (sizeof(struct config_data)).
 */
#define SET_THROTTLING_RULE _IOW('T', 1, struct config_data)

#endif // DEFENDER_API_H