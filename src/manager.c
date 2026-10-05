#include <sys/wait.h>
#include "manager.h"
#include "scenario.h"
#include "logger.h"
#include "recovery.h"
#include "process_demo.h"
#include "ipc.h"

void system_manager_init(SystemManager *sm) {
    if (!sm) return;
    process_manager_init(&sm->pm);
    resource_manager_init(&sm->rm);
    sm->is_running = true;
    sm->banker_avoidance_enabled = false; /* Detection mode by default; enable via banker-mode on */
    logger_init(DEFAULT_LOG_FILE);
    logger_log(LOG_INFO, "DeadlockDoctor system manager initialized.");
}

void system_manager_cleanup(SystemManager *sm) {
    if (!sm) return;
    logger_log(LOG_INFO, "DeadlockDoctor system manager shutting down.");
    logger_close();
    process_manager_cleanup(&sm->pm);
    resource_manager_cleanup(&sm->rm);
    sm->is_running = false;
}

Process *system_manager_create_process(SystemManager *sm, int id, const char *name) {
    if (!sm) return NULL;
    Process *p = process_create(&sm->pm, id, name);
    if (p) {
        if (sm->rm.count > 0) {
            process_ensure_resource_capacity(p, sm->rm.count);
        }
        logger_log(LOG_PROCESS_CREATE, "Process '%s' (PID %d) created (State: %s).",
                   p->name, p->id, process_state_to_string(p->state));
    }
    return p;
}

Resource *system_manager_create_resource(SystemManager *sm, int id, const char *name, int total_instances) {
    if (!sm) return NULL;
    Resource *r = resource_create(&sm->rm, id, name, total_instances);
    if (r) {
        for (int i = 0; i < sm->pm.count; i++) {
            if (sm->pm.processes[i]) {
                process_ensure_resource_capacity(sm->pm.processes[i], sm->rm.count);
            }
        }
        logger_log(LOG_RESOURCE_CREATE, "Resource '%s' (ID %d) created with %d total instances.",
                   r->name, r->id, r->total_instances);
    }
    return r;
}

bool system_manager_terminate_process(SystemManager *sm, int id) {
    if (!sm) return false;
    Process *proc = process_find_by_id(&sm->pm, id);
    if (!proc) {
        fprintf(stderr, "[Manager Error] Process %d not found.\n", id);
        return false;
    }

    /* Release any allocated resources back to resource manager */
    for (int r = 0; r < sm->rm.count; r++) {
        if (r < proc->res_capacity && proc->allocated[r] > 0) {
            sm->rm.resources[r].available_instances += proc->allocated[r];
            proc->allocated[r] = 0;
        }
        if (r < proc->res_capacity) {
            proc->requested[r] = 0;
        }
    }

    logger_log(LOG_PROCESS_TERM, "Process '%s' (PID %d) terminated. All held resources released.",
               proc->name, proc->id);

    bool ok = process_terminate(&sm->pm, id);
    if (ok) {
        system_manager_check_pending_requests(sm);
    }
    return ok;
}

