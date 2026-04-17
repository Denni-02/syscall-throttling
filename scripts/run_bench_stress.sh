#!/bin/bash
# Script per l'automazione dello Stress Test

set -e

TARGET_SYSCALL=${1:-39}

echo "========================================================="
echo " STRESS TEST & CONTEXT SWITCH (Syscall $TARGET_SYSCALL)"
echo "========================================================="

echo "[*] STEP 1: Compilazione bench_stress..."
gcc -Wall -O3 test/bench_stress.c -o test/bench_stress -pthread

echo "[*] STEP 2: Reload Modulo..."
make reload > /dev/null

echo "[*] STEP 3: Configurazione Regola (Syscall $TARGET_SYSCALL, MAX 5)..."
sudo ./userspace/cli_tool -s $TARGET_SYSCALL -m 5 -p bench_stress

echo -e "\n[*] STEP 4: Avvio Profiling Hardware (perf stat)..."
echo "---------------------------------------------------------"
sudo perf stat -e context-switches,cpu-migrations ./test/bench_stress $TARGET_SYSCALL
echo "---------------------------------------------------------"

echo -e "\n[*] STEP 5: Rimozione regola e scaricamento modulo..."
make unload > /dev/null
echo "Test concluso."