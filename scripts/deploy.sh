#!/bin/bash
set -e 

echo "[+] Compilazione del modulo..."
make

echo "[+] Inserimento del modulo nel kernel (insmod)..."
sudo insmod syscall_defender.ko

echo "[+] Creazione dinamica del Device Node..."
MAJOR=$(awk "\$2==\"syscall_defender\" {print \$1}" /proc/devices)

if [ -z "$MAJOR" ]; then
    echo "[!] Errore: Major number non trovato."
    exit 1
fi

# Crea il file e imposta i permessi
sudo mknod /dev/syscall_defender c $MAJOR 0
sudo chmod 666 /dev/syscall_defender
echo "[+] Device /dev/syscall_defender creato con Major Number: $MAJOR"

echo "[+] Ultime righe di dmesg:"
sudo dmesg | tail -n 2