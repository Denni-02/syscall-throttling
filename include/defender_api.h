#ifndef DEFENDER_API_H
#define DEFENDER_API_H

#include <linux/ioctl.h>

#define MAX_COMM_LEN 16 // Lunghezza massima del nome di un processo in Linux (TASK_COMM_LEN)

// Struttura passata dal Ring 3 al Ring 0
struct config_data {
    int target_uid;              // UID da bloccare
    char comm[MAX_COMM_LEN];     // Nome del programma da bloccare
    int syscall_num;             // Numero della system call da monitorare
    int max_calls;               // Soglia massima di chiamate per secondo (MAX)
};

// Magic Number per la IOCTL
// 'T' identifica il modulo (Throttling), 1 è l'ID del comando
#define SET_THROTTLING_RULE _IOW('T', 1, struct config_data)

#endif // DEFENDER_API_H