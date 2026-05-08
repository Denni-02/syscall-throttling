/**
 * Implementazione del Motore di Throttling.
*/

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/cred.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/wait.h>
#include <linux/atomic.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <asm/msr.h>
#include "policy_engine.h"
#include "registry_data.h" 

extern int is_throttled(int uid, const char *comm, int syscall_num, int *out_max_calls);
extern void update_peak_delay(int syscall_num, unsigned long long delay_cycles, int victim_uid, const char *victim_comm);
extern void update_thread_stats(int syscall_num, int current_blocked_now);
extern void reset_all_counters(void);

int global_monitor_state __read_mostly = 1; // 1 = ON, 0 = OFF

// Gestione Safe Unloading
atomic_t active_wrappers = ATOMIC_INIT(0);
DECLARE_WAIT_QUEUE_HEAD(unload_waitqueue);

// Statistiche
static atomic_t total_blocked_threads = ATOMIC_INIT(0);

// Orologio del modulo
static struct timer_list epoch_timer;
atomic_t time_epoch = ATOMIC_INIT(0);

// Struttura privata allocata sullo STACK di ogni thread sospeso
struct scth_waiter {
    struct list_head list;
    wait_queue_head_t wq;
    int syscall_num;
    uid_t uid;
    char comm[TASK_COMM_LEN];
    bool granted; // Il "Token" d'accesso
};

// Lock per proteggere la coda dai timer asincroni
DEFINE_SPINLOCK(fifo_lock);
LIST_HEAD(fifo_waiters);

// Macro che legge il Time-Stamp Counter e l'ID del processore
static inline unsigned long long read_rdtscp_strict(unsigned int *cpu_id) {
    unsigned int lo, hi;
    asm volatile("rdtscp" : "=a" (lo), "=d" (hi), "=c" (*cpu_id));
    return ((unsigned long long)hi << 32) | lo;
}

// Entry point per l'intercettazione.
void enforce_syscall_policy(int syscall_num) {
    uid_t caller_uid;
    char caller_comm[TASK_COMM_LEN];
    int max_calls;
    int is_monitored = 0;

    // Ingresso nel modulo
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
        unsigned long long tsc_start, tsc_end, delay_cycles;
        unsigned int cpu_start, cpu_end;
        int current_val;
        int wait_ret;

        struct scth_waiter waiter;
        INIT_LIST_HEAD(&waiter.list);
        init_waitqueue_head(&waiter.wq);
        waiter.syscall_num = syscall_num;
        waiter.uid = caller_uid;
        strscpy(waiter.comm, caller_comm, TASK_COMM_LEN);
        waiter.granted = false;

        // Cattura tempo inizio
        tsc_start = read_rdtscp_strict(&cpu_start);

        // Incrementiamo il numero di thread bloccati
        current_val = atomic_inc_return(&total_blocked_threads);

        // Aggiorniamo il picco nel database
        update_thread_stats(syscall_num, current_val);
        
        spin_lock_bh(&fifo_lock);
        list_add_tail(&waiter.list, &fifo_waiters);
        spin_unlock_bh(&fifo_lock);

        // Congelamento fisico del thread nella Wait Queue
        wait_ret = wait_event_interruptible(waiter.wq, waiter.granted || !global_monitor_state);
        if (wait_ret < 0) {
            spin_lock_bh(&fifo_lock);
            if (!waiter.granted) {
                list_del_init(&waiter.list); 
            }
            spin_unlock_bh(&fifo_lock);
            
            atomic_dec(&total_blocked_threads);
            if (atomic_dec_and_test(&active_wrappers)) {
                wake_up(&unload_waitqueue);
            }
            return;
        }

        // RISVEGLIO: Decrementiamo il contatore
        atomic_dec(&total_blocked_threads);

        // Cattura tempo fine
        tsc_end = read_rdtscp_strict(&cpu_end);

        if (tsc_end > tsc_start) {
            delay_cycles = tsc_end - tsc_start;
            update_peak_delay(syscall_num, delay_cycles, (int)caller_uid, caller_comm);
        }

    } 

    if (atomic_dec_and_test(&active_wrappers)) {
        wake_up(&unload_waitqueue);
    }
}

void wait_for_zero_wrappers(void) {
    wait_event(unload_waitqueue, atomic_read(&active_wrappers) == 0);
}

/* ========================================================================= *
 * DEMONE DEL TEMPO (KERNEL TIMER IN SOFTIRQ)                                                *
 * ========================================================================= */

static void epoch_timer_callback(struct timer_list *t) {
    struct scth_waiter *w, *tmp;

    // Resetta i contatori RCU
    reset_all_counters();
    atomic_inc(&time_epoch);

    // Scorriamo la fila ordinata (FIFO) e distribuiamo i permessi
    spin_lock_bh(&fifo_lock);
    list_for_each_entry_safe(w, tmp, &fifo_waiters, list) {
        int dummy_max;
        
        // Chiediamo al DB se c'è un token. is_throttled farà l'incremento!
        // Se restituisce 0, significa che la soglia MAX non è ancora stata sfondata
        if (!is_throttled((int)w->uid, w->comm, w->syscall_num, &dummy_max)) {
            w->granted = true;          // Diamo il pass
            list_del_init(&w->list);    // Lo togliamo dalla coda
            wake_up(&w->wq);            // Svegliamo SOLO lui (Niente Context Switch inutili)
        }
    }
    spin_unlock_bh(&fifo_lock);

    // Riprogrammiamo il timer
    mod_timer(&epoch_timer, jiffies + HZ);
}

int start_policy_engine(void) {
    timer_setup(&epoch_timer, epoch_timer_callback, 0);
    mod_timer(&epoch_timer, jiffies + HZ);
    return 0;
}

void flush_all_waiters(void) {
    struct scth_waiter *w, *tmp;
    spin_lock_bh(&fifo_lock);
    list_for_each_entry_safe(w, tmp, &fifo_waiters, list) {
        w->granted = true; // Sblocca la condizione
        list_del_init(&w->list);
        wake_up(&w->wq);    // Sveglia il thread fisico
    }
    spin_unlock_bh(&fifo_lock);
}

void set_global_monitor_state(int state) {
    global_monitor_state = state;
    if (state == 0) {
        flush_all_waiters(); // Notifica immediata a tutti i thread in coda
    }
}

void stop_policy_engine(void) {
    set_global_monitor_state(0);
    del_timer_sync(&epoch_timer);
}