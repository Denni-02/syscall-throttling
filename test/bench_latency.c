/**
 * @file bench_latency.c
 * @brief Benchmark di Latenza Hardware
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/syscall.h>
#include <fcntl.h>

#define ITERATIONS 1000000

int main(int argc, char *argv[]) {
    struct timespec start, end;
    long long total_ns = 0;
    long long max_ns = 0;
    
    int syscall_num = 39; // sys_getpid di default
    if (argc > 1) {
        syscall_num = atoi(argv[1]);
    }

    int dev_null_fd = -1;
    if (syscall_num == SYS_write) {
        dev_null_fd = open("/dev/null", O_WRONLY);
        if (dev_null_fd < 0) {
            perror("[-] Impossibile aprire /dev/null per il benchmark");
            return 1;
        }
    }

    printf("[*] Misurazione Latenza Syscall %d...\n", syscall_num);
    printf("[*] Iterazioni: %d\n", ITERATIONS);

    for (int i = 0; i < ITERATIONS; i++) {
        long long delta_ns = 0;

        if (syscall_num == SYS_write) {
            // Misuriamo il tempo della syscall
            clock_gettime(CLOCK_MONOTONIC, &start);
            syscall(SYS_write, dev_null_fd, "X", 1);
            clock_gettime(CLOCK_MONOTONIC, &end);
            
        } else if (syscall_num == SYS_openat) {
            clock_gettime(CLOCK_MONOTONIC, &start);
            int fd = syscall(SYS_openat, AT_FDCWD, "/dev/null", O_RDONLY, 0);
            clock_gettime(CLOCK_MONOTONIC, &end);
            
            if (fd >= 0) close(fd);
            
        } else {
            clock_gettime(CLOCK_MONOTONIC, &start);
            syscall(syscall_num);
            clock_gettime(CLOCK_MONOTONIC, &end);
        }

        delta_ns = (end.tv_sec - start.tv_sec) * 1000000000LL + (end.tv_nsec - start.tv_nsec);
        total_ns += delta_ns;

        if (delta_ns > max_ns) {
            max_ns = delta_ns;
        }
    }

    if (dev_null_fd >= 0) {
        close(dev_null_fd);
    }

    long long avg_ns = total_ns / ITERATIONS;

    printf("\n[+] Risultati Latenza Ring 3 -> Ring 0 -> Ring 3:\n");
    printf("    Media: %lld ns per chiamata\n", avg_ns);
    printf("    Picco: %lld ns\n\n", max_ns);

    FILE *f = fopen("latency.txt", "w");
    if (f) {
        fprintf(f, "calls,%d\navg_ns,%lld\nmax_ns,%lld\n", ITERATIONS, avg_ns, max_ns);
        fclose(f);
    }

    printf("[+] Dati salvati in latency.txt\n");
    return 0;
}