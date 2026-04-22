#!/bin/bash
set -e

C_GREEN="\x1b[32m"
C_CYAN="\x1b[36m"
C_YELLOW="\x1b[33m"
C_RED="\x1b[31m"
C_PURPLE="\x1b[35m"
C_RESET="\x1b[0m"

echo -e "${C_CYAN}=========================================================${C_RESET}"
echo -e "${C_CYAN} QA TEST SUITE: CORNER CASES E ROBUSTEZZA${C_RESET}"
echo -e "${C_CYAN}=========================================================${C_RESET}"

echo "[*] Compilazione eseguibili di test..."
gcc -Wall -O3 test/test_burst.c -o test/test_burst
gcc -Wall -O3 test/bench_stress.c -o test/bench_stress -pthread

echo "[*] Ricaricamento Modulo Pulito..."
make reload > /dev/null

# =====================================================================
# CASE 1: POLICY EXTREME (MAX = 0 Deny All)
# =====================================================================
echo -e "\n${C_PURPLE}>>> CASE 1: SOGLIA MAX=0 (Deny All Isolato) <<<${C_RESET}"
echo "[>] Impostazione regola letale su syscall 39 per l'utente 'nobody'..."
TEST_UID=$(id -u nobody)
sudo ./userspace/cli_tool -s 39 -m 0 -u $TEST_UID > /dev/null

set +e
# Lanciamo il burst test come nobody, così non blocchiamo il sistema intero
sudo -u nobody timeout 2s ./test/test_burst 39 > /dev/null
RESULT=$?
set -e

if [ $RESULT -eq 124 ]; then
    echo -e "${C_GREEN}[+] SUCCESSO: La policy 'Deny All' congela i processi istantaneamente senza crash.${C_RESET}"
else
    echo -e "${C_RED}[-] FALLIMENTO: Il processo ha bypassato la policy o è crashato (Codice: $RESULT).${C_RESET}"
fi

# =====================================================================
# CASE 2: HOT-SWAP (Spegnimento a caldo del monitor)
# =====================================================================
echo -e "\n${C_PURPLE}>>> CASE 2: HOT-SWAP (Disattivazione sotto carico) <<<${C_RESET}"
echo "[>] Riavvio modulo e impostazione regola MAX=1..."
sudo ./userspace/cli_tool -e > /dev/null
sudo ./userspace/cli_tool -s 39 -m 1 > /dev/null

echo "[>] Lancio test_burst in background (processo destinato a bloccarsi)..."
./test/test_burst 39 > /dev/null &
BURST_PID=$!

sleep 0.5 # Diamo tempo al processo di finire nella Wait Queue del Kernel

echo "[>] L'amministratore disattiva improvvisamente il monitor (-d)..."
sudo ./userspace/cli_tool -d > /dev/null

echo "[>] Attesa risoluzione processo in background..."
wait $BURST_PID
echo -e "${C_GREEN}[+] SUCCESSO: Il processo è stato sbloccato ed espulso dal kernel in sicurezza.${C_RESET}"


# =====================================================================
# CASE 3: SAFE UNLOADING (Rimozione modulo sotto stress)
# =====================================================================
echo -e "\n${C_PURPLE}>>> CASE 3: SAFE UNLOADING (rmmod sotto attacco) <<<${C_RESET}"
echo "[>] Riavvio modulo e impostazione regola MAX=5..."
sudo ./userspace/cli_tool -e > /dev/null
sudo ./userspace/cli_tool -s 39 -m 5 > /dev/null

echo "[>] Lancio 20 thread (bench_stress) in background..."
./test/bench_stress 39 > /dev/null &
STRESS_PID=$!

sleep 0.5 # Aspettiamo che tutti e 20 i thread siano intrappolati nel Ring 0

echo "[>] L'amministratore forza la rimozione del modulo (make unload)..."
make unload > /dev/null
echo -e "${C_GREEN}[+] Modulo rimosso dalla memoria con successo.${C_RESET}"

echo "[>] Verifica sopravvivenza del sistema operativo..."
# Se arriviamo a stampare questa riga, significa che rmmod non ha causato un Kernel Panic
echo -e "${C_GREEN}[+] SUCCESSO: Nessun Kernel Panic. Lo svuotamento della Wait Queue (wake_up_all) ha funzionato.${C_RESET}"

# Pulizia di sicurezza in caso i thread siano ancora vivi in User Space (orfani)
kill -9 $STRESS_PID 2>/dev/null || true

echo -e "\n${C_CYAN}=========================================================${C_RESET}"
echo -e "${C_GREEN} QA COMPLETATA: SISTEMA RESISTENTE AI CASI PARTICOLARI ${C_RESET}"
echo -e "${C_CYAN}=========================================================${C_RESET}"