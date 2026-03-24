#ifndef REGISTRY_DATA_H
#define REGISTRY_DATA_H

#include <linux/types.h>
#include "../include/defender_api.h"

// Prototipi delle funzioni esposte dal registry
int add_rule(int uid, const char *comm, int syscall_num, int max_calls);
void debug_print_rules(void); // Per il Test Dummy

#endif // REGISTRY_DATA_H