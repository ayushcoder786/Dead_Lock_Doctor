#include "process.h"

const char *process_state_to_string(ProcessState state) {
    switch (state) {
        case PROCESS_STATE_READY:     return "READY";
        case PROCESS_STATE_RUNNING:   return "RUNNING";
        case PROCESS_STATE_BLOCKED:   return "BLOCKED";
        case PROCESS_STATE_COMPLETED: return "COMPLETED";
        case PROCESS_STATE_ABORTED:   return "ABORTED";
        default:                      return "UNKNOWN";
    }
}

const char *alloc_result_to_string(AllocResult res) {
    switch (res) {
        case ALLOC_GRANTED:  return "GRANTED";
        case ALLOC_BLOCKED:  return "BLOCKED";
        case ALLOC_RELEASED: return "RELEASED";
        case ALLOC_INVALID:  return "INVALID REQUEST";
        default:             return "UNKNOWN";
    }
}

void process_manager_init(ProcessManager *pm) {
    if (!pm) return;
    pm->processes = NULL;
    pm->count = 0;
    pm->capacity = 0;
}

void process_destroy(Process *proc) {
    if (!proc) return;
    if (proc->allocated) {
        free(proc->allocated);
        proc->allocated = NULL;
    }
    if (proc->requested) {
        free(proc->requested);
        proc->requested = NULL;
    }
    if (proc->max_demand) {
        free(proc->max_demand);
        proc->max_demand = NULL;
    }
    free(proc);
}

void process_manager_cleanup(ProcessManager *pm) {
    if (!pm) return;
    for (int i = 0; i < pm->count; i++) {
        if (pm->processes[i]) {
            process_destroy(pm->processes[i]);
            pm->processes[i] = NULL;
        }
    }
    if (pm->processes) {
        free(pm->processes);
        pm->processes = NULL;
    }
    pm->count = 0;
    pm->capacity = 0;
}

bool process_ensure_resource_capacity(Process *proc, int needed_capacity) {
    if (!proc) return false;
    if (needed_capacity <= proc->res_capacity) return true;

    int new_cap = proc->res_capacity == 0 ? 4 : proc->res_capacity;
    while (new_cap < needed_capacity) {
        new_cap *= 2;
    }

    int *new_alloc = (int *)realloc(proc->allocated, new_cap * sizeof(int));
    if (!new_alloc) return false;
    proc->allocated = new_alloc;

    int *new_req = (int *)realloc(proc->requested, new_cap * sizeof(int));
    if (!new_req) return false;
    proc->requested = new_req;

    int *new_max = (int *)realloc(proc->max_demand, new_cap * sizeof(int));
    if (!new_max) return false;
    proc->max_demand = new_max;

    /* Zero-initialize the newly extended resource slots */
    memset(proc->allocated + proc->res_capacity, 0, (new_cap - proc->res_capacity) * sizeof(int));
    memset(proc->requested + proc->res_capacity, 0, (new_cap - proc->res_capacity) * sizeof(int));
    memset(proc->max_demand + proc->res_capacity, 0, (new_cap - proc->res_capacity) * sizeof(int));

    proc->res_capacity = new_cap;
    return true;
}

Process *process_create(ProcessManager *pm, int id, const char *name) {
    if (!pm || !name) {
        fprintf(stderr, "[Process Error] Invalid parameters.\n");
        return NULL;
    }

    if (id < 0) {
        fprintf(stderr, "[Process Error] Process ID must be non-negative.\n");
        return NULL;
    }

    if (process_find_by_id(pm, id) != NULL) {
        fprintf(stderr, "[Process Error] Process with ID %d already exists.\n", id);
        return NULL;
    }

    /* Dynamically grow process pointer array */
    if (pm->count >= pm->capacity) {
        int new_cap = pm->capacity == 0 ? 4 : pm->capacity * 2;
        Process **new_procs = (Process **)realloc(pm->processes, new_cap * sizeof(Process *));
        if (!new_procs) {
            fprintf(stderr, "[Process Error] Memory reallocation failed for process list.\n");
            return NULL;
        }
        pm->processes = new_procs;
        pm->capacity = new_cap;
    }

    Process *proc = (Process *)malloc(sizeof(Process));
    if (!proc) {
        fprintf(stderr, "[Process Error] Memory allocation failed for process %d.\n", id);
        return NULL;
    }

    proc->id = id;
    proc->priority = 1;
    strncpy(proc->name, name, MAX_NAME_LEN - 1);
    proc->name[MAX_NAME_LEN - 1] = '\0';
    proc->state = PROCESS_STATE_READY;
    proc->res_capacity = 0;
    proc->allocated = NULL;
    proc->requested = NULL;
    proc->max_demand = NULL;

    /* Initial resource capacity */
    if (!process_ensure_resource_capacity(proc, 4)) {
        process_destroy(proc);
        return NULL;
    }

    pm->processes[pm->count++] = proc;
    return proc;
}

