#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <errno.h>

int main() {
    printf("[*] Invocazione della syscall 134 in corso...\n");
    
    /* Chiamiamo direttamente l'hardware */
    long res = syscall(134);
    
    if (res == -1) {
        printf("[-] Esito: NESSUN HOOK. Risultato = %ld, Errno = %d (Funzione non implementata)\n", res, errno);
        printf("[-] Il kernel originale è ancora intatto.\n");
    } else if (res == 0) {
        printf("[+] Esito: SUCCESSO! Risultato = %ld, Errno = %d\n", res, errno);
        printf("[+] La nostra my_dummy_syscall ha preso il controllo!\n");
    } else {
        printf("[?] Esito anomalo. Risultato = %ld\n", res);
    }
    
    return 0;
}
