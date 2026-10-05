#ifndef DEADLOCKDOCTOR_PROCESS_H
#define DEADLOCKDOCTOR_PROCESS_H

#include "common.h"
#include "resource.h"

typedef struct {
    int id;
    char name[MAX_NAME_LEN];
    ProcessState state;
    int priority;                  /* Process priority for scheduling/recovery cost (default 1) */
    int res_capacity;              /* current capacity of resource vectors */
    int *allocated;                /* dynamically allocated instances per resource index */
    int *requested;                /* dynamically allocated pending requests per resource index */
    int *max_demand;               /* dynamically allocated maximum instances needed */
} Process;

typedef struct {
    Process **processes;           /* dynamically resized array of Process pointers */
    int count;
    int capacity;
} ProcessManager;

void process_manager_init(ProcessManager *pm);
void process_manager_cleanup(ProcessManager *pm);
Process *process_create(ProcessManager *pm, int id, const char *name);
void process_destroy(Process *proc);
bool process_remove(ProcessManager *pm, int id);
bool process_ensure_resource_capacity(Process *proc, int needed_capacity);
Process *process_find_by_id(ProcessManager *pm, int id);
const Process *process_find_by_id_const(const ProcessManager *pm, int id);
Process *process_find_by_id_or_name(ProcessManager *pm, const char *ident);
const Process *process_find_by_id_or_name_const(const ProcessManager *pm, const char *ident);
bool process_terminate(ProcessManager *pm, int id);
void process_set_priority(Process *proc, int priority);
int process_get_need(const Process *proc, int res_index, int total_instances);
void process_list(const ProcessManager *pm, const ResourceManager *rm);

#endif /* DEADLOCKDOCTOR_PROCESS_H */

