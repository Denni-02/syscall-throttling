# Nome del modulo finale
obj-m += syscall_defender.o

# File sorgenti che compongono il modulo
syscall_defender-objs := kmod/core_main.o

# Variabili d'ambiente
KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

# Target predefinito
all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

# Target per la pulizia dei file temporanei
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean