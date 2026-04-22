#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/time.h>

#define C_GREEN  "\x1b[32m"
#define C_YELLOW "\x1b[33m"
#define C_CYAN   "\x1b[36m"
#define C_RESET  "\x1b[0m"

long long get_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000LL) + (tv.tv_usec / 1000);
}

int main(int argc, char *argv[]) {
    printf("\n" C_CYAN "=====================================================" C_RESET "\n");
    printf(C_CYAN " [TEST SUITE] VALIDAZIONE LOGICA TEMPORALE (Syscall 39)" C_RESET "\n");
    printf(C_CYAN "=====================================================" C_RESET "\n\n");

    /* FASE 1 */
    printf("[*] FASE 1: Lancio 2 chiamate veloci (Sotto Soglia MAX=3)...\n");
    for (int i = 1; i <= 2; i++) {
        long long start = get_time_ms();
        syscall(39); // Usiamo solo getpid, nuda e cruda
        long long delay = get_time_ms() - start;

        if (delay > 50) printf("    " C_YELLOW "[~] Chiamata %d: THROTTLING! (%lld ms)" C_RESET "\n", i, delay);
        else printf("    " C_GREEN "[+] Chiamata %d: FAST-PATH   (%lld ms)" C_RESET "\n", i, delay);
    }

    /* FASE 2 */
    printf("\n" C_CYAN "[*] FASE 2: Pausa 1.5s (Attesa Reset Epoca da SoftIRQ)..." C_RESET "\n\n");
    usleep(1500000); 

    /* FASE 3 */
    printf("[*] FASE 3: Lancio 4 chiamate veloci (Superamento Soglia)...\n");
    for (int i = 1; i <= 4; i++) {
        long long start = get_time_ms();
        syscall(39);
        long long delay = get_time_ms() - start;

        if (delay > 50) printf("    " C_YELLOW "[~] Chiamata %d: THROTTLING! (%lld ms)" C_RESET "\n", i, delay);
        else printf("    " C_GREEN "[+] Chiamata %d: FAST-PATH   (%lld ms)" C_RESET "\n", i, delay);
    }

    printf("\n" C_CYAN "=====================================================" C_RESET "\n");
    printf(C_CYAN " TEST CONCLUSO" C_RESET "\n\n");
    return 0;
}