#!/bin/bash

set -e 

echo "[+] Compilazione del modulo..."
make

echo "[+] Inserimento del modulo nel kernel (insmod)..."
sudo insmod syscall_defender.ko

echo "[+] Ultime righe di dmesg:"
sudo dmesg | tail -n 2