#ifndef DEADLOCKDOCTOR_DETECTOR_H
#define DEADLOCKDOCTOR_DETECTOR_H

#include "common.h"
#include "process.h"
#include "resource.h"
#include "graph.h"

typedef struct {
    bool deadlock_detected;
    WaitForGraph wfg;
    int *cycle;                    /* dynamically allocated cycle array */
    int cycle_len;
    int cycle_capacity;
    int *deadlocked_pids;          /* dynamically allocated deadlocked pids */
    char **deadlocked_names;       /* dynamically allocated deadlocked names */
    int deadlocked_count;
    int deadlocked_capacity;
} DeadlockReport;

void deadlock_report_init(DeadlockReport *report);
void deadlock_report_cleanup(DeadlockReport *report);
bool detector_run(const ProcessManager *pm, const ResourceManager *rm, DeadlockReport *report);
void detector_print_report(const DeadlockReport *report);

#endif /* DEADLOCKDOCTOR_DETECTOR_H */

