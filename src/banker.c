#include "banker.h"
#include "logger.h"

void banker_system_init(BankerSystem *bs) {
    if (!bs) return;
    memset(bs, 0, sizeof(BankerSystem));
}

void banker_system_cleanup(BankerSystem *bs) {
    if (!bs) return;

    if (bs->allocation) {
        for (int i = 0; i < bs->num_processes; i++) {
            if (bs->allocation[i]) free(bs->allocation[i]);
        }
        free(bs->allocation);
        bs->allocation = NULL;
    }

    if (bs->max) {
        for (int i = 0; i < bs->num_processes; i++) {
            if (bs->max[i]) free(bs->max[i]);
        }
        free(bs->max);
        bs->max = NULL;
    }

    if (bs->need) {
        for (int i = 0; i < bs->num_processes; i++) {
            if (bs->need[i]) free(bs->need[i]);
        }
        free(bs->need);
        bs->need = NULL;
    }

    if (bs->process_names) {
        for (int i = 0; i < bs->num_processes; i++) {
            if (bs->process_names[i]) free(bs->process_names[i]);
        }
        free(bs->process_names);
        bs->process_names = NULL;
    }

    if (bs->resource_names) {
        for (int j = 0; j < bs->num_resources; j++) {
            if (bs->resource_names[j]) free(bs->resource_names[j]);
        }
        free(bs->resource_names);
        bs->resource_names = NULL;
    }

    if (bs->process_ids) {
        free(bs->process_ids);
        bs->process_ids = NULL;
    }

    if (bs->available) {
        free(bs->available);
        bs->available = NULL;
    }

    bs->num_processes = 0;
    bs->num_resources = 0;
}

bool banker_system_allocate(BankerSystem *bs, int num_processes, int num_resources) {
    if (!bs) return false;
    banker_system_cleanup(bs);

    bs->num_processes = num_processes;
    bs->num_resources = num_resources;

    if (num_processes > 0) {
        bs->process_ids = (int *)malloc(num_processes * sizeof(int));
        bs->process_names = (char **)malloc(num_processes * sizeof(char *));
        bs->allocation = (int **)malloc(num_processes * sizeof(int *));
        bs->max = (int **)malloc(num_processes * sizeof(int *));
        bs->need = (int **)malloc(num_processes * sizeof(int *));

        for (int i = 0; i < num_processes; i++) {
            bs->process_ids[i] = -1;
            bs->process_names[i] = (char *)calloc(MAX_NAME_LEN, sizeof(char));

            if (num_resources > 0) {
                bs->allocation[i] = (int *)calloc(num_resources, sizeof(int));
                bs->max[i] = (int *)calloc(num_resources, sizeof(int));
                bs->need[i] = (int *)calloc(num_resources, sizeof(int));
            } else {
                bs->allocation[i] = NULL;
                bs->max[i] = NULL;
                bs->need[i] = NULL;
            }
        }
    }

    if (num_resources > 0) {
        bs->resource_names = (char **)malloc(num_resources * sizeof(char *));
        for (int j = 0; j < num_resources; j++) {
            bs->resource_names[j] = (char *)calloc(MAX_NAME_LEN, sizeof(char));
        }
        bs->available = (int *)calloc(num_resources, sizeof(int));
    }

    return true;
}

bool banker_system_copy(BankerSystem *dest, const BankerSystem *src) {
    if (!dest || !src) return false;

    banker_system_allocate(dest, src->num_processes, src->num_resources);

    for (int j = 0; j < src->num_resources; j++) {
        strncpy(dest->resource_names[j], src->resource_names[j], MAX_NAME_LEN - 1);
        dest->resource_names[j][MAX_NAME_LEN - 1] = '\0';
        dest->available[j] = src->available[j];
    }

    for (int i = 0; i < src->num_processes; i++) {
        dest->process_ids[i] = src->process_ids[i];
        strncpy(dest->process_names[i], src->process_names[i], MAX_NAME_LEN - 1);
        dest->process_names[i][MAX_NAME_LEN - 1] = '\0';

        for (int j = 0; j < src->num_resources; j++) {
            dest->allocation[i][j] = src->allocation[i][j];
            dest->max[i][j] = src->max[i][j];
            dest->need[i][j] = src->need[i][j];
        }
    }

    return true;
}

