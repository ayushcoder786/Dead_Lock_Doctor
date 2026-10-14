#include "manager.h"
#include "monitor.h"
#include "timeline.h"
#include "trace.h"
#include "timeout.h"
#include "scenario.h"
#include "logger.h"
#include "recovery.h"
#include "process_demo.h"
#include "ipc.h"
#include "benchmark.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <limits.h>
#include <readline/readline.h>
#include <readline/history.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define MAX_SHELL_ARGS 128

static int s_last_exit_status = 0;
static char s_old_pwd[PATH_MAX] = "";
static volatile sig_atomic_t s_shell_sigint = 0;

static void shell_sigint_handler(int sig) {
    (void)sig;
    s_shell_sigint = 1;
}

static int parse_command_line(const char *input, char *token_buf, size_t buf_size, char *args[], int max_args) {
    if (!input || !token_buf || buf_size == 0 || !args || max_args <= 1) {
        if (args && max_args > 0) args[0] = NULL;
        return 0;
    }

    int argc = 0;
    size_t out_idx = 0;
    const char *p = input;

    while (*p && argc < max_args - 1) {
        while (*p && isspace((unsigned char)*p)) {
            p++;
        }
        if (!*p) break;

        args[argc++] = &token_buf[out_idx];

        while (*p && !isspace((unsigned char)*p)) {
            if (*p == 34 || *p == 39) {
                char quote = *p++;
                while (*p && *p != quote) {
                    if (*p == 92 && *(p + 1) == quote) {
                        p++;
                    }
                    if (out_idx + 1 < buf_size) {
                        token_buf[out_idx++] = *p;
                    }
                    p++;
                }
                if (*p == quote) {
                    p++;
                }
            } else if (*p == 92 && *(p + 1) != 0) {
                p++;
                if (out_idx + 1 < buf_size) {
                    token_buf[out_idx++] = *p;
                }
                p++;
            } else {
                if (out_idx + 1 < buf_size) {
                    token_buf[out_idx++] = *p;
                }
                p++;
            }
        }

        if (out_idx < buf_size) {
            token_buf[out_idx++] = 0;
        } else {
            token_buf[buf_size - 1] = 0;
            break;
        }
    }

    args[argc] = NULL;
    return argc;
}

static void expand_exit_status(char *args[], int argc, char *exit_buf, size_t exit_buf_size, int exit_code) {
    snprintf(exit_buf, exit_buf_size, "%d", exit_code);
    for (int i = 0; i < argc; i++) {
        if (strcmp(args[i], "$?") == 0) {
            args[i] = exit_buf;
        }
    }
}

static int execute_external_command(char **args) {
    if (!args || !args[0] || args[0][0] == 0) {
        return 0;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return -1;
    }

    if (pid == 0) {
        struct sigaction sa_dfl;
        memset(&sa_dfl, 0, sizeof(sa_dfl));
        sa_dfl.sa_handler = SIG_DFL;
        sigaction(SIGINT, &sa_dfl, NULL);
        sigaction(SIGQUIT, &sa_dfl, NULL);

        execvp(args[0], args);

        if (errno == ENOENT) {
            if (strchr(args[0], '/') == NULL) {
                fprintf(stderr, "%s: command not found\n", args[0]);
            } else {
                fprintf(stderr, "%s: No such file or directory\n", args[0]);
            }
            _exit(127);
        } else {
            fprintf(stderr, "%s: %s\n", args[0], strerror(errno));
            _exit(126);
        }
    }

    struct sigaction sa_ign, sa_orig_int, sa_orig_quit;
    memset(&sa_ign, 0, sizeof(sa_ign));
    sa_ign.sa_handler = SIG_IGN;
    sigaction(SIGINT, &sa_ign, &sa_orig_int);
    sigaction(SIGQUIT, &sa_ign, &sa_orig_quit);

    int status = 0;
    int exit_code = 0;

    while (waitpid(pid, &status, 0) < 0) {
        if (errno == EINTR) {
            continue;
        }
        perror("waitpid");
        exit_code = -1;
        break;
    }

    sigaction(SIGINT, &sa_orig_int, NULL);
    sigaction(SIGQUIT, &sa_orig_quit, NULL);

    if (exit_code == 0) {
        if (WIFEXITED(status)) {
            exit_code = WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            exit_code = 128 + WTERMSIG(status);
            if (WTERMSIG(status) == SIGINT) {
                printf("\n");
            }
        }
    }

    return exit_code;
}

void system_manager_init(SystemManager *sm) {
    if (!sm) return;
    process_manager_init(&sm->pm);
    resource_manager_init(&sm->rm);
    sm->is_running = true;
    sm->strategy = STRATEGY_DETECTION_RECOVERY;
    timeline_init(&sm->timeline);
    sm->banker_avoidance_enabled = false; /* Detection mode by default; enable via banker-mode on */
    logger_init(DEFAULT_LOG_FILE);
    logger_log(LOG_INFO, "DeadlockDoctor system manager initialized.");
}

