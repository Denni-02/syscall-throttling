/**
 * Tool a riga di comando (CLI) in User Space. 
 * Esegue il parsing degli argomenti (getopt), formatta il payload 
 * e invia le regole di throttling al modulo kernel tramite IOCTL.
*/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include "../include/defender_api.h"

#define DEVICE_PATH "/dev/syscall_defender"

/**
 * print_usage() - Mostra l'interfaccia a riga di comando (CLI)
 * @prog_name: Il nome dell'eseguibile invocato (argv[0])
 *
 * Fornisce un feedback immediato all'utente in caso di sintassi errata,
 * documentando i flag supportati dal tool.
 */
void print_usage(const char *prog_name) {
    printf("Syscall Throttling - Pannello di Controllo\n");
    printf("Uso: %s -s <syscall_num> -m <max_calls> [-u <uid>] [-p <program>]\n", prog_name);
    printf("Opzioni obbligatorie:\n");
    printf("  -s    Numero della syscall da monitorare\n");
    printf("  -m    Limite massimo di chiamate al secondo (MAX)\n");
    printf("Opzioni filtro (usarne almeno una):\n");
    printf("  -u    UID dell'utente target\n");
    printf("  -p    Nome del programma target (max %d char)\n", MAX_COMM_LEN - 1);
}

/**
 * main() - Entry point dello User Space Application
 * Esegue il parsing degli argomenti, popola la struttura
 * dati di configurazione e la trasmette al Character Device 
 * del kernel mediante la system call ioctl().
 */
int main(int argc, char *argv[]) {
    int fd;
    int opt;
    struct config_data config;

    // Inizializziamo la struct con valori di default (-1 e stringa vuota)
    config.target_uid = -1;
    config.syscall_num = -1;
    config.max_calls = -1;
    memset(config.comm, 0, MAX_COMM_LEN);

    // Se l'utente non passa argomenti, mostriamo il manuale
    if (argc < 2) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    // Parsing degli argomenti da riga di comando tramite getopt
    while ((opt = getopt(argc, argv, "s:m:u:p:")) != -1) {
        switch (opt) {
            case 's':
                config.syscall_num = atoi(optarg);
                break;
            case 'm':
                config.max_calls = atoi(optarg);
                break;
            case 'u':
                config.target_uid = atoi(optarg);
                break;
            case 'p':
                strncpy(config.comm, optarg, MAX_COMM_LEN - 1);
                break;
            default:
                print_usage(argv[0]);
                return EXIT_FAILURE;
        }
    }

    // Controllo di validità base
    if (config.syscall_num == -1 || config.max_calls == -1) {
        printf("[!] Errore: Le opzioni -s (syscall) e -m (max) sono obbligatorie.\n");
        return EXIT_FAILURE;
    }
    if (config.target_uid == -1 && strlen(config.comm) == 0) {
        printf("[!] Errore: Devi specificare almeno un filtro (-u o -p).\n");
        return EXIT_FAILURE;
    }

    // 1. Apertura del Device
    printf("[CLI] Tentativo di apertura del device %s...\n", DEVICE_PATH);
    fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        perror("[!] Errore nell'apertura del device (Sei root?)");
        return EXIT_FAILURE;
    }

    printf("[CLI] Invio regola: Syscall %d, MAX %d, UID %d, Programma '%s'\n",
           config.syscall_num, config.max_calls, config.target_uid, config.comm);
    

    // 2. Invio della struct al Kernel tramite IOCTL
    if (ioctl(fd, SET_THROTTLING_RULE, &config) < 0) {
        perror("[!] Errore nell'invio della IOCTL");
        close(fd);
        return EXIT_FAILURE;
    }

    printf("[CLI] Regola configurata con successo!\n");

    // Chiusura del file descriptor
    close(fd);
    return EXIT_SUCCESS;
}