bool banker_init_from_manager(BankerSystem *bs, const ProcessManager *pm, const ResourceManager *rm) {
    if (!bs || !pm || !rm) return false;
    banker_system_init(bs);

    /* Count active processes */
    int active_procs = 0;
    for (int i = 0; i < pm->count; i++) {
        const Process *p = pm->processes[i];
        if (!p) continue;
        if (p->state == PROCESS_STATE_COMPLETED || p->state == PROCESS_STATE_ABORTED) {
            continue;
        }
        active_procs++;
    }

    banker_system_allocate(bs, active_procs, rm->count);

    for (int j = 0; j < rm->count; j++) {
        strncpy(bs->resource_names[j], rm->resources[j].name, MAX_NAME_LEN - 1);
        bs->resource_names[j][MAX_NAME_LEN - 1] = '\0';
        bs->available[j] = rm->resources[j].available_instances;
    }

    int p_idx = 0;
    for (int i = 0; i < pm->count; i++) {
        const Process *p = pm->processes[i];
        if (!p) continue;
        if (p->state == PROCESS_STATE_COMPLETED || p->state == PROCESS_STATE_ABORTED) {
            continue;
        }

        bs->process_ids[p_idx] = p->id;
        strncpy(bs->process_names[p_idx], p->name, MAX_NAME_LEN - 1);
        bs->process_names[p_idx][MAX_NAME_LEN - 1] = '\0';

        for (int j = 0; j < rm->count; j++) {
            int alloc_val = (j < p->res_capacity) ? p->allocated[j] : 0;
            int max_val = (j < p->res_capacity) ? p->max_demand[j] : 0;

            bs->allocation[p_idx][j] = alloc_val;
            bs->max[p_idx][j] = max_val;

            if (max_val > 0) {
                int need = max_val - alloc_val;
                bs->need[p_idx][j] = (need > 0) ? need : 0;
            } else {
                int need = rm->resources[j].total_instances - alloc_val;
                bs->need[p_idx][j] = (need > 0) ? need : 0;
            }
        }
        p_idx++;
    }

    return true;
}

bool banker_is_safe(const BankerSystem *bs, int *safe_sequence, int *seq_len) {
    if (!bs || !safe_sequence || !seq_len) return false;

    *seq_len = 0;
    int n = bs->num_processes;
    int m = bs->num_resources;

    if (n == 0) {
        return true;
    }

    int *work = (int *)malloc(m * sizeof(int));
    if (!work && m > 0) return false;
    for (int j = 0; j < m; j++) {
        work[j] = bs->available[j];
    }

    bool *finish = (bool *)calloc(n, sizeof(bool));
    if (!finish) {
        free(work);
        return false;
    }

    int count = 0;
    while (count < n) {
        bool found = false;

        for (int i = 0; i < n; i++) {
            if (!finish[i]) {
                bool can_satisfy = true;
                for (int j = 0; j < m; j++) {
                    if (bs->need[i][j] > work[j]) {
                        can_satisfy = false;
                        break;
                    }
                }

                if (can_satisfy) {
                    for (int j = 0; j < m; j++) {
                        work[j] += bs->allocation[i][j];
                    }
                    safe_sequence[count++] = i;
                    finish[i] = true;
                    found = true;
                }
            }
        }

        if (!found) {
            break;
        }
    }

    *seq_len = count;
    bool is_safe = (count == n);

    free(work);
    free(finish);
    return is_safe;
}

