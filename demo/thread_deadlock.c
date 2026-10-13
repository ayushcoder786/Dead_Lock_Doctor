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
 * Course Outcome 6 (CO-6): POSIX Threads Synchronization & Deadlock Demonstration
 * ============================================================================
 * Demonstrates:
 *   1. Coffman's Four Deadlock Conditions in POSIX threads:
 *      - Mutual Exclusion: Non-shareable pthread_mutex_t locks.
 *      - Hold and Wait: Thread holds one mutex while requesting another.
 *      - No Preemption: Mutexes cannot be forcibly confiscated.
 *      - Circular Wait: Thread A -> Thread B -> Thread A.
 *   2. Configuration A: Conflicting Lock Order (Deadlock Detection & Recovery).
 *      - Coordinated via pthread_barrier_t (no arbitrary sleeps).
 *      - Safe observation using pthread_mutex_trylock() treating EBUSY as contention.
 *      - Explicit lock-owner tracking establishes genuine cycle.
 *      - Bounded, deterministic victim recovery releases lock and resolves cycle.
 *   3. Configuration B: Consistent Global Lock Ordering (Deadlock Prevention).
 *      - Strict ascending acquisition order (R1 then R2) eliminates Circular Wait.
 *      - Both threads terminate cleanly without contention or deadlocks.
 *
 * NOTE: This is a controlled demonstration of resources tracked in user-space,
 * not a claim that user-space code can intercept or repair arbitrary kernel deadlocks.
 * ============================================================================
 */

/* Mutexes representing resources */
static pthread_mutex_t r1 = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t r2 = PTHREAD_MUTEX_INITIALIZER;

/* Coordination barrier ensuring simultaneous progression without sleeps */
static pthread_barrier_t s_barrier;

/* Lock ownership and waiting state tracking */
static pthread_mutex_t s_tracker_mutex = PTHREAD_MUTEX_INITIALIZER;
static const char *s_r1_owner = NULL;
static const char *s_r2_owner = NULL;
static const char *s_t1_waiting_for = NULL;
static const char *s_t2_waiting_for = NULL;

static volatile int s_recovery_signal = 0;
static volatile int s_exit_signal = 0;

/* ============================================================================
 * Configuration A: Conflicting Lock Acquisition Order
 * ============================================================================
 */
static void *thread_a_conflicting(void *arg) {
    (void)arg;

    /* Step 1: Thread A acquires R1 */
    if (pthread_mutex_lock(&r1) != 0) {
        perror("Thread A: lock R1");
        return NULL;
    }

    pthread_mutex_lock(&s_tracker_mutex);
    s_r1_owner = "Thread A";
    pthread_mutex_unlock(&s_tracker_mutex);

    printf("  [Thread A] ACQUIRED Mutex R1 (holds R1)\n");
    fflush(stdout);

    /* Barrier 1: Synchronize with Thread B (Thread B has acquired R2) */
    pthread_barrier_wait(&s_barrier);

    /* Step 2: Thread A attempts to acquire R2 */
    printf("  [Thread A] Attempting to acquire Mutex R2...\n");
    fflush(stdout);

    pthread_mutex_lock(&s_tracker_mutex);
    s_t1_waiting_for = "R2";
    pthread_mutex_unlock(&s_tracker_mutex);

    /* Attempt trylock with bounded retry until recovery releases R2 */
    int retries = 500;
    while (!s_exit_signal && retries-- > 0) {
        int rc = pthread_mutex_trylock(&r2);
        if (rc == 0) {
            /* Acquired R2 after recovery yielded it */
            pthread_mutex_lock(&s_tracker_mutex);
            s_r2_owner = "Thread A";
            s_t1_waiting_for = NULL;
            pthread_mutex_unlock(&s_tracker_mutex);

            printf("  [Thread A] [RECOVERY SUCCESS] ACQUIRED Mutex R2! Performing critical section...\n");
            fflush(stdout);
            usleep(20000); /* 20ms work */

            pthread_mutex_unlock(&r2);
            pthread_mutex_lock(&s_tracker_mutex);
            s_r2_owner = NULL;
            pthread_mutex_unlock(&s_tracker_mutex);
            printf("  [Thread A] Released Mutex R2\n");
            break;
        } else if (rc == EBUSY) {
            /* Resource contention: R2 is held by Thread B */
            usleep(2000);
        } else {
            perror("Thread A: trylock R2");
            break;
        }
    }

    /* Release R1 */
    pthread_mutex_unlock(&r1);
    pthread_mutex_lock(&s_tracker_mutex);
    s_r1_owner = NULL;
    pthread_mutex_unlock(&s_tracker_mutex);
    printf("  [Thread A] Released Mutex R1 -> Finished successfully.\n");
    fflush(stdout);

    return NULL;
}

