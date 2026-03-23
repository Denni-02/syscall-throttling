#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

// Informazioni del modulo
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Dennis/0365494");
MODULE_DESCRIPTION("Syscall Throttling LKM - Advanced Operating Systems");
MODULE_VERSION("0.1");

// Funzione chiamata al caricamento del modulo (insmod)
static int __init core_init(void) {
    printk(KERN_INFO "[Syscall_Throttling] Modulo Caricato con successo.\n");
    return 0; // caricamento completato senza errori
}

// Funzione chiamata allo scaricamento del modulo (rmmod)
static void __exit core_exit(void) {
    printk(KERN_INFO "[Syscall_Throttling] Modulo Scaricato con successo.\n");
}

// Registrazione delle macro di init ed exit
module_init(core_init);
module_exit(core_exit);