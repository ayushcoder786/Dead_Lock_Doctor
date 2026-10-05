#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>

/*
 * ============================================================================
 * Milestone 12: Real POSIX Threads (pthread) Deadlock Demonstration
 * ============================================================================
 * Demonstrates Coffman's four conditions for deadlock using POSIX threads:
 *  1. Mutual Exclusion: R1 and R2 are non-shareable mutexes.
 *  2. Hold and Wait: T1 holds R1 while waiting for R2; T2 holds R2 while waiting for R1.
 *  3. No Preemption: Mutexes cannot be forcibly confiscated.
 *  4. Circular Wait: T1 -> T2 -> T1.
 *
 * Safe Observation Mechanism:
 *  Uses pthread_mutex_trylock() inside the threads rather than an unconditional
 *  blocking lock, allowing DeadlockDoctor to safely observe and report the
 *  circular wait condition without permanently hanging the terminal.
 *  Once reported, the mutexes are cleanly unlocked and threads are joined.
 * ============================================================================
 */

/* Mutexes representing physical/logical resources */
static pthread_mutex_t r1 = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t r2 = PTHREAD_MUTEX_INITIALIZER;

/* Coordination and observation flags */
static volatile int s_t1_acquired = 0;
static volatile int s_t2_acquired = 0;
static volatile int s_t1_waiting = 0;
static volatile int s_t2_waiting = 0;
static volatile int s_cleanup_and_exit = 0;

/*
 * Thread T1:
 *  1. Acquire R1
 *  2. Wait for T2 to acquire R2
 *  3. Attempt to acquire R2 (observed via pthread_mutex_trylock)
 *  4. Cleanly release R1 upon signal
 */
static void *thread_t1(void *arg) {
    (void)arg;

    /* Step 1: Acquire R1 */
    if (pthread_mutex_lock(&r1) != 0) {
        perror("T1: pthread_mutex_lock R1");
        return NULL;
    }
    printf("T1 acquired R1\n");
    fflush(stdout);
    s_t1_acquired = 1;

    /* Synchronize: Wait until T2 has acquired R2 */
    while (!s_t2_acquired && !s_cleanup_and_exit) {
        usleep(1000);
    }

    /* Step 2: Attempt to acquire R2 */
    printf("T1 waiting for R2\n");
    fflush(stdout);
    s_t1_waiting = 1;

    /*
     * Attempt to acquire R2 using pthread_mutex_trylock().
     * Since T2 currently holds R2, trylock will return EBUSY.
     * We loop checking with backoff until the deadlock is resolved
     * by DeadlockDoctor, ensuring the terminal NEVER permanently hangs.
     */
    int max_attempts = 1000; /* 5 second safety limit */
    while (!s_cleanup_and_exit && max_attempts-- > 0) {
        int rc = pthread_mutex_trylock(&r2);
        if (rc == 0) {
            /* Acquired if T2 released early */
            pthread_mutex_unlock(&r2);
            break;
        } else if (rc == EBUSY) {
            /* Expected: R2 is held by T2 (circular wait condition) */
        }
        usleep(5000);
    }

    /* Clean release of held mutex */
    pthread_mutex_unlock(&r1);
    return NULL;
}

/*
 * Thread T2:
 *  1. Wait for T1 to acquire R1 (ensuring strict sequence order)
 *  2. Acquire R2
 *  3. Wait for T1 to announce waiting for R2
 *  4. Attempt to acquire R1 (observed via pthread_mutex_trylock)
 *  5. Cleanly release R2 upon signal
 */
