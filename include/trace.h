#ifndef DEADLOCKDOCTOR_TRACE_H
#define DEADLOCKDOCTOR_TRACE_H

#include "common.h"

/**
 * Checks whether the 'strace' utility is installed and executable in PATH.
 */
bool trace_is_strace_available(void);

/**
 * Executes the Course Outcome 1 (CO-1) system-call tracing demonstration:
 * - If strace is available, invokes strace on DeadlockDoctor workloads and displays
 *   actual intercepted kernel system calls.
 * - If strace is unavailable, reports the status cleanly and presents the documented
 *   POSIX system-call architecture trace for viva reference.
 */
void trace_run_demonstration(void);

#endif /* DEADLOCKDOCTOR_TRACE_H */
