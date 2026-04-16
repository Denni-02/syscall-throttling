/**
 * @file test_throttling.c
 * @brief Collaudo Funzionale del Motore FIFO
 * Esegue una sequenza mirata di chiamate di sistema per verificare
 * che il Reference Monitor in Ring 0 blocchi correttamente i thread
 * solo al superamento della soglia MAX, rispettando il reset dell'epoca
 * temporale guidato dal Kernel Timer in Softirq.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <errno.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("[-] Errore sintassi.\n");
        printf("[*] Uso: %s <numero_syscall>\n", argv[0]);
        return 1;
    }

    int target_syscall = atoi(argv[1]);

    printf("[*] --- INIZIO TEST FUNZIONALE THROTTLING (Syscall %d) ---\n", target_syscall);

    // FASE 1: Sotto-soglia
    printf("\n[*] FASE 1: Lancio 2 chiamate veloci. Non dovrebbero bloccarsi.\n");
    for (int i = 1; i <= 2; i++) {
        syscall(target_syscall, NULL, 0);
        printf("[+] Chiamata FASE 1 - Iterazione %d completata.\n", i);
    }

    // FASE 2: Attesa reset epoca (1 secondo hardware)
    printf("\n[*] FASE 2: Pausa di 1.5 secondi...\n");
    printf("[*] Attesa del Kernel Timer (Softirq) per il reset dei contatori nel Ring 0.\n");
    usleep(1500000); 

    // FASE 3: Sfondamento soglia
    printf("\n[*] FASE 3: Lancio 4 chiamate veloci. Dalla terza in poi DEVONO congelarsi in FIFO.\n");
    for (int i = 1; i <= 4; i++) {
        printf("[>] Chiamata %d...\n", i);
        syscall(target_syscall, NULL, 0);
        printf("[+] Chiamata FASE 3 - Iterazione %d completata (Pass/Svegliato).\n", i);
    }

    printf("\n[*] --- TEST CONCLUSO ---\n");
    return 0;
}