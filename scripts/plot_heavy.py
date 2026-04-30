import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns

sns.set_theme(style="whitegrid")
plt.figure(figsize=(8, 6))

try:
    df = pd.read_csv('heavy_thread_stats2.csv')
except FileNotFoundError:
    print("[-] Errore: File heavy_thread_stats2.csv non trovato.")
    exit(1)

# Creazione del Box Plot
ax = sns.boxplot(
    data=df, 
    x='Syscall', 
    y='Time_ms', 
    hue='Sync',
    palette={'RCU': '#2ca02c', 'Spinlock': '#d62728'},
    linewidth=2,
    showfliers=False 
)

# Aggiungiamo i punti densità (SENZA legend=False per evitare crash su Ubuntu 22.04)
sns.stripplot(
    data=df, 
    x='Syscall', 
    y='Time_ms', 
    hue='Sync', 
    dodge=True, 
    color='black', 
    alpha=0.5, 
    ax=ax
)

plt.title('Lock Contention SOTTO CARICO (50 Regole attive, 4 CPU Core)\nRCU vs Spinlock (20 run x 20 thread x 1.000 calls)', fontsize=13, pad=15)
plt.ylabel('Tempo di Attraversamento Totale (ms)', fontsize=12)
plt.xlabel('Syscall Sotto Test (getpid)', fontsize=12)

output_file = 'heavy_boxplot2.png'
plt.tight_layout()
plt.savefig(output_file, dpi=300)
print(f"[+] Grafico Box Plot generato con successo: {output_file}")