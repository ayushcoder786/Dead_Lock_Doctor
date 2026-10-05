#include "process_demo.h"
#include "logger.h"
#include <unistd.h>
#include <errno.h>

int process_demo_worker_main(int argc, char *argv[]) {
    const char *name = (argc > 1) ? argv[1] : "Worker";
    int delay_mult = (argc > 2) ? atoi(argv[2]) : 1;
    if (delay_mult <= 0) delay_mult = 1;

    pid_t pid = getpid();
    pid_t ppid = getppid();

    printf("  [Child Exec: %s] PID: %d, Parent PID: %d -> Started execution via exec()\n",
           name, (int)pid, (int)ppid);
    fflush(stdout);

    /* Simulate discrete work steps */
    for (int step = 1; step <= 2; step++) {
        usleep(delay_mult * 40000); /* 40-80ms */
        printf("  [Child Exec: %s] PID: %d -> Performing work step %d/2...\n",
               name, (int)pid, step);
        fflush(stdout);
    }

    printf("  [Child Exec: %s] PID: %d -> Terminating successfully with exit code 0\n",
           name, (int)pid);
    fflush(stdout);

    return 0;
}

bool process_demo_run(void) {
    pid_t parent_pid = getpid();

    printf("\n======================================================================\n");
    printf("  CO-2 LINUX PROCESS MANAGEMENT DEMONSTRATION (fork, exec, waitpid)   \n");
    printf("======================================================================\n\n");

    printf("[Parent] DeadlockDoctor main process running: PID = %d\n\n", (int)parent_pid);

    printf("[Process Hierarchy]\n");
    printf("DeadlockDoctor (Parent PID: %d)\n", (int)parent_pid);
    printf("    |\n");
    printf("    +-- child process P1 (spawning via fork)\n");
    printf("    +-- child process P2 (spawning via fork)\n\n");

    logger_log(LOG_INFO, "Linux Process Demo: Starting demonstration from Parent PID=%d.", (int)parent_pid);

    /* Check available worker executable: prefer build/worker, else fallback to self-exec */
    const char *worker_bin = NULL;
    if (access("./build/worker", X_OK) == 0) {
        worker_bin = "./build/worker";
    } else if (access("./worker", X_OK) == 0) {
        worker_bin = "./worker";
    }

    /* 1. Fork Child Process P1 */
    printf("[1] Forking Child Process P1...\n");
    fflush(stdout);

    pid_t pid1 = fork();
    if (pid1 < 0) {
        fprintf(stderr, "[Parent Error] fork() failed for child P1 (errno=%d: %s)\n",
                errno, strerror(errno));
        return false;
    } else if (pid1 == 0) {
        /* Inside Child 1 */
        if (worker_bin) {
            execl(worker_bin, "worker", "P1", "1", NULL);
        } else {
            execl("/proc/self/exe", "deadlockdoctor", "--worker", "P1", "1", NULL);
        }
        /* If exec returns, an error occurred */
        fprintf(stderr, "[Child P1 Error] exec() failed (errno=%d: %s)\n", errno, strerror(errno));
        _exit(127);
    }

    printf("[Parent] fork() successful: Created child P1 with PID = %d\n\n", (int)pid1);
    logger_log(LOG_INFO, "Linux Process Demo: Created child P1 (PID=%d)", (int)pid1);
    fflush(stdout);

    /* Brief pause to stagger child creation */
    usleep(20000);

    /* 2. Fork Child Process P2 */
    printf("[2] Forking Child Process P2...\n");
    fflush(stdout);

    pid_t pid2 = fork();
    if (pid2 < 0) {
        fprintf(stderr, "[Parent Error] fork() failed for child P2 (errno=%d: %s)\n",
                errno, strerror(errno));
        return false;
    } else if (pid2 == 0) {
        /* Inside Child 2 */
        if (worker_bin) {
            execl(worker_bin, "worker", "P2", "2", NULL);
        } else {
            execl("/proc/self/exe", "deadlockdoctor", "--worker", "P2", "2", NULL);
        }
        /* If exec returns, an error occurred */
        fprintf(stderr, "[Child P2 Error] exec() failed (errno=%d: %s)\n", errno, strerror(errno));
        _exit(127);
    }

    printf("[Parent] fork() successful: Created child P2 with PID = %d\n\n", (int)pid2);
    logger_log(LOG_INFO, "Linux Process Demo: Created child P2 (PID=%d)", (int)pid2);
    fflush(stdout);

    /* 3. Reaping with waitpid() */
    printf("[3] Waiting for child processes using waitpid()...\n");
    fflush(stdout);

    int status1 = 0, status2 = 0;
    pid_t reaped1 = waitpid(pid1, &status1, 0);
    pid_t reaped2 = waitpid(pid2, &status2, 0);

    bool ok1 = false, ok2 = false;

    if (reaped1 == pid1 && WIFEXITED(status1)) {
        int code = WEXITSTATUS(status1);
        printf("[Parent] waitpid(%d) reaped Child P1: Exited normally (status %d).\n",
               (int)pid1, code);
        logger_log(LOG_INFO, "Linux Process Demo: waitpid(%d) reaped P1 (exit code %d)",
                   (int)pid1, code);
        ok1 = (code == 0);
    } else {
        printf("[Parent Error] waitpid(%d) failed or Child P1 exited abnormally.\n", (int)pid1);
    }

    if (reaped2 == pid2 && WIFEXITED(status2)) {
        int code = WEXITSTATUS(status2);
        printf("[Parent] waitpid(%d) reaped Child P2: Exited normally (status %d).\n",
               (int)pid2, code);
        logger_log(LOG_INFO, "Linux Process Demo: waitpid(%d) reaped P2 (exit code %d)",
                   (int)pid2, code);
        ok2 = (code == 0);
    } else {
        printf("[Parent Error] waitpid(%d) failed or Child P2 exited abnormally.\n", (int)pid2);
    }

    printf("\n[4] Process Demonstration Completed Successfully.\n");
    printf("======================================================================\n\n");
    fflush(stdout);

    return (ok1 && ok2);
}
