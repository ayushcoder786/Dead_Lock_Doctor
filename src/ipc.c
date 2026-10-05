#include "ipc.h"
#include "manager.h"
#include "logger.h"
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

static volatile sig_atomic_t s_sigusr1_received = 0;
static volatile sig_atomic_t s_sigalrm_received = 0;
static volatile sig_atomic_t s_sigterm_received = 0;

static void safe_sig_handler(int sig) {
    if (sig == SIGUSR1) {
        s_sigusr1_received = 1;
    } else if (sig == SIGALRM) {
        s_sigalrm_received = 1;
    } else if (sig == SIGTERM) {
        s_sigterm_received = 1;
    }
}

static void ensure_test_entities(SystemManager *sm) {
    if (!sm) return;
    if (!process_find_by_id_or_name(&sm->pm, "P1")) {
        system_manager_create_process(sm, 1, "P1");
    }
    if (!process_find_by_id_or_name(&sm->pm, "P2")) {
        system_manager_create_process(sm, 2, "P2");
    }
    if (!resource_find_by_id_or_name(&sm->rm, "R1")) {
        system_manager_create_resource(sm, 1, "R1", 3);
    }
    if (!resource_find_by_id_or_name(&sm->rm, "R2")) {
        system_manager_create_resource(sm, 2, "R2", 3);
    }
}

bool ipc_demo_pipe(SystemManager *sm) {
    printf("\n----------------------------------------------------------------------\n");
    printf("  1. ANONYMOUS PIPE DEMONSTRATION (pipe)\n");
    printf("----------------------------------------------------------------------\n");

    ensure_test_entities(sm);

    int pipefd[2];
    if (pipe(pipefd) < 0) {
        fprintf(stderr, "[IPC Error] pipe() failed (errno=%d: %s)\n", errno, strerror(errno));
        return false;
    }

    pid_t parent_pid = getpid();
    pid_t pid = fork();

    if (pid < 0) {
        fprintf(stderr, "[IPC Error] fork() failed (errno=%d: %s)\n", errno, strerror(errno));
        close(pipefd[0]);
        close(pipefd[1]);
        return false;
    } else if (pid == 0) {
        /* Child process: Worker */
        close(pipefd[0]); /* Close unused read end */

        const char *request_msg = "REQUEST P1 R1 1\n";
        printf("  [Worker Child PID: %d]\n", (int)getpid());
        printf("  Worker:\n");
        printf("  REQUEST P1 R1 1\n");
        printf("          |\n");
        printf("          v\n");
        printf("        PIPE\n");
        printf("          |\n");
        printf("          v\n");
        printf("  DeadlockDoctor Manager (PID: %d)\n\n", (int)parent_pid);
        fflush(stdout);

        ssize_t written = write(pipefd[1], request_msg, strlen(request_msg));
        close(pipefd[1]);
        _exit((written > 0) ? 0 : 1);
    }

    /* Parent process: Manager */
    close(pipefd[1]); /* Close unused write end */

    char buffer[256];
    memset(buffer, 0, sizeof(buffer));
    ssize_t bytes_read = read(pipefd[0], buffer, sizeof(buffer) - 1);
    close(pipefd[0]);

    int status = 0;
    waitpid(pid, &status, 0);

    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        while (bytes_read > 0 && (buffer[bytes_read - 1] == '\n' || buffer[bytes_read - 1] == '\r')) {
            buffer[--bytes_read] = '\0';
        }

        printf("  [Manager] Received message via anonymous pipe: \"%s\"\n", buffer);

        char cmd[32], proc[MAX_NAME_LEN], res[MAX_NAME_LEN];
        int units = 0;
        if (sscanf(buffer, "%31s %63s %63s %d", cmd, proc, res, &units) == 4 &&
            strcasecmp(cmd, "REQUEST") == 0) {
            printf("  [Manager] Executing resource request for %s (%d units of %s)...\n",
                   proc, units, res);
            AllocResult res_alloc = system_manager_request(sm, proc, res, units);
            logger_log(LOG_INFO, "IPC Pipe: Processed \"%s\" -> %s", buffer, alloc_result_to_string(res_alloc));
            return (res_alloc != ALLOC_INVALID);
        }
    }

    return false;
}

