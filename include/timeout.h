#ifndef DEADLOCKDOCTOR_TIMEOUT_H
#define DEADLOCKDOCTOR_TIMEOUT_H

#include "common.h"
#include "manager.h"
#include "detector.h"

typedef enum {
    WAIT_NORMAL,             /* Waiting duration within configured threshold */
    WAIT_SUSPECTED_DEADLOCK, /* Exceeded timeout threshold; suspected deadlock */
    WAIT_CONFIRMED_DEADLOCK, /* Exceeded timeout AND verified by multi-instance detector */
    WAIT_ORDINARY_CONTENTION /* Exceeded timeout BUT detector verified NO deadlock (False Alarm / Contention) */
} TimeoutVerdict;

typedef struct {
    int pid;
    char name[MAX_NAME_LEN];
    double wait_time_ms;
    TimeoutVerdict verdict;
    char details[128];
} ProcessTimeoutRecord;

typedef struct {
    double threshold_ms;
    int total_blocked;
    int suspected_count;
    int confirmed_deadlocks;
    int false_alarms;
    double false_alarm_rate_pct;
    ProcessTimeoutRecord records[MAX_PROCESSES];
    int record_count;
    bool detector_ran;
    DeadlockReport report;
} TimeoutAuditResult;

void timeout_init_audit(TimeoutAuditResult *res, double threshold_ms);
void timeout_cleanup_audit(TimeoutAuditResult *res);
void timeout_run_audit(SystemManager *sm, double threshold_ms, TimeoutAuditResult *res);
void timeout_print_audit_report(const TimeoutAuditResult *res);
void timeout_inject_wait_time(Process *proc, double additional_ms);

#endif /* DEADLOCKDOCTOR_TIMEOUT_H */
