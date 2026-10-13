#ifndef DEADLOCKDOCTOR_BANKER_H
#define DEADLOCKDOCTOR_BANKER_H

#include "common.h"
#include "process.h"
#include "resource.h"

typedef struct {
    int num_processes;
    int num_resources;
    int *process_ids;              /* dynamically allocated process ID array [num_processes] */
    char **process_names;          /* dynamically allocated process names [num_processes][MAX_NAME_LEN] */
    char **resource_names;         /* dynamically allocated resource names [num_resources][MAX_NAME_LEN] */
    int *available;                /* dynamically allocated Available vector [num_resources] */
    int **allocation;              /* dynamically allocated Allocation matrix [num_processes][num_resources] */
    int **max;                     /* dynamically allocated Max matrix [num_processes][num_resources] */
    int **need;                    /* dynamically allocated Need matrix [num_processes][num_resources] */
} BankerSystem;

void banker_system_init(BankerSystem *bs);
bool banker_system_allocate(BankerSystem *bs, int num_processes, int num_resources);
void banker_system_cleanup(BankerSystem *bs);
bool banker_system_copy(BankerSystem *dest, const BankerSystem *src);

bool banker_init_from_manager(BankerSystem *bs, const ProcessManager *pm, const ResourceManager *rm);

bool banker_is_safe(const BankerSystem *bs, int *safe_sequence, int *seq_len);

bool banker_check_request_safety(const BankerSystem *bs,
                                int process_idx,
                                const int *request,
                                BankerSystem *out_new_state,
                                int *safe_sequence,
                                int *seq_len);

void banker_print_matrices(const BankerSystem *bs);
void banker_print_safety_report(const BankerSystem *bs);

#endif /* DEADLOCKDOCTOR_BANKER_H */