AllocResult system_manager_request(SystemManager *sm, const char *proc_ident, const char *res_ident, int units) {
    if (!sm || !proc_ident || !res_ident) {
        printf("[INVALID REQUEST] Invalid arguments.\n");
        return ALLOC_INVALID;
    }

    Process *proc = process_find_by_id_or_name(&sm->pm, proc_ident);
    if (!proc) {
        printf("[INVALID REQUEST] Process '%s' not found.\n", proc_ident);
        return ALLOC_INVALID;
    }

    if (proc->state == PROCESS_STATE_COMPLETED || proc->state == PROCESS_STATE_ABORTED) {
        printf("[INVALID REQUEST] Process '%s' (PID %d) is terminated (%s).\n",
               proc->name, proc->id, process_state_to_string(proc->state));
        return ALLOC_INVALID;
    }

    Resource *res = resource_find_by_id_or_name(&sm->rm, res_ident);
    if (!res) {
        printf("[INVALID REQUEST] Resource '%s' not found.\n", res_ident);
        return ALLOC_INVALID;
    }

    int res_idx = resource_get_index_by_id(&sm->rm, res->id);
    if (res_idx < 0) {
        printf("[INVALID REQUEST] Internal resource index error.\n");
        return ALLOC_INVALID;
    }

    process_ensure_resource_capacity(proc, sm->rm.count);

    if (units <= 0) {
        printf("[INVALID REQUEST] Requested units must be positive (got %d).\n", units);
        return ALLOC_INVALID;
    }

    logger_log(LOG_RESOURCE_REQUEST, "Process '%s' (PID %d) requested %d units of resource '%s'.",
               proc->name, proc->id, units, res->name);

    /* 1. Check whether requested amount is <= Need */
    if (proc->max_demand[res_idx] > 0) {
        int need = proc->max_demand[res_idx] - proc->allocated[res_idx];
        if (units > need) {
            printf("[INVALID REQUEST] Process '%s' requested %d units of '%s', exceeding remaining Need (%d) [Max=%d, Alloc=%d].\n",
                   proc->name, units, res->name, need, proc->max_demand[res_idx], proc->allocated[res_idx]);
            return ALLOC_INVALID;
        }
    } else {
        /* If Max is not explicitly set, request cannot exceed total system capacity */
        if (units + proc->allocated[res_idx] > res->total_instances) {
            printf("[INVALID REQUEST] Process '%s' requested %d units of '%s', exceeding total capacity (%d).\n",
                   proc->name, units, res->name, res->total_instances);
            return ALLOC_INVALID;
        }
    }

    /* 2. Check whether requested amount is <= Available */
    if (units > res->available_instances) {
        proc->requested[res_idx] = units;
        proc->state = PROCESS_STATE_BLOCKED;
        printf("[BLOCKED] Process '%s' (PID %d) requested %d units of '%s', but only %d available. Process marked BLOCKED.\n",
               proc->name, proc->id, units, res->name, res->available_instances);
        logger_log(LOG_RESOURCE_BLOCK, "Process '%s' (PID %d) request for %d units of '%s' BLOCKED (Insufficient available: %d).",
                   proc->name, proc->id, units, res->name, res->available_instances);
        return ALLOC_BLOCKED;
    }

    /* 3. Check safety if Banker avoidance mode is active */
    if (sm->banker_avoidance_enabled) {
        BankerSystem bs;
        banker_system_init(&bs);
        banker_init_from_manager(&bs, &sm->pm, &sm->rm);

        int p_idx = -1;
        for (int i = 0; i < bs.num_processes; i++) {
            if (bs.process_ids[i] == proc->id) {
                p_idx = i;
                break;
            }
        }

        if (p_idx >= 0) {
            int *req = (int *)calloc(bs.num_resources, sizeof(int));
            if (res_idx < bs.num_resources) req[res_idx] = units;
            int *safe_seq = (int *)malloc(bs.num_processes * sizeof(int));
            int seq_len = 0;

            bool is_safe = banker_check_request_safety(&bs, p_idx, req, NULL, safe_seq, &seq_len);

            free(req);
            free(safe_seq);
            banker_system_cleanup(&bs);

            if (!is_safe) {
                /* Resulting state is UNSAFE: block request and restore previous state */
                proc->requested[res_idx] = units;
                proc->state = PROCESS_STATE_BLOCKED;
                printf("SYSTEM STATE: UNSAFE\n");
                printf("Request BLOCKED\n");
                logger_log(LOG_RESOURCE_BLOCK, "Process '%s' (PID %d) request for %d units of '%s' BLOCKED by Banker avoidance (SYSTEM STATE: UNSAFE).",
                           proc->name, proc->id, units, res->name);
                return ALLOC_BLOCKED;
            }

            /* Resulting state is SAFE: grant request */
            res->available_instances -= units;
            proc->allocated[res_idx] += units;
            proc->requested[res_idx] = 0;

            bool still_blocked = false;
            for (int r = 0; r < sm->rm.count; r++) {
                if (r < proc->res_capacity && proc->requested[r] > 0) {
                    still_blocked = true;
                    break;
                }
            }
            if (!still_blocked) {
                proc->state = PROCESS_STATE_RUNNING;
            }

            printf("[GRANTED] Process '%s' (PID %d) allocated %d units of '%s'. (Allocated: %d, Available: %d)\n",
                   proc->name, proc->id, units, res->name, proc->allocated[res_idx], res->available_instances);
            logger_log(LOG_RESOURCE_GRANT, "Process '%s' (PID %d) GRANTED %d units of '%s' (Safe allocation). (Allocated: %d, Available: %d).",
                       proc->name, proc->id, units, res->name, proc->allocated[res_idx], res->available_instances);

            system_manager_check_pending_requests(sm);
            return ALLOC_GRANTED;
        } else {
            banker_system_cleanup(&bs);
        }
    }

    /* Direct allocation when Banker avoidance is disabled (Detection mode) */
    res->available_instances -= units;
    proc->allocated[res_idx] += units;
    proc->requested[res_idx] = 0;
    proc->state = PROCESS_STATE_RUNNING;
    printf("[GRANTED] Process '%s' (PID %d) allocated %d units of '%s'. (Allocated: %d, Available: %d)\n",
           proc->name, proc->id, units, res->name, proc->allocated[res_idx], res->available_instances);
    logger_log(LOG_RESOURCE_GRANT, "Process '%s' (PID %d) GRANTED %d units of '%s' (Allocated: %d, Available: %d).",
               proc->name, proc->id, units, res->name, proc->allocated[res_idx], res->available_instances);
    return ALLOC_GRANTED;
}

