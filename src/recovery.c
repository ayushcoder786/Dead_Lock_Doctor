#include "recovery.h"
#include "manager.h"
#include "logger.h"
#include "detector.h"
#include "banker.h"
#include "graph.h"

int recovery_calculate_cost(const Process *proc, int num_resources) {
    if (!proc) return 0;

    int held_units = 0;
    int pending_units = 0;

    for (int r = 0; r < num_resources; r++) {
        if (r < proc->res_capacity) {
            held_units += proc->allocated[r];
            pending_units += proc->requested[r];
        }
    }

    int priority = proc->priority > 0 ? proc->priority : 1;
    /* Transparent cost formula: (held_units * 2) + (pending_units * 1) + priority */
    return (held_units * 2) + (pending_units * 1) + priority;
}

bool recovery_run(SystemManager *sm) {
    if (!sm) return false;

    DeadlockReport report;
    deadlock_report_init(&report);
    detector_run(&sm->pm, &sm->rm, &report);

    if (!report.deadlock_detected || report.deadlocked_count == 0) {
        printf("\n[RECOVERY] No deadlock detected. The system does not require recovery.\n\n");
        deadlock_report_cleanup(&report);
        return true;
    }

    printf("\n======================================================================\n");
    printf("               DEADLOCKDOCTOR DEADLOCK RECOVERY (CO-4)                \n");
    printf("======================================================================\n");
    printf("[SIMULATION NOTE: Recovery terminates the candidate in DeadlockDoctor's\n");
    printf(" internal simulation model and reclaims its allocated resources.\n");
    printf(" No external OS kernel processes are killed.]\n\n");

    /* 1. Display the deadlocked set */
    printf("[Step 1] Deadlocked Process Set:\n");
    printf("  Identified %d process(es) in deadlock cycle:\n", report.deadlocked_count);
    for (int i = 0; i < report.deadlocked_count; i++) {
        printf("   * %s (PID %d)\n", report.deadlocked_names[i], report.deadlocked_pids[i]);
    }
    printf("\n");

    /* 2. Calculate a simple recovery cost for each candidate process */
    printf("[Step 2] Candidate Recovery Cost Evaluation:\n");
    printf("  Cost Formula: (Held Units * 2) + (Pending Requests * 1) + Priority\n");
    printf("  +---------+--------------------+------------+--------------+----------+------+\n");
    printf("  | PID     | Process Name       | Held Units | Pending Reqs | Priority | Cost |\n");
    printf("  +---------+--------------------+------------+--------------+----------+------+\n");

    ProcessRecoveryCost *costs = (ProcessRecoveryCost *)malloc(report.deadlocked_count * sizeof(ProcessRecoveryCost));
    int victim_idx = 0;
    int lowest_cost = 99999999;
    int lowest_pid = 99999999;

    for (int i = 0; i < report.deadlocked_count; i++) {
        int pid = report.deadlocked_pids[i];
        Process *proc = process_find_by_id(&sm->pm, pid);
        costs[i].pid = pid;
        strncpy(costs[i].name, report.deadlocked_names[i], MAX_NAME_LEN - 1);
        costs[i].name[MAX_NAME_LEN - 1] = '\0';
        costs[i].held_resources = 0;
        costs[i].pending_requests = 0;
        costs[i].priority = proc ? proc->priority : 1;

        if (proc) {
            for (int r = 0; r < sm->rm.count; r++) {
                if (r < proc->res_capacity) {
                    costs[i].held_resources += proc->allocated[r];
                    costs[i].pending_requests += proc->requested[r];
                }
            }
        }
        costs[i].cost = recovery_calculate_cost(proc, sm->rm.count);

        printf("  | %-7d | %-18s | %-10d | %-12d | %-8d | %-4d |\n",
               costs[i].pid, costs[i].name, costs[i].held_resources,
               costs[i].pending_requests, costs[i].priority, costs[i].cost);

        /* Select lowest cost, breaking ties by lowest PID */
        if (costs[i].cost < lowest_cost ||
            (costs[i].cost == lowest_cost && costs[i].pid < lowest_pid)) {
            lowest_cost = costs[i].cost;
            lowest_pid = costs[i].pid;
            victim_idx = i;
        }
    }
    printf("  +---------+--------------------+------------+--------------+----------+------+\n\n");

    /* 3. Select a victim with the lowest recovery cost */
    int victim_pid = costs[victim_idx].pid;
    char victim_name[MAX_NAME_LEN];
    strncpy(victim_name, costs[victim_idx].name, MAX_NAME_LEN - 1);
    victim_name[MAX_NAME_LEN - 1] = '\0';
    free(costs);

    printf("[Step 3] Victim Selection:\n");
    printf("  Selected '%s' (PID %d) with the lowest recovery cost (%d).\n\n",
           victim_name, victim_pid, lowest_cost);

    /* 4. Abort the selected process in the simulator */
    Process *victim_proc = process_find_by_id(&sm->pm, victim_pid);
    if (!victim_proc) {
        deadlock_report_cleanup(&report);
        return false;
    }
    victim_proc->state = PROCESS_STATE_ABORTED;
    printf("[Step 4] Aborting victim in simulator:\n");
    printf("  Process '%s' (PID %d) state transitioned to ABORTED.\n\n",
           victim_name, victim_pid);

    /* 5. Release all resources held by the victim */
    printf("[Step 5] Releasing all resources held by victim '%s':\n", victim_name);
    bool released_any = false;
    for (int r = 0; r < sm->rm.count; r++) {
        if (r < victim_proc->res_capacity && victim_proc->allocated[r] > 0) {
            int amt = victim_proc->allocated[r];
            sm->rm.resources[r].available_instances += amt;
            printf("  - Reclaimed %d unit(s) of resource '%s'.\n", amt, sm->rm.resources[r].name);
            victim_proc->allocated[r] = 0;
            released_any = true;
        }
        if (r < victim_proc->res_capacity) {
            victim_proc->requested[r] = 0;
        }
    }
    if (!released_any) {
        printf("  - (Victim held 0 allocated resources)\n");
    }
    printf("\n");

    /* 6. Update Available */
    printf("[Step 6] Updated Available Resource Vector:\n");
    for (int r = 0; r < sm->rm.count; r++) {
        printf("  * %s: %d available (Total: %d)\n",
               sm->rm.resources[r].name,
               sm->rm.resources[r].available_instances,
               sm->rm.resources[r].total_instances);
    }
    printf("  Checking pending blocked requests for remaining active processes...\n");
    system_manager_check_pending_requests(sm);
    printf("\n");

    /* 7. Remove the victim from the wait-for graph */
    printf("[Step 7] Removing victim '%s' (PID %d) from Wait-For Graph:\n",
           victim_name, victim_pid);
    printf("  Victim node removed; incoming request edges and outgoing holding dependencies pruned.\n\n");

    /* 8. Re-run deadlock detection */
    printf("[Step 8] Re-running Deadlock Detection:\n");
    DeadlockReport new_report;
    deadlock_report_init(&new_report);
    bool has_deadlock = detector_run(&sm->pm, &sm->rm, &new_report);
    if (has_deadlock && new_report.deadlock_detected) {
        printf("  [Result] Deadlock still present (%d process(es) in cycle):\n",
               new_report.deadlocked_count);
        for (int i = 0; i < new_report.deadlocked_count; i++) {
            printf("   * %s (PID %d)\n", new_report.deadlocked_names[i], new_report.deadlocked_pids[i]);
        }
    } else {
        printf("  [Result] NO DEADLOCK DETECTED! Circular wait successfully resolved.\n");
    }
    printf("\n");

    /* 9. Run the Banker's safety check */
    printf("[Step 9] Running Banker's Safety Check:\n");
    BankerSystem bs;
    banker_system_init(&bs);
    banker_init_from_manager(&bs, &sm->pm, &sm->rm);

    int *safe_seq = (int *)malloc((bs.num_processes + 1) * sizeof(int));
    int seq_len = 0;
    bool is_safe = banker_is_safe(&bs, safe_seq, &seq_len);

    banker_print_safety_report(&bs);

    /* 10. Report whether the system returned to a safe state */
    printf("[Step 10] Recovery Outcome Verdict:\n");
    bool fully_resolved = (!has_deadlock || !new_report.deadlock_detected);
    if (fully_resolved && is_safe) {
        printf("  >>> [RECOVERY SUCCESS] The system is free of deadlocks and has returned to a SAFE STATE! <<<\n");
    } else if (fully_resolved) {
        printf("  >>> [RECOVERY PARTIAL] Deadlock cycle was resolved, but system state is currently UNSAFE. <<<\n");
    } else {
        printf("  >>> [RECOVERY INCOMPLETE] Deadlock remains present. Further candidate termination required. <<<\n");
    }
    printf("======================================================================\n\n");

    logger_log(LOG_RECOVERY_EVENT,
               "Deadlock Recovery: Aborted victim '%s' (PID %d). Deadlock resolved: %s. Safe: %s.",
               victim_name, victim_pid, fully_resolved ? "YES" : "NO", is_safe ? "YES" : "NO");

    free(safe_seq);
    banker_system_cleanup(&bs);
    deadlock_report_cleanup(&new_report);
    deadlock_report_cleanup(&report);

    return fully_resolved;
}

