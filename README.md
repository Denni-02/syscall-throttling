# Syscall Throttling - Advanced Operating Systems

**Autore:** Dennis Mariani 
**Corso:** Sistemi Operativi Avanzati (A.A. 2025/2026)  

---

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

Per compilare e iniettare il modulo nel kernel, utilizzare lo script fornito:

```bash
./scripts/deploy.sh
```

Per rimuovere il modulo e pulire l'ambiente:

```bash
./scripts/teardown.sh
```