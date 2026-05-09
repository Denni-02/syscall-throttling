#include <unistd.h>
#include <sys/syscall.h>

int main(void) {
    /* Rimane bloccato qui finché il modulo non lo sveglia */
    while (1) syscall(39);
    return 0;
}