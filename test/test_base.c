#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <errno.h>

#define ITERATIONS 1

int main() {
    printf("[*] Invocazione multipla della syscall 134 in corso (%d iterazioni)...\n", ITERATIONS);
    
    for (int i = 1; i <= ITERATIONS; i++) {
        long res = syscall(134);
        
        if (res == -1) {
            printf("[%d] [-] NESSUN HOOK. Risultato = %ld, Errno = %d (Funzione non implementata)\n", i, res, errno);
        } else if (res == 0) {
            printf("[%d] [+] SUCCESSO! Risultato = 0\n", i);
        } else {
            printf("[%d] [?] Esito anomalo. Risultato = %ld\n", i, res);
        }
    }
    
    printf("[*] Test di stress completato.\n");
    return 0;
}
