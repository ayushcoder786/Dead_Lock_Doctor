#include "timeout.h"
#include <math.h>

void timeout_init_audit(TimeoutAuditResult *res, double threshold_ms) {
    if (!res) return;
    memset(res, 0, sizeof(TimeoutAuditResult));
    res->threshold_ms = threshold_ms;
    deadlock_report_init(&res->report);
}

void timeout_cleanup_audit(TimeoutAuditResult *res) {
    if (!res) return;
    deadlock_report_cleanup(&res->report);
}

void timeout_inject_wait_time(Process *proc, double additional_ms) {
    if (!proc || proc->state != PROCESS_STATE_BLOCKED) return;
    if (proc->blocked_time.tv_sec == 0 && proc->blocked_time.tv_nsec == 0) {
        clock_gettime(CLOCK_MONOTONIC, &proc->blocked_time);
    }
    time_t sec_delta = (time_t)(additional_ms / 1000.0);
    long nsec_delta = (long)((additional_ms - (sec_delta * 1000.0)) * 1000000.0);

    proc->blocked_time.tv_sec -= sec_delta;
    if (proc->blocked_time.tv_nsec < nsec_delta) {
        proc->blocked_time.tv_sec -= 1;
        proc->blocked_time.tv_nsec += 1000000000L;
    }
    proc->blocked_time.tv_nsec -= nsec_delta;
}

void timeout_run_audit(SystemManager *sm, double threshold_ms, TimeoutAuditResult *res) {
    if (!sm || !res) return;
    timeout_init_audit(res, threshold_ms);

    /* 1. Evaluate wait duration for all blocked processes */
    for (int i = 0; i < sm->pm.count; i++) {
        Process *p = sm->pm.processes[i];
        if (!p || p->state != PROCESS_STATE_BLOCKED) continue;

        res->total_blocked++;
        if (res->record_count < MAX_PROCESSES) {
            ProcessTimeoutRecord *rec = &res->records[res->record_count++];
            rec->pid = p->id;
            strncpy(rec->name, p->name, sizeof(rec->name) - 1);
            rec->name[sizeof(rec->name) - 1] = '\0';
            rec->wait_time_ms = process_get_waiting_duration_ms(p);

            if (rec->wait_time_ms >= threshold_ms) {
                rec->verdict = WAIT_SUSPECTED_DEADLOCK;
                res->suspected_count++;
                snprintf(rec->details, sizeof(rec->details), "Wait (%.1f ms) >= Threshold (%.1f ms)",
                         rec->wait_time_ms, threshold_ms);
            } else {
                rec->verdict = WAIT_NORMAL;
                snprintf(rec->details, sizeof(rec->details), "Wait (%.1f ms) < Threshold (%.1f ms)",
                         rec->wait_time_ms, threshold_ms);
            }
        }
    }

    /* 2. Run multi-instance detector to confirm whether deadlocks actually exist */
    res->detector_ran = true;
    bool has_deadlock = detector_run(&sm->pm, &sm->rm, &res->report);

    /* 3. Classify suspected deadlocks as CONFIRMED vs ORDINARY CONTENTION (False Alarm) */
    for (int k = 0; k < res->record_count; k++) {
        ProcessTimeoutRecord *rec = &res->records[k];
        if (rec->verdict != WAIT_SUSPECTED_DEADLOCK) continue;

        bool in_deadlock_set = false;
        if (has_deadlock) {
            for (int d = 0; d < res->report.deadlocked_count; d++) {
                if (res->report.deadlocked_pids[d] == rec->pid) {
                    in_deadlock_set = true;
                    break;
                }
            }
        }

        if (in_deadlock_set) {
            rec->verdict = WAIT_CONFIRMED_DEADLOCK;
            res->confirmed_deadlocks++;
            snprintf(rec->details, sizeof(rec->details), "Confirmed in circular wait set (Wait: %.1f ms)", rec->wait_time_ms);
        } else {
            rec->verdict = WAIT_ORDINARY_CONTENTION;
            res->false_alarms++;
            snprintf(rec->details, sizeof(rec->details), "False Alarm: Long wait/contention, NOT deadlocked");
        }
    }

    if (res->suspected_count > 0) {
        res->false_alarm_rate_pct = ((double)res->false_alarms / (double)res->suspected_count) * 100.0;
    } else {
        res->false_alarm_rate_pct = 0.0;
    }
}

void timeout_print_audit_report(const TimeoutAuditResult *res) {
    if (!res) return;

    printf("\n========================================================================================================\n");
    printf("                  TIMEOUT-BASED DEADLOCK DETECTION & FALSE-ALARM AUDIT                                  \n");
    printf("========================================================================================================\n");
    printf("Configured Threshold : %.1f ms | Blocked Processes : %d | Suspected: %d\n",
           res->threshold_ms, res->total_blocked, res->suspected_count);
    printf("Confirmed Deadlocks  : %d | False Alarms (Contention/Starvation) : %d\n",
           res->confirmed_deadlocks, res->false_alarms);
    printf("False Alarm Rate     : %5.1f%%\n\n", res->false_alarm_rate_pct);

    printf("+-----+----------------------+-------------+--------------------------+--------------------------------------+\n");
    printf("| PID | Process Name         | Wait Time   | Timeout Classification   | Scientific Diagnosis & Audit Details |\n");
    printf("+-----+----------------------+-------------+--------------------------+--------------------------------------+\n");

    if (res->record_count == 0) {
        printf("| %-100s |\n", "  [No blocked processes in system]");
    } else {
        for (int i = 0; i < res->record_count; i++) {
            const ProcessTimeoutRecord *rec = &res->records[i];
            const char *verdict_str = "NORMAL";
            if (rec->verdict == WAIT_CONFIRMED_DEADLOCK) verdict_str = "CONFIRMED DEADLOCK";
            else if (rec->verdict == WAIT_ORDINARY_CONTENTION) verdict_str = "FALSE ALARM (CONTENTION)";
            else if (rec->verdict == WAIT_SUSPECTED_DEADLOCK) verdict_str = "SUSPECTED DEADLOCK";

            printf("| %-3d | %-20s | %9.1f ms | %-24s | %-36.36s |\n",
                   rec->pid, rec->name, rec->wait_time_ms, verdict_str, rec->details);
        }
    }

    printf("+-----+----------------------+-------------+--------------------------+--------------------------------------+\n");
    printf("[TIMEOUT TRADE-OFF SUMMARY]\n");
    printf("  * Aggressive (Short) Threshold: Low detection latency, but HIGH false alarm rate on resource contention.\n");
    printf("  * Conservative (Long) Threshold: Low false alarms, but prolonged system latency before deadlock resolution.\n");
    printf("  * Verdict Principle: A timeout constitutes SUSPICION ONLY; graph/vector reduction is mandatory for confirmation!\n");
    printf("========================================================================================================\n\n");
}
