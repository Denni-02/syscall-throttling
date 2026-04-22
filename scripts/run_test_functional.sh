#!/bin/bash
set -e

C_GREEN="\x1b[32m"
C_CYAN="\x1b[36m"
C_RESET="\x1b[0m"

echo -e "${C_CYAN}=========================================================${C_RESET}"
echo -e "${C_CYAN} QA TEST SUITE: TIMER RESET E LOGICA${C_RESET}"
echo -e "${C_CYAN}=========================================================${C_RESET}"

echo "[*] Compilazione test_logic..."
gcc -Wall -O3 test/test_logic.c -o test/test_logic

echo "[*] Ricaricamento Modulo Pulito..."
make reload > /dev/null

echo "[*] Configurazione Regola: Syscall=39 (getpid), MAX=3, Target=test_logic"
sudo ./userspace/cli_tool -s 39 -m 3 -p test_logic > /dev/null

echo "[*] Verifica Regola nel Database:"
sudo ./userspace/cli_tool -l
echo "---------------------------------------------------------"

# Lanciamo il test
./test/test_logic 39

# Pulizia
sudo ./userspace/cli_tool -d > /dev/null
echo -e "${C_GREEN}[+] TEST FUNZIONALE COMPLETATO CON SUCCESSO!${C_RESET}"