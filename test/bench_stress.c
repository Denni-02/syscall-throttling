/**
 * @file bench_stress.c
 * @brief Benchmark di Concorrenza e Thundering Herd
 * * Genera chiamate di sistema massicce tramite pthreads per saturare
 * la Wait Queue del Kernel. Progettato per essere eseguito con "perf stat"
 * al fine di misurare i Context Switches e validare l'efficienza 
 * dell'algoritmo di scheduling FIFO Strict del modulo.
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/syscall.h>

#define NUM_THREADS 20
#define CALLS_PER_THREAD 10

int target_syscall;

void *spam_syscall(void *arg) {
    for (int i = 0; i < CALLS_PER_THREAD; i++) {
        // Invocazione della syscall che verrà throttlata
        syscall(target_syscall);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Uso: %s <syscall_num>\n", argv[0]);
        return 1;
    }
    
    target_syscall = atoi(argv[1]);
    pthread_t threads[NUM_THREADS];

    printf("[*] Stress Test: Avvio %d thread (10 chiamate ciascuno) sulla syscall %d...\n", 
            NUM_THREADS, target_syscall);
    
    for (int i = 0; i < NUM_THREADS; i++) {
        if (pthread_create(&threads[i], NULL, spam_syscall, NULL) != 0) {
            perror("[-] Errore creazione thread");
            return 1;
        }
    }
    
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    printf("[+] Stress test completato.\n");
    return 0;
}