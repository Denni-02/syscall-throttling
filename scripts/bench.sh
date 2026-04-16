#!/bin/bash

# Rilevamento percorsi assoluti
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)
BASE_DIR=$(dirname "$SCRIPT_DIR")
CLI_TOOL="$BASE_DIR/userspace/cli_tool"
SPAMMER="$BASE_DIR/test/spammer"
OUTPUT_FILE="$SCRIPT_DIR/benchmark_raw_data.txt"

SYSCALL=39       
MAX_CALLS=1000   
CALLS_PER_THREAD=500000 # Mezzo milione di chiamate per tenere vivo il thread

# Pulizia report precedente
echo "=== INIZIO BENCHMARK SYSCALL DEFENDER ===" > "$OUTPUT_FILE"
date >> "$OUTPUT_FILE"

# Setup Regola Mirata: Prende il tuo UID reale e colpisce solo spammer
CURRENT_UID=$(id -u)
echo "[*] Configurazione regola di stress per UID $CURRENT_UID e processo 'spammer'..."
sudo "$CLI_TOOL" -d > /dev/null 2>&1 
sudo "$CLI_TOOL" -s $SYSCALL -m $MAX_CALLS -u $CURRENT_UID -p "spammer" > /dev/null 2>&1
sudo "$CLI_TOOL" -e > /dev/null 2>&1

THREAD_COUNTS=(1 10 50 100)

for THREADS in "${THREAD_COUNTS[@]}"; do
    echo -e "\n=======================================================" | tee -a "$OUTPUT_FILE"
    echo " TEST: $THREADS Thread Paralleli" | tee -a "$OUTPUT_FILE"
    echo "=======================================================" | tee -a "$OUTPUT_FILE"

    # Lanciamo lo spammer in background
    sudo perf stat -e context-switches,cpu-clock,task-clock "$SPAMMER" $THREADS $CALLS_PER_THREAD 2>> "$OUTPUT_FILE" &
    PERF_PID=$!
    
    # Aspettiamo 1 secondo pieno affinché il Throttling entri in azione pesantemente
    sleep 1 

    # SNAPSHOT LIVE: Leggiamo i dati dal Ring 0
    echo -e "\n[!] Snapshot Hardware (LIVE) dal Ring 0:" >> "$OUTPUT_FILE"
    sudo "$CLI_TOOL" -g $SYSCALL >> "$OUTPUT_FILE"

    # Attendiamo la fine del test corrente
    wait $PERF_PID
    
    echo "[+] Test con $THREADS thread completato."
    sleep 1
done

echo -e "\n[*] Benchmark terminato. Risultati in: $OUTPUT_FILE"