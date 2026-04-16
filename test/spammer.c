#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <pthread.h>

int num_threads = 10;
int calls_per_thread = 1000;

// La funzione che ogni thread eseguirà in parallelo
void* spam_syscall(void* arg) {
    for (int i = 0; i < calls_per_thread; i++) {
        syscall(39); // Invocazione diretta di sys_getpid
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc >= 2) num_threads = atoi(argv[1]);
    if (argc >= 3) calls_per_thread = atoi(argv[2]);

    printf("[*] Avvio Spammer: %d Thread, %d Chiamate per Thread...\n", num_threads, calls_per_thread);

    pthread_t *threads = malloc(num_threads * sizeof(pthread_t));

    // Crea i thread
    for (int i = 0; i < num_threads; i++) {
        pthread_create(&threads[i], NULL, spam_syscall, NULL);
    }

    // Aspetta che abbiano finito
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    free(threads);
    printf("[+] Spammer terminato con successo.\n");
    return 0;
}