import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns

# Configurazione stile
sns.set_theme(style="whitegrid")
plt.figure(figsize=(10, 6))

# Caricamento Dati
try:
    df = pd.read_csv('multi_thread_stats.csv')
except FileNotFoundError:
    print("Errore: File multi_thread_stats.csv non trovato.")
    exit(1)

# Mappatura nomi syscall per leggibilità
syscall_map = {39: 'getpid (39)', 1: 'write (1)'}
df['Syscall_Name'] = df['Syscall'].map(syscall_map)

# Creazione del Box Plot
ax = sns.boxplot(
    data=df, 
    x='Syscall_Name', 
    y='Time_ms', 
    hue='Sync',
    palette={'RCU': '#2ca02c', 'Spinlock': '#d62728'}, # Verde per RCU, Rosso per Spinlock
    linewidth=2,
    fliersize=5
)

# Aggiungiamo i punti esatti sopra i box per far vedere la densità (opzionale ma bellissimo)
sns.stripplot(
    data=df, 
    x='Syscall_Name', 
    y='Time_ms', 
    hue='Sync', 
    dodge=True, 
    color='black', 
    alpha=0.5, 
    ax=ax
)

# Titoli e label
plt.title('Distribuzione della Latenza nel Multi-Thread\nRCU vs Spinlock Globale (20 runs x 10 Mln chiamate)', fontsize=14, pad=15)
plt.ylabel('Tempo di Attraversamento Totale (ms)', fontsize=12)
plt.xlabel('System Call', fontsize=12)

# Salvataggio
output_file = 'multi_thread_boxplot.png'
plt.tight_layout()
plt.savefig(output_file, dpi=300)
print(f"[+] Grafico Box Plot generato con successo: {output_file}")