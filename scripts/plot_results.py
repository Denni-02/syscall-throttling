#!/usr/bin/env python3
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

# 1. Caricamento Dati
try:
    df = pd.read_csv('benchmark_results.csv')
except FileNotFoundError:
    print("[-] Errore: File benchmark_results.csv non trovato.")
    exit(1)

# Mappatura nomi syscall per i grafici
syscall_names = {39: 'getpid (CPU)', 257: 'openat (VFS)', 1: 'write (I/O)'}
df['Syscall_Name'] = df['Syscall'].map(syscall_names)

# Separiamo le configurazioni
rcu_kprobes = df[df['Configurazione'] == 'RCU_Kprobes']
spin_kprobes = df[df['Configurazione'] == 'Spinlock_Kprobes']
rcu_scanner = df[df['Configurazione'] == 'RCU_Scanner']

x_labels = rcu_kprobes['Syscall_Name'].tolist()
x = np.arange(len(x_labels))
width = 0.25  # Larghezza delle barre

# ==========================================
# GRAFICO 1: Overhead Architetturale (ns)
# ==========================================
plt.figure(figsize=(10, 6))

# Sostituiamo gli 0 con un valore minimo (es. 1) solo per far apparire un filo visivo nel grafico
rcu_k_over = [val if val > 0 else 1 for val in rcu_kprobes['Overhead_ns']]
spin_k_over = [val if val > 0 else 1 for val in spin_kprobes['Overhead_ns']]
rcu_s_over = [val if val > 0 else 1 for val in rcu_scanner['Overhead_ns']]

plt.bar(x - width, rcu_k_over, width, label='RCU + Kprobes', color='#2ca02c', edgecolor='black')
plt.bar(x, spin_k_over, width, label='Spinlock + Kprobes', color='#d62728', edgecolor='black')
plt.bar(x + width, rcu_s_over, width, label='RCU + Scanner', color='#1f77b4', edgecolor='black')

plt.ylabel('Overhead Latenza (Nanosecondi)', fontsize=12)
plt.title('Costo di Intercettazione sul Fast-Path (Latenza Aggiuntiva)', fontsize=14, fontweight='bold')
plt.xticks(x, x_labels, fontsize=11)
plt.legend()
plt.grid(axis='y', linestyle='--', alpha=0.7)

# Aggiungiamo un testo esplicativo per i valori nulli
plt.text(0.5, -0.15, "* I valori prossimi allo zero indicano un overhead assorbito dal rumore di fondo del SO.", 
         ha='center', va='center', transform=plt.gca().transAxes, fontsize=9, style='italic')

plt.tight_layout()
plt.savefig('overhead_chart.png', dpi=300)
print("[+] Grafico Overhead salvato come 'overhead_chart.png'")


# ==========================================
# GRAFICO 2: Efficienza Wait Queue (Context Switches)
# ==========================================
plt.figure(figsize=(10, 6))

rcu_k_cs = rcu_kprobes['Context_Switches'].tolist()
spin_k_cs = spin_kprobes['Context_Switches'].tolist()

plt.bar(x - width/2, rcu_k_cs, width, label='RCU (Lock-Free)', color='#9467bd', edgecolor='black')
plt.bar(x + width/2, spin_k_cs, width, label='Spinlock (Blocking)', color='#ff7f0e', edgecolor='black')

# Linea teorica perfetta (200 chiamate bloccate = 200 context switches ideali)
plt.axhline(y=200, color='red', linestyle='--', linewidth=2, label='Ideale Teorico (Strict FIFO)')

plt.ylabel('Numero di Context Switches', fontsize=12)
plt.title('Gestione Thundering Herd sotto Stress (20 Thread)', fontsize=14, fontweight='bold')
plt.xticks(x, x_labels, fontsize=11)
plt.ylim(0, max(max(rcu_k_cs), max(spin_k_cs)) * 1.5) # Diamo respiro in alto
plt.legend()
plt.grid(axis='y', linestyle='--', alpha=0.7)

plt.tight_layout()
plt.savefig('context_switches_chart.png', dpi=300)
print("[+] Grafico Context Switches salvato come 'context_switches_chart.png'")