AllocResult system_manager_release(SystemManager *sm, const char *proc_ident, const char *res_ident, int units) {
    if (!sm || !proc_ident || !res_ident) {
        printf("[INVALID REQUEST] Invalid arguments.\n");
        return ALLOC_INVALID;
    }

    Process *proc = process_find_by_id_or_name(&sm->pm, proc_ident);
    if (!proc) {
        printf("[INVALID REQUEST] Process '%s' not found.\n", proc_ident);
        return ALLOC_INVALID;
    }

    Resource *res = resource_find_by_id_or_name(&sm->rm, res_ident);
    if (!res) {
        printf("[INVALID REQUEST] Resource '%s' not found.\n", res_ident);
        return ALLOC_INVALID;
    }

    int res_idx = resource_get_index_by_id(&sm->rm, res->id);
    if (res_idx < 0) {
        printf("[INVALID REQUEST] Internal resource index error.\n");
        return ALLOC_INVALID;
    }

    process_ensure_resource_capacity(proc, sm->rm.count);

    if (units <= 0) {
        printf("[INVALID REQUEST] Released units must be positive (got %d).\n", units);
        return ALLOC_INVALID;
    }

    /* Check if releasing more than currently held */
    if (units > proc->allocated[res_idx]) {
        printf("[INVALID REQUEST] Process '%s' holds %d units of '%s', cannot release %d units.\n",
               proc->name, proc->allocated[res_idx], res->name, units);
        return ALLOC_INVALID;
    }

    proc->allocated[res_idx] -= units;
    res->available_instances += units;

    printf("[RELEASED] Process '%s' (PID %d) released %d units of '%s'. (Allocated: %d, Available: %d)\n",
           proc->name, proc->id, units, res->name, proc->allocated[res_idx], res->available_instances);
    logger_log(LOG_RESOURCE_RELEASE, "Process '%s' (PID %d) RELEASED %d units of resource '%s' (Remaining: %d, Available: %d).",
               proc->name, proc->id, units, res->name, proc->allocated[res_idx], res->available_instances);

    /* Check if pending requests can now be satisfied */
    system_manager_check_pending_requests(sm);

    return ALLOC_RELEASED;
}

bool system_manager_set_max(SystemManager *sm, const char *proc_ident, const char *res_ident, int max_units) {
    if (!sm || !proc_ident || !res_ident) {
        printf("[INVALID REQUEST] Invalid arguments.\n");
        return false;
    }

    Process *proc = process_find_by_id_or_name(&sm->pm, proc_ident);
    if (!proc) {
        printf("[INVALID REQUEST] Process '%s' not found.\n", proc_ident);
        return false;
    }

    Resource *res = resource_find_by_id_or_name(&sm->rm, res_ident);
    if (!res) {
        printf("[INVALID REQUEST] Resource '%s' not found.\n", res_ident);
        return false;
    }

    int res_idx = resource_get_index_by_id(&sm->rm, res->id);
    if (res_idx < 0) return false;

    process_ensure_resource_capacity(proc, sm->rm.count);

    if (max_units < 0) {
        printf("[INVALID REQUEST] Max demand cannot be negative.\n");
        return false;
    }

    if (max_units > res->total_instances) {
        printf("[INVALID REQUEST] Max demand (%d) exceeds total resource capacity (%d).\n",
               max_units, res->total_instances);
        return false;
    }

    if (max_units < proc->allocated[res_idx]) {
        printf("[INVALID REQUEST] Max demand (%d) cannot be less than currently allocated units (%d).\n",
               max_units, proc->allocated[res_idx]);
        return false;
    }

    proc->max_demand[res_idx] = max_units;
    int need = max_units - proc->allocated[res_idx];
    printf("[OK] Process '%s' Max demand for '%s' set to %d. (Allocation: %d, Need: %d)\n",
           proc->name, res->name, max_units, proc->allocated[res_idx], need);
    return true;
}

void system_manager_check_pending_requests(SystemManager *sm) {
    if (!sm) return;

    bool progress = true;
    while (progress) {
        progress = false;
        for (int i = 0; i < sm->pm.count; i++) {
            Process *proc = sm->pm.processes[i];
            if (!proc || proc->state != PROCESS_STATE_BLOCKED) continue;

            bool can_satisfy_all = true;
            bool has_pending = false;

            for (int r = 0; r < sm->rm.count; r++) {
                int req_val = (r < proc->res_capacity) ? proc->requested[r] : 0;
                if (req_val > 0) {
                    has_pending = true;
                    if (sm->rm.resources[r].available_instances < req_val) {
                        can_satisfy_all = false;
                        break;
                    }
                }
            }

            if (has_pending && can_satisfy_all) {
                if (sm->banker_avoidance_enabled) {
                    BankerSystem bs;
                    banker_system_init(&bs);
                    banker_init_from_manager(&bs, &sm->pm, &sm->rm);
                    int p_idx = -1;
                    for (int p = 0; p < bs.num_processes; p++) {
                        if (bs.process_ids[p] == proc->id) {
                            p_idx = p;
                            break;
                        }
                    }

                    if (p_idx >= 0) {
                        int *safe_seq = (int *)malloc(bs.num_processes * sizeof(int));
                        int seq_len = 0;
                        bool safe = banker_check_request_safety(&bs, p_idx, proc->requested, NULL, safe_seq, &seq_len);
                        free(safe_seq);
                        banker_system_cleanup(&bs);

                        if (!safe) {
                            continue;
                        }
                    } else {
                        banker_system_cleanup(&bs);
                        continue;
                    }
                }

                for (int r = 0; r < sm->rm.count; r++) {
                    int req_val = (r < proc->res_capacity) ? proc->requested[r] : 0;
                    if (req_val > 0) {
                        sm->rm.resources[r].available_instances -= req_val;
                        proc->allocated[r] += req_val;
                        proc->requested[r] = 0;
                        printf("[GRANTED] Pending request fulfilled: Process '%s' allocated %d units of '%s'. (Available: %d)\n",
                               proc->name, req_val, sm->rm.resources[r].name, sm->rm.resources[r].available_instances);
                        logger_log(LOG_RESOURCE_GRANT, "Pending request fulfilled: Process '%s' (PID %d) GRANTED %d units of '%s' (Available: %d).",
                                   proc->name, proc->id, req_val, sm->rm.resources[r].name, sm->rm.resources[r].available_instances);
                    }
                }
                proc->state = PROCESS_STATE_RUNNING;
                progress = true;
                break;
            }
        }
    }
}