bool process_remove(ProcessManager *pm, int id) {
    if (!pm) return false;
    int idx = -1;
    for (int i = 0; i < pm->count; i++) {
        if (pm->processes[i] && pm->processes[i]->id == id) {
            idx = i;
            break;
        }
    }

    if (idx < 0) return false;

    process_destroy(pm->processes[idx]);
    for (int i = idx; i < pm->count - 1; i++) {
        pm->processes[i] = pm->processes[i + 1];
    }
    pm->processes[pm->count - 1] = NULL;
    pm->count--;
    return true;
}

Process *process_find_by_id(ProcessManager *pm, int id) {
    if (!pm) return NULL;
    for (int i = 0; i < pm->count; i++) {
        if (pm->processes[i] && pm->processes[i]->id == id) {
            return pm->processes[i];
        }
    }
    return NULL;
}

const Process *process_find_by_id_const(const ProcessManager *pm, int id) {
    if (!pm) return NULL;
    for (int i = 0; i < pm->count; i++) {
        if (pm->processes[i] && pm->processes[i]->id == id) {
            return pm->processes[i];
        }
    }
    return NULL;
}

Process *process_find_by_id_or_name(ProcessManager *pm, const char *ident) {
    if (!pm || !ident) return NULL;

    /* 1. Try finding by name */
    for (int i = 0; i < pm->count; i++) {
        if (pm->processes[i] && strcasecmp(pm->processes[i]->name, ident) == 0) {
            return pm->processes[i];
        }
    }

    /* 2. Try prefix 'P' or 'p' (e.g. P1, p101) */
    if ((ident[0] == 'P' || ident[0] == 'p') && isdigit((unsigned char)ident[1])) {
        int id = atoi(&ident[1]);
        Process *p = process_find_by_id(pm, id);
        if (p) return p;
    }

    /* 3. Try pure numeric string */
    char *endptr = NULL;
    long val = strtol(ident, &endptr, 10);
    if (endptr != ident && *endptr == '\0') {
        Process *p = process_find_by_id(pm, (int)val);
        if (p) return p;
    }

    return NULL;
}

const Process *process_find_by_id_or_name_const(const ProcessManager *pm, const char *ident) {
    if (!pm || !ident) return NULL;

    for (int i = 0; i < pm->count; i++) {
        if (pm->processes[i] && strcasecmp(pm->processes[i]->name, ident) == 0) {
            return pm->processes[i];
        }
    }

    if ((ident[0] == 'P' || ident[0] == 'p') && isdigit((unsigned char)ident[1])) {
        int id = atoi(&ident[1]);
        return process_find_by_id_const(pm, id);
    }

    char *endptr = NULL;
    long val = strtol(ident, &endptr, 10);
    if (endptr != ident && *endptr == '\0') {
        return process_find_by_id_const(pm, (int)val);
    }

    return NULL;
}

bool process_terminate(ProcessManager *pm, int id) {
    Process *proc = process_find_by_id(pm, id);
    if (!proc) {
        fprintf(stderr, "[Process Error] Process with ID %d not found.\n", id);
        return false;
    }

    if (proc->state == PROCESS_STATE_COMPLETED || proc->state == PROCESS_STATE_ABORTED) {
        fprintf(stderr, "[Process Error] Process %d is already terminated (%s).\n",
                id, process_state_to_string(proc->state));
        return false;
    }

    proc->state = PROCESS_STATE_COMPLETED;
    return true;
}

int process_get_need(const Process *proc, int res_index, int total_instances) {
    (void)total_instances;
    if (!proc || res_index < 0 || res_index >= proc->res_capacity) return 0;
    int max_val = proc->max_demand[res_index];
    if (max_val > 0) {
        int need = max_val - proc->allocated[res_index];
        return need > 0 ? need : 0;
    }
    return 0;
}

void process_list(const ProcessManager *pm, const ResourceManager *rm) {
    if (!pm || pm->count == 0) {
        printf("  [No processes registered in system]\n");
        return;
    }

    printf("+-----+----------------------+-----------+---------------------+---------------------+---------------------+---------------------+\n");
    printf("| PID | Process Name         | State     | Allocation          | Max Claim           | Need (Max-Alloc)    | Pending Request     |\n");
    printf("+-----+----------------------+-----------+---------------------+---------------------+---------------------+---------------------+\n");

    for (int i = 0; i < pm->count; i++) {
        const Process *proc = pm->processes[i];
        if (!proc) continue;

        char alloc_str[256] = "-";
        char max_str[256] = "-";
        char need_str[256] = "-";
        char req_str[256] = "-";

        if (rm && rm->count > 0) {
            char temp_alloc[256] = "";
            char temp_max[256] = "";
            char temp_need[256] = "";
            char temp_req[256] = "";

            bool has_alloc = false, has_max = false, has_need = false, has_req = false;

            for (int r = 0; r < rm->count; r++) {
                char entry[MAX_NAME_LEN + 32];

                int alloc_val = (r < proc->res_capacity) ? proc->allocated[r] : 0;
                int max_val = (r < proc->res_capacity) ? proc->max_demand[r] : 0;
                int req_val = (r < proc->res_capacity) ? proc->requested[r] : 0;

                if (alloc_val > 0) {
                    snprintf(entry, sizeof(entry), "%s:%d ", rm->resources[r].name, alloc_val);
                    strncat(temp_alloc, entry, sizeof(temp_alloc) - strlen(temp_alloc) - 1);
                    has_alloc = true;
                }

                if (max_val > 0) {
                    snprintf(entry, sizeof(entry), "%s:%d ", rm->resources[r].name, max_val);
                    strncat(temp_max, entry, sizeof(temp_max) - strlen(temp_max) - 1);
                    has_max = true;
                }

                int need = process_get_need(proc, r, rm->resources[r].total_instances);
                if (need > 0) {
                    snprintf(entry, sizeof(entry), "%s:%d ", rm->resources[r].name, need);
                    strncat(temp_need, entry, sizeof(temp_need) - strlen(temp_need) - 1);
                    has_need = true;
                }

                if (req_val > 0) {
                    snprintf(entry, sizeof(entry), "%s:%d ", rm->resources[r].name, req_val);
                    strncat(temp_req, entry, sizeof(temp_req) - strlen(temp_req) - 1);
                    has_req = true;
                }
            }

            if (has_alloc) {
                strncpy(alloc_str, temp_alloc, sizeof(alloc_str) - 1);
                alloc_str[sizeof(alloc_str) - 1] = '\0';
            }
            if (has_max) {
                strncpy(max_str, temp_max, sizeof(max_str) - 1);
                max_str[sizeof(max_str) - 1] = '\0';
            }
            if (has_need) {
                strncpy(need_str, temp_need, sizeof(need_str) - 1);
                need_str[sizeof(need_str) - 1] = '\0';
            }
            if (has_req) {
                strncpy(req_str, temp_req, sizeof(req_str) - 1);
                req_str[sizeof(req_str) - 1] = '\0';
            }
        }

        printf("| %-3d | %-20s | %-9s | %-19s | %-19s | %-19s | %-19s |\n",
               proc->id, proc->name, process_state_to_string(proc->state),
               alloc_str, max_str, need_str, req_str);
    }
    printf("+-----+----------------------+-----------+---------------------+---------------------+---------------------+---------------------+\n");
}

void process_set_priority(Process *proc, int priority) {
    if (proc) {
        proc->priority = priority;
    }
}