bool ipc_demo_fifo(SystemManager *sm) {
    printf("\n----------------------------------------------------------------------\n");
    printf("  2. NAMED PIPE / FIFO DEMONSTRATION (mkfifo)\n");
    printf("----------------------------------------------------------------------\n");

    ensure_test_entities(sm);

    const char *fifo_path = "/tmp/deadlockdoctor.fifo";
    unlink(fifo_path);

    if (mkfifo(fifo_path, 0666) < 0) {
        fprintf(stderr, "[IPC Error] mkfifo() failed for '%s' (errno=%d: %s)\n",
                fifo_path, errno, strerror(errno));
        return false;
    }

    pid_t parent_pid = getpid();
    pid_t pid = fork();

    if (pid < 0) {
        fprintf(stderr, "[IPC Error] fork() failed (errno=%d: %s)\n", errno, strerror(errno));
        unlink(fifo_path);
        return false;
    } else if (pid == 0) {
        /* Child process: Worker */
        int fd_write = open(fifo_path, O_WRONLY);
        if (fd_write < 0) {
            fprintf(stderr, "[Child Error] open('%s') failed (errno=%d: %s)\n",
                    fifo_path, errno, strerror(errno));
            _exit(1);
        }

        const char *request_msg = "REQUEST P2 R2 1\n";
        printf("  [Worker Child PID: %d]\n", (int)getpid());
        printf("  Worker:\n");
        printf("  REQUEST P2 R2 1\n");
        printf("          |\n");
        printf("          v\n");
        printf("     NAMED FIFO (%s)\n", fifo_path);
        printf("          |\n");
        printf("          v\n");
        printf("  DeadlockDoctor Manager (PID: %d)\n\n", (int)parent_pid);
        fflush(stdout);

        ssize_t written = write(fd_write, request_msg, strlen(request_msg));
        close(fd_write);
        _exit((written > 0) ? 0 : 1);
    }

    /* Parent process: Manager */
    int fd_read = open(fifo_path, O_RDONLY);
    if (fd_read < 0) {
        fprintf(stderr, "[Manager Error] open('%s') for read failed (errno=%d: %s)\n",
                fifo_path, errno, strerror(errno));
        waitpid(pid, NULL, 0);
        unlink(fifo_path);
        return false;
    }

    char buffer[256];
    memset(buffer, 0, sizeof(buffer));
    ssize_t bytes_read = read(fd_read, buffer, sizeof(buffer) - 1);
    close(fd_read);

    int status = 0;
    waitpid(pid, &status, 0);
    unlink(fifo_path);

    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        while (bytes_read > 0 && (buffer[bytes_read - 1] == '\n' || buffer[bytes_read - 1] == '\r')) {
            buffer[--bytes_read] = '\0';
        }

        printf("  [Manager] Received message via named pipe (FIFO): \"%s\"\n", buffer);

        char cmd[32], proc[MAX_NAME_LEN], res[MAX_NAME_LEN];
        int units = 0;
        if (sscanf(buffer, "%31s %63s %63s %d", cmd, proc, res, &units) == 4 &&
            strcasecmp(cmd, "REQUEST") == 0) {
            printf("  [Manager] Executing resource request for %s (%d units of %s)...\n",
                   proc, units, res);
            AllocResult res_alloc = system_manager_request(sm, proc, res, units);
            logger_log(LOG_INFO, "IPC FIFO: Processed \"%s\" -> %s", buffer, alloc_result_to_string(res_alloc));
            return (res_alloc != ALLOC_INVALID);
        }
    }

    return false;
}