void system_manager_run_detector(const SystemManager *sm) {
    if (!sm) return;
    DeadlockReport report;
    deadlock_report_init(&report);
    detector_run(&sm->pm, &sm->rm, &report);
    detector_print_report(&report);

    if (report.deadlock_detected) {
        char cycle_str[256] = "";
        for (int i = 0; i < report.cycle_len; i++) {
            int node_idx = report.cycle[i];
            strncat(cycle_str, report.wfg.node_names[node_idx], sizeof(cycle_str) - strlen(cycle_str) - 1);
            if (i < report.cycle_len - 1) {
                strncat(cycle_str, " -> ", sizeof(cycle_str) - strlen(cycle_str) - 1);
            }
        }
        char set_str[256] = "";
        for (int i = 0; i < report.deadlocked_count; i++) {
            strncat(set_str, report.deadlocked_names[i], sizeof(set_str) - strlen(set_str) - 1);
            if (i < report.deadlocked_count - 1) {
                strncat(set_str, " ", sizeof(set_str) - strlen(set_str) - 1);
            }
        }
        logger_log(LOG_DEADLOCK_DETECT, "Deadlock detected! Cycle: %s. Deadlocked set: %s.",
                   cycle_str, set_str);
    } else {
        logger_log(LOG_DEADLOCK_DETECT, "Deadlock check completed: NO DEADLOCK DETECTED.");
    }
    deadlock_report_cleanup(&report);
}

void system_manager_show_graph(const SystemManager *sm) {
    if (!sm) return;
    graph_visualize(&sm->pm, &sm->rm);
}

void system_manager_show_status(const SystemManager *sm) {
    if (!sm) return;

    int ready_cnt = 0, running_cnt = 0, blocked_cnt = 0, completed_cnt = 0, aborted_cnt = 0;
    for (int i = 0; i < sm->pm.count; i++) {
        if (!sm->pm.processes[i]) continue;
        switch (sm->pm.processes[i]->state) {
            case PROCESS_STATE_READY:     ready_cnt++; break;
            case PROCESS_STATE_RUNNING:   running_cnt++; break;
            case PROCESS_STATE_BLOCKED:   blocked_cnt++; break;
            case PROCESS_STATE_COMPLETED: completed_cnt++; break;
            case PROCESS_STATE_ABORTED:   aborted_cnt++; break;
        }
    }

    int total_inst = 0, avail_inst = 0;
    for (int i = 0; i < sm->rm.count; i++) {
        total_inst += sm->rm.resources[i].total_instances;
        avail_inst += sm->rm.resources[i].available_instances;
    }

    printf("\n================================ SYSTEM STATUS REPORT ================================\n");
    printf(" Processes : %d total | %d Ready, %d Running, %d Blocked, %d Completed, %d Aborted\n",
           sm->pm.count, ready_cnt, running_cnt, blocked_cnt, completed_cnt, aborted_cnt);
    printf(" Resources : %d types | %d Total Instances | %d Available | %d Allocated\n",
           sm->rm.count, total_inst, avail_inst, total_inst - avail_inst);

    printf(" Available Vector: [ ");
    for (int r = 0; r < sm->rm.count; r++) {
        printf("%s: %d%s", sm->rm.resources[r].name, sm->rm.resources[r].available_instances,
               (r == sm->rm.count - 1) ? "" : ", ");
    }
    printf(" ]\n");
    printf("--------------------------------------------------------------------------------------\n");

    printf("[RESOURCE INVENTORY]\n");
    resource_list(&sm->rm);

    printf("\n[PROCESS MATRIX: ALLOCATION, MAX, NEED, REQUEST]\n");
    process_list(&sm->pm, &sm->rm);

    if (blocked_cnt > 0) {
        printf("\n[BLOCKED PROCESSES QUEUE]\n");
        for (int i = 0; i < sm->pm.count; i++) {
            const Process *p = sm->pm.processes[i];
            if (p && p->state == PROCESS_STATE_BLOCKED) {
                printf("  - Process '%s' (PID %d) waiting for: ", p->name, p->id);
                for (int r = 0; r < sm->rm.count; r++) {
                    int req_val = (r < p->res_capacity) ? p->requested[r] : 0;
                    if (req_val > 0) {
                        printf("%s:%d (available: %d) ", sm->rm.resources[r].name,
                               req_val, sm->rm.resources[r].available_instances);
                    }
                }
                printf("\n");
            }
        }
    }

    printf("======================================================================================\n\n");
}

