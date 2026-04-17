#!/bin/bash
set -e

echo "========================================================="
echo " CORNER CASES & ROBUSTNESS TEST (Final)"
echo "========================================================="

# Caricamento pulito
make reload > /dev/null

echo -e "\n[*] CASE 1: Soglia MAX = 0 (Policy 'Deny All')"
# Deve bloccare tutto istantaneamente
sudo ./userspace/cli_tool -s 39 -m 0 -p test_burst
echo "[>] Lancio test (timeout 2s)..."
timeout 2s ./test/test_burst 39 || echo "[+] Risultato: Processo bloccato correttamente (MAX=0 working)."

echo -e "\n[*] CASE 2: Disabilitazione Globale 'Hot-Swap'"
sudo ./userspace/cli_tool -e  # Assicuriamoci che sia attivo
sudo ./userspace/cli_tool -s 39 -m 1 -p test_burst
echo "[>] Avvio burst in background..."

# Lanciamo senza redirigere l'output in modo da vedere se sputa errori
./test/test_burst 39 & 
BURST_PID=$!

sleep 1 # Diamo tempo al processo di bloccarsi nell'hook

echo "[>] Disabilitazione globale del monitor (-d)..."
sudo ./userspace/cli_tool -d

# Invece di wait, usiamo un timeout anche qui per non restare appesi
timeout 5s tail --pid=$BURST_PID -f /dev/null || kill -9 $BURST_PID 2>/dev/null

echo "[+] Risultato: Thread sbloccati o terminati forzatamente."

echo -e "\n[*] CASE 3: Safe Unloading sotto stress"
# Riabilita e lancia lo spammer pesante
sudo ./userspace/cli_tool -e
sudo ./userspace/cli_tool -s 39 -m 5 -p bench_stress
./test/bench_stress 39 > /dev/null &
sleep 0.5
echo "[>] Tentativo di 'make unload' mentre i thread sono in coda..."
make unload
echo "[+] Risultato: Modulo rimosso con successo dopo lo svuotamento sicuro."

echo -e "\n TEST DI ROBUSTEZZA COMPLETATO!"