bool ipc_demo_signals(SystemManager *sm) {
    printf("\n----------------------------------------------------------------------\n");
    printf("  3. ASYNCHRONOUS SIGNAL HANDLING DEMONSTRATION (sigaction)\n");
    printf("----------------------------------------------------------------------\n");

    s_sigusr1_received = 0;
    s_sigalrm_received = 0;
    s_sigterm_received = 0;

    struct sigaction sa, old_usr1, old_alrm, old_term;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = safe_sig_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGUSR1, &sa, &old_usr1) < 0 ||
        sigaction(SIGALRM, &sa, &old_alrm) < 0 ||
        sigaction(SIGTERM, &sa, &old_term) < 0) {
        fprintf(stderr, "[IPC Error] sigaction() installation failed (errno=%d: %s)\n",
                errno, strerror(errno));
        return false;
    }

    /* 3A: SIGUSR1 - worker notification */
    printf("  [Step 3A] Testing SIGUSR1: Worker notification event\n");
    pid_t parent_pid = getpid();
    pid_t child_pid = fork();
    if (child_pid == 0) {
        usleep(20000); /* 20ms */
        printf("    [Worker Child PID: %d] Raising SIGUSR1 to Manager (PID: %d)...\n",
               (int)getpid(), (int)parent_pid);
        kill(parent_pid, SIGUSR1);
        _exit(0);
    }

    int timeout = 100;
    while (!s_sigusr1_received && timeout-- > 0) {
        usleep(5000);
    }
    waitpid(child_pid, NULL, 0);

    if (s_sigusr1_received) {
        printf("  -> [Manager] SIGUSR1 captured safely: Worker process signaled event notification!\n");
        logger_log(LOG_INFO, "IPC Signal: SIGUSR1 handled safely (worker event notification).");
    } else {
        fprintf(stderr, "  -> [Manager Error] SIGUSR1 not captured in time.\n");
    }

    /* 3B: SIGALRM - timeout / periodic check */
    printf("\n  [Step 3B] Testing SIGALRM: Timeout / periodic deadlock detector check\n");
    ualarm(25000, 0); /* 25ms timer */
    timeout = 100;
    while (!s_sigalrm_received && timeout-- > 0) {
        usleep(5000);
    }
    ualarm(0, 0); /* cancel any pending alarm */

    if (s_sigalrm_received) {
        printf("  -> [Manager] SIGALRM captured safely: Timer expired -> Triggering periodic deadlock check...\n");
        if (sm) {
            system_manager_run_detector(sm);
        }
        logger_log(LOG_INFO, "IPC Signal: SIGALRM handled safely (periodic deadlock check triggered).");
    } else {
        fprintf(stderr, "  -> [Manager Error] SIGALRM not captured.\n");
    }

    /* 3C: SIGTERM - safe termination request */
    printf("\n  [Step 3C] Testing SIGTERM: Safe termination request\n");
    kill(getpid(), SIGTERM);
    timeout = 100;
    while (!s_sigterm_received && timeout-- > 0) {
        usleep(5000);
    }

    if (s_sigterm_received) {
        printf("  -> [Manager] SIGTERM captured safely: Termination request received -> Ready for graceful cleanup.\n");
        logger_log(LOG_INFO, "IPC Signal: SIGTERM handled safely (clean shutdown request).");
    } else {
        fprintf(stderr, "  -> [Manager Error] SIGTERM not captured.\n");
    }

    /* Restore original handlers */
    sigaction(SIGUSR1, &old_usr1, NULL);
    sigaction(SIGALRM, &old_alrm, NULL);
    sigaction(SIGTERM, &old_term, NULL);

    return (s_sigusr1_received && s_sigalrm_received && s_sigterm_received);
}

bool ipc_demo_run_all(SystemManager *sm) {
    printf("\n======================================================================\n");
    printf("       CO-3 LINUX IPC & ASYNCHRONOUS NOTIFICATION DEMONSTRATION       \n");
    printf("======================================================================\n");

    bool local_mgr = false;
    SystemManager local_sm;
    if (!sm) {
        system_manager_init(&local_sm);
        sm = &local_sm;
        local_mgr = true;
    }

    bool ok1 = ipc_demo_pipe(sm);
    bool ok2 = ipc_demo_fifo(sm);
    bool ok3 = ipc_demo_signals(sm);

    printf("\n======================================================================\n");
    if (ok1 && ok2 && ok3) {
        printf(">>> ALL IPC MECHANISMS (PIPE, FIFO, SIGNALS) COMPLETED SUCCESSFULLY <<<\n");
    } else {
        printf(">>> IPC DEMONSTRATION FINISHED WITH ONE OR MORE WARNINGS <<<\n");
    }
    printf("======================================================================\n\n");

    if (local_mgr) {
        system_manager_cleanup(&local_sm);
    }

    return (ok1 && ok2 && ok3);
}