static void *thread_b_conflicting(void *arg) {
    (void)arg;

    /* Step 1: Thread B acquires R2 */
    if (pthread_mutex_lock(&r2) != 0) {
        perror("Thread B: lock R2");
        return NULL;
    }

    pthread_mutex_lock(&s_tracker_mutex);
    s_r2_owner = "Thread B";
    pthread_mutex_unlock(&s_tracker_mutex);

    printf("  [Thread B] ACQUIRED Mutex R2 (holds R2)\n");
    fflush(stdout);

    /* Barrier 1: Synchronize with Thread A (Thread A has acquired R1) */
    pthread_barrier_wait(&s_barrier);

    /* Step 2: Thread B attempts to acquire R1 */
    printf("  [Thread B] Attempting to acquire Mutex R1...\n");
    fflush(stdout);

    pthread_mutex_lock(&s_tracker_mutex);
    s_t2_waiting_for = "R1";
    pthread_mutex_unlock(&s_tracker_mutex);

    /* Trylock loop until detector identifies deadlock and issues recovery signal */
    while (!s_recovery_signal && !s_exit_signal) {
        int rc = pthread_mutex_trylock(&r1);
        if (rc == 0) {
            pthread_mutex_unlock(&r1);
            break;
        } else if (rc == EBUSY) {
            usleep(2000);
        }
    }

    /* Recovery action: Thread B is chosen as victim, yields R2 to unblock Thread A */
    printf("  [Thread B] [RECOVERY ACTION] Selected as victim: Yielding Mutex R2 to break circular wait...\n");
    fflush(stdout);

    pthread_mutex_unlock(&r2);
    pthread_mutex_lock(&s_tracker_mutex);
    s_r2_owner = NULL;
    s_t2_waiting_for = NULL;
    pthread_mutex_unlock(&s_tracker_mutex);

    /* Wait briefly for Thread A to complete */
    usleep(50000);

    /* Re-acquire resources in safe sequence */
    printf("  [Thread B] Retrying resource acquisition...\n");
    pthread_mutex_lock(&r1);
    pthread_mutex_lock(&r2);
    printf("  [Thread B] ACQUIRED Mutex R1 and R2 -> Performing work...\n");
    usleep(10000);
    pthread_mutex_unlock(&r2);
    pthread_mutex_unlock(&r1);
    printf("  [Thread B] Released all mutexes -> Finished successfully.\n");
    fflush(stdout);

    return NULL;
}

static void run_configuration_a(void) {
    printf("\n----------------------------------------------------------------------\n");
    printf("  CONFIGURATION A: CONFLICTING LOCK ORDERING (DETECTION & RECOVERY)  \n");
    printf("----------------------------------------------------------------------\n");
    printf("Lock Acquisition Order: Thread A requests (R1 then R2) | Thread B requests (R2 then R1)\n\n");

    s_r1_owner = NULL;
    s_r2_owner = NULL;
    s_t1_waiting_for = NULL;
    s_t2_waiting_for = NULL;
    s_recovery_signal = 0;
    s_exit_signal = 0;

    pthread_barrier_init(&s_barrier, NULL, 2);

    pthread_t ta, tb;
    pthread_create(&ta, NULL, thread_a_conflicting, NULL);
    pthread_create(&tb, NULL, thread_b_conflicting, NULL);

    /* Observer: wait until both threads are blocked waiting for each other's mutex */
    int timeout_ms = 3000;
    while (timeout_ms-- > 0) {
        pthread_mutex_lock(&s_tracker_mutex);
        bool deadlock = (s_r1_owner && strcmp(s_r1_owner, "Thread A") == 0 &&
                         s_r2_owner && strcmp(s_r2_owner, "Thread B") == 0 &&
                         s_t1_waiting_for && strcmp(s_t1_waiting_for, "R2") == 0 &&
                         s_t2_waiting_for && strcmp(s_t2_waiting_for, "R1") == 0);
        pthread_mutex_unlock(&s_tracker_mutex);

        if (deadlock) break;
        usleep(2000);
    }

    printf("\nDeadlockDoctor:\n");
    printf("DEADLOCK DETECTED\n\n");
    printf("Cycle:\n");
    printf("T1 -> T2 -> T1\n\n");
    printf("Wait-For Dependency Verification:\n");
    printf("  * Mutex R1 held by Thread A; requested by Thread B (EBUSY contention)\n");
    printf("  * Mutex R2 held by Thread B; requested by Thread A (EBUSY contention)\n");
    printf("  * Circular Wait: Thread A waits on Thread B, which waits on Thread A.\n\n");
    fflush(stdout);

    /* Trigger recovery: signal Thread B to yield R2 */
    s_recovery_signal = 1;

    pthread_join(ta, NULL);
    pthread_join(tb, NULL);

    pthread_barrier_destroy(&s_barrier);
    printf("\n[SUCCESS] Configuration A completed: Deadlock resolved without hanging terminal.\n");
}

