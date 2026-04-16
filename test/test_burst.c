#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>

int main(int argc, char *argv[]) {
    int syscall_num = (argc > 1) ? atoi(argv[1]) : 39;
    printf("[Burst] Inizio 100 chiamate a raffica sulla syscall %d...\n", syscall_num);
    for (int i = 0; i < 100; i++) {
        syscall(syscall_num);
    }
    printf("[Burst] Completato.\n");
    return 0;
}