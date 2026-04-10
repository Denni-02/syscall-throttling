/**
 * Implementazione del Motore di Throttling.
*/

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/cred.h>
#include <linux/string.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <asm/msr.h>
#include "policy_engine.h"
#include "registry_data.h" 

extern int is_throttled(int uid, const char *comm, int syscall_num, int *out_max_calls);
extern void update_peak_delay(int syscall_num, unsigned long long delay_cycles, int victim_uid, const char *victim_comm);
extern void update_thread_stats(int syscall_num, int current_blocked_now);

// Struttura per i thread dormienti
DECLARE_WAIT_QUEUE_HEAD(throttle_queue);

// Orologio del modulo. Incrementato dal demone ogni secondo
atomic_t time_epoch = ATOMIC_INIT(0);

static atomic_t total_blocked_threads = ATOMIC_INIT(0);
int global_monitor_state = 1; // 1 = ON, 0 = OFF

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

    if (!global_monitor_state) {
        return; 
    }

    // 1. Identificazione
    caller_uid = current_euid().val;
    get_task_comm(caller_comm, current);

    printk(KERN_INFO "[Syscall_Throttling] Invocazione rilevata - Syscall: %d | UID: %u | Comm: %s\n",
           syscall_num, caller_uid, caller_comm);

    // 2. Verifica
    is_monitored = is_throttled((int)caller_uid, caller_comm, syscall_num, &max_calls);
    
    if (is_monitored) {
        int sleep_epoch;
        unsigned long long tsc_start, tsc_end, delay_cycles;
        unsigned int cpu_start, cpu_end;
        int current_val;

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
        
        /* RISVEGLIO: Decrementiamo il contatore */
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

    } else {
        printk(KERN_INFO "[Syscall_Throttling] PASS: Invocazione %d consentita.\n", syscall_num);
    }
}

/* ========================================================================= *
 * DEMONE DEL TEMPO (KTHREAD)                                                *
 * ========================================================================= */

static struct task_struct *daemon_task = NULL;
extern void reset_all_counters(void); 

// Loop infinito del demone in background 
static int daemon_loop(void *data) {
    printk(KERN_INFO "[Syscall_Throttling] Demone del tempo avviato (PID: %d)\n", current->pid);

    // Il demone gira finché non riceve il segnale di stop allo scaricamento del modulo
    while (!kthread_should_stop()) {
        
        set_current_state(TASK_INTERRUPTIBLE);
        if (kthread_should_stop()) {
            set_current_state(TASK_RUNNING);
            break;
        }
        schedule_timeout(HZ); // Dorme per 1 secondo

        /* --- RISVEGLIO --- */

        // Resetta i contatori di tutte le policy 
        reset_all_counters();

        // Avanza l'orologio globale
        atomic_inc(&time_epoch);

        // Sveglia TUTTI i thread congelati nella Wait Queue 
        wake_up_all(&throttle_queue);
    }

    printk(KERN_INFO "[Syscall_Throttling] Demone del tempo arrestato in sicurezza.\n");
    return 0;
}

int start_policy_engine(void) {
    // Crea e avvia il thread immediatamente
    daemon_task = kthread_run(daemon_loop, NULL, "syscall_daemon");
    if (IS_ERR(daemon_task)) {
        printk(KERN_ERR "[Syscall_Throttling] Errore critico: impossibile avviare il demone!\n");
        return PTR_ERR(daemon_task);
    }
    return 0;
}

void stop_policy_engine(void) {
    if (daemon_task) {
        // Invia il segnale di stop e attende che il demone esca dal loop
        kthread_stop(daemon_task);
        daemon_task = NULL;
    }
}