#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

// Informazioni del modulo
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Dennis/0365494");
MODULE_DESCRIPTION("Syscall Throttling LKM - Advanced Operating Systems");
MODULE_VERSION("0.1");

// Dichiarazione funzioni esterne char_device.c
extern int init_char_device(void);
extern void cleanup_char_device(void);

// Funzione chiamata al caricamento del modulo (insmod)
static int __init core_init(void) {
    int ret;
    printk(KERN_INFO "[Syscall_Throttling] Modulo Caricato con successo.\n");

    ret = init_char_device();
    if (ret < 0) return ret;

    return 0; // caricamento completato senza errori
}

// Funzione chiamata allo scaricamento del modulo (rmmod)
static void __exit core_exit(void) {
    cleanup_char_device();
    printk(KERN_INFO "[Syscall_Throttling] Modulo Scaricato con successo.\n");
}

// Registrazione delle macro di init ed exit
module_init(core_init);
module_exit(core_exit);