#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include "../include/defender_api.h"

#define DEVICE_PATH "/dev/syscall_defender"

int main() {
    int fd;
    struct config_data config;

    printf("[CLI] Tentativo di apertura del device %s...\n", DEVICE_PATH);
    fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        perror("[!] Errore nell'apertura del device (Sei root?)");
        return EXIT_FAILURE;
    }

    // Regola finta per il Test Dummy
    config.target_uid = 1000;         // Vogliamo bloccare l'utente 1000
    config.syscall_num = 2;           // Syscall numero 2 (es. sys_open)
    config.max_calls = 5;             // Massimo 5 chiamate al secondo
    strncpy(config.comm, "spammer", MAX_COMM_LEN); 

    printf("[CLI] Invio regola di throttling al Kernel (Ring 0)...\n");
    
    // Invio della struct al Kernel tramite la syscall IOCTL
    if (ioctl(fd, SET_THROTTLING_RULE, &config) < 0) {
        perror("[!] Errore nell'invio della IOCTL");
        close(fd);
        return EXIT_FAILURE;
    }

    printf("[CLI] Regola configurata con successo!\n");
    close(fd);
    return EXIT_SUCCESS;
}