void system_manager_run_process_demo(void) {
    process_demo_run();
}

void system_manager_run_ipc_demo(SystemManager *sm) {
    ipc_demo_run_all(sm);
}

void system_manager_run_thread_demo(void) {
    pid_t pid = fork();
    if (pid == 0) {
        char *args[] = {"build/thread_deadlock", NULL};
        execv("build/thread_deadlock", args);
        char *args2[] = {"./thread_deadlock", NULL};
        execv("./thread_deadlock", args2);
        perror("[Error] Failed to execute build/thread_deadlock");
        _exit(1);
    } else if (pid > 0) {
        int status = 0;
        waitpid(pid, &status, 0);
    } else {
        perror("fork");
    }
}

void system_manager_print_help(void) {
    printf("\n================================================================================\n");
    printf("                    DEADLOCKDOCTOR COMMAND REFERENCE (CLI)                      \n");
    printf("================================================================================\n\n");
    printf("  [SYSTEM STATUS & HELP]\n");
    printf("    help                               - Display this beginner-friendly command guide\n");
    printf("    status                             - Display system matrices (Available, Alloc, Max, Need)\n\n");

    printf("  [PROCESS MANAGEMENT]\n");
    printf("    process create <name> [id]         - Register a new process (e.g. process create P1)\n");
    printf("    process list                       - List all processes, states, and resource vectors\n");
    printf("    process kill <name|id>             - Terminate a process and reclaim all its held resources\n\n");

    printf("  [RESOURCE MANAGEMENT]\n");
    printf("    resource create <name> <units>     - Register multi-instance resource (e.g. resource create R1 3)\n");
    printf("    resource list                      - List all resources and instance counts\n\n");

    printf("  [RESOURCE CLAIMS & ALLOCATION]\n");
    printf("    max <process> <resource> <units>   - Declare process maximum resource demand\n");
    printf("    request <proc> <res> <units>       - Request resource instances for a process\n");
    printf("    release <proc> <res> <units>       - Release held resource instances back to system\n\n");

    printf("  [DEADLOCK ANALYSIS & RESOLUTION]\n");
    printf("    banker                             - Run Banker's Safety Algorithm to find safe sequence\n");
    printf("    banker-mode <on|off>               - Toggle avoidance mode (on: safe check, off: direct allocation)\n");
    printf("    detect                             - Run Deadlock Detector (Wait-For Graph DFS cycle search)\n");
    printf("    graph                              - Display Resource Allocation Graph & Wait-For Graph (ASCII)\n");
    printf("    recover                            - Run 10-step Deadlock Recovery with lowest-cost victim selection\n");
    printf("    priority <proc> <val>              - Set process priority for recovery cost evaluation\n\n");

    printf("  [SCENARIOS & LOGGING]\n");
    printf("    load <scenario-file>               - Load and replay scenario file (e.g. load scenarios/deadlock.txt)\n");
    printf("    log                                - View persistent POSIX event log (logs/deadlockdoctor.log)\n\n");

    printf("  [DEMONSTRATIONS (VIVA REFERENCE)]\n");
    printf("    demo [thread|fork|ipc]             - Run OS demonstrations (pthreads, fork/exec, IPC/signals)\n\n");

    printf("  [EXIT]\n");
    printf("    exit | quit                        - Safely exit DeadlockDoctor\n");
    printf("================================================================================\n\n");
}

