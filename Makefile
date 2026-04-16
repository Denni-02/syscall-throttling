# Nome del modulo finale
obj-m += syscall_defender.o

# File sorgenti che compongono il modulo
syscall_defender-objs := kmod/core_main.o kmod/char_device.o kmod/registry_data.o kmod/sys_interceptor.o kmod/mmu_scanner.o kmod/kprobes_discovery.o kmod/policy_engine.o

# --- Flag Architetturali Sincronizzazione ---

# Impostazione default per la sincronizzazione (RCU)
SYNC ?= rcu
ifeq ($(SYNC), spinlock)
    ccflags-y += -DUSE_SPINLOCK=1
    $(info [Build] Sincronizzazione: SPINLOCK GLOBALE)
else
    ccflags-y += -DUSE_SPINLOCK=0
    $(info [Build] Sincronizzazione: LOCK-FREE RCU (Default))
endif

# --- Flag Architetturali Discovery ---

# Impostazione default per la ricerca del discovery (Kprobes)
DISCOVERY ?= kprobes
ifeq ($(DISCOVERY), kprobes)
    ccflags-y += -DUSE_KPROBES_DISCOVERY=1
    $(info [Build] Discovery Syscall Table: KPROBES)
else
    ccflags-y += -DUSE_KPROBES_DISCOVERY=0
    $(info [Build] Discovery Syscall Table: SCANNER)
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

load: all
	@echo "\n[+] Caricamento modulo nel kernel..."
	sudo insmod syscall_defender.ko
	@echo "[+] Creazione dinamica del device node..."
	sudo rm -f /dev/syscall_defender
	sudo mknod /dev/syscall_defender c $$(awk '$$2=="syscall_defender" {print $$1}' /proc/devices) 0
	sudo chmod 666 /dev/syscall_defender
	@echo "[+] Modulo pronto all'uso!"

unload:
	@echo "\n[+] Rimozione modulo..."
	-sudo rmmod syscall_defender 2>/dev/null || true
	-sudo rm -f /dev/syscall_defender
	@echo "[+] Pulizia dei file di build..."
	$(MAKE) -C $(KDIR) M=$(PWD) clean

reload: unload load
	@echo "\n[+] Reload completato con successo!"