#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <errno.h>

int main(int argc, char *argv[]) {
    // Controllo argomenti 
    if (argc != 2) {
        printf("[-] Errore sintassi.\n");
        printf("[*] Uso: %s <numero_syscall>\n", argv[0]);
        printf("[*] Esempio: %s 83\n", argv[0]);
        return 1;
    }

    int target_syscall = atoi(argv[1]);

    printf("[*] --- INIZIO TEST AVANZATO DI THROTTLING (Syscall %d) ---\n", target_syscall);

    // Restiamo sotto il radar
    printf("\n[*] FASE 1: Lancio 2 chiamate veloci. Non dovrebbero bloccarsi.\n");
    for (int i = 1; i <= 2; i++) {
        syscall(target_syscall, NULL, 0);
        printf("[+] Chiamata FASE 1 - Iterazione %d completata.\n", i);
    }

    // Reset
    printf("\n[*] FASE 2: Pausa di 1.5 secondi...\n");
    printf("[*] Questo darà tempo al Kernel Timer (Softirq) di azzerare i contatori nel Ring 0.\n");
    usleep(1500000); 

    // Saturazione
    printf("\n[*] FASE 3: Lancio 4 chiamate veloci. Dalla terza in poi DEVONO congelarsi.\n");
    for (int i = 1; i <= 4; i++) {
        printf("[>] Chiamata %d...\n", i);
        syscall(target_syscall, NULL, 0);
        printf("[+] Chiamata FASE 3 - Iterazione %d completata (Pass/Svegliato).\n", i);
    }

    printf("\n[*] --- TEST CONCLUSO ---\n");
    return 0;
}