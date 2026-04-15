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

void print_usage(const char *prog_name) {
    printf("\n=== Syscall Throttling - Pannello di Controllo ===\n");
    printf("1. Aggiungi regola:\n");
    printf("   %s -s <syscall_num> -m <max_calls> [-u <uid>] [-p <program>]\n", prog_name);
    printf("2. Estrai statistiche:\n");
    printf("   %s -g <syscall_num>\n", prog_name);
    printf("3. Accendi/Spegni Monitor Globale:\n");
    printf("   %s -e  (Abilita)\n", prog_name);
    printf("   %s -d  (Disabilita)\n", prog_name);
    printf("4. Mostra Regole Attive:\n");
    printf("   %s -l\n\n", prog_name);
}

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
    printf(" [-] Media Thread Bloccati : %d\n", stats->average_threads_blocked);
    printf("======================================================\n\n");
}

int main(int argc, char *argv[]) {
    int fd;
    int opt;
    struct config_data config;

    int get_stats_syscall = -1;
    int toggle_monitor = -1; 
    int list_rules = 0;

    config.target_uid = -1;
    config.syscall_num = -1;
    config.max_calls = -1;
    memset(config.comm, 0, MAX_COMM_LEN);

    if (argc < 2) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    // IL SEGRETO E' QUI: abbiamo aggiunto la 'l' nel getopt
    while ((opt = getopt(argc, argv, "s:m:u:p:g:edl")) != -1) {
        switch (opt) {
            case 's': config.syscall_num = atoi(optarg); break;
            case 'm': config.max_calls = atoi(optarg); break;
            case 'u': config.target_uid = atoi(optarg); break;
            case 'p': strncpy(config.comm, optarg, MAX_COMM_LEN - 1); break;
            case 'g': get_stats_syscall = atoi(optarg); break;
            case 'e': toggle_monitor = 1; break;
            case 'd': toggle_monitor = 0; break;
            case 'l': list_rules = 1; break; // GESTIONE DEL FLAG -l
            default:
                print_usage(argv[0]);
                return EXIT_FAILURE;
        }
    }

    fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        perror("[!] Errore nell'apertura del device (Sei root?)");
        return EXIT_FAILURE;
    }

    /* --- AZIONE A: ACCENDI/SPEGNI MONITOR --- */
    if (toggle_monitor != -1) {
        if (ioctl(fd, IOCTL_TOGGLE_MONITOR, &toggle_monitor) < 0) {
            perror("[!] Errore IOCTL_TOGGLE_MONITOR");
            close(fd); return EXIT_FAILURE;
        }
        printf("[CLI] Motore Globale di Throttling: %s\n", toggle_monitor ? "ATTIVO" : "DISATTIVATO");
        close(fd); return EXIT_SUCCESS;
    }

    /* --- AZIONE B: ESTRAI STATISTICHE --- */
    if (get_stats_syscall != -1) {
        struct stats_payload stats;
        stats.syscall_num = get_stats_syscall;

        if (ioctl(fd, IOCTL_GET_STATS, &stats) < 0) {
            perror("[!] Errore IOCTL_GET_STATS");
            close(fd); return EXIT_FAILURE;
        }
        
        print_stats_report(&stats);
        close(fd); return EXIT_SUCCESS;
    }

    /* --- AZIONE D: LISTA REGOLE ATTIVE --- */
    if (list_rules) {
        struct list_payload list_data;
        
        if (ioctl(fd, IOCTL_LIST_RULES, &list_data) < 0) {
            perror("[!] Errore durante il recupero della lista regole");
            close(fd); return EXIT_FAILURE;
        }
        
        printf("\n======================================================================\n");
        printf(" REGOLE DI THROTTLING ATTIVE (%d/%d)\n", list_data.count, MAX_RULES_EXPORT);
        printf("======================================================================\n");
        
        if (list_data.count == 0) {
            printf(" Nessuna regola configurata nel database del Kernel.\n");
        } else {
            printf(" %-4s | %-8s | %-8s | %-8s | %-16s\n", "ID", "SYSCALL", "MAX/s", "UID", "PROGRAMMA");
            printf("----------------------------------------------------------------------\n");
            for (int i = 0; i < list_data.count; i++) {
                printf(" [%02d] | %-8d | %-8d | %-8d | %-16s\n", 
                       i + 1,
                       list_data.rules[i].syscall_num, 
                       list_data.rules[i].max_calls,
                       list_data.rules[i].target_uid,
                       strlen(list_data.rules[i].comm) > 0 ? list_data.rules[i].comm : "TUTTI");
            }
        }
        printf("======================================================================\n\n");
        close(fd); return EXIT_SUCCESS;
    }

    /* --- AZIONE C: INSERISCI REGOLA --- */
    if (config.syscall_num == -1 || config.max_calls == -1) {
        printf("[!] Errore: Per aggiungere una regola servono -s e -m.\n");
        close(fd); return EXIT_FAILURE;
    }
    
    printf("[CLI] Invio regola: Syscall %d, MAX %d, UID %d, Prog '%s'\n",
           config.syscall_num, config.max_calls, config.target_uid, config.comm);
    
    if (ioctl(fd, SET_THROTTLING_RULE, &config) < 0) {
        perror("[!] Errore IOCTL_SET_THROTTLING_RULE");
        close(fd); return EXIT_FAILURE;
    }

    printf("[CLI] Regola configurata con successo!\n");
    close(fd); return EXIT_SUCCESS;
}