/**
 * @file bench_latency.c
 * @brief Benchmark di Latenza Hardware (Overhead Ring 0)
 * * Misura l'overhead introdotto dall'infrastruttura RCU e dall'Hook
 * della Syscall Table quando una chiamata di sistema NON deve essere 
 * bloccata (Fast-Path Bypass). Esegue misurazioni al nanosecondo.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/syscall.h>

#define ITERATIONS 1000000 
#define TARGET_SYSCALL 39 // sys_getpid (Baseline perfetta)

int main(int argc, char *argv[]) {
    struct timespec start, end;
    long long total_ns = 0;
    long long max_ns = 0;
    int syscall_num = TARGET_SYSCALL;

    if (argc > 1) {
        syscall_num = atoi(argv[1]);
    }

    printf("[*] Misurazione Latenza Syscall %d (Bypass Mode)...\n", syscall_num);
    printf("[*] Iterazioni: %d\n", ITERATIONS);

    for (int i = 0; i < ITERATIONS; i++) {
        clock_gettime(CLOCK_MONOTONIC, &start);
        syscall(syscall_num);
        clock_gettime(CLOCK_MONOTONIC, &end);

        long long delta_ns = (end.tv_sec - start.tv_sec) * 1000000000LL + (end.tv_nsec - start.tv_nsec);

        total_ns += delta_ns;
        if (delta_ns > max_ns) {
            max_ns = delta_ns;
        }
    }

    long long avg_ns = total_ns / ITERATIONS;
    
    printf("\n[+] Risultati Latenza Ring 3 -> Ring 0 -> Ring 3:\n");
    printf("    Media: %lld ns per chiamata\n", avg_ns);
    printf("    Picco: %lld ns\n\n", max_ns);
    
    FILE *f = fopen("latency.txt", "w");
    if (f) {
        fprintf(f, "calls,%d\navg_ns,%lld\nmax_ns,%lld\n", ITERATIONS, avg_ns, max_ns);
        fclose(f);
        printf("[+] Dati salvati in latency.txt\n");
    }

    return 0;
}