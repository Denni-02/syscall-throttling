#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <errno.h>

int main() {
    printf("[*] --- INIZIO TEST AVANZATO DI THROTTLING ---\n");

    /* FASE 1: Restiamo sotto il radar */
    printf("\n[*] FASE 1: Lancio 2 chiamate veloci (Limite = 3). Non dovrebbero bloccarsi.\n");
    for (int i = 1; i <= 2; i++) {
        syscall(134);
        printf("[+] Chiamata FASE 1 - Iterazione %d completata.\n", i);
    }

    /* FASE 2: Il Reset */
    printf("\n[*] FASE 2: Pausa di 1.5 secondi...\n");
    printf("[*] Questo darà tempo al Kthread di azzerare i contatori nel Ring 0.\n");
    usleep(1500000); /* 1.5 milioni di microsecondi */

    /* FASE 3: L'Attacco di saturazione */
    printf("\n[*] FASE 3: Lancio 4 chiamate veloci. La quarta DEVE congelarsi.\n");
    for (int i = 1; i <= 4; i++) {
        printf("[>] Sparo chiamata %d...\n", i);
        syscall(134);
        printf("[+] Chiamata FASE 3 - Iterazione %d completata (Svegliato!).\n", i);
    }

    printf("\n[*] --- TEST CONCLUSO ---\n");
    return 0;
}