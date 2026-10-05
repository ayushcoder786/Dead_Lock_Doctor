#ifndef DEADLOCKDOCTOR_IPC_H
#define DEADLOCKDOCTOR_IPC_H

#include "common.h"
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

/* Forward declaration of SystemManager */
typedef struct SystemManager SystemManager;

/**
 * Demonstrates an anonymous pipe created via pipe().
 * A child worker process sends a resource request through the pipe
 * to the DeadlockDoctor manager.
 */
bool ipc_demo_pipe(SystemManager *sm);

/**
 * Demonstrates a named pipe (FIFO) created via mkfifo().
 * A child process writes a resource request to the FIFO, which
 * the manager reads and processes.
 */
bool ipc_demo_fifo(SystemManager *sm);

/**
 * Demonstrates signal handling using sigaction() and volatile sig_atomic_t flags.
 * Demonstrates:
 *   - SIGUSR1: Worker notification event
 *   - SIGALRM: Timeout / periodic deadlock check
 *   - SIGTERM: Termination request
 */
bool ipc_demo_signals(SystemManager *sm);

/**
 * Runs the full Course Outcome 3 (CO-3) Linux IPC demonstration:
 * 1. Anonymous Pipe (pipe)
 * 2. Named Pipe (mkfifo)
 * 3. Signal Handling (sigaction)
 */
bool ipc_demo_run_all(SystemManager *sm);

#endif /* DEADLOCKDOCTOR_IPC_H */
