#ifndef REGISTRY_DATA_H
#define REGISTRY_DATA_H

#include <linux/types.h>
#include "../include/defender_api.h"

// Prototipi delle funzioni esposte dal registry
int add_rule(int uid, const char *comm, int syscall_num, int max_calls);
int remove_rule(int syscall_num);
void debug_print_rules(void); // Per il Test Dummy

// Funzione che la syscall intercettata chiamerà per sapere se deve bloccarsi o meno
int is_throttled(int uid, const char *comm, int syscall_num, int *out_max_calls);

#endif // REGISTRY_DATA_H