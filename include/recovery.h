#ifndef DEADLOCKDOCTOR_RECOVERY_H
#define DEADLOCKDOCTOR_RECOVERY_H

#include "common.h"
#include "process.h"

struct SystemManager;

typedef struct {
    int pid;
    char name[MAX_NAME_LEN];
    int held_resources;
    int pending_requests;
    int priority;
    int cost;
} ProcessRecoveryCost;

/**
 * Calculates a simple, transparent recovery cost for a candidate process:
 * Cost = (held_units * 2) + (pending_units * 1) + priority
 */
int recovery_calculate_cost(const Process *proc, int num_resources);

/**
 * Executes Milestone 13 Deadlock Recovery:
 *  1. Displays the deadlocked set
 *  2. Calculates recovery cost for each candidate process
 *  3. Selects a victim with the lowest recovery cost
 *  4. Aborts the selected process in the simulator
 *  5. Releases all resources held by the victim
 *  6. Updates Available resource vector
 *  7. Removes the victim from the wait-for graph
 *  8. Re-runs deadlock detection
 *  9. Runs Banker's safety check
 * 10. Reports whether the system returned to a safe state
 *
 * Returns true if recovery succeeded in resolving deadlock, false otherwise.
 */
bool recovery_run(struct SystemManager *sm);

/**
 * Aborts a specific process to recover from deadlock, releasing all held resources.
 * Logs a recovery event to logs/deadlockdoctor.log.
 */
bool recovery_abort_process(struct SystemManager *sm, int pid);

/**
 * Automatically detects deadlocks and iteratively aborts deadlocked processes
 * until the circular wait is completely resolved.
 * Logs recovery events to logs/deadlockdoctor.log.
 */
bool recovery_auto_recover(struct SystemManager *sm);

#endif /* DEADLOCKDOCTOR_RECOVERY_H */