bool banker_check_request_safety(const BankerSystem *bs,
                                int process_idx,
                                const int *request,
                                BankerSystem *out_new_state,
                                int *safe_sequence,
                                int *seq_len) {
    if (!bs || !request || process_idx < 0 || process_idx >= bs->num_processes) {
        return false;
    }

    int m = bs->num_resources;

    /* Step 1: Check if Request <= Need */
    for (int j = 0; j < m; j++) {
        if (request[j] > bs->need[process_idx][j]) {
            return false;
        }
    }

    /* Step 2: Check if Request <= Available */
    for (int j = 0; j < m; j++) {
        if (request[j] > bs->available[j]) {
            return false;
        }
    }

    /* Step 3: Deep copy tentative state */
    BankerSystem temp;
    banker_system_init(&temp);
    if (!banker_system_copy(&temp, bs)) {
        return false;
    }

    for (int j = 0; j < m; j++) {
        temp.available[j] -= request[j];
        temp.allocation[process_idx][j] += request[j];
        temp.need[process_idx][j] -= request[j];
    }

    /* Step 4: Run safety algorithm on tentative state */
    bool is_safe = banker_is_safe(&temp, safe_sequence, seq_len);

    if (is_safe && out_new_state) {
        banker_system_copy(out_new_state, &temp);
    }

    banker_system_cleanup(&temp);
    return is_safe;
}

void banker_print_matrices(const BankerSystem *bs) {
    if (!bs) return;

    printf("\n+---------+--------------------+-------------------------+-------------------------+-------------------------+\n");
    printf("| PID     | Process Name       | Allocation              | Max Claim               | Need (Max - Alloc)      |\n");
    printf("+---------+--------------------+-------------------------+-------------------------+-------------------------+\n");

    for (int i = 0; i < bs->num_processes; i++) {
        char alloc_str[256] = "";
        char max_str[256] = "";
        char need_str[256] = "";

        for (int j = 0; j < bs->num_resources; j++) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%s:%d ", bs->resource_names[j], bs->allocation[i][j]);
            strncat(alloc_str, buf, sizeof(alloc_str) - strlen(alloc_str) - 1);

            snprintf(buf, sizeof(buf), "%s:%d ", bs->resource_names[j], bs->max[i][j]);
            strncat(max_str, buf, sizeof(max_str) - strlen(max_str) - 1);

            snprintf(buf, sizeof(buf), "%s:%d ", bs->resource_names[j], bs->need[i][j]);
            strncat(need_str, buf, sizeof(need_str) - strlen(need_str) - 1);
        }

        printf("| %-7d | %-18s | %-23s | %-23s | %-23s |\n",
               bs->process_ids[i], bs->process_names[i], alloc_str, max_str, need_str);
    }
    printf("+---------+--------------------+-------------------------+-------------------------+-------------------------+\n\n");
}

