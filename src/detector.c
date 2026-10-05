#include "detector.h"

void deadlock_report_init(DeadlockReport *report) {
    if (!report) return;
    memset(report, 0, sizeof(DeadlockReport));
    wfg_init(&report->wfg);
}

void deadlock_report_cleanup(DeadlockReport *report) {
    if (!report) return;
    wfg_cleanup(&report->wfg);

    if (report->deadlocked_names) {
        for (int i = 0; i < report->deadlocked_capacity; i++) {
            if (report->deadlocked_names[i]) {
                free(report->deadlocked_names[i]);
            }
        }
        free(report->deadlocked_names);
        report->deadlocked_names = NULL;
    }

    if (report->deadlocked_pids) {
        free(report->deadlocked_pids);
        report->deadlocked_pids = NULL;
    }

    if (report->cycle) {
        free(report->cycle);
        report->cycle = NULL;
    }

    report->deadlock_detected = false;
    report->cycle_len = 0;
    report->cycle_capacity = 0;
    report->deadlocked_count = 0;
    report->deadlocked_capacity = 0;
}

bool detector_run(const ProcessManager *pm, const ResourceManager *rm, DeadlockReport *report) {
    if (!pm || !rm || !report) return false;
    deadlock_report_init(report);

    int m = rm->count;
    if (m == 0 || pm->count == 0) {
        return false;
    }

    /* 1. Identify active processes */
    int *active_indices = (int *)malloc(pm->count * sizeof(int));
    if (!active_indices) return false;

    int n = 0;
    for (int i = 0; i < pm->count; i++) {
        const Process *p = pm->processes[i];
        if (!p) continue;
        if (p->state == PROCESS_STATE_COMPLETED || p->state == PROCESS_STATE_ABORTED) {
            continue;
        }
        active_indices[n++] = i;
    }

    if (n == 0) {
        free(active_indices);
        return false;
    }

    /* 2. Multiple-Instance Deadlock Detection Algorithm (Reduction) */
    int *work = (int *)malloc(m * sizeof(int));
    bool *finish = (bool *)calloc(n, sizeof(bool));
    if (!work || !finish) {
        if (work) free(work);
        if (finish) free(finish);
        free(active_indices);
        return false;
    }

    for (int j = 0; j < m; j++) {
        work[j] = rm->resources[j].available_instances;
    }

    for (int i = 0; i < n; i++) {
        const Process *p = pm->processes[active_indices[i]];
        bool has_alloc = false;
        bool has_req = false;

        for (int j = 0; j < m; j++) {
            int alloc_val = (j < p->res_capacity) ? p->allocated[j] : 0;
            int req_val = (j < p->res_capacity) ? p->requested[j] : 0;
            if (alloc_val > 0) has_alloc = true;
            if (req_val > 0) has_req = true;
        }

        /* If process holds nothing and requests nothing, it is finished */
        if (!has_alloc && !has_req) {
            finish[i] = true;
        } else if (!has_req) {
            /* Process is not waiting for any resource: it can complete and release held resources */
            finish[i] = true;
            for (int j = 0; j < m; j++) {
                int alloc_val = (j < p->res_capacity) ? p->allocated[j] : 0;
                work[j] += alloc_val;
            }
        } else {
            finish[i] = false;
        }
    }

    /* Iteratively find a process whose pending requests can be satisfied */
    bool progress = true;
    while (progress) {
        progress = false;
        for (int i = 0; i < n; i++) {
            if (!finish[i]) {
                const Process *p = pm->processes[active_indices[i]];
                bool can_satisfy = true;

                for (int j = 0; j < m; j++) {
                    int req_val = (j < p->res_capacity) ? p->requested[j] : 0;
                    if (req_val > work[j]) {
                        can_satisfy = false;
                        break;
                    }
                }

                if (can_satisfy) {
                    for (int j = 0; j < m; j++) {
                        int alloc_val = (j < p->res_capacity) ? p->allocated[j] : 0;
                        work[j] += alloc_val;
                    }
                    finish[i] = true;
                    progress = true;
                }
            }
        }
    }

    /* 3. Processes that remain unfinished are deadlocked */
    int deadlocked_count = 0;
    for (int i = 0; i < n; i++) {
        if (!finish[i]) {
            deadlocked_count++;
        }
    }

    if (deadlocked_count == 0) {
        report->deadlock_detected = false;
        free(work);
        free(finish);
        free(active_indices);
        return false;
    }

    report->deadlock_detected = true;
    report->deadlocked_count = deadlocked_count;
    report->deadlocked_capacity = deadlocked_count;
    report->deadlocked_pids = (int *)malloc(deadlocked_count * sizeof(int));
    report->deadlocked_names = (char **)malloc(deadlocked_count * sizeof(char *));
    for (int i = 0; i < deadlocked_count; i++) {
        report->deadlocked_names[i] = (char *)calloc(MAX_NAME_LEN, sizeof(char));
    }

    int d_idx = 0;
    for (int i = 0; i < n; i++) {
        if (!finish[i]) {
            const Process *p = pm->processes[active_indices[i]];
            report->deadlocked_pids[d_idx] = p->id;
            strncpy(report->deadlocked_names[d_idx], p->name, MAX_NAME_LEN - 1);
            report->deadlocked_names[d_idx][MAX_NAME_LEN - 1] = '\0';
            d_idx++;
        }
    }

    /* 4. Build Wait-For Graph among the deadlocked processes */
    for (int i = 0; i < report->deadlocked_count; i++) {
        wfg_add_node(&report->wfg, report->deadlocked_pids[i], report->deadlocked_names[i]);
    }

    for (int i = 0; i < n; i++) {
        if (finish[i]) continue;
        const Process *p_u = pm->processes[active_indices[i]];
        int u_idx = wfg_get_node_index(&report->wfg, p_u->id);
        if (u_idx < 0) continue;

        for (int r = 0; r < m; r++) {
            int req_val = (r < p_u->res_capacity) ? p_u->requested[r] : 0;
            if (req_val > 0) {
                /* Find other deadlocked processes holding resource r */
                for (int k = 0; k < n; k++) {
                    if (k == i || finish[k]) continue;
                    const Process *p_v = pm->processes[active_indices[k]];
                    int alloc_val = (r < p_v->res_capacity) ? p_v->allocated[r] : 0;
                    if (alloc_val > 0) {
                        int v_idx = wfg_get_node_index(&report->wfg, p_v->id);
                        if (v_idx >= 0) {
                            wfg_add_edge(&report->wfg, u_idx, v_idx);
                        }
                    }
                }
            }
        }
    }

    /* 5. Detect cycle in the Wait-For Graph */
    report->cycle_capacity = deadlocked_count + 2;
    report->cycle = (int *)malloc(report->cycle_capacity * sizeof(int));
    wfg_find_cycle_dfs(&report->wfg, report->cycle, &report->cycle_len);

    free(work);
    free(finish);
    free(active_indices);
    return true;
}

void detector_print_report(const DeadlockReport *report) {
    if (!report) return;

    if (report->deadlock_detected) {
        printf("\nDEADLOCK DETECTED\n\n");

        if (report->cycle_len > 0) {
            printf("Cycle:\n");
            for (int i = 0; i < report->cycle_len; i++) {
                int node_idx = report->cycle[i];
                printf("%s%s", report->wfg.node_names[node_idx],
                       (i == report->cycle_len - 1) ? "\n" : " -> ");
            }
            printf("\n");
        }

        printf("Deadlocked set:\n");
        for (int i = 0; i < report->deadlocked_count; i++) {
            printf("%s%s", report->deadlocked_names[i],
                   (i == report->deadlocked_count - 1) ? "\n" : " ");
        }
        printf("\n");
    } else {
        printf("\nNO DEADLOCK DETECTED\n\n");
    }
}
