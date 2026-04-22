/**
 * @file test_burst.c
 * @brief Test Suite: Burst & Deny All
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <fcntl.h>

#define C_GREEN  "\x1b[32m"
#define C_CYAN   "\x1b[36m"
#define C_YELLOW "\x1b[33m"
#define C_RESET  "\x1b[0m"

#define BURST_SIZE 100

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Uso: %s <syscall_num>\n", argv[0]);
        return 1;
    }

    int target_syscall = atoi(argv[1]);
    int dev_null_fd = open("/dev/null", O_WRONLY);

    printf("\n" C_CYAN "=====================================================" C_RESET "\n");
    printf(C_CYAN " [TEST SUITE] VALIDAZIONE BURST & CORNER CASES (Syscall %d)" C_RESET "\n", target_syscall);
    printf(C_CYAN "=====================================================" C_RESET "\n\n");

    printf("[*] Inizio raffica di %d chiamate in un singolo ciclo FOR...\n", BURST_SIZE);
    printf("[*] Se la policy e' MAX=0, questo processo DEVE congelarsi immediatamente.\n\n");

    for (int i = 1; i <= BURST_SIZE; i++) {
        // Sandboxing di sicurezza
        if (target_syscall == SYS_write) {
            syscall(SYS_write, dev_null_fd, "X", 1);
        } else {
            syscall(target_syscall);
        }

        // Se arriviamo qui, la chiamata non è stata bloccata permanentemente
        printf(C_YELLOW "    [>] Chiamata %d elaborata." C_RESET "\n", i);
    }

    printf("\n" C_GREEN "[+] Raffica completata o sbloccata forzatamente." C_RESET "\n\n");

    if (dev_null_fd >= 0) close(dev_null_fd);
    return 0;
}