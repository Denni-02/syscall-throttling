#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <errno.h>

int main(int argc, char *argv[]) {
    // Controllo degli argomenti 
    if (argc != 3) {
        printf("[-] Errore sintassi.\n");
        printf("[*] Uso: %s <numero_syscall> <iterazioni>\n", argv[0]);
        printf("[*] Esempio: %s 83 10\n", argv[0]);
        return 1;
    }

    // Estrazione parametri 
    int target_syscall = atoi(argv[1]);
    int iterations = atoi(argv[2]);

    printf("[*] Invocazione multipla della syscall %d in corso (%d iterazioni)...\n", target_syscall, iterations);
    
    for (int i = 1; i <= iterations; i++) {
        long res = syscall(target_syscall, NULL, 0); // Passiamo argomenti dummy per evitare errori di EINVAL
        
        if (res == -1) {
            printf("[%d] [-] Risultato = -1, Errno = %d\n", i, errno);
        } else {
            printf("[%d] [+] Risultato = %ld\n", i, res);
        }
    }
    
    printf("[*] Test di stress completato.\n");
    return 0;
}