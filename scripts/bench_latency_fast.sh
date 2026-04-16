#!/bin/bash
# Script per il benchmarking rapido dell'overhead 

# Esce in caso di errore
set -e

echo "========================================================="
echo " BENCHMARK OVERHEAD: BASELINE vs RING 0 HOOK"
echo "========================================================="

# 1. Compilazione del benchmark con massima ottimizzazione
echo -e "\n[*] STEP 1: Compilazione bench_latency con -O3..."
gcc -Wall -O3 test/bench_latency.c -o test/bench_latency

# 2. Misurazione Baseline (Senza Modulo)
echo -e "\n[*] STEP 2: Misurazione BASELINE (Kernel originale)..."
make unload > /dev/null 2>&1 || true
./test/bench_latency
# Salviamo il risultato della baseline per il confronto finale
BASELINE_AVG=$(grep "avg_ns" latency.txt | cut -d',' -f2)

# 3. Caricamento Modulo e Misurazione Overhead
echo -e "\n[*] STEP 3: Caricamento Modulo e misurazione OVERHEAD..."
make load > /dev/null
# Assicuriamoci che non ci siano regole attive per la syscall 39 (getpid)
# per misurare solo il costo dell'attraversamento dell'hook RCU
./test/bench_latency
OVERHEAD_AVG=$(grep "avg_ns" latency.txt | cut -d',' -f2)

# 4. Analisi Finale
echo -e "\n========================================================="
echo " RISULTATI FINALI (Media su 1M di iterazioni)"
echo "---------------------------------------------------------"
echo "  [A] Baseline Kernel  : $BASELINE_AVG ns"
echo "  [B] Modulo Attivo    : $OVERHEAD_AVG ns"
echo "---------------------------------------------------------"
DIFF=$((OVERHEAD_AVG - BASELINE_AVG))
echo "  >>> COSTO DELL'HOOK : $DIFF ns"
echo "========================================================="

if [ "$DIFF" -lt 50 ]; then
    echo -e "OTTIMO: Overhead trascurabile (< 50ns). L'architettura RCU è efficiente.\n"
else
    echo -e "NOTA: Overhead rilevato. Verifica se ci sono log dmesg attivi.\n"
fi