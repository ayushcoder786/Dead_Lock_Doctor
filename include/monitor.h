#ifndef DEADLOCKDOCTOR_MONITOR_H
#define DEADLOCKDOCTOR_MONITOR_H

#include "common.h"
#include "process.h"
#include "resource.h"

struct SystemManager;

/**
 * Feature 1: Live Process Monitor
 * Displays a formatted table with:
 *  - PID
 *  - Process Name
 *  - State (READY, RUNNING, BLOCKED, COMPLETED, ABORTED)
 *  - Priority
 *  - Held Resources (e.g. R1:2 R2:1 or None)
 *  - Pending Requests (e.g. R2:1 or None)
 *  - Waiting Duration (derived from CLOCK_MONOTONIC state transitions)
 */
void monitor_show_process_table(const ProcessManager *pm, const ResourceManager *rm);

/**
 * Feature 2: Resource Utilization Dashboard
 * Displays a formatted table with:
 *  - Resource ID and Name
 *  - Total units
 *  - Allocated units
 *  - Available units
 *  - Utilization percentage: (allocated / total) * 100.0%
 *  - Visual utilization gauge: [==========          ] 50.0%
 *  - Current holding processes: e.g. P1:2, P2:1
 *
 * Also calculates:
 *  - System-wide overall utilization percentage
 *  - Strict runtime invariant assertion:
 *    For every resource: available + sum(allocated across all processes) == total
 */
void monitor_show_resource_dashboard(const ResourceManager *rm, const ProcessManager *pm);

/**
 * Combined executive view of both Process Monitor and Resource Dashboard.
 */
void monitor_show_combined(const struct SystemManager *sm);

/**
 * Runs a live refreshing monitor view in the terminal.
 * If max_ticks > 0, refreshes max_ticks times then returns.
 * If max_ticks <= 0, runs until 'q' or Ctrl-C.
 */
void monitor_run_live(const struct SystemManager *sm, int interval_ms, int max_ticks);

#endif /* DEADLOCKDOCTOR_MONITOR_H */
