/**
 * @file bench_stress.c
 * @brief Benchmark di Concorrenza
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <fcntl.h>

#define NUM_THREADS 20
#define CALLS_PER_THREAD 10

int target_syscall;
int dev_null_fd = -1; // File descriptor di sicurezza

void *spam_syscall(void *arg) {
    for (int i = 0; i < CALLS_PER_THREAD; i++) {
        
        if (target_syscall == SYS_write) {
            // Test sicuro: Scrive 1 byte su /dev/null
            syscall(SYS_write, dev_null_fd, "X", 1);
            
        } else if (target_syscall == SYS_openat) {
            // Test sicuro: Apre /dev/null in sola lettura e lo chiude
            int fd = syscall(SYS_openat, AT_FDCWD, "/dev/null", O_RDONLY, 0);
            if (fd >= 0) close(fd);
            
        } else {
            // Default
            syscall(target_syscall);
        }
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Uso: %s <syscall_num>\n", argv[0]);
        return 1;
    }
    
    target_syscall = atoi(argv[1]);

    // Se il target è la sys_write, apriamo /dev/null
    if (target_syscall == SYS_write) {
        dev_null_fd = open("/dev/null", O_WRONLY);
        if (dev_null_fd < 0) {
            perror("[-] Impossibile aprire /dev/null per il sandboxing");
            return 1;
        }
    }

    printf("[*] Stress Test: Avvio %d thread (10 chiamate ciascuno) su Syscall %d...\n", 
            NUM_THREADS, target_syscall);
    
    pthread_t threads[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++) {
        if (pthread_create(&threads[i], NULL, spam_syscall, NULL) != 0) {
            perror("[-] Errore creazione thread");
            return 1;
        }
    }
    
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    if (dev_null_fd >= 0) {
        close(dev_null_fd);
    }

    printf("[+] Stress test completato.\n");
    return 0;
}