void system_manager_set_strategy(SystemManager *sm, DeadlockStrategy strat) {
    if (!sm) return;
    sm->strategy = strat;
    sm->banker_avoidance_enabled = (strat == STRATEGY_BANKER_AVOIDANCE);
    logger_log(LOG_INFO, "Deadlock management strategy set to: %s", system_manager_get_strategy_name(strat));
}

const char *system_manager_get_strategy_name(DeadlockStrategy strat) {
    switch (strat) {
        case STRATEGY_DETECTION_RECOVERY: return "Detection & Recovery";
        case STRATEGY_RESOURCE_ORDERING:  return "Resource Ordering (Prevention)";
        case STRATEGY_ALL_OR_NOTHING:     return "All-or-Nothing (Prevention)";
        case STRATEGY_BANKER_AVOIDANCE:   return "Banker's Avoidance";
        default:                          return "Unknown";
    }
}

void system_manager_print_strategy_comparison(void) {
    printf("\n========================================================================================================\n");
    printf("                     DEADLOCK MANAGEMENT STRATEGIES & TRADE-OFF COMPARISON                              \n");
    printf("========================================================================================================\n\n");

    printf("+-----------------------------------+---------------+-------------+--------------+------------------+------------------+-------------------+\n");
    printf("| Strategy                          | Resource Util | Concurrency | Waiting Time | Implementation   | Starvation Risk  | Recovery Overhead |\n");
    printf("+-----------------------------------+---------------+-------------+--------------+------------------+------------------+-------------------+\n");
    printf("| 1. Global Resource Ordering (Prev)| Moderate      | Moderate    | Low          | Simple (O(1))    | Low              | None (0 cycles)   |\n");
    printf("| 2. All-or-Nothing (Hold-Wait Prev)| Low           | Low         | High         | Low (Atomic)     | HIGH (Starvation)| None (0 cycles)   |\n");
    printf("| 3. Banker's Algorithm (Avoidance) | High          | High        | Moderate     | High (O(m*n^2))  | Low              | None (0 cycles)   |\n");
    printf("| 4. Deadlock Detection & Recovery  | Very High     | Maximum     | Minimal      | Moderate (O(m*n))| Zero             | High (Abortion)   |\n");
    printf("+-----------------------------------+---------------+-------------+--------------+------------------+------------------+-------------------+\n\n");

    printf("[STRATEGY ANALYSIS & OS VIVA REFERENCE]\n");
    printf("  A. Global Resource Ordering (Havender's Rule):\n");
    printf("     - Breaks Coffman Condition #4: Circular Wait.\n");
    printf("     - Requires resources to be acquired in ascending global index order (R1 < R2 < R3 ...).\n");
    printf("     - Advantage: No runtime deadlock detection overhead; guarantees zero deadlocks.\n");
    printf("     - Disadvantage: Inflexible; out-of-order resource requests are blocked even if free.\n\n");

    printf("  B. All-or-Nothing Allocation:\n");
    printf("     - Breaks Coffman Condition #2: Hold and Wait.\n");
    printf("     - A process must acquire its entire resource set atomically; if any resource is busy, none are held.\n");
    printf("     - Advantage: Simple to enforce.\n");
    printf("     - Disadvantage: Severe resource under-utilization; processes requiring multiple resources risk starvation!\n\n");

    printf("  C. Banker's Avoidance Algorithm:\n");
    printf("     - Dynamically inspects maximum demand and remaining available resources before every grant.\n");
    printf("     - Advantage: High utilization without ever needing to abort running processes.\n");
    printf("     - Disadvantage: Requires processes to declare maximum demands in advance; O(m * n^2) runtime overhead.\n\n");

    printf("  D. Deadlock Detection & Victim Recovery:\n");
    printf("     - Grants resources optimistically as requested. Detects cycles periodically via vector reduction.\n");
    printf("     - Advantage: Maximum concurrency; no pre-declared maximum needed.\n");
    printf("     - Disadvantage: Incurs victim abortion, lost computation, and resource preemption overhead.\n");
    printf("========================================================================================================\n\n");
}

void system_manager_cleanup(SystemManager *sm) {
    if (!sm) return;
    logger_log(LOG_INFO, "DeadlockDoctor system manager shutting down.");
    logger_close();
    process_manager_cleanup(&sm->pm);
    resource_manager_cleanup(&sm->rm);
    timeline_cleanup(&sm->timeline);
    sm->is_running = false;
}

