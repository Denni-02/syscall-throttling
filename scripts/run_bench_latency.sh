#!/bin/bash
# Script per il benchmarking rapido dell'overhead

set -e

# Se non passi parametri, usa la 39 di default
TARGET_SYSCALL=${1:-39}

echo "========================================================="
echo " BENCHMARK OVERHEAD: BASELINE vs RING 0 (Syscall $TARGET_SYSCALL)"
echo "========================================================="

echo -e "\n[*] STEP 1: Compilazione bench_latency..."
gcc -Wall -O3 test/bench_latency.c -o test/bench_latency

echo -e "\n[*] STEP 2: Misurazione BASELINE (Kernel originale)..."
make unload > /dev/null 2>&1 || true
./test/bench_latency $TARGET_SYSCALL
BASELINE_AVG=$(grep "avg_ns" latency.txt | cut -d',' -f2)

echo -e "\n[*] STEP 3: Misurazione OVERHEAD (Modulo Attivo)..."
make load > /dev/null
./test/bench_latency $TARGET_SYSCALL
OVERHEAD_AVG=$(grep "avg_ns" latency.txt | cut -d',' -f2)

echo -e "\n========================================================="
echo " RISULTATI FINALI (Media su 1M di iterazioni)"
echo "---------------------------------------------------------"
echo "  [A] Baseline Kernel  : $BASELINE_AVG ns"
echo "  [B] Modulo Attivo    : $OVERHEAD_AVG ns"
echo "---------------------------------------------------------"
DIFF=$((OVERHEAD_AVG - BASELINE_AVG))
echo "  >>> COSTO DELL'HOOK : $DIFF ns"
echo "========================================================="