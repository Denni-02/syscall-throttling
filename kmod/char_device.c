#include <linux/fs.h>
#include <linux/uaccess.h> 
#include <linux/cred.h>   
#include <linux/sched.h>
#include "../include/defender_api.h"

#define DEVICE_NAME "syscall_defender"

static int major_number;

// Handler della System Call ioctl()
static long defender_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    
    struct config_data user_config;

    // Solo root può modificare le policy
    if (current_euid().val != 0) {
        printk(KERN_WARNING "[Syscall_Throttling] Accesso negato: UID non root.\n");
        return -EPERM; // Operation not permitted
    }

    switch (cmd) {
        case SET_THROTTLING_RULE:
            // Trasferimento sicuro dal Ring 3 al Ring 0
            if (copy_from_user(&user_config, (struct config_data __user *)arg, sizeof(user_config))) {
                return -EFAULT; // Bad address
            }
            
            // Debug: Stampa a video i dati per confermare l'attraversamento
            printk(KERN_INFO "[Syscall_Throttling] Ricevuto MAX = %d per UID = %d (Programma: %s, Syscall: %d)\n", 
                   user_config.max_calls, user_config.target_uid, user_config.comm, user_config.syscall_num);
            break;
            
        default:
            return -ENOTTY; // Inappropriate ioctl for device
    }

    return 0; // Successo
}

// Struttura che mappa le operazioni supportate dal nostro device
static struct file_operations fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = defender_ioctl,
};

int init_char_device(void) {
    // Registrazione dinamica Major Number
    major_number = register_chrdev(0, DEVICE_NAME, &fops);
    if (major_number < 0) {
        printk(KERN_ALERT "[Syscall_Throttling] Registrazione device fallita.\n");
        return major_number;
    }
    printk(KERN_INFO "[Syscall_Throttling] Device registrato. Major number: %d\n", major_number);
    return 0;
}

void cleanup_char_device(void) {
    unregister_chrdev(major_number, DEVICE_NAME);
    printk(KERN_INFO "[Syscall_Throttling] Device deregistrato.\n");
}