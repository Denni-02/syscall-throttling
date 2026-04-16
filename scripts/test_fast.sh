#!/bin/bash
set -e 

echo "========================================================="
echo " AVVIO FAST-TRACK TEST (Syscall Throttling)"
echo "========================================================="

echo -e "\n[*] STEP 1: Compilazione Test e Ricaricamento Modulo"
# Compiliamo rapidamente il nostro nuovo test funzionale
gcc -Wall -O2 test/test_throttling.c -o test/test_throttling
cd userspace && make > /dev/null && cd ..
make reload

echo -e "\n[*] STEP 2: Configurazione Regola (Syscall 83, MAX 2, test_throttling)"
sudo ./userspace/cli_tool -s 83 -m 2 -p test_throttling

echo -e "\n[*] STEP 3: Ispezione Database Ring 0"
sudo ./userspace/cli_tool -l

echo -e "\n[*] STEP 4: Esecuzione Trigger Funzionale"
set +e 
./test/test_throttling 83
set -e

echo -e "\n[*] STEP 5: Estrazione Statistiche Hardware (RDTSCP)"
sudo ./userspace/cli_tool -g 83

echo -e "\n[*] STEP 6: Snapshot log del Kernel (dmesg)"
echo "---------------------------------------------------------"
sudo dmesg | tail -n 15
echo "---------------------------------------------------------"

echo -e "\n TEST COMPLETATO CON SUCCESSO!"