static void *thread_t2(void *arg) {
    (void)arg;

    /* Ensure T1 acquires R1 first to guarantee consistent display order */
    while (!s_t1_acquired && !s_cleanup_and_exit) {
        usleep(1000);
    }

    /* Step 1: Acquire R2 */
    if (pthread_mutex_lock(&r2) != 0) {
        perror("T2: pthread_mutex_lock R2");
        return NULL;
    }
    printf("T2 acquired R2\n");
    fflush(stdout);
    s_t2_acquired = 1;

    /* Ensure T1 has announced its wait for R2 */
    while (!s_t1_waiting && !s_cleanup_and_exit) {
        usleep(1000);
    }

    /* Step 2: Attempt to acquire R1 */
    printf("T2 waiting for R1\n");
    fflush(stdout);
    s_t2_waiting = 1;

    /*
     * Attempt to acquire R1 using pthread_mutex_trylock().
     * Since T1 holds R1, trylock returns EBUSY without blocking indefinitely.
     */
    int max_attempts = 1000; /* 5 second safety limit */
    while (!s_cleanup_and_exit && max_attempts-- > 0) {
        int rc = pthread_mutex_trylock(&r1);
        if (rc == 0) {
            /* Acquired if T1 released early */
            pthread_mutex_unlock(&r1);
            break;
        } else if (rc == EBUSY) {
            /* Expected: R1 is held by T1 (circular wait condition) */
        }
        usleep(5000);
    }

    /* Clean release of held mutex */
    pthread_mutex_unlock(&r2);
    return NULL;
}

/*
 * Runs the complete thread deadlock demonstration.
 * Returns 0 on success, non-zero on failure.
 */
int thread_deadlock_run(bool verbose) {
    pthread_t t1, t2;

    /* Reset flags */
    s_t1_acquired = 0;
    s_t2_acquired = 0;
    s_t1_waiting = 0;
    s_t2_waiting = 0;
    s_cleanup_and_exit = 0;

    /* Spawn Thread T1 */
    if (pthread_create(&t1, NULL, thread_t1, NULL) != 0) {
        perror("Failed to create Thread T1");
        return 1;
    }

    /* Spawn Thread T2 */
    if (pthread_create(&t2, NULL, thread_t2, NULL) != 0) {
        perror("Failed to create Thread T2");
        s_cleanup_and_exit = 1;
        pthread_join(t1, NULL);
        return 1;
    }

    /*
     * Main / Observer Thread:
     * Wait until both threads have acquired their first mutex and are
     * actively waiting for each other's held resource.
     */
    int timeout_ms = 4000;
    while ((!s_t1_waiting || !s_t2_waiting) && timeout_ms > 0) {
        usleep(1000);
        timeout_ms--;
    }

    if (timeout_ms <= 0) {
        fprintf(stderr, "Error: Timeout waiting for thread deadlock condition.\n");
        s_cleanup_and_exit = 1;
        pthread_join(t1, NULL);
        pthread_join(t2, NULL);
        return 1;
    }

    /* Small delay to allow any pending thread stdout to flush */
    usleep(15000);

    /*
     * DeadlockDoctor Detection Report
     */
    printf("\nDeadlockDoctor:\n");
    printf("DEADLOCK DETECTED\n\n");
    printf("Cycle:\n");
    printf("T1 -> T2 -> T1\n\n");
    fflush(stdout);

    /* Signal both threads to exit observation loops and release mutexes */
    s_cleanup_and_exit = 1;

    /* Join threads cleanly */
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    /* Destroy mutexes */
    pthread_mutex_destroy(&r1);
    pthread_mutex_destroy(&r2);

    printf("Cleanly released mutexes and terminated threads.\n");

    if (verbose) {
        printf("\n[Viva Reference: Coffman Deadlock Conditions]\n");
        printf(" 1. Mutual Exclusion  : R1 and R2 are non-shareable pthread_mutex_t locks.\n");
        printf(" 2. Hold and Wait     : T1 holds R1 and waits for R2; T2 holds R2 and waits for R1.\n");
        printf(" 3. No Preemption     : Neither thread can unilaterally revoke a locked mutex.\n");
        printf(" 4. Circular Wait     : T1 -> T2 -> T1 cycle detected in Wait-For dependencies.\n");
        printf(" Safe observation via pthread_mutex_trylock() prevents terminal hanging.\n");
    }

    return 0;
}

int main(int argc, char *argv[]) {
    bool verbose = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0 ||
            strcmp(argv[i], "--viva") == 0) {
            verbose = true;
        }
    }

    return thread_deadlock_run(verbose);
}
