#include "monitor.h"
#include "manager.h"
#include <sys/select.h>
#include <termios.h>

void monitor_show_process_table(const ProcessManager *pm, const ResourceManager *rm) {
    if (!pm) return;

    int ready_cnt = 0, running_cnt = 0, blocked_cnt = 0, completed_cnt = 0, aborted_cnt = 0;
    for (int i = 0; i < pm->count; i++) {
        if (!pm->processes[i]) continue;
        switch (pm->processes[i]->state) {
            case PROCESS_STATE_READY:     ready_cnt++; break;
            case PROCESS_STATE_RUNNING:   running_cnt++; break;
            case PROCESS_STATE_BLOCKED:   blocked_cnt++; break;
            case PROCESS_STATE_COMPLETED: completed_cnt++; break;
            case PROCESS_STATE_ABORTED:   aborted_cnt++; break;
        }
    }

    printf("=================================================================================================================\n");
    printf("                                          LIVE PROCESS MONITOR                                                   \n");
    printf("=================================================================================================================\n");
    printf("+-----+----------------------+-----------+----------+----------------------+----------------------+-------------+\n");
    printf("| PID | Process Name         | State     | Priority | Held Resources       | Pending Requests     | Waiting     |\n");
    printf("+-----+----------------------+-----------+----------+----------------------+----------------------+-------------+\n");

    if (pm->count == 0) {
        printf("| %-109s |\n", "  [No processes currently registered in the system]");
    } else {
        for (int i = 0; i < pm->count; i++) {
            const Process *proc = pm->processes[i];
            if (!proc) continue;

            char held_str[256] = "";
            char req_str[256] = "";

            if (rm && rm->count > 0) {
                for (int r = 0; r < rm->count; r++) {
                    int alloc_val = (r < proc->res_capacity) ? proc->allocated[r] : 0;
                    int req_val = (r < proc->res_capacity) ? proc->requested[r] : 0;

                    if (alloc_val > 0) {
                        char entry[MAX_NAME_LEN + 32];
                        snprintf(entry, sizeof(entry), "%s:%d ", rm->resources[r].name, alloc_val);
                        strncat(held_str, entry, sizeof(held_str) - strlen(held_str) - 1);
                    }
                    if (req_val > 0) {
                        char entry[MAX_NAME_LEN + 32];
                        snprintf(entry, sizeof(entry), "%s:%d ", rm->resources[r].name, req_val);
                        strncat(req_str, entry, sizeof(req_str) - strlen(req_str) - 1);
                    }
                }
            }

            if (held_str[0] == '\0') strncpy(held_str, "None", sizeof(held_str) - 1);
            if (req_str[0] == '\0') strncpy(req_str, "None", sizeof(req_str) - 1);

            char wait_str[64];
            double wait_ms = process_get_waiting_duration_ms(proc);
            if (proc->state == PROCESS_STATE_BLOCKED) {
                if (wait_ms >= 1000.0) {
                    snprintf(wait_str, sizeof(wait_str), "%.2f s", wait_ms / 1000.0);
                } else {
                    snprintf(wait_str, sizeof(wait_str), "%.1f ms", wait_ms);
                }
            } else {
                snprintf(wait_str, sizeof(wait_str), "0.0 ms");
            }

            printf("| %-3d | %-20s | %-9s | %-8d | %-20s | %-20s | %-11s |\n",
                   proc->id, proc->name, process_state_to_string(proc->state),
                   proc->priority, held_str, req_str, wait_str);
        }
    }

    printf("+-----+----------------------+-----------+----------+----------------------+----------------------+-------------+\n");
    printf("Total Processes: %d | Ready: %d | Running: %d | Blocked: %d | Completed: %d | Aborted: %d\n",
           pm->count, ready_cnt, running_cnt, blocked_cnt, completed_cnt, aborted_cnt);
    printf("=================================================================================================================\n");
}

