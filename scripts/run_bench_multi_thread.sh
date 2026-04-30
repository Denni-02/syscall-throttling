#!/bin/bash
set -e

CSV_FILE="heavy_thread_stats.csv"
RUNS=20
SYSCALLS=(39) # Usiamo solo getpid per questo test di forza

echo "Syscall,Sync,Run,Time_ms" > $CSV_FILE

echo "[*] Compilazione benchmark..."
gcc -Wall -O3 test/bench_multi_thread.c -o test/bench_multi_thread -pthread

# ---------------------------------------------------------
# TEST RCU
# ---------------------------------------------------------
echo "========================================"
echo "[*] Caricamento Modulo: RCU (Lock-Free)"
echo "========================================"
make reload > /dev/null

echo "[*] Iniezione regole ..."
for i in {1..20}; do
    sudo ./userspace/cli_tool -s 39 -m 1000 -p processo_$i > /dev/null 2>&1
    echo -n "." # Stampa un puntino per ogni regola inserita per mostrare il progresso
done
echo ""

echo "[*] Esecuzione ..."
for i in $(seq 1 $RUNS); do
    TIME_MS=$(./test/bench_multi_thread 39 | grep "TEMPO TOTALE" | awk '{print $6}')
    echo "39,RCU,$i,$TIME_MS" >> $CSV_FILE
    echo -ne "\r    -> Esecuzione RCU $i/$RUNS completata ($TIME_MS ms)"
done
echo ""

# Pulizia
sudo ./userspace/cli_tool -d > /dev/null

# ---------------------------------------------------------
# TEST SPINLOCK
# ---------------------------------------------------------
echo "========================================"
echo "[*] Caricamento Modulo: SPINLOCK GLOBALE"
echo "========================================"
SYNC=spinlock make reload > /dev/null

echo "[*] Iniezione regole ..."
for i in {1..20}; do
    sudo ./userspace/cli_tool -s 39 -m 1000 -p processo_$i > /dev/null
    echo -n "." # Stampa un puntino per ogni regola inserita per mostrare il progresso
done
echo ""

echo "[*] Esecuzione ..."
for i in $(seq 1 $RUNS); do
    TIME_MS=$(./test/bench_multi_thread 39 | grep "TEMPO TOTALE" | awk '{print $6}')
    echo "39,Spinlock,$i,$TIME_MS" >> $CSV_FILE
    echo -ne "\r    -> Esecuzione Spinlock $i/$RUNS completata ($TIME_MS ms)"
done
echo ""

# Pulizia finale
sudo ./userspace/cli_tool -d > /dev/null
echo "[+] Raccolta Dati completata! Risultati salvati in: $CSV_FILE"