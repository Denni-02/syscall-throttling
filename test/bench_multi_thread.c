/**
 * @file bench_multi_thread.c
 * @brief Benchmark Multi-Thread per misurare la Lock Contention (RCU vs Spinlock)
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/time.h>

#define NUM_THREADS 8
#define ITERATIONS_PER_THREAD 15000

int target_syscall = 39; // sys_getpid

void *worker(void *arg) {
    for (int i = 0; i < ITERATIONS_PER_THREAD; i++) {
        syscall(target_syscall);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc > 1) {
        target_syscall = atoi(argv[1]);
    }

    pthread_t threads[NUM_THREADS];
    struct timeval start, end;

    printf("[*] Avvio test ...\n");
    printf("[*] Configurazione: %d Thread, %d Chiamate per thread\n", 
            NUM_THREADS, ITERATIONS_PER_THREAD);
    printf("[*] Totale System Call: %d\n", NUM_THREADS * ITERATIONS_PER_THREAD);
    
    // Inizio misurazione
    gettimeofday(&start, NULL);

    // Esplosione dei thread
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL, worker, NULL);
    }
    
    // Attesa sincronizzata
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    // Fine misurazione
    gettimeofday(&end, NULL);
    long long delta_ms = (end.tv_sec - start.tv_sec) * 1000LL + (end.tv_usec - start.tv_usec) / 1000LL;

    printf("\n[+] TEMPO TOTALE DI ATTRAVERSAMENTO: %lld ms\n", delta_ms);
    return 0;
}