void banker_print_safety_report(const BankerSystem *bs) {
    if (!bs) return;

    printf("\n================================================================================\n");
    printf("                    BANKER'S ALGORITHM SAFETY EVALUATION                        \n");
    printf("================================================================================\n");

    printf("\n[BANKER RESOURCE MATRICES]\n");
    banker_print_matrices(bs);

    printf("BANKER SAFETY CHECK\n\n");
    printf("Available:\n");
    for (int j = 0; j < bs->num_resources; j++) {
        printf("%s%s", bs->resource_names[j], (j == bs->num_resources - 1) ? "\n" : " ");
    }
    for (int j = 0; j < bs->num_resources; j++) {
        printf("%-3d%s", bs->available[j], (j == bs->num_resources - 1) ? "\n" : " ");
    }
    printf("\n");

    if (bs->num_processes == 0) {
        printf("Safe sequence:\n(none - no active processes)\n\nSYSTEM STATE: SAFE\n\n");
        return;
    }

    int *safe_seq = (int *)malloc(bs->num_processes * sizeof(int));
    if (!safe_seq) return;
    int seq_len = 0;

    /* Step-by-step trace simulation */
    int *work = (int *)malloc(bs->num_resources * sizeof(int));
    bool *finish = (bool *)calloc(bs->num_processes, sizeof(bool));
    for (int j = 0; j < bs->num_resources; j++) {
        work[j] = bs->available[j];
    }

    printf("[SAFETY ALGORITHM STEP-BY-STEP TRACE]\n");
    int step = 1;
    bool safe = true;

    while (seq_len < bs->num_processes) {
        bool found = false;
        for (int i = 0; i < bs->num_processes; i++) {
            if (!finish[i]) {
                bool can_satisfy = true;
                for (int j = 0; j < bs->num_resources; j++) {
                    if (bs->need[i][j] > work[j]) {
                        can_satisfy = false;
                        break;
                    }
                }

                if (can_satisfy) {
                    printf("  Step %d: Process '%s' (PID %d) Need [ ", step++, bs->process_names[i], bs->process_ids[i]);
                    for (int j = 0; j < bs->num_resources; j++) {
                        printf("%s:%d%s", bs->resource_names[j], bs->need[i][j], (j == bs->num_resources - 1) ? "" : " ");
                    }
                    printf(" ] <= Work [ ");
                    for (int j = 0; j < bs->num_resources; j++) {
                        printf("%s:%d%s", bs->resource_names[j], work[j], (j == bs->num_resources - 1) ? "" : " ");
                    }
                    printf(" ]\n");

                    for (int j = 0; j < bs->num_resources; j++) {
                        work[j] += bs->allocation[i][j];
                    }

                    printf("          -> '%s' finishes and releases held resources. New Work = [ ", bs->process_names[i]);
                    for (int j = 0; j < bs->num_resources; j++) {
                        printf("%s:%d%s", bs->resource_names[j], work[j], (j == bs->num_resources - 1) ? "" : " ");
                    }
                    printf(" ]\n");

                    safe_seq[seq_len++] = i;
                    finish[i] = true;
                    found = true;
                    break;
                }
            }
        }

        if (!found) {
            safe = false;
            printf("  Unsafe Step %d: No remaining unfinished process can be satisfied by current Work vector [ ", step);
            for (int j = 0; j < bs->num_resources; j++) {
                printf("%s:%d%s", bs->resource_names[j], work[j], (j == bs->num_resources - 1) ? "" : " ");
            }
            printf(" ]\n");

            printf("  [Unsatisfied Process Constraints]:\n");
            for (int i = 0; i < bs->num_processes; i++) {
                if (!finish[i]) {
                    printf("    * '%s' needs [ ", bs->process_names[i]);
                    for (int j = 0; j < bs->num_resources; j++) {
                        printf("%s:%d%s", bs->resource_names[j], bs->need[i][j], (j == bs->num_resources - 1) ? "" : " ");
                    }
                    printf(" ] exceeding available Work\n");
                }
            }
            break;
        }
    }

    printf("\n");

    if (safe) {
        printf("Safe sequence:\n");
        char seq_buf[256] = "";
        for (int i = 0; i < seq_len; i++) {
            int p_idx = safe_seq[i];
            printf("%s%s", bs->process_names[p_idx], (i == seq_len - 1) ? "\n" : " -> ");
            strncat(seq_buf, bs->process_names[p_idx], sizeof(seq_buf) - strlen(seq_buf) - 1);
            if (i < seq_len - 1) {
                strncat(seq_buf, " -> ", sizeof(seq_buf) - strlen(seq_buf) - 1);
            }
        }
        printf("\nSYSTEM STATE: SAFE\n\n");
        logger_log(LOG_BANKER_RESULT, "Banker safety check: SYSTEM STATE: SAFE. Safe sequence: %s.", seq_buf);
    } else {
        printf("SYSTEM STATE: UNSAFE\n");
        printf("Request BLOCKED\n\n");
        printf("[EXPLANATION OF UNSAFE STATE]\n");
        printf("  The system cannot guarantee that all processes can finish without risk of circular wait.\n");
        printf("  An unsafe state is NOT necessarily a deadlock, but deadlock could occur if processes request their maximum claims.\n\n");
        logger_log(LOG_BANKER_RESULT, "Banker safety check: SYSTEM STATE: UNSAFE. No safe sequence found.");
    }

    free(work);
    free(finish);
    free(safe_seq);
}
