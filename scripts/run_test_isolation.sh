#!/bin/bash
set -e

C_GREEN="\x1b[32m"
C_CYAN="\x1b[36m"
C_YELLOW="\x1b[33m"
C_RED="\x1b[31m"
C_RESET="\x1b[0m"

echo -e "${C_CYAN}=========================================================${C_RESET}"
echo -e "${C_CYAN} QA TEST SUITE: ISOLAMENTO E PRIVILEGE SEPARATION${C_RESET}"
echo -e "${C_CYAN}=========================================================${C_RESET}"

# Usiamo l'utente di sistema 'nobody' (isolamento sicuro che non fa crollare VS Code/SSH)
TEST_USER="nobody"
TEST_UID=$(id -u $TEST_USER)

echo "[*] Compilazione eseguibile (test_burst.c)..."
gcc -Wall -O3 test/test_burst.c -o test/test_burst

echo "[*] Ricaricamento Modulo Pulito..."
make reload > /dev/null

# Inseriamo la regola: Syscall 39, MAX 0 (Blocco totale), limitata a 'nobody'
echo "[*] Configurazione Regola: Syscall=39, MAX=0 (Deny All), UID Target=$TEST_UID ($TEST_USER)"
sudo ./userspace/cli_tool -s 39 -m 0 -u $TEST_UID > /dev/null

echo -e "\n[*] Verifica Regola nel Database:"
sudo ./userspace/cli_tool -l
echo "---------------------------------------------------------"

# =====================================================================
# SCENARIO 1: L'utente limitato (nobody) tenta l'esecuzione
# =====================================================================
echo -e "\n${C_YELLOW}>>> SCENARIO 1: Esecuzione come utente non privilegiato ($TEST_USER) <<<${C_RESET}"
echo "[>] Il processo dovrebbe essere bloccato istantaneamente (Timeout di sicurezza: 2s)..."

set +e 
# Eseguiamo il burst test COME utente 'nobody'
sudo -u $TEST_USER timeout 2s ./test/test_burst 39 > /dev/null
RESULT=$?
set -e

if [ $RESULT -eq 124 ]; then
    echo -e "${C_GREEN}[+] SUCCESSO: Il processo è stato congelato nel Ring 0! L'utente è bloccato.${C_RESET}"
else
    echo -e "${C_RED}[-] ANOMALIA: Il processo è passato o si è chiuso prematuramente.${C_RESET}"
fi

# =====================================================================
# SCENARIO 2: L'amministratore (root) tenta l'esecuzione
# =====================================================================
echo -e "\n${C_YELLOW}>>> SCENARIO 2: Esecuzione come amministratore (root) <<<${C_RESET}"
echo "[>] Il root bypassa il filtro UID. Il test deve completare le sue 100 chiamate istantaneamente..."

# Lanciamo lo stesso identico eseguibile, ma con sudo (UID 0)
sudo ./test/test_burst 39

# Pulizia finale
sudo ./userspace/cli_tool -d > /dev/null
echo -e "\n${C_GREEN}[+] TEST DI ISOLAMENTO COMPLETATO!${C_RESET}"
echo -e "${C_CYAN}=========================================================${C_RESET}"