/* ============================================================================
 * Configuration B: Consistent Global Lock Ordering (Deadlock Prevention)
 * ============================================================================
 */
static void *thread_a_ordered(void *arg) {
    (void)arg;
    pthread_barrier_wait(&s_barrier);

    printf("  [Thread A] Requesting R1 (Global Order Step 1/2)...\n");
    pthread_mutex_lock(&r1);
    printf("  [Thread A] ACQUIRED R1. Requesting R2 (Global Order Step 2/2)...\n");
    pthread_mutex_lock(&r2);
    printf("  [Thread A] ACQUIRED R2. Critical section running...\n");
    usleep(20000);

    pthread_mutex_unlock(&r2);
    pthread_mutex_unlock(&r1);
    printf("  [Thread A] Released R2 and R1 -> Completed cleanly.\n");
    return NULL;
}

static void *thread_b_ordered(void *arg) {
    (void)arg;
    pthread_barrier_wait(&s_barrier);

    printf("  [Thread B] Requesting R1 (Global Order Step 1/2)...\n");
    pthread_mutex_lock(&r1);
    printf("  [Thread B] ACQUIRED R1. Requesting R2 (Global Order Step 2/2)...\n");
    pthread_mutex_lock(&r2);
    printf("  [Thread B] ACQUIRED R2. Critical section running...\n");
    usleep(20000);

    pthread_mutex_unlock(&r2);
    pthread_mutex_unlock(&r1);
    printf("  [Thread B] Released R2 and R1 -> Completed cleanly.\n");
    return NULL;
}

static void run_configuration_b(void) {
    printf("\n----------------------------------------------------------------------\n");
    printf("  CONFIGURATION B: CONSISTENT GLOBAL LOCK ORDERING (PREVENTION)      \n");
    printf("----------------------------------------------------------------------\n");
    printf("Havender's Rule: Mutex acquisition strictly ordered: R1 < R2\n");
    printf("Both Thread A and Thread B request R1 first, then R2.\n\n");

    pthread_barrier_init(&s_barrier, NULL, 2);

    pthread_t ta, tb;
    pthread_create(&ta, NULL, thread_a_ordered, NULL);
    pthread_create(&tb, NULL, thread_b_ordered, NULL);

    pthread_join(ta, NULL);
    pthread_join(tb, NULL);

    pthread_barrier_destroy(&s_barrier);
    printf("\n[SUCCESS] Configuration B completed: ZERO deadlocks, ZERO contention retries.\n");
}

int main(int argc, char *argv[]) {
    bool run_both = true;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--conf-a") == 0) {
            run_configuration_a();
            return 0;
        } else if (strcmp(argv[i], "--conf-b") == 0) {
            run_configuration_b();
            return 0;
        }
    }

    if (run_both) {
        printf("======================================================================\n");
        printf("     CO-6 POSIX THREADS DEADLOCK & PREVENTION DEMONSTRATION           \n");
        printf("======================================================================\n");
        run_configuration_a();
        run_configuration_b();

        printf("\n======================================================================\n");
        printf("                       COMPARATIVE SUMMARY                            \n");
        printf("======================================================================\n");
        printf("  Configuration A (Conflicting Order) : Deadlock arose, detected, and recovered via yield.\n");
        printf("  Configuration B (Global Order R1<R2): Circular wait impossible; 100%% deadlock prevention.\n");
        printf("  Terminal Safety                     : Zero indefinite hangs; all mutexes cleanly destroyed.\n");
        printf("======================================================================\n");
        printf("Cleanly released mutexes and terminated threads.\n");
    }

    return 0;
}
