/**
 * Implementazione del Character Device. 
 * Gestisce l'interfaccia IOCTL, validando i privilegi e ricevendo le policy di throttling dallo User Space.
*/

#include <linux/fs.h>
#include <linux/uaccess.h> 
#include <linux/cred.h>   
#include <linux/sched.h>
#include "../include/defender_api.h"
#include "registry_data.h"

#define DEVICE_NAME "syscall_defender"

// Major Number assegnato dinamicamente al momento della registrazione del device
static int major_number;

/**
 * defender_ioctl() - Handler per la system call ioctl() sul character device
 * @file: Puntatore alla struttura file del Virtual File System
 * @cmd:  Il Magic Number del comando inviato dallo User Space
 * @arg:  Puntatore alla memoria dello User Space
 *
 * Riceve i comandi di configurazione, esegue i controlli di sicurezza (EUID)
 * e trasferisce in modo sicuro le strutture dati da Ring 3 a Ring 0.
 *
 * Return: 0 in caso di successo, un codice di errore negativo in caso di violazione o fallimento.
 */
static long defender_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    
    struct config_data user_config;

    // Verifichiamo l'Effective UID: solo root può modificare le regole di throttling
    if (current_euid().val != 0) {
        printk(KERN_WARNING "[Syscall_Throttling] Accesso negato: UID non root.\n");
        return -EPERM; // Operation not permitted
    }

    switch (cmd) {
        case SET_THROTTLING_RULE:
            // Trasferimento sicuro dei dati dal Ring 3 al Ring 0
            if (copy_from_user(&user_config, (struct config_data __user *)arg, sizeof(user_config))) {
                return -EFAULT; // Bad address
            }
            
            // Inseriamo dinamicamente la regola nel database kernel
            if (add_rule(user_config.target_uid, user_config.comm, user_config.syscall_num, user_config.max_calls) < 0) {
                return -ENOMEM; // Errore di allocazione RAM
            }
            
            // Debug: Stampiamo la lista delle regole per verificare l'inserimento
            debug_print_rules();
            break;
            
        default:
            // Se il comando non corrisponde al Magic Number definito, restituiamo un errore
            return -ENOTTY; // Inappropriate ioctl for device
    }

    return 0; // Successo
}

/**
 * struct fops - Mappatura delle operazioni supportate dal Character Device.
 */
static struct file_operations fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = defender_ioctl,
};

/**
 * init_char_device() - Inizializza ed espone il dispositivo virtuale.
 *
 * Richiede al kernel l'allocazione di un Major Number libero e registra le 
 * file_operations nel VFS. 
 *
 * Return: 0 in caso di successo, errore negativo se il kernel rifiuta la registrazione.
 */
int init_char_device(void) {
    major_number = register_chrdev(0, DEVICE_NAME, &fops);
    if (major_number < 0) {
        printk(KERN_ALERT "[Syscall_Throttling] Registrazione device fallita.\n");
        return major_number;
    }
    printk(KERN_INFO "[Syscall_Throttling] Device registrato. Major number: %d\n", major_number);
    return 0;
}

/**
 * cleanup_char_device() - Deregistra il dispositivo dal VFS.
 *
 * Libera il Major Number, rendendo impossibile per i processi in User Space
 * effettuare nuove chiamate ioctl() verso questo modulo.
 */
void cleanup_char_device(void) {
    unregister_chrdev(major_number, DEVICE_NAME);
    printk(KERN_INFO "[Syscall_Throttling] Device deregistrato.\n");
}