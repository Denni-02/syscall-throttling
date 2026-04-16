#!/bin/bash
set -e 

echo "========================================================="
echo " AVVIO FAST-TRACK TEST (Syscall Throttling)"
echo "========================================================="

echo -e "\n[*] STEP 1: Ricompilazione e Ricaricamento Modulo"
cd userspace && make > /dev/null && cd ..
make reload

echo -e "\n[*] STEP 2: Configurazione Regola (Syscall 83, MAX 2, test_advanced)"
sudo ./userspace/cli_tool -s 83 -m 2 -p test_advanced

echo -e "\n[*] STEP 3: Ispezione Database Ring 0"
sudo ./userspace/cli_tool -l

echo -e "\n[*] STEP 4: Esecuzione Trigger (test_advanced)"

set +e 
./test/test_advanced 83
set -e

echo -e "\n[*] STEP 5: Estrazione Statistiche Hardware (RDTSCP)"
sudo ./userspace/cli_tool -g 83

echo -e "\n[*] STEP 6: Snapshot log del Kernel (dmesg)"
echo "---------------------------------------------------------"
sudo dmesg | tail -n 15
echo "---------------------------------------------------------"

echo -e "\n TEST COMPLETATO CON SUCCESSO!"