bool recovery_abort_process(SystemManager *sm, int pid) {
    if (!sm) return false;

    Process *proc = process_find_by_id(&sm->pm, pid);
    if (!proc) {
        printf("[RECOVERY ERROR] Process ID %d not found.\n", pid);
        return false;
    }

    if (proc->state == PROCESS_STATE_COMPLETED || proc->state == PROCESS_STATE_ABORTED) {
        printf("[RECOVERY ERROR] Process '%s' (PID %d) is already terminated.\n", proc->name, proc->id);
        return false;
    }

    printf("\n[RECOVERY] Manual recovery action: Aborting process '%s' (PID %d)...\n",
           proc->name, proc->id);

    /* Release all held resources back to system */
    for (int r = 0; r < sm->rm.count; r++) {
        if (r < proc->res_capacity && proc->allocated[r] > 0) {
            sm->rm.resources[r].available_instances += proc->allocated[r];
            printf("[RECOVERY] Reclaimed %d units of '%s' from '%s'.\n",
                   proc->allocated[r], sm->rm.resources[r].name, proc->name);
            proc->allocated[r] = 0;
        }
        if (r < proc->res_capacity) {
            proc->requested[r] = 0;
        }
    }

    proc->state = PROCESS_STATE_ABORTED;

    logger_log(LOG_RECOVERY_EVENT,
               "Deadlock Recovery: Aborted process '%s' (PID %d) and reclaimed all held resources.",
               proc->name, proc->id);

    /* Re-evaluate pending blocked requests now that resources are freed */
    system_manager_check_pending_requests(sm);

    printf("[RECOVERY] Process '%s' (PID %d) state set to ABORTED. Recovery action complete.\n\n",
           proc->name, proc->id);
    return true;
}

bool recovery_auto_recover(SystemManager *sm) {
    if (!sm) return false;

    DeadlockReport report;
    deadlock_report_init(&report);
    detector_run(&sm->pm, &sm->rm, &report);

    if (!report.deadlock_detected) {
        printf("[RECOVERY] No deadlock detected. No recovery required.\n");
        deadlock_report_cleanup(&report);
        return true;
    }
    deadlock_report_cleanup(&report);

    printf("\n======================================================================\n");
    printf("         AUTOMATIC DEADLOCK RECOVERY INITIATED                         \n");
    printf("======================================================================\n");

    int recovery_step = 1;
    bool resolved = false;
    int max_steps = sm->pm.count;
    while (max_steps-- > 0) {
        printf("\n>>> Automatic Recovery Iteration %d <<<\n", recovery_step++);
        resolved = recovery_run(sm);
        if (resolved) break;
    }

    if (resolved) {
        printf("[RECOVERY SUCCESS] All circular wait cycles broken! System is free of deadlock.\n");
        logger_log(LOG_RECOVERY_EVENT, "Automatic recovery completed successfully. Deadlock resolved.");
    }
    return resolved;
}
