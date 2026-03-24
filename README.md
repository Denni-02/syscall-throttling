# Syscall Throttling - Advanced Operating Systems

**Autore:** Dennis Mariani 
**Corso:** Sistemi Operativi Avanzati (A.A. 2025/2026)  

## Descrizione del Progetto

Questo repository contiene l'implementazione di un Modulo Kernel Linux (LKM) progettato per agire come un *Reference Monitor* per le chiamate di sistema (System Call Throttling). 

Il modulo permette di intercettare e limitare dinamicamente la frequenza di invocazione di specifiche syscall in base a:
- Nome dell'eseguibile (Program Name)
- Identificativo dell'utente (User ID)

Se il volume di chiamate supera una soglia massima (MAX) configurabile, all'interno di una finestra temporale di 1 secondo, i thread invocanti vengono temporaneamente sospesi (throttling) per mitigare abusi o comportamenti anomali, garantendo al contempo le prestazioni e la stabilità del sistema operativo.

---

## Struttura del Repository

- `kmod/`: Codice sorgente del Modulo Kernel (Ring 0).
- `userspace/`: Tool da riga di comando per l'amministrazione del demone (Ring 3).
- `include/`: File header e API condivise tra User Space e Kernel Space.
- `scripts/`: Script bash per l'automazione del build, deploy e teardown.

---

## Requisiti e Compilazione

L'ambiente di sviluppo e test è basato su **Ubuntu Server 22.04 LTS** (Kernel 5.15.x).

Per compilare il progetto sono necessari i pacchetti essenziali di build e gli header del kernel:

```bash
sudo apt update
sudo apt install build-essential linux-headers-$(uname -r)
```
**Opzioni di Sincronizzazione (RCU vs Spinlock)**: Il motore del database interno (Ring 0) implementa due differenti design pattern per la sincronizzazione multi-core, selezionabili a tempo di compilazione.

Per compilare e iniettare il modulo utilizzando il design ottimizzato RCU (Read-Copy-Update), che garantisce l'assenza di lock in lettura (Zero Lock Contention) massimizzando le prestazioni del Reference Monitor:

```bash
./scripts/deploy.sh
```
Per compilare e iniettare il modulo utilizzando uno Spinlock Globale per lettori e scrittori :

```bash
SYNC=spinlock ./scripts/deploy.sh
```

Per rimuovere il modulo, liberare la memoria (Garbage Collection) e pulire l'ambiente:

```bash
./scripts/teardown.sh
```

---

## Utilizzo e Configurazione (CLI Tool)

L'interazione con il modulo Ring 0 avviene tramite un Character Device generato dinamicamente (`/dev/syscall_defender`) utilizzando la system call `ioctl`. 

È stato sviluppato un tool C dedicato nello User Space (`userspace/cli_tool.c`) per l'amministrazione delle policy di throttling.

**Sicurezza (Effective UID):** Come richiesto dalle specifiche, l'accesso al device e la configurazione delle regole richiedono rigorosamente i privilegi di amministratore. Il modulo verifica le credenziali del chiamante (`current_euid()`). Qualsiasi tentativo di accesso da parte di utenti non privilegiati verrà intercettato e respinto con errore `-EPERM` (Operation not permitted).

Per compilare il pannello di controllo utente:

```bash
cd userspace
make
```

La sintassi del comando richiede privilegi di root e accetta parametri dinamici tramite flag:

```bash
sudo ./cli_tool -s <syscall_num> -m <max_calls> [-u <uid>] [-p <program>]
```

**Esempio:** Limitare la system call 2 (sys_open) a un massimo di 10 chiamate al secondo per l'utente con UID 1000:

```bash
sudo ./cli_tool -s 2 -m 10 -u 1000
```