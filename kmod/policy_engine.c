/**
 * Implementazione del Motore di Throttling.
*/

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/cred.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <asm/msr.h>
#include "policy_engine.h"
#include "registry_data.h" 

extern int is_throttled(int uid, const char *comm, int syscall_num, int *out_max_calls);
extern void update_peak_delay(int syscall_num, unsigned long long delay_cycles, int victim_uid, const char *victim_comm);
extern void update_thread_stats(int syscall_num, int current_blocked_now);

// Gestione Safe Unloading
atomic_t active_wrappers = ATOMIC_INIT(0);
DECLARE_WAIT_QUEUE_HEAD(unload_waitqueue);

// Struttura per i thread dormienti
DECLARE_WAIT_QUEUE_HEAD(throttle_queue);

// Orologio del modulo. Incrementato dal demone ogni secondo
atomic_t time_epoch = ATOMIC_INIT(0);

static atomic_t total_blocked_threads = ATOMIC_INIT(0);
int global_monitor_state __read_mostly = 1; // 1 = ON, 0 = OFF

// Macro che legge il Time-Stamp Counter e l'ID del processore
static inline unsigned long long read_rdtscp_strict(unsigned int *cpu_id) {
    unsigned int lo, hi;
    asm volatile("rdtscp" : "=a" (lo), "=d" (hi), "=c" (*cpu_id));
    return ((unsigned long long)hi << 32) | lo;
}

void enforce_syscall_policy(int syscall_num) {
    uid_t caller_uid;
    char caller_comm[TASK_COMM_LEN];
    int max_calls;
    int is_monitored = 0;

    atomic_inc(&active_wrappers);

    if (!global_monitor_state) {
        if (atomic_dec_and_test(&active_wrappers)) {
            wake_up(&unload_waitqueue);
        }
        return; 
    }

    // 1. Identificazione
    caller_uid = current_euid().val;
    get_task_comm(caller_comm, current);

    // 2. Verifica
    is_monitored = is_throttled((int)caller_uid, caller_comm, syscall_num, &max_calls);
    
    if (is_monitored) {
        int sleep_epoch;
        unsigned long long tsc_start, tsc_end, delay_cycles;
        unsigned int cpu_start, cpu_end;
        int current_val;
        int wait_ret;

        printk(KERN_WARNING "[Syscall_Throttling] BLOCCO! Processo %s (UID %u) ha superato il limite di %d. SOSPENSIONE...\n", 
               caller_comm, caller_uid, max_calls);
        
        // Salviamo il tick attuale dell'orologio
        sleep_epoch = atomic_read(&time_epoch);

        // Cattura tempo inizio
        tsc_start = read_rdtscp_strict(&cpu_start);

        // Incrementiamo il numero di thread bloccati
        current_val = atomic_inc_return(&total_blocked_threads);

        // Aggiorniamo il picco nel database
        update_thread_stats(syscall_num, current_val);
        
        // Congelamento fisico del thread nella Wait Queue
        wait_event_interruptible(throttle_queue, atomic_read(&time_epoch) > sleep_epoch);
        if (wait_ret < 0) {
            atomic_dec(&total_blocked_threads);
            if (atomic_dec_and_test(&active_wrappers)) {
                wake_up(&unload_waitqueue);
            }
            printk(KERN_INFO "[Syscall_Throttling] Syscall abortita (SIGINT) per %s. Ritiro pulito.\n", caller_comm);
            return; 
        }

        // RISVEGLIO: Decrementiamo il contatore
        atomic_dec(&total_blocked_threads);

        // Cattura tempo fine
        tsc_end = read_rdtscp_strict(&cpu_end);

        if (tsc_end > tsc_start) {
            delay_cycles = tsc_end - tsc_start;
            
            /* Se la CPU è cambiata durante il sonno (CPU Migration), lo notifichiamo 
             * perché i registri TSC di Core diversi potrebbero avere un leggero offset.
            */
            if (cpu_start != cpu_end) {
                printk(KERN_DEBUG "[Syscall_Throttling] Avviso: Migrazione CPU rilevata (Core %u -> Core %u)\n", 
                       cpu_start, cpu_end);
            }
            
            // Aggiorniamo il picco nel database
            update_peak_delay(syscall_num, delay_cycles, (int)caller_uid, caller_comm);
            
            printk(KERN_INFO "[Syscall_Throttling] RISVEGLIO %s. Ritardo: %llu cicli CPU.\n", 
                   caller_comm, delay_cycles);
        }

    } 

    if (atomic_dec_and_test(&active_wrappers)) {
        wake_up(&unload_waitqueue);
    }
}

void wait_for_zero_wrappers(void) {
    printk(KERN_INFO "[Syscall_Throttling] Attesa svuotamento hook (Safe Unloading)...\n");
    // Dorme finché active_wrappers non è esattamente 0
    wait_event(unload_waitqueue, atomic_read(&active_wrappers) == 0);
    printk(KERN_INFO "[Syscall_Throttling] Hook svuotato. Procedo allo scaricamento.\n");
}

/* ========================================================================= *
 * DEMONE DEL TEMPO (KERNEL TIMER IN SOFTIRQ)                                                *
 * ========================================================================= */

static struct timer_list epoch_timer;
extern void reset_all_counters(void); 

/* Callback del Timer: Gira in contesto di Interrupt (Softirq).
 * ATTENZIONE: Non può dormire, non può usare lock bloccanti (mutex).
 */
static void epoch_timer_callback(struct timer_list *t) {
    // Resetta i contatori di tutte le policy (RCU/Spinlock safe)
    reset_all_counters();

    // Avanza l'orologio globale (operazione atomica, safe)
    atomic_inc(&time_epoch);

    // Sveglia i thread congelati (wake_up è safe in interrupt context)
    wake_up_all(&throttle_queue);

    // Riprogramma il timer per scattare di nuovo tra esattamente 1 secondo
    mod_timer(&epoch_timer, jiffies + HZ);
}

int start_policy_engine(void) {
    printk(KERN_INFO "[Syscall_Throttling] Avvio orologio di sistema (Kernel Timer in Softirq)...\n");
    
    // Inizializza la struttura del timer e collega la callback
    timer_setup(&epoch_timer, epoch_timer_callback, 0);
    
    // Innesca il primo scatto tra 1 secondo esatto (HZ jiffies)
    mod_timer(&epoch_timer, jiffies + HZ);
    
    return 0;
}

void stop_policy_engine(void) {
    printk(KERN_INFO "[Syscall_Throttling] Spegnimento orologio di sistema...\n");
    
    // del_timer_sync attende in sicurezza se la callback è in esecuzione su un altro core
    del_timer_sync(&epoch_timer);
}