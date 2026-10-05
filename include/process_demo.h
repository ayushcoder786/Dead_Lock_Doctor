#ifndef DEADLOCKDOCTOR_PROCESS_DEMO_H
#define DEADLOCKDOCTOR_PROCESS_DEMO_H

#include "common.h"
#include <sys/types.h>
#include <sys/wait.h>

/**
 * Executes the Course Outcome 2 (CO-2) Linux process demonstration.
 * Demonstrates:
 *   - fork(): process creation (Parent PID, Child PID)
 *   - exec(): process image replacement with worker tasks
 *   - waitpid(): parent reaping child termination and exit status
 * Returns true if both child processes were created and exited cleanly with code 0.
 */
bool process_demo_run(void);

/**
 * Entry point for built-in worker task execution (via self-exec).
 */
int process_demo_worker_main(int argc, char *argv[]);

#endif /* DEADLOCKDOCTOR_PROCESS_DEMO_H */
