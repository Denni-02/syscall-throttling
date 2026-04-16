# Syscall Throttling - Advanced Operating Systems

**Autore:** Dennis Mariani 
**Corso:** Sistemi Operativi Avanzati (A.A. 2025/2026)  

---

## Indice
- [Descrizione del Progetto](#descrizione-del-progetto)
- [Architettura del Sottosistema](#architettura-del-sottosistema)
- [Struttura del Repository](#struttura-del-repository)
- [Modalità di Compilazione e Design Pattern](#modalità-di-compilazione-e-design-pattern)
- [Deploy e Teardown](#deploy-e-teardown)
- [Utilizzo e Configurazione (CLI Tool)](#utilizzo-e-configurazione-cli-tool)
- [Debugging e Log del Kernel](#debugging-e-log-del-kernel)

---

## Descrizione del Progetto

Questo repository contiene l'implementazione di un Modulo Kernel Linux (LKM) progettato per agire come un *Reference Monitor* per le chiamate di sistema (System Call Throttling). 

Il modulo permette di intercettare e limitare dinamicamente la frequenza di invocazione di specifiche syscall in base a:
- Nome dell'eseguibile (Program Name)
- Identificativo dell'utente (User ID)

Se il volume di chiamate supera una soglia massima (MAX) configurabile, all'interno di una finestra temporale di 1 secondo, i thread invocanti vengono temporaneamente sospesi (throttling) per mitigare abusi o comportamenti anomali, garantendo al contempo le prestazioni e la stabilità del sistema operativo.

## Architettura del Sottosistema

Il progetto è diviso in:
- **Kernel Space (Ring 0):** Il motore principale. Implementa l'hooking della `sys_call_table` disabilitando le protezioni hardware (bit WP del registro CR0). 
- **User Space (Ring 3):** Un tool di amministrazione CLI che comunica con il kernel tramite un Character Device (`/dev/syscall_defender`) sfruttando comandi `ioctl` per l'inserimento o la rimozione delle policy a runtime.

## Struttura del Repository

- `kmod/`: Codice sorgente del Modulo Kernel (Ring 0).
- `userspace/`: Tool da riga di comando per l'amministrazione del Reference Monitor (Ring 3).
- `include/`: File header e API condivise tra User Space e Kernel Space.
- `scripts/`: Script bash per automazioni di testing avanzato e stress-test.

---

## Modalità

L'ambiente di sviluppo e test è basato su **Ubuntu Server 22.04 LTS** (Kernel 5.15.x).

Per compilare il progetto sono necessari i pacchetti essenziali di build e gli header del kernel:

```bash
sudo apt update
sudo apt install build-essential linux-headers-$(uname -r)
```

Il modulo è stato realizzato per permettere l'abilitazione di differenti modalità (Sincronizzazione e Hacking della Syscall Table) a tempo di compilazione tramite appositi flag nel Makefile.

**Opzioni di Sincronizzazione (RCU vs Spinlock)**: Il motore del database interno (Ring 0) implementa due meccanismi per la sincronizzazione multi-core:
- **RCU (Default)**: Utilizza Read-Copy-Update, garantendo l'assenza di lock in lettura e massimizzando le prestazioni del Reference Monitor.
- **Spinlock**: Utilizza un lock globale in lettura/scrittura.

**Opzioni di Discovery della Syscall Table (Kprobes vs Scanner)**: L'intercettazione del flusso di esecuzione richiede la scoperta dinamica in memoria della sys_call_table. Il modulo offre due algoritmi:
- **Kprobes (Default)**: Metodo che sfrutta le API di debugging del kernel per risolvere dinamicamente i simboli oscurati.
- **Scanner**: Metodo brute-force che attraversa manualmente la gerarchia hardware della Memory Management Unit (MMU) — navigando tra le Page Tables (PML4, PUD, PMD, PTE) per evitare Page Fault fatali — scansionando la memoria alla ricerca di firme note (es. sys_ni_syscall).


## Comandi di deploy

Il sistema di build riconosce le variabili d'ambiente `SYNC` e `DISCOVERY`. Se non specificate, verranno utilizzati i default (`rcu` e `kprobes`). 

**Compilazione di default (RCU + Kprobes):**

```bash
make reload
```

**Compilazione variando solo l'algoritmo di Discovery (RCU + Scanner):**
```bash
DISCOVERY=scanner make reload
```
**Compilazione variando solo il modello di Sincronizzazione (Spinlock + Kprobes):**
```bash
SYNC=spinlock make reload
```

**Compilazione alterando entrambi i design pattern (Spinlock + Scanner):**
```bash
SYNC=spinlock DISCOVERY=scanner make reload
```

Per scaricare il modulo in sicurezza (attendendo l'uscita dei thread grazie al Safe Unloading), rimuovere il device node e pulire i file di compilazione:

```bash
make unload
```

---

## Utilizzo e Configurazione (CLI Tool)

L'interazione con il modulo Ring 0 avviene tramite un Character Device generato dinamicamente (`/dev/syscall_defender`) utilizzando la system call `ioctl`. 

È stato sviluppato un tool C dedicato nello User Space (`userspace/cli_tool.c`) per l'amministrazione delle policy di throttling, l'estrazione delle statistiche e il controllo globale del motore di throttling (guidato da Kernel Timers in Softirq).

**Sicurezza (Effective UID):** Come richiesto dalle specifiche, l'accesso al device e la configurazione delle regole richiedono rigorosamente i privilegi di amministratore. Il modulo verifica le credenziali del chiamante (`current_euid()`). Qualsiasi tentativo di accesso da parte di utenti non privilegiati verrà intercettato e respinto con errore `-EPERM` (Operation not permitted).

Per compilare il pannello di controllo utente:

```bash
cd userspace
make
```
**1. Inserimento Regole di Throttling**
La sintassi del comando richiede privilegi di root e accetta parametri dinamici tramite flag:

```bash
sudo ./cli_tool -s <syscall_num> -m <max_calls> [-u <uid>] [-p <program>]
```

**Esempio:** Limitare la system call 2 (sys_open) a un massimo di 10 chiamate al secondo per l'utente con UID 1000:

```bash
sudo ./cli_tool -s 2 -m 10 -u 1000
```

**2. Estrazione Statistiche** 
Il modulo tiene traccia delle tempistiche di sospensione dei processi calcolando i cicli hardware della CPU tramite l'istruzione assembly rdtscp (superando il problema della migrazione della CPU).

Per estrarre il report statistico di una regola:

```bash
sudo ./cli_tool -g <syscall_num>
```

L'output mostrerà il picco di ritardo (in cicli di clock), l'identificativo della vittima (UID e Programma) e il numero massimo e medio di thread bloccati simultaneamente per quella syscall.

**3. Visualizzazione Regole Attive**
Il tool permette di interrogare in tempo reale il database in Ring 0 (tramite un'allocazione sicura sull'heap del kernel) per ottenere un prospetto tabellare delle policy attualmente in enforcement.

```bash
sudo ./cli_tool -l
```

**4. Interruttore Globale**
È possibile bypassare istantaneamente le regole di throttling senza dover disinstallare il modulo o cancellare le regole dalla RAM:

```bash
sudo ./cli_tool -d  # Disabilita il monitor
sudo ./cli_tool -e  # Riabilita il monitor
```

---

## Debugging e Log del Kernel

Le operazioni di livello Kernel (attivazioni regole, calcoli hardware, intercettazioni) non vengono stampate sullo standard output dell'utente, ma scritte nel ring buffer del sistema operativo.

Per visualizzare i log del modulo in tempo reale:

```bash
sudo dmesg -wH | grep Syscall_Throttling
```

Per leggere le ultime attività registrate:

```bash
sudo dmesg | tail -n 30
```