void monitor_show_resource_dashboard(const ResourceManager *rm, const ProcessManager *pm) {
    if (!rm) return;

    printf("======================================================================================================================\n");
    printf("                                         RESOURCE UTILIZATION DASHBOARD                                               \n");
    printf("======================================================================================================================\n");
    printf("+-----+---------------+-------+-----------+-----------+----------+----------------------+-----------------------------+\n");
    printf("| ID  | Resource Name | Total | Allocated | Available | Util %%   | Gauge                | Current Holders             |\n");
    printf("+-----+---------------+-------+-----------+-----------+----------+----------------------+-----------------------------+\n");

    int sys_total = 0, sys_allocated = 0, sys_available = 0;
    bool all_invariants_pass = true;

    if (rm->count == 0) {
        printf("| %-114s |\n", "  [No resources currently registered in the system]");
    } else {
        for (int r = 0; r < rm->count; r++) {
            const Resource *res = &rm->resources[r];
            int total = res->total_instances;
            int avail = res->available_instances;
            int allocated = total - avail;
            double util_pct = (total > 0) ? (((double)allocated / (double)total) * 100.0) : 0.0;

            sys_total += total;
            sys_allocated += allocated;
            sys_available += avail;

            char gauge[32];
            int filled = (int)((util_pct / 100.0) * 16.0 + 0.5);
            if (filled < 0) filled = 0;
            if (filled > 16) filled = 16;
            gauge[0] = '[';
            for (int k = 0; k < 16; k++) {
                gauge[k + 1] = (k < filled) ? '=' : ' ';
            }
            gauge[17] = ']';
            gauge[18] = '\0';

            char holders[256] = "";
            int proc_sum_alloc = 0;
            if (pm) {
                for (int p = 0; p < pm->count; p++) {
                    const Process *proc = pm->processes[p];
                    if (proc && r < proc->res_capacity && proc->allocated[r] > 0) {
                        proc_sum_alloc += proc->allocated[r];
                        char entry[MAX_NAME_LEN + 32];
                        snprintf(entry, sizeof(entry), "%s:%d ", proc->name, proc->allocated[r]);
                        strncat(holders, entry, sizeof(holders) - strlen(holders) - 1);
                    }
                }
            }
            if (holders[0] == '\0') strncpy(holders, "None", sizeof(holders) - 1);

            if (avail + proc_sum_alloc != total || allocated != proc_sum_alloc) {
                all_invariants_pass = false;
            }

            printf("| %-3d | %-13s | %-5d | %-9d | %-9d | %6.1f%%  | %-20s | %-27s |\n",
                   res->id, res->name, total, allocated, avail, util_pct, gauge, holders);
        }
    }

    printf("+-----+---------------+-------+-----------+-----------+----------+----------------------+-----------------------------+\n");

    double sys_util_pct = (sys_total > 0) ? (((double)sys_allocated / (double)sys_total) * 100.0) : 0.0;
    printf("System Capacity: %d units | Total Allocated: %d units | Total Available: %d units | Overall Utilization: %5.1f%%\n",
           sys_total, sys_allocated, sys_available, sys_util_pct);

    printf("[RESOURCE ACCOUNTING INVARIANT AUDIT]\n");
    if (rm->count > 0 && pm) {
        for (int r = 0; r < rm->count; r++) {
            const Resource *res = &rm->resources[r];
            int proc_sum_alloc = 0;
            for (int p = 0; p < pm->count; p++) {
                const Process *proc = pm->processes[p];
                if (proc && r < proc->res_capacity) proc_sum_alloc += proc->allocated[r];
            }
            bool ok = (res->available_instances + proc_sum_alloc == res->total_instances);
            printf("  * %-4s: Total(%d) == Available(%d) + Allocated(%d) -> %s\n",
                   res->name, res->total_instances, res->available_instances, proc_sum_alloc,
                   ok ? "INVARIANT VALIDATED [OK]" : "INVARIANT VIOLATION [ERROR]");
        }
    }

    if (all_invariants_pass) {
        printf("  >>> ALL RESOURCE ACCOUNTING INVARIANTS SATISFIED (Zero Leak / Zero Over-Allocation) <<<\n");
    } else {
        printf("  >>> WARNING: RESOURCE ACCOUNTING INVARIANT DISCREPANCY DETECTED! <<<\n");
    }
    printf("======================================================================================================================\n");
}

void monitor_show_combined(const SystemManager *sm) {
    if (!sm) return;
    monitor_show_process_table(&sm->pm, &sm->rm);
    printf("\n");
    monitor_show_resource_dashboard(&sm->rm, &sm->pm);
}

void monitor_run_live(const SystemManager *sm, int interval_ms, int max_ticks) {
    if (!sm) return;
    if (interval_ms <= 0) interval_ms = 1000;

    int tick = 0;
    bool is_term = isatty(STDIN_FILENO);

    while (sm->is_running) {
        printf("\033[H\033[J");
        printf("[LIVE AUTO-REFRESH MONITOR - Press 'q' or Ctrl+C to return to CLI]\n\n");
        monitor_show_combined(sm);

        tick++;
        if (max_ticks > 0 && tick >= max_ticks) {
            break;
        }

        if (is_term) {
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(STDIN_FILENO, &fds);
            struct timeval tv;
            tv.tv_sec = interval_ms / 1000;
            tv.tv_usec = (interval_ms % 1000) * 1000;

            int ret = select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
            if (ret > 0 && FD_ISSET(STDIN_FILENO, &fds)) {
                char ch = 0;
                if (read(STDIN_FILENO, &ch, 1) > 0) {
                    if (ch == 'q' || ch == 'Q') {
                        printf("\n[Exiting Live Monitor...]\n");
                        break;
                    }
                }
            }
        } else {
            usleep(interval_ms * 1000);
        }
    }
}
