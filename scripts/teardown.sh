#!/bin/bash

echo "[+] Rimozione del modulo dal kernel (rmmod)..."
sudo rmmod syscall_defender

echo "[+] Rimozione del Device Node..."
sudo rm -f /dev/syscall_defender

echo "[+] Pulizia dei file di build..."
make clean

echo "[+] Ultime righe di dmesg:"
sudo dmesg | tail -n 2