Process *system_manager_create_process(SystemManager *sm, int id, const char *name) {
    if (!sm) return NULL;
    Process *p = process_create(&sm->pm, id, name);
    if (p) {
        timeline_record(&sm->timeline, TIMELINE_PROCESS_CREATE, name, NULL, 0, NULL, "Process '%s' (PID %d) registered", name, id);
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
        timeline_record(&sm->timeline, TIMELINE_RESOURCE_CREATE, NULL, name, total_instances, NULL, "Resource '%s' created (Total: %d)", name, total_instances);
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
    timeline_record(&sm->timeline, TIMELINE_RESOURCE_REQUEST, proc->name, res->name, units, NULL, "Request for %d units of '%s'", units, res->name);

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

    /* Check Strategy Constraints */
    if (sm->strategy == STRATEGY_RESOURCE_ORDERING) {
        int max_held_idx = -1;
        for (int r = 0; r < sm->rm.count; r++) {
            if (r < proc->res_capacity && proc->allocated[r] > 0) {
                if (r > max_held_idx) max_held_idx = r;
            }
        }
        if (max_held_idx >= 0 && res_idx < max_held_idx) {
            printf("[PREVENTION - RESOURCE ORDERING] Request REJECTED!\n");
            printf("  Process '%s' currently holds '%s' (Index %d), but requested '%s' (Index %d).\n",
                   proc->name, sm->rm.resources[max_held_idx].name, max_held_idx, res->name, res_idx);
            printf("  Enforcing strict global ordering (R1 < R2 < R3 ...) to eliminate Circular Wait.\n");
            logger_log(LOG_RESOURCE_BLOCK, "Process '%s' request for '%s' REJECTED by Resource Ordering (Holds higher index '%s').",
                       proc->name, res->name, sm->rm.resources[max_held_idx].name);
            return ALLOC_BLOCKED;
        }
    } else if (sm->strategy == STRATEGY_ALL_OR_NOTHING) {
        if (units > res->available_instances) {
            printf("[PREVENTION - ALL-OR-NOTHING] Request BLOCKED!\n");
            printf("  Process '%s' requested %d units of '%s' (only %d available).\n",
                   proc->name, units, res->name, res->available_instances);
            printf("  All-or-Nothing policy prevents holding partial allocations to eliminate Hold-and-Wait.\n");
            return ALLOC_BLOCKED;
        }
    }

    /* 2. Check whether requested amount is <= Available */
    if (units > res->available_instances) {
        proc->requested[res_idx] = units;
        process_set_state(proc, PROCESS_STATE_BLOCKED);
        printf("[BLOCKED] Process '%s' (PID %d) requested %d units of '%s', but only %d available. Process marked BLOCKED.\n",
               proc->name, proc->id, units, res->name, res->available_instances);
        logger_log(LOG_RESOURCE_BLOCK, "Process '%s' (PID %d) request for %d units of '%s' BLOCKED (Insufficient available: %d).",
                   proc->name, proc->id, units, res->name, res->available_instances);
        timeline_record(&sm->timeline, TIMELINE_RESOURCE_BLOCK, proc->name, res->name, units, NULL, "BLOCKED: Insufficient available (%d/%d)", res->available_instances, units);
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
                process_set_state(proc, PROCESS_STATE_BLOCKED);
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
                process_set_state(proc, PROCESS_STATE_RUNNING);
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
    process_set_state(proc, PROCESS_STATE_RUNNING);
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
                process_set_state(proc, PROCESS_STATE_RUNNING);
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


void system_manager_run_full_demo(void) {
    printf("\n");
    printf("================================================================================\n");
    printf("          DEADLOCKDOCTOR: COMPLETE OPERATING SYSTEMS DEMONSTRATION             \n");
    printf("================================================================================\n");
    printf(" Presentation-ready demonstration of Process & Resource Management,\n");
    printf(" Banker's Safety Algorithm, Deadlock Cycle Detection, Graph Theory, and Recovery.\n");
    printf("================================================================================\n\n");

    /* ========================================================================
     * STAGE 1: RESOURCE MANAGEMENT
     * ======================================================================== */
    printf("--------------------------------------------------------------------------------\n");
    printf("STAGE 1: RESOURCE MANAGEMENT (CREATION, CLAIMS, ALLOCATION, REQUESTS)\n");
    printf("--------------------------------------------------------------------------------\n");
    printf("Concept: Operating Systems track active processes and resource inventory via\n");
    printf("state vectors and allocation matrices.\n\n");

    SystemManager sm1;
    system_manager_init(&sm1);

    printf("[Step 1.1] Registering multi-instance system resources...\n");
    system_manager_create_resource(&sm1, 1, "R1", 10);
    system_manager_create_resource(&sm1, 2, "R2", 5);
    system_manager_create_resource(&sm1, 3, "R3", 7);
    printf("  -> Created R1 (10 units), R2 (5 units), R3 (7 units)\n\n");

    printf("[Step 1.2] Registering active processes...\n");
    system_manager_create_process(&sm1, 1, "P1");
    system_manager_create_process(&sm1, 2, "P2");
    system_manager_create_process(&sm1, 3, "P3");
    printf("  -> Created processes P1, P2, P3 in READY state\n\n");

    printf("[Step 1.3] Declaring maximum resource demands (Max Claims)...\n");
    system_manager_set_max(&sm1, "P1", "R1", 7);
    system_manager_set_max(&sm1, "P1", "R2", 5);
    system_manager_set_max(&sm1, "P1", "R3", 3);
    system_manager_set_max(&sm1, "P2", "R1", 3);
    system_manager_set_max(&sm1, "P2", "R2", 2);
    system_manager_set_max(&sm1, "P2", "R3", 2);
    system_manager_set_max(&sm1, "P3", "R1", 9);
    system_manager_set_max(&sm1, "P3", "R2", 0);
    system_manager_set_max(&sm1, "P3", "R3", 2);
    printf("  -> Max claims declared for all processes across R1, R2, R3\n\n");

    printf("[Step 1.4] Allocating initial resource instances...\n");
    system_manager_request(&sm1, "P1", "R2", 1);
    system_manager_request(&sm1, "P2", "R1", 2);
    system_manager_request(&sm1, "P3", "R1", 3);
    system_manager_request(&sm1, "P3", "R3", 2);
    printf("\n");

    printf("[Step 1.5] Inspecting Resource Inventory & Process Allocation Matrix:\n");
    system_manager_show_status(&sm1);

    printf("[Step 1.6] Demonstrating Resource Release & Re-allocation...\n");
    system_manager_release(&sm1, "P1", "R2", 1);
    system_manager_request(&sm1, "P1", "R2", 1);
    printf("  -> Verified dynamic resource release and grant cycles.\n\n");

    system_manager_cleanup(&sm1);
    printf(">>> STAGE 1 COMPLETED: Resource state cleanly verified and reset <<<\n\n");

    /* ========================================================================
     * STAGE 2: BANKER'S ALGORITHM
     * ======================================================================== */
    printf("--------------------------------------------------------------------------------\n");
    printf("STAGE 2: BANKER'S ALGORITHM (MATHEMATICALLY VALID SAFE STATE & UNSAFE REQUEST)\n");
    printf("--------------------------------------------------------------------------------\n");
    printf("Concept: Dijkstra's Banker's Algorithm prevents deadlock by verifying that at\n");
    printf("least one safe execution sequence exists where all processes can finish.\n\n");

    SystemManager sm2;
    system_manager_init(&sm2);

    /* Setup textbook safe state */
    system_manager_create_resource(&sm2, 1, "R1", 10);
    system_manager_create_resource(&sm2, 2, "R2", 5);
    system_manager_create_resource(&sm2, 3, "R3", 7);

    system_manager_create_process(&sm2, 1, "P1");
    system_manager_create_process(&sm2, 2, "P2");
    system_manager_create_process(&sm2, 3, "P3");

    system_manager_set_max(&sm2, "P1", "R1", 7);
    system_manager_set_max(&sm2, "P1", "R2", 5);
    system_manager_set_max(&sm2, "P1", "R3", 3);
    system_manager_set_max(&sm2, "P2", "R1", 3);
    system_manager_set_max(&sm2, "P2", "R2", 2);
    system_manager_set_max(&sm2, "P2", "R3", 2);
    system_manager_set_max(&sm2, "P3", "R1", 9);
    system_manager_set_max(&sm2, "P3", "R2", 0);
    system_manager_set_max(&sm2, "P3", "R3", 2);

    system_manager_request(&sm2, "P1", "R2", 1);
    system_manager_request(&sm2, "P2", "R1", 2);
    system_manager_request(&sm2, "P3", "R1", 3);
    system_manager_request(&sm2, "P3", "R3", 2);

    printf("[Step 2.1] Evaluating Banker's Safety Algorithm on Current State:\n");
    BankerSystem bs;
    banker_system_init(&bs);
    banker_init_from_manager(&bs, &sm2.pm, &sm2.rm);
    banker_print_safety_report(&bs);

    int safe_seq[MAX_PROCESSES];
    int seq_len = 0;
    bool is_safe = banker_is_safe(&bs, safe_seq, &seq_len);
    printf("  [Mathematical Verification]\n");
    printf("  * Available Vector : [ R1:%d, R2:%d, R3:%d ]\n",
           bs.available[0], bs.available[1], bs.available[2]);
    printf("  * Need Matrix      : P1=[7,4,3], P2=[1,2,2], P3=[6,0,0]\n");
    printf("  * State Verdict    : %s\n", is_safe ? "SAFE STATE GUARANTEED" : "UNSAFE");
    if (is_safe) {
        printf("  * Safe Sequence    : ");
        for (int i = 0; i < seq_len; i++) {
            printf("%s%s", bs.process_names[safe_seq[i]], (i == seq_len - 1) ? "\n\n" : " -> ");
        }
    }
    banker_system_cleanup(&bs);

    printf("[Step 2.2] Demonstrating Banker Avoidance of an Unsafe Request:\n");
    printf("  Enabling Banker Avoidance Mode (banker-mode on)...\n");
    sm2.banker_avoidance_enabled = true;
    printf("  Process 'P1' attempts to request 5 additional units of 'R1'...\n");
    printf("  Analysis: If granted, Available R1 drops to 0 (cannot satisfy P2's need of 1).\n");
    AllocResult unsafe_req = system_manager_request(&sm2, "P1", "R1", 5);
    if (unsafe_req == ALLOC_BLOCKED) {
        printf("  >>> [SUCCESS] Banker Avoidance BLOCKED the request! System remains in a SAFE state. <<<\n\n");
    }

    system_manager_cleanup(&sm2);
    printf(">>> STAGE 2 COMPLETED: Banker safe sequence & avoidance verified <<<\n\n");

    /* ========================================================================
     * STAGE 3: DEADLOCK DETECTION & GRAPH
     * ======================================================================== */
    printf("--------------------------------------------------------------------------------\n");
    printf("STAGE 3: DEADLOCK DETECTION (CIRCULAR WAIT & ASCII GRAPH VISUALIZATION)\n");
    printf("--------------------------------------------------------------------------------\n");
    printf("Concept: Coffman's circular wait condition occurs when processes form a closed\n");
    printf("cycle of dependency. DeadlockDoctor builds the Wait-For Graph (WFG) and executes\n");
    printf("DFS cycle detection to identify the exact deadlocked process set.\n\n");

    SystemManager sm3;
    system_manager_init(&sm3);

    printf("[Step 3.1] Creating Circular-Wait Scenario (2 Processes, 2 Mutex Resources)...\n");
    system_manager_create_resource(&sm3, 1, "R1", 1);
    system_manager_create_resource(&sm3, 2, "R2", 1);

    system_manager_create_process(&sm3, 1, "P1");
    system_manager_create_process(&sm3, 2, "P2");

    system_manager_set_max(&sm3, "P1", "R1", 1);
    system_manager_set_max(&sm3, "P1", "R2", 1);
    system_manager_set_max(&sm3, "P2", "R1", 1);
    system_manager_set_max(&sm3, "P2", "R2", 1);

    printf("  * P1 acquires R1 (held)\n");
    system_manager_request(&sm3, "P1", "R1", 1);

    printf("  * P2 acquires R2 (held)\n");
    system_manager_request(&sm3, "P2", "R2", 1);

    printf("  * P1 requests R2 -> Blocked (held by P2)\n");
    system_manager_request(&sm3, "P1", "R2", 1);

    printf("  * P2 requests R1 -> Blocked (held by P1)\n");
    system_manager_request(&sm3, "P2", "R1", 1);
    printf("\n");

    printf("[Step 3.2] Visualizing Resource Allocation Graph & Wait-For Graph:\n");
    system_manager_show_graph(&sm3);

    printf("[Step 3.3] Running Deadlock Detector (Wait-For Graph DFS Cycle Search):\n");
    DeadlockReport report;
    deadlock_report_init(&report);
    bool detected = detector_run(&sm3.pm, &sm3.rm, &report);
    detector_print_report(&report);

    printf("  [Detector Verification]\n");
    printf("  * Deadlock Flag     : %s\n", detected ? "TRUE (DEADLOCK DETECTED)" : "FALSE");
    printf("  * Cycle Length      : %d\n", report.cycle_len);
    printf("  * Deadlocked Set (%d): { ", report.deadlocked_count);
    for (int i = 0; i < report.deadlocked_count; i++) {
        printf("%s%s", report.deadlocked_names[i], (i == report.deadlocked_count - 1) ? "" : ", ");
    }
    printf(" }\n\n");
    deadlock_report_cleanup(&report);

    printf(">>> STAGE 3 COMPLETED: Circular wait and exact deadlocked set verified <<<\n\n");

    /* ========================================================================
     * STAGE 4: DEADLOCK RECOVERY
     * ======================================================================== */
    printf("--------------------------------------------------------------------------------\n");
    printf("STAGE 4: DEADLOCK RECOVERY (VICTIM SELECTION, RESOURCE RECLAMATION & RESOLUTION)\n");
    printf("--------------------------------------------------------------------------------\n");
    printf("Concept: To break circular wait, DeadlockDoctor evaluates candidate recovery costs:\n");
    printf("Cost = (Held Units * 2) + (Pending Requests * 1) + Priority.\n");
    printf("The process with the lowest cost is chosen as victim, aborted, and its resources\n");
    printf("are reclaimed to unblock remaining processes.\n\n");

    printf("[Step 4.1] Setting process priorities for recovery cost evaluation...\n");
    Process *p1 = process_find_by_id_or_name(&sm3.pm, "P1");
    Process *p2 = process_find_by_id_or_name(&sm3.pm, "P2");
    if (p1) process_set_priority(p1, 100); /* High priority: protect P1 */
    if (p2) process_set_priority(p2, 1);   /* Low priority: candidate victim P2 */
    printf("  -> Process 'P1' priority = 100 (protect)\n");
    printf("  -> Process 'P2' priority = 1   (candidate)\n\n");

    printf("[Step 4.2] Executing 10-Step Deadlock Recovery:\n");
    bool recovered = recovery_run(&sm3);

    printf("\n[Step 4.3] Post-Recovery Verification:\n");
    printf("  * Recovery Status   : %s\n", recovered ? "SUCCESS (DEADLOCK RESOLVED)" : "FAILED");
    if (p2) {
        printf("  * Victim 'P2' State : %s\n", process_state_to_string(p2->state));
    }
    if (p1) {
        printf("  * Surviving 'P1'    : %s (Acquired pending resource R2)\n", process_state_to_string(p1->state));
    }

    DeadlockReport post_report;
    deadlock_report_init(&post_report);
    detector_run(&sm3.pm, &sm3.rm, &post_report);
    printf("  * Re-run Detector   : %s\n", post_report.deadlock_detected ? "DEADLOCK PERSISTS" : "NO DEADLOCK DETECTED (CYCLE BROKEN)");
    deadlock_report_cleanup(&post_report);

    system_manager_cleanup(&sm3);
    printf("\n>>> STAGE 4 COMPLETED: Deadlock resolved and system returned to safe state <<<\n\n");

    /* ========================================================================
     * CONCLUSION
     * ======================================================================== */
    printf("================================================================================\n");
    printf("                    DEMONSTRATION CONCLUSION & VIVA SUMMARY                     \n");
    printf("================================================================================\n");
    printf(" [1] RESOURCE MANAGEMENT : Registered resources & processes, allocated/released units.\n");
    printf(" [2] BANKER'S ALGORITHM  : Validated safe sequence <P2 -> P1 -> P3> & blocked unsafe req.\n");
    printf(" [3] DEADLOCK DETECTION  : DFS cycle search identified exact deadlocked set { P1, P2 }.\n");
    printf(" [4] GRAPH THEORY        : Generated ASCII Resource Allocation Graph & Wait-For Graph.\n");
    printf(" [5] DEADLOCK RECOVERY   : Aborted lowest-cost victim P2, unblocked P1 -> System safe.\n");
    printf("================================================================================\n\n");
}

void system_manager_print_help(void) {
    printf("\n================================================================================\n");
    printf("                    DEADLOCKDOCTOR COMMAND REFERENCE (CLI)                      \n");
    printf("================================================================================\n\n");
    printf("  [SYSTEM STATUS & HELP]\n");
    printf("    help                               - Display this beginner-friendly command guide\n");
    printf("    status                             - Display system matrices (Available, Alloc, Max, Need)\n");
    printf("    monitor                            - Live process monitor (state, held, pending, wait duration)\n");
    printf("    dashboard                          - Resource utilization dashboard (total, alloc, avail, %%%%)\n");
    printf("    live                               - Interactive auto-refreshing monitor and dashboard\n");
    printf("    pwd                                - Print current working directory\n");
    printf("    cd [path]                          - Change directory (~, .., /path, -)\n\n");

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
    printf("    demo [all|thread|fork|ipc]         - Run OS demonstrations (all: complete viva demo)\n\n");

    printf("  [EXTERNAL LINUX COMMANDS]\n");
    printf("    <any Linux command>                - Execute external Ubuntu command (e.g. ls, cat, nano, gcc)\n\n");

    printf("  [EXIT]\n");
    printf("    exit | quit                        - Safely exit DeadlockDoctor\n");
    printf("================================================================================\n\n");
}

static const char *s_builtin_commands[] = {
    "process", "resource", "request", "release", "banker", "detect",
    "graph", "recover", "recover-auto", "status", "pwd", "cd", "demo",
    "load", "log", "remove-res", "fork-demo", "process-demo",
    "ipc", "ipc-demo", "help", "exit", "quit", NULL
};

static char *shell_command_generator(const char *text, int state) {
    static int list_index, len;
    const char *name;

    if (!state) {
        list_index = 0;
        len = (int)strlen(text);
    }

    while ((name = s_builtin_commands[list_index++]) != NULL) {
        if (strncasecmp(name, text, len) == 0) {
            return strdup(name);
        }
    }

    return NULL;
}

static char **shell_completion(const char *text, int start, int end) {
    (void)end;
    if (start == 0) {
        return rl_completion_matches(text, shell_command_generator);
    }
    return NULL;
}

static char s_history_file[PATH_MAX] = "";

static void setup_shell_history(void) {
    rl_attempted_completion_function = shell_completion;
    const char *home = getenv("HOME");
    if (home && home[0] != '\0') {
        snprintf(s_history_file, sizeof(s_history_file), "%s/.deadlockdoctor_history", home);
        read_history(s_history_file);
    }
}

static void save_shell_history(void) {
    if (s_history_file[0] != '\0') {
        write_history(s_history_file);
        history_truncate_file(s_history_file, 1000);
    }
}

void system_manager_run_cli(SystemManager *sm) {
    if (!sm) return;

    setup_shell_history();

    struct sigaction sa_int;
    memset(&sa_int, 0, sizeof(sa_int));
    sa_int.sa_handler = shell_sigint_handler;
    sigaction(SIGINT, &sa_int, NULL);

    char line[MAX_CMD_LEN];
    char token_buf[MAX_CMD_LEN * 2];
    char *args[MAX_SHELL_ARGS];
    char exit_buf[16];
    int is_interactive = isatty(STDIN_FILENO);

    while (sm->is_running) {
        if (is_interactive) {
            char *raw = readline("deadlockdoctor> ");
            if (!raw) {
                if (s_shell_sigint) {
                    s_shell_sigint = 0;
                    printf("\n");
                    continue;
                }
                /* End of file / Ctrl+D */
                printf("\n");
                break;
            }

            size_t raw_len = strlen(raw);
            while (raw_len > 0 && (raw[raw_len - 1] == '\n' || raw[raw_len - 1] == '\r' || raw[raw_len - 1] == ' ')) {
                raw[--raw_len] = '\0';
            }

            if (raw_len > 0) {
                HIST_ENTRY *last = history_get(history_length);
                if (!last || strcmp(last->line, raw) != 0) {
                    add_history(raw);
                    if (s_history_file[0] != '\0') {
                        append_history(1, s_history_file);
                    }
                }
            }

            if (raw_len >= sizeof(line)) {
                raw_len = sizeof(line) - 1;
            }
            memcpy(line, raw, raw_len);
            line[raw_len] = '\0';
            free(raw);
        } else {
            if (!fgets(line, sizeof(line), stdin)) {
                if (feof(stdin)) {
                    break; /* EOF */
                }
                if (s_shell_sigint) {
                    s_shell_sigint = 0;
                    clearerr(stdin);
                    printf("\n");
                    continue;
                }
                break;
            }

            size_t len = strlen(line);
            while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r' || line[len - 1] == ' ')) {
                line[--len] = 0;
            }
        }

        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r' || line[len - 1] == ' ')) {
            line[--len] = 0;
        }

        /* Skip empty or comment lines */
        if (len == 0 || line[0] == '#') {
            continue;
        }

        int argc = parse_command_line(line, token_buf, sizeof(token_buf), args, MAX_SHELL_ARGS);
        if (argc <= 0 || !args[0] || args[0][0] == 0) {
            continue;
        }

        expand_exit_status(args, argc, exit_buf, sizeof(exit_buf), s_last_exit_status);

        const char *cmd = args[0];

        if (strcasecmp(cmd, "pwd") == 0) {
            char cwd[PATH_MAX];
            if (getcwd(cwd, sizeof(cwd)) != NULL) {
                printf("%s\n", cwd);
                s_last_exit_status = 0;
            } else {
                perror("pwd");
                s_last_exit_status = 1;
            }
        } else if (strcasecmp(cmd, "cd") == 0) {
            const char *target = NULL;
            char path_buf[PATH_MAX];

            if (argc < 2 || args[1] == NULL || strlen(args[1]) == 0 || strcmp(args[1], "~") == 0) {
                target = getenv("HOME");
                if (!target) {
                    target = "/";
                }
            } else if (strncmp(args[1], "~/", 2) == 0) {
                const char *home = getenv("HOME");
                if (!home) home = "";
                snprintf(path_buf, sizeof(path_buf), "%s/%s", home, args[1] + 2);
                target = path_buf;
            } else if (strcmp(args[1], "-") == 0) {
                if (s_old_pwd[0] == 0) {
                    fprintf(stderr, "cd: OLDPWD not set\n");
                    s_last_exit_status = 1;
                    target = NULL;
                } else {
                    target = s_old_pwd;
                    printf("%s\n", target);
                }
            } else {
                target = args[1];
            }

            if (target) {
                char current_cwd[PATH_MAX];
                char *got_cwd = getcwd(current_cwd, sizeof(current_cwd));
                if (chdir(target) != 0) {
                    fprintf(stderr, "cd: %s: %s\n", target, strerror(errno));
                    s_last_exit_status = 1;
                } else {
                    if (got_cwd) {
                        strncpy(s_old_pwd, current_cwd, sizeof(s_old_pwd) - 1);
                        s_old_pwd[sizeof(s_old_pwd) - 1] = 0;
                    }
                    s_last_exit_status = 0;
                }
            }
        } else if (strcasecmp(cmd, "process") == 0 || strcasecmp(cmd, "proc") == 0) {
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
        } else if (strcasecmp(cmd, "timeline") == 0 || strcasecmp(cmd, "events") == 0) {
            int max_ev = 0;
            char filter[64] = "";
            if (argc > 1) {
                if (isdigit((unsigned char)args[1][0])) {
                    max_ev = atoi(args[1]);
                } else if (strcasecmp(args[1], "clear") == 0) {
                    timeline_clear(&sm->timeline);
                    printf("[OK] Event timeline cleared.\n");
                    continue;
                } else {
                    strncpy(filter, args[1], sizeof(filter) - 1);
                }
            }
            if (argc > 2 && filter[0] == '\0') {
                strncpy(filter, args[2], sizeof(filter) - 1);
            }
            timeline_show(&sm->timeline, max_ev, filter[0] != '\0' ? filter : NULL);
        } else if (strcasecmp(cmd, "step") == 0 || strcasecmp(cmd, "replay-step") == 0) {
            if (argc > 1) {
                scenario_step_session_run_interactive(sm, args[1]);
            } else {
                printf("[Error] Usage: step <scenario-file>\n");
            }
        } else if (strcasecmp(cmd, "monitor") == 0 || strcasecmp(cmd, "proc-monitor") == 0) {
            monitor_show_process_table(&sm->pm, &sm->rm);
        } else if (strcasecmp(cmd, "dashboard") == 0 || strcasecmp(cmd, "res-dashboard") == 0) {
            monitor_show_resource_dashboard(&sm->rm, &sm->pm);
        } else if (strcasecmp(cmd, "live") == 0 || strcasecmp(cmd, "top") == 0) {
            monitor_run_live(sm, 1000, 0);
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
                } else if (strcasecmp(demo_type, "all") == 0 || strcasecmp(demo_type, "full") == 0 ||
                           strcasecmp(demo_type, "viva") == 0) {
                    system_manager_run_full_demo();
                } else {
                    printf("[Error] Unknown demo '%s'. Available demos: demo [all|thread|fork|ipc]\n", demo_type);
                }
            } else {
                system_manager_run_full_demo();
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
        } else if (strcasecmp(cmd, "strategy") == 0) {
            if (argc > 1) {
                if (strcasecmp(args[1], "compare") == 0) {
                    system_manager_print_strategy_comparison();
                } else if (strcasecmp(args[1], "ordered") == 0 || strcasecmp(args[1], "ordering") == 0) {
                    system_manager_set_strategy(sm, STRATEGY_RESOURCE_ORDERING);
                    printf("[OK] Active strategy set to: Resource Ordering (Prevention)\n");
                } else if (strcasecmp(args[1], "atomic") == 0 || strcasecmp(args[1], "all-or-nothing") == 0) {
                    system_manager_set_strategy(sm, STRATEGY_ALL_OR_NOTHING);
                    printf("[OK] Active strategy set to: All-or-Nothing (Hold-and-Wait Prevention)\n");
                } else if (strcasecmp(args[1], "banker") == 0 || strcasecmp(args[1], "avoidance") == 0) {
                    system_manager_set_strategy(sm, STRATEGY_BANKER_AVOIDANCE);
                    printf("[OK] Active strategy set to: Banker's Avoidance Algorithm\n");
                } else if (strcasecmp(args[1], "detect") == 0 || strcasecmp(args[1], "recovery") == 0) {
                    system_manager_set_strategy(sm, STRATEGY_DETECTION_RECOVERY);
                    printf("[OK] Active strategy set to: Deadlock Detection & Recovery\n");
                } else {
                    printf("Usage: strategy [detect | ordered | atomic | banker | compare]\n");
                }
            } else {
                printf("Current deadlock management strategy: %s\n", system_manager_get_strategy_name(sm->strategy));
                printf("Use 'strategy compare' to view comparative trade-offs.\n");
            }
        } else if (strcasecmp(cmd, "timeout") == 0 || strcasecmp(cmd, "timeout-audit") == 0) {
            double thresh = 1000.0;
            if (argc > 1) {
                thresh = atof(args[1]);
                if (thresh <= 0.0) thresh = 1000.0;
            }
            TimeoutAuditResult tres;
            timeout_run_audit(sm, thresh, &tres);
            timeout_print_audit_report(&tres);
            timeout_cleanup_audit(&tres);
        } else if (strcasecmp(cmd, "trace") == 0 || strcasecmp(cmd, "strace") == 0 || strcasecmp(cmd, "strace-demo") == 0) {
            trace_run_demonstration();
        } else if (strcasecmp(cmd, "compare") == 0 || strcasecmp(cmd, "algo-compare") == 0) {
            int p = 10, r = 4, ops = 80;
            unsigned int s = 42;
            if (argc > 1) p = atoi(args[1]);
            if (argc > 2) r = atoi(args[2]);
            if (argc > 3) ops = atoi(args[3]);
            if (argc > 4) s = (unsigned int)atoi(args[4]);
            benchmark_compare_algorithms(p, r, ops, s);
        } else if (strcasecmp(cmd, "stress") == 0) {
            int p = 20, r = 6, ops = 300;
            unsigned int s = 777;
            if (argc > 1) p = atoi(args[1]);
            if (argc > 2) r = atoi(args[2]);
            if (argc > 3) ops = atoi(args[3]);
            if (argc > 4) s = (unsigned int)atoi(args[4]);
            benchmark_run_stress_test(p, r, ops, s);
        } else if (strcasecmp(cmd, "benchmark") == 0) {
            benchmark_run_suite();
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
            s_last_exit_status = execute_external_command(args);
        }
    }

    save_shell_history();
    clear_history();
}
