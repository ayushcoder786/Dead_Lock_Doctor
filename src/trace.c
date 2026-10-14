#include "trace.h"
#include <sys/wait.h>

bool trace_is_strace_available(void) {
    if (access("/usr/bin/strace", X_OK) == 0 || access("/bin/strace", X_OK) == 0) {
        return true;
    }
    int ret = system("which strace > /dev/null 2>&1");
    return (ret == 0);
}

void trace_run_demonstration(void) {
    printf("\n========================================================================================================\n");
    printf("                    DEADLOCKDOCTOR SYSTEM-CALL TRACING DEMONSTRATION (CO-1)                              \n");
    printf("========================================================================================================\n");

    bool has_strace = trace_is_strace_available();
    printf("Host strace Detection: %s\n\n",
           has_strace ? "[AVAILABLE] (Live kernel tracing enabled)" : "[NOT INSTALLED] (Fallback to documented reference)");

    if (has_strace) {
        printf("[RUNNING LIVE STRACE ON SCENARIO REPLAY]\n");
        printf("Command: strace -c -e trace=openat,read,write,close ./deadlockdoctor scenarios/safe.txt\n\n");
        fflush(stdout);
        int ret = system("strace -c -e trace=openat,read,write,close ./deadlockdoctor scenarios/safe.txt > /dev/null");
        (void)ret;

        printf("\n[RUNNING LIVE STRACE ON THREAD SYNCHRONIZATION WORKLOAD]\n");
        printf("Command: strace -c -e trace=futex,clone,rt_sigaction ./build/thread_deadlock\n\n");
        fflush(stdout);
        ret = system("strace -c -e trace=futex,clone,rt_sigaction ./build/thread_deadlock > /dev/null");
        (void)ret;
    } else {
        printf("[SYSTEM-CALL PROBE NOTICE]\n");
        printf("  'strace' is not installed in the current environment.\n");
        printf("  To install strace on Ubuntu/Debian, execute:\n");
        printf("      sudo apt-get update && sudo apt-get install -y strace\n\n");
    }

    printf("[DOCUMENTED DEADLOCKDOCTOR SYSTEM-CALL ARCHITECTURE TRACE]\n");
    printf("+----------------------+-------------------+-----------------------------------+------------------------------------+\n");
    printf("| Subsystem            | Linux Syscall     | DeadlockDoctor Source Location    | Operating System Role              |\n");
    printf("+----------------------+-------------------+-----------------------------------+------------------------------------+\n");
    printf("| File I/O             | openat()          | src/scenario.c, src/logger.c      | Open scenario files and audit log  |\n");
    printf("| File I/O             | read()            | src/scenario.c (PosixFileReader)  | Buffered line reading of commands  |\n");
    printf("| File I/O             | write()           | src/logger.c                      | Persistent append-only audit log   |\n");
    printf("| File I/O             | close()           | src/scenario.c, src/logger.c      | Release file descriptors cleanly   |\n");
    printf("| Process Management   | clone() / clone3()| src/process_demo.c (fork)         | Create child worker processes      |\n");
    printf("| Process Management   | execve()          | src/process_demo.c (execvp)       | Replace child address space        |\n");
    printf("| Process Management   | wait4()           | src/process_demo.c (waitpid)      | Harvest child exit status          |\n");
    printf("| Inter-Process Comm   | pipe2() / pipe()  | src/ipc.c (pipe_demo_run)         | Unidirectional IPC channel         |\n");
    printf("| Inter-Process Comm   | mknodat() / mknod | src/ipc.c (fifo_demo_run)         | Create POSIX named pipe (FIFO)     |\n");
    printf("| Signal Handling      | rt_sigaction()    | src/ipc.c, src/manager.c          | Register SIGINT, SIGUSR1 handlers  |\n");
    printf("| Signal Delivery      | kill() / tgkill() | src/ipc.c (signal_demo_run)       | Send asynchronous POSIX signals    |\n");
    printf("| Thread Synchronization| futex()           | demo/thread_deadlock.c            | Fast userspace mutex lock / wait   |\n");
    printf("| High-Res Timing      | clock_gettime()   | src/process.c, src/benchmark.c    | Monotonic process wait & latency   |\n");
    printf("| Terminal Control     | ioctl() / select()| src/monitor.c, src/manager.c      | Non-blocking keyboard poll for 'q' |\n");
    printf("+----------------------+-------------------+-----------------------------------+------------------------------------+\n\n");

    printf("[MANUAL VIVA VERIFICATION COMMANDS]\n");
    printf("  1. Trace file I/O operations:\n");
    printf("     strace -e trace=openat,read,write,close ./deadlockdoctor scenarios/deadlock.txt\n");
    printf("  2. Trace process creation and reaping:\n");
    printf("     strace -f -e trace=clone,wait4,execve ./deadlockdoctor --process-demo\n");
    printf("  3. Trace IPC pipes and signals:\n");
    printf("     strace -f -e trace=pipe2,kill,rt_sigaction ./deadlockdoctor --ipc-demo\n");
    printf("  4. Trace pthread mutex contention and futex calls:\n");
    printf("     strace -f -e trace=futex ./build/thread_deadlock\n");
    printf("========================================================================================================\n\n");
}