void system_manager_run_cli(SystemManager *sm) {
    if (!sm) return;

    char line[MAX_CMD_LEN];
    int is_interactive = isatty(STDIN_FILENO);

    while (sm->is_running) {
        if (is_interactive) {
            printf("deadlockdoctor> ");
            fflush(stdout);
        }

        if (!fgets(line, sizeof(line), stdin)) {
            break; /* EOF */
        }

        /* Strip trailing newlines and spaces */
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r' || line[len - 1] == ' ')) {
            line[--len] = '\0';
        }

        /* Skip empty or comment lines */
        if (len == 0 || line[0] == '#') {
            continue;
        }

        char cmd[64];
        int scanned = sscanf(line, "%63s", cmd);
        if (scanned <= 0) continue;

        if (strcasecmp(cmd, "process") == 0 || strcasecmp(cmd, "proc") == 0) {
            char sub[64] = "";
            int sub_count = sscanf(line, "%*s %63s", sub);
            if (sub_count < 1) {
                printf("[Error] Usage: process create <name> [id] | process list | process kill <name|id>\n");
            } else if (strcasecmp(sub, "create") == 0 || strcasecmp(sub, "add") == 0) {
                char name[MAX_NAME_LEN];
                int custom_id = -1;
                int count = sscanf(line, "%*s %*s %63s %d", name, &custom_id);
                if (count < 1) {
                    printf("[Error] Usage: process create <name> [id]\n");
                } else {
                    int id = custom_id;
                    if (id <= 0) {
                        if ((name[0] == 'P' || name[0] == 'p') && isdigit((unsigned char)name[1])) {
                            id = atoi(&name[1]);
                        }
                    }
                    if (id <= 0 || process_find_by_id(&sm->pm, id) != NULL) {
                        id = 1;
                        while (process_find_by_id(&sm->pm, id) != NULL) {
                            id++;
                        }
                    }
                    Process *p = system_manager_create_process(sm, id, name);
                    if (p) {
                        printf("[OK] Process created: Name='%s', ID=%d, State=%s\n",
                               p->name, p->id, process_state_to_string(p->state));
                    } else {
                        printf("[Error] Failed to create process '%s'.\n", name);
                    }
                }
            } else if (strcasecmp(sub, "list") == 0 || strcasecmp(sub, "show") == 0 || strcasecmp(sub, "ls") == 0) {
                process_list(&sm->pm, &sm->rm);
            } else if (strcasecmp(sub, "kill") == 0 || strcasecmp(sub, "abort") == 0 ||
                       strcasecmp(sub, "term") == 0 || strcasecmp(sub, "remove") == 0) {
                char target[MAX_NAME_LEN];
                if (sscanf(line, "%*s %*s %63s", target) == 1) {
                    Process *p = process_find_by_id_or_name(&sm->pm, target);
                    if (p) {
                        int pid = p->id;
                        char pname[MAX_NAME_LEN];
                        strncpy(pname, p->name, sizeof(pname) - 1);
                        pname[sizeof(pname) - 1] = '\0';
                        system_manager_terminate_process(sm, pid);
                        printf("[OK] Process '%s' (PID %d) terminated and resources reclaimed.\n", pname, pid);
                    } else {
                        printf("[Error] Process '%s' not found.\n", target);
                    }
                } else {
                    printf("[Error] Usage: process kill <name|id>\n");
                }
            } else {
                printf("[Error] Unknown process subcommand '%s'. Valid: process create <name>, process list, process kill <name>\n", sub);
            }
        } else if (strcasecmp(cmd, "resource") == 0 || strcasecmp(cmd, "res") == 0) {
            char sub[64] = "";
            int sub_count = sscanf(line, "%*s %63s", sub);
            if (sub_count < 1) {
                printf("[Error] Usage: resource create <name> <instances> | resource list\n");
            } else if (strcasecmp(sub, "create") == 0 || strcasecmp(sub, "add") == 0) {
                char name[MAX_NAME_LEN];
                int instances = 1;
                int custom_id = -1;
                int count = sscanf(line, "%*s %*s %63s %d %d", name, &instances, &custom_id);
                if (count < 1) {
                    printf("[Error] Usage: resource create <name> <instances> [id]\n");
                } else {
                    if (count < 2 || instances <= 0) {
                        instances = 1;
                    }
                    int id = custom_id;
                    if (id <= 0) {
                        if ((name[0] == 'R' || name[0] == 'r') && isdigit((unsigned char)name[1])) {
                            id = atoi(&name[1]);
                        }
                    }
                    if (id <= 0 || resource_find_by_id(&sm->rm, id) != NULL) {
                        id = 1;
                        while (resource_find_by_id(&sm->rm, id) != NULL) {
                            id++;
                        }
                    }
                    Resource *r = system_manager_create_resource(sm, id, name, instances);
                    if (r) {
                        printf("[OK] Resource created: Name='%s', ID=%d, Total Instances=%d (Available: %d)\n",
                               r->name, r->id, r->total_instances, r->available_instances);
                    } else {
                        printf("[Error] Failed to create resource '%s'.\n", name);
                    }
                }
            } else if (strcasecmp(sub, "list") == 0 || strcasecmp(sub, "show") == 0 || strcasecmp(sub, "ls") == 0) {
                resource_list(&sm->rm);
            } else if (strcasecmp(sub, "remove") == 0 || strcasecmp(sub, "delete") == 0) {
                char target[MAX_NAME_LEN];
                if (sscanf(line, "%*s %*s %63s", target) == 1) {
                    Resource *r = resource_find_by_id_or_name(&sm->rm, target);
                    if (r) {
                        int rid = r->id;
                        resource_remove(&sm->rm, rid);
                        printf("[OK] Resource '%s' (ID %d) removed from system.\n", target, rid);
                    } else {
                        printf("[Error] Resource '%s' not found.\n", target);
                    }
                } else {
                    printf("[Error] Usage: resource remove <name|id>\n");
                }
            } else {
                printf("[Error] Unknown resource subcommand '%s'. Valid: resource create <name> <instances>, resource list\n", sub);
            }
        } else if (strcasecmp(cmd, "request") == 0 || strcasecmp(cmd, "req") == 0) {
            char proc_id[MAX_NAME_LEN], res_id[MAX_NAME_LEN];
            int units = 0;
            if (sscanf(line, "%*s %63s %63s %d", proc_id, res_id, &units) == 3) {
                system_manager_request(sm, proc_id, res_id, units);
            } else {
                printf("[Error] Usage: request <process> <resource> <amount>\n");
            }
        } else if (strcasecmp(cmd, "release") == 0 || strcasecmp(cmd, "rel") == 0) {
            char proc_id[MAX_NAME_LEN], res_id[MAX_NAME_LEN];
            int units = 0;
            if (sscanf(line, "%*s %63s %63s %d", proc_id, res_id, &units) == 3) {
                system_manager_release(sm, proc_id, res_id, units);
            } else {
                printf("[Error] Usage: release <process> <resource> <amount>\n");
            }
        } else if (strcasecmp(cmd, "max") == 0 || strcasecmp(cmd, "set-max") == 0) {
            char proc_id[MAX_NAME_LEN], res_id[MAX_NAME_LEN];
            int units = 0;
            if (sscanf(line, "%*s %63s %63s %d", proc_id, res_id, &units) == 3) {
                system_manager_set_max(sm, proc_id, res_id, units);
            } else {
                printf("[Error] Usage: max <process> <resource> <amount>\n");
            }
        } else if (strcasecmp(cmd, "banker") == 0 || strcasecmp(cmd, "safety") == 0) {
            BankerSystem bs;
            banker_system_init(&bs);
            banker_init_from_manager(&bs, &sm->pm, &sm->rm);
            banker_print_safety_report(&bs);
            banker_system_cleanup(&bs);
        } else if (strcasecmp(cmd, "banker-mode") == 0 || strcasecmp(cmd, "mode") == 0) {
            char opt[32] = "";
            if (sscanf(line, "%*s %31s", opt) == 1) {
                if (strcasecmp(opt, "on") == 0 || strcasecmp(opt, "avoidance") == 0) {
                    sm->banker_avoidance_enabled = true;
                    printf("[OK] Banker avoidance mode ENABLED (unsafe requests blocked).\n");
                } else if (strcasecmp(opt, "off") == 0 || strcasecmp(opt, "detection") == 0) {
                    sm->banker_avoidance_enabled = false;
                    printf("[OK] Banker avoidance mode DISABLED (Detection mode: direct allocation).\n");
                } else {
                    printf("[Error] Usage: banker-mode <on|off>\n");
                }
            } else {
                printf("Banker avoidance mode is currently %s.\n", sm->banker_avoidance_enabled ? "ON" : "OFF");
            }
        } else if (strcasecmp(cmd, "detect") == 0 || strcasecmp(cmd, "deadlock") == 0) {
            system_manager_run_detector(sm);
        } else if (strcasecmp(cmd, "graph") == 0 || strcasecmp(cmd, "rag") == 0 || strcasecmp(cmd, "wfg") == 0) {
            system_manager_show_graph(sm);
        } else if (strcasecmp(cmd, "recover") == 0) {
            int pid = -1;
            if (sscanf(line, "%*s %d", &pid) == 1) {
                recovery_abort_process(sm, pid);
            } else {
                recovery_run(sm);
            }
        } else if (strcasecmp(cmd, "abort") == 0) {
            int pid = -1;
            if (sscanf(line, "%*s %d", &pid) == 1) {
                recovery_abort_process(sm, pid);
            } else {
                printf("[Error] Usage: abort <process_id>\n");
            }
        } else if (strcasecmp(cmd, "load") == 0 || strcasecmp(cmd, "replay") == 0) {
            char filepath[MAX_CMD_LEN];
            if (sscanf(line, "%*s %255s", filepath) == 1) {
                scenario_replay_file(sm, filepath);
            } else {
                printf("[Error] Usage: load <scenario-file>\n");
            }
        } else if (strcasecmp(cmd, "log") == 0 || strcasecmp(cmd, "show-log") == 0) {
            logger_print_log();
        } else if (strcasecmp(cmd, "status") == 0) {
            system_manager_show_status(sm);
        } else if (strcasecmp(cmd, "demo") == 0) {
            char demo_type[64] = "";
            if (sscanf(line, "%*s %63s", demo_type) == 1) {
                if (strcasecmp(demo_type, "thread") == 0 || strcasecmp(demo_type, "threads") == 0) {
                    system_manager_run_thread_demo();
                } else if (strcasecmp(demo_type, "fork") == 0 || strcasecmp(demo_type, "process") == 0) {
                    system_manager_run_process_demo();
                } else if (strcasecmp(demo_type, "ipc") == 0) {
                    system_manager_run_ipc_demo(sm);
                } else {
                    printf("[Error] Unknown demo '%s'. Available demos: demo thread, demo fork, demo ipc\n", demo_type);
                }
            } else {
                printf("\n[Running default POSIX threads deadlock demonstration]\n");
                system_manager_run_thread_demo();
                printf("[Demo Options available: demo thread, demo fork, demo ipc]\n\n");
            }
        } else if (strcasecmp(cmd, "priority") == 0 || strcasecmp(cmd, "set-priority") == 0) {
            char pname[MAX_NAME_LEN];
            int prio = 1;
            if (sscanf(line, "%*s %63s %d", pname, &prio) == 2) {
                Process *p = process_find_by_id_or_name(&sm->pm, pname);
                if (p) {
                    process_set_priority(p, prio);
                    printf("[OK] Process '%s' (PID %d) priority set to %d.\n", p->name, p->id, prio);
                } else {
                    printf("[Error] Process '%s' not found.\n", pname);
                }
            } else {
                printf("[Error] Usage: priority <proc> <val>\n");
            }
        } else if (strcasecmp(cmd, "add-proc") == 0 || strcasecmp(cmd, "create-proc") == 0) {
            int pid = -1;
            char name[MAX_NAME_LEN];
            if (sscanf(line, "%*s %d %63s", &pid, name) == 2) {
                Process *p = system_manager_create_process(sm, pid, name);
                if (p) {
                    printf("[OK] Process created: ID=%d, Name='%s', State=%s\n",
                           p->id, p->name, process_state_to_string(p->state));
                }
            } else {
                printf("[Error] Usage: add-proc <id> <name>\n");
            }
        } else if (strcasecmp(cmd, "list-proc") == 0 || strcasecmp(cmd, "procs") == 0) {
            process_list(&sm->pm, &sm->rm);
        } else if (strcasecmp(cmd, "term-proc") == 0) {
            int pid = -1;
            if (sscanf(line, "%*s %d", &pid) == 1) {
                if (system_manager_terminate_process(sm, pid)) {
                    printf("[OK] Process %d terminated.\n", pid);
                }
            } else {
                printf("[Error] Usage: term-proc <id>\n");
            }
        } else if (strcasecmp(cmd, "remove-proc") == 0 || strcasecmp(cmd, "del-proc") == 0) {
            int pid = -1;
            if (sscanf(line, "%*s %d", &pid) == 1) {
                if (process_remove(&sm->pm, pid)) {
                    printf("[OK] Process %d removed from system.\n", pid);
                } else {
                    printf("[Error] Process %d not found.\n", pid);
                }
            } else {
                printf("[Error] Usage: remove-proc <id>\n");
            }
        } else if (strcasecmp(cmd, "add-res") == 0 || strcasecmp(cmd, "create-res") == 0) {
            int rid = -1, total = 0;
            char name[MAX_NAME_LEN];
            if (sscanf(line, "%*s %d %63s %d", &rid, name, &total) == 3) {
                Resource *r = system_manager_create_resource(sm, rid, name, total);
                if (r) {
                    printf("[OK] Resource created: ID=%d, Name='%s', Total Instances=%d\n",
                           r->id, r->name, r->total_instances);
                }
            } else {
                printf("[Error] Usage: add-res <id> <name> <instances>\n");
            }
        } else if (strcasecmp(cmd, "list-res") == 0 || strcasecmp(cmd, "resources") == 0) {
            resource_list(&sm->rm);
        } else if (strcasecmp(cmd, "remove-res") == 0 || strcasecmp(cmd, "del-res") == 0) {
            int rid = -1;
            if (sscanf(line, "%*s %d", &rid) == 1) {
                if (resource_remove(&sm->rm, rid)) {
                    printf("[OK] Resource %d removed from system.\n", rid);
                } else {
                    printf("[Error] Resource %d not found.\n", rid);
                }
            } else {
                printf("[Error] Usage: remove-res <id>\n");
            }
        } else if (strcasecmp(cmd, "fork-demo") == 0 || strcasecmp(cmd, "process-demo") == 0) {
            system_manager_run_process_demo();
        } else if (strcasecmp(cmd, "ipc") == 0 || strcasecmp(cmd, "ipc-demo") == 0) {
            system_manager_run_ipc_demo(sm);
        } else if (strcasecmp(cmd, "recover-auto") == 0) {
            recovery_auto_recover(sm);
        } else if (strcasecmp(cmd, "help") == 0 || strcmp(cmd, "?") == 0) {
            system_manager_print_help();
        } else if (strcasecmp(cmd, "exit") == 0 || strcasecmp(cmd, "quit") == 0) {
            sm->is_running = false;
            printf("Exiting DeadlockDoctor. Goodbye!\n");
        } else {
            printf("[Error] Unknown command: '%s'. Type 'help' for available commands.\n", cmd);
        }
    }
}
