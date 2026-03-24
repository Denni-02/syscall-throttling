# Nome del modulo finale
obj-m += syscall_defender.o

# File sorgenti che compongono il modulo
syscall_defender-objs := kmod/core_main.o kmod/char_device.o kmod/registry_data.o

# Impostazione default per la sincronizzazione (RCU)
SYNC ?= rcu

ifeq ($(SYNC), spinlock)
    ccflags-y += -DUSE_SPINLOCK=1
    $(info [Build] Sincronizzazione: SPINLOCK GLOBALE)
else
    ccflags-y += -DUSE_SPINLOCK=0
    $(info [Build] Sincronizzazione: LOCK-FREE RCU (Default))
endif

# Variabili d'ambiente
KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

# Target predefinito
all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

# Target per la pulizia dei file temporanei
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean