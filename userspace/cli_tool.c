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
    printf("\n=== Syscall Throttling - Pannello di Controllo ===\n");
    printf("1. Aggiungi regola:\n");
    printf("   %s -s <syscall_num> -m <max_calls> [-u <uid>] [-p <program>]\n", prog_name);
    printf("2. Estrai statistiche:\n");
    printf("   %s -g <syscall_num>\n", prog_name);
    printf("3. Accendi/Spegni Monitor Globale:\n");
    printf("   %s -e  (Abilita)\n", prog_name);
    printf("   %s -d  (Disabilita)\n\n", prog_name);
}

/**
 * print_stats_report() - Formatta i dati estratti dal kernel
 * @stats: Struttura contenente i dati popolati dal Ring 0
*/
void print_stats_report(struct stats_payload *stats) {
    printf("\n======================================================\n");
    printf(" REPORT STATISTICHE - SYSCALL %d\n", stats->syscall_num);
    printf("======================================================\n");
    printf(" [-] Picco Ritardo Massimo : %llu Cicli CPU\n", stats->peak_delay);
    if (stats->peak_victim_uid != -1) {
        printf(" [-] Vittima (UID)         : %d\n", stats->peak_victim_uid);
        printf(" [-] Vittima (Processo)    : %s\n", stats->peak_victim_comm);
    } else {
        printf(" [-] Vittima               : [Nessun blocco registrato]\n");
    }
    printf(" [-] Picco Thread Bloccati : %d\n", stats->peak_threads_blocked);
    printf("======================================================\n\n");
}

/**
 * main() - Entry point dello User Space Application
*/
int main(int argc, char *argv[]) {
    int fd;
    int opt;
    struct config_data config;

    int get_stats_syscall = -1;
    int toggle_monitor = -1; // 1 = Enable, 0 = Disable

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
    while ((opt = getopt(argc, argv, "s:m:u:p:g:ed")) != -1) {
        switch (opt) {
            case 's': config.syscall_num = atoi(optarg); break;
            case 'm': config.max_calls = atoi(optarg); break;
            case 'u': config.target_uid = atoi(optarg); break;
            case 'p': strncpy(config.comm, optarg, MAX_COMM_LEN - 1); break;
            case 'g': get_stats_syscall = atoi(optarg); break;
            case 'e': toggle_monitor = 1; break;
            case 'd': toggle_monitor = 0; break;
            default:
                print_usage(argv[0]);
                return EXIT_FAILURE;
        }
    }

    // Apertura del Device
    printf("[CLI] Tentativo di apertura del device %s...\n", DEVICE_PATH);
    fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        perror("[!] Errore nell'apertura del device (Sei root?)");
        return EXIT_FAILURE;
    }

    /* --- AZIONE A: ACCENDI/SPEGNI MONITOR --- */
    if (toggle_monitor != -1) {
        if (ioctl(fd, IOCTL_TOGGLE_MONITOR, &toggle_monitor) < 0) {
            perror("[!] Errore IOCTL_TOGGLE_MONITOR");
            close(fd);
            return EXIT_FAILURE;
        }
        printf("[CLI] Motore Globale di Throttling: %s\n", toggle_monitor ? "ATTIVO" : "DISATTIVATO");
        close(fd);
        return EXIT_SUCCESS;
    }

    /* --- AZIONE B: ESTRAI STATISTICHE --- */
    if (get_stats_syscall != -1) {
        struct stats_payload stats;
        stats.syscall_num = get_stats_syscall; // Impostiamo la syscall richiesta

        if (ioctl(fd, IOCTL_GET_STATS, &stats) < 0) {
            perror("[!] Errore IOCTL_GET_STATS (La regola esiste?)");
            close(fd);
            return EXIT_FAILURE;
        }
        
        print_stats_report(&stats);
        close(fd);
        return EXIT_SUCCESS;
    }

    /* --- AZIONE C: INSERISCI REGOLA (Logica Originale) --- */
    if (config.syscall_num == -1 || config.max_calls == -1) {
        printf("[!] Errore: Per aggiungere una regola servono -s e -m.\n");
        close(fd);
        return EXIT_FAILURE;
    }
    if (config.target_uid == -1 && strlen(config.comm) == 0) {
        printf("[!] Errore: Specifica un filtro (-u o -p).\n");
        close(fd);
        return EXIT_FAILURE;
    }

    printf("[CLI] Invio regola: Syscall %d, MAX %d, UID %d, Prog '%s'\n",
           config.syscall_num, config.max_calls, config.target_uid, config.comm);
    
    if (ioctl(fd, SET_THROTTLING_RULE, &config) < 0) {
        perror("[!] Errore IOCTL_SET_THROTTLING_RULE");
        close(fd);
        return EXIT_FAILURE;
    }

    printf("[CLI] Regola configurata con successo!\n");

    // Chiusura del file descriptor
    close(fd);
    return EXIT_SUCCESS;
}