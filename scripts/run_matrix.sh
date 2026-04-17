#!/bin/bash

set -e

CSV_FILE="benchmark_results.csv"

# Inizializziamo il file CSV con le intestazioni
echo "Syscall,Configurazione,Baseline_ns,Bypass_ns,Overhead_ns,Context_Switches" > $CSV_FILE

SYSCALLS=(39 257 1) # getpid, openat, write
CONFIGS=("rcu kprobes" "spinlock kprobes" "rcu scanner")

echo "========================================================="
echo " AVVIO MATRIX RUNNER                                     "
echo "========================================================="

# Compilazione Esecutori
echo "[*] Compilazione eseguibili di test (Ring 3)..."
gcc -Wall -O3 test/bench_latency.c -o test/bench_latency
gcc -Wall -O3 test/bench_stress.c -o test/bench_stress -pthread

for SYS in "${SYSCALLS[@]}"; do
    echo -e "\n[*] ================= SYSCALL $SYS ================="

    # 1. BASELINE
    echo "[*] Calcolo Baseline Kernel (Modulo Scaricato)..."
    make unload > /dev/null 2>&1 || true
    ./test/bench_latency $SYS > /dev/null
    BASELINE=$(grep "avg_ns" latency.txt | cut -d',' -f2)

    for CONF in "${CONFIGS[@]}"; do
        read -r SYNC DISC <<< "$CONF"
        CONFIG_NAME="${SYNC}_${DISC}"
        echo -e "\n  [>] Test Configurazione: $CONFIG_NAME"

        # 2. BYPASS (Misurazione dell'Overhead dell'Hook)
        echo "      - Compilazione e Caricamento Modulo..."
        # Usiamo le variabili d'ambiente per guidare il Makefile
        SYNC=$SYNC DISCOVERY=$DISC make reload > /dev/null

        echo "      - Misurazione Overhead (Bypass Mode)..."
        ./test/bench_latency $SYS > /dev/null
        BYPASS=$(grep "avg_ns" latency.txt | cut -d',' -f2)
        OVERHEAD=$((BYPASS - BASELINE))

        # 3. THROTTLED (Stress Test e Efficienza Wait Queue)
        echo "      - Impostazione Regola di Throttling (MAX=5)..."
        sudo ./userspace/cli_tool -s $SYS -m 5 -p bench_stress > /dev/null

        echo "      - Esecuzione Stress Test e profilazione Context Switches..."
        # Estraiamo i context switches con perf (redirezionando stderr su stdout)
        # awk pulisce l'output per prendere solo il numero e tr rimuove i punti delle migliaia
        CS_RAW=$(sudo perf stat -e context-switches ./test/bench_stress $SYS 2>&1 | grep "context-switches")
        CS=$(echo $CS_RAW | awk '{print $1}' | tr -d '.,') 

        # 4. SALVATAGGIO DATI
        echo "$SYS,$CONFIG_NAME,$BASELINE,$BYPASS,$OVERHEAD,$CS" >> $CSV_FILE
        echo "      Dati estratti: Overhead = ${OVERHEAD} ns | Context Switches = ${CS}"
    done
done

# Pulizia finale della macchina virtuale
make unload > /dev/null 2>&1 || true
rm -f latency.txt
echo -e "\n========================================================="
echo " MATRICE COMPLETATA! Risultati salvati in $CSV_FILE"
echo "========================================================="