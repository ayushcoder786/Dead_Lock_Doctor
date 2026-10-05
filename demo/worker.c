#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    const char *name = (argc > 1) ? argv[1] : "Worker";
    int delay_multiplier = (argc > 2) ? atoi(argv[2]) : 1;
    if (delay_multiplier <= 0) delay_multiplier = 1;

    pid_t pid = getpid();
    pid_t ppid = getppid();

    printf("  [Child Exec: %s] PID: %d, Parent PID: %d -> Started execution via exec()\n",
           name, (int)pid, (int)ppid);
    fflush(stdout);

    /* Simulate brief discrete work steps */
    for (int step = 1; step <= 2; step++) {
        usleep(delay_multiplier * 40000); /* 40-80ms */
        printf("  [Child Exec: %s] PID: %d -> Performing work step %d/2...\n",
               name, (int)pid, step);
        fflush(stdout);
    }

    printf("  [Child Exec: %s] PID: %d -> Terminating successfully with exit code 0\n",
           name, (int)pid);
    fflush(stdout);

    return 0;
}
