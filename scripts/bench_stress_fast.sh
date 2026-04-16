#!/bin/bash
# Script per l'automazione dello Stress Test e Profiling (Fase 3.2)

set -e

echo "========================================================="
echo " STRESS TEST & CONTEXT SWITCH PROFILING"
echo "========================================================="

# 1. Compilazione
echo "[*] STEP 1: Compilazione bench_stress..."
gcc -Wall -O3 test/bench_stress.c -o test/bench_stress -pthread

# 2. Caricamento Modulo
echo "[*] STEP 2: Reload Modulo..."
make reload > /dev/null

# 3. Configurazione Regola
# Limitiamo a 5 chiamate al secondo per forzare i 20 thread ad accodarsi
echo "[*] STEP 3: Configurazione Regola (Syscall 39, MAX 5)..."
sudo ./userspace/cli_tool -s 39 -m 5 -p bench_stress

# 4. Esecuzione Profiling con Perf
echo -e "\n[*] STEP 4: Avvio Profiling Hardware (perf stat)..."
echo "---------------------------------------------------------"
# Eseguiamo perf stat misurando i context switches
sudo perf stat -e context-switches,cpu-migrations ./test/bench_stress 39
echo "---------------------------------------------------------"

# 5. Pulizia
echo -e "\n[*] STEP 5: Rimozione regola e scaricamento modulo..."
make unload > /dev/null
echo "Test concluso."