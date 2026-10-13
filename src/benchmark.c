#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include "benchmark.h"
#include "recovery.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static double get_time_diff_ms(struct timespec start, struct timespec end) {
    double sec = (double)(end.tv_sec - start.tv_sec);
    double nsec = (double)(end.tv_nsec - start.tv_nsec);
    return (sec * 1000.0) + (nsec / 1000000.0);
}

void benchmark_run_single(int num_processes, int num_resources, int iterations,
                          DeadlockStrategy strategy, unsigned int seed, BenchmarkResult *res) {
    if (!res) return;
    memset(res, 0, sizeof(BenchmarkResult));

    /* POSIX stdout redirection to /dev/null during entire benchmark run to prevent terminal clutter */
    fflush(stdout);
    int saved_stdout = dup(STDOUT_FILENO);
    int null_fd = open("/dev/null", O_WRONLY);
    if (null_fd >= 0) {
        dup2(null_fd, STDOUT_FILENO);
        close(null_fd);
    }

    res->num_processes = num_processes;
    res->num_resources = num_resources;
    res->strategy = strategy;
    res->seed = seed;

    srand(seed);

    SystemManager sm;
    system_manager_init(&sm);
    system_manager_set_strategy(&sm, strategy);

    /* 1. Register Resources */
    for (int r = 0; r < num_resources; r++) {
        char rname[32];
        snprintf(rname, sizeof(rname), "R%d", r + 1);
        int total_inst = 6 + (rand() % 8); /* 6 to 13 instances */
        system_manager_create_resource(&sm, r + 1, rname, total_inst);
    }

    /* 2. Register Processes and declared maximums */
    for (int p = 0; p < num_processes; p++) {
        char pname[32];
        snprintf(pname, sizeof(pname), "P%d", p + 1);
        system_manager_create_process(&sm, p + 1, pname);
        for (int r = 0; r < num_resources; r++) {
            char rname[32];
            snprintf(rname, sizeof(rname), "R%d", r + 1);
            int max_claim = 1 + (rand() % 4);
            system_manager_set_max(&sm, pname, rname, max_claim);
        }
    }

    struct timespec bench_start, bench_end;
    clock_gettime(CLOCK_MONOTONIC, &bench_start);

    /* 3. Run workload iterations */
    for (int it = 0; it < iterations; it++) {
        int p_idx = rand() % num_processes;
        int r_idx = rand() % num_resources;
        char pname[32], rname[32];
        snprintf(pname, sizeof(pname), "P%d", p_idx + 1);
        snprintf(rname, sizeof(rname), "R%d", r_idx + 1);

        int action = rand() % 100;
        if (action < 70) {
            /* 70% chance: Request resource units */
            int units = 1 + (rand() % 2);
            res->total_requests++;
            if (strategy == STRATEGY_BANKER_AVOIDANCE) {
                res->safety_checks++;
            }

            AllocResult ar = system_manager_request(&sm, pname, rname, units);
            if (ar == ALLOC_GRANTED) {
                res->granted_requests++;
            } else {
                res->blocked_requests++;
            }
        } else {
            /* 30% chance: Release resource units */
            int units = 1;
            system_manager_release(&sm, pname, rname, units);
        }

        /* Periodic Deadlock Detection check in Detection & Recovery strategy */
        if (strategy == STRATEGY_DETECTION_RECOVERY && (it % 10 == 0 || it == iterations - 1)) {
            struct timespec det_start, det_end;
            clock_gettime(CLOCK_MONOTONIC, &det_start);

            DeadlockReport report;
            deadlock_report_init(&report);
            bool has_deadlock = detector_run(&sm.pm, &sm.rm, &report);
            clock_gettime(CLOCK_MONOTONIC, &det_end);
            res->detection_time_ms += get_time_diff_ms(det_start, det_end);

            if (has_deadlock) {
                res->deadlocks_detected++;

                struct timespec rec_start, rec_end;
                clock_gettime(CLOCK_MONOTONIC, &rec_start);
                recovery_run(&sm);
                clock_gettime(CLOCK_MONOTONIC, &rec_end);

                res->recoveries_executed++;
                res->recovery_time_ms += get_time_diff_ms(rec_start, rec_end);
            }

            deadlock_report_cleanup(&report);
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &bench_end);

    /* Restore stdout */
    fflush(stdout);
    if (saved_stdout >= 0) {
        dup2(saved_stdout, STDOUT_FILENO);
        close(saved_stdout);
    }
    res->total_runtime_ms = get_time_diff_ms(bench_start, bench_end);

    /* 4. Calculate Resource Utilization */
    int total_system_units = 0;
    int allocated_units = 0;
    for (int r = 0; r < sm.rm.count; r++) {
        total_system_units += sm.rm.resources[r].total_instances;
        allocated_units += (sm.rm.resources[r].total_instances - sm.rm.resources[r].available_instances);
    }
    if (total_system_units > 0) {
        res->resource_utilization_pct = ((double)allocated_units / (double)total_system_units) * 100.0;
    }

    system_manager_cleanup(&sm);
}

void benchmark_run_suite(void) {
    printf("\n========================================================================================================\n");
    printf("                     DEADLOCKDOCTOR EMPIRICAL PERFORMANCE BENCHMARK SUITE                               \n");
    printf("========================================================================================================\n");
    printf("Monotonic Clock: clock_gettime(CLOCK_MONOTONIC) | Deterministic Pseudo-Random Workload Generation\n\n");

    struct {
        const char *name;
        int procs;
        int res;
        int iters;
        unsigned int seed;
    } workloads[] = {
        {"Small Workload ", 5, 3, 40, 101},
        {"Medium Workload", 20, 5, 120, 202},
        {"Large Workload ", 50, 10, 250, 303}
    };

    DeadlockStrategy strats[] = {
        STRATEGY_BANKER_AVOIDANCE,
        STRATEGY_RESOURCE_ORDERING,
        STRATEGY_DETECTION_RECOVERY
    };

    printf("+-----------------+---------------------+----------+----------+----------+------------+------------+---------+------------+\n");
    printf("| Workload        | Strategy            | Requests | Granted  | Blocked  | Det Time   | Recoveries | Util %%  | Total Time |\n");
    printf("+-----------------+---------------------+----------+----------+----------+------------+------------+---------+------------+\n");

    for (size_t w = 0; w < sizeof(workloads)/sizeof(workloads[0]); w++) {
        for (size_t s = 0; s < sizeof(strats)/sizeof(strats[0]); s++) {
            BenchmarkResult bres;
            benchmark_run_single(workloads[w].procs, workloads[w].res, workloads[w].iters,
                                 strats[s], workloads[w].seed, &bres);

            printf("| %-15s | %-19s | %-8d | %-8d | %-8d | %7.3f ms | %-10d | %6.1f%% | %7.3f ms |\n",
                   workloads[w].name,
                   system_manager_get_strategy_name(strats[s]),
                   bres.total_requests,
                   bres.granted_requests,
                   bres.blocked_requests,
                   bres.detection_time_ms,
                   bres.recoveries_executed,
                   bres.resource_utilization_pct,
                   bres.total_runtime_ms);
        }
        if (w < sizeof(workloads)/sizeof(workloads[0]) - 1) {
            printf("+-----------------+---------------------+----------+----------+----------+------------+------------+---------+------------+\n");
        }
    }
    printf("+-----------------+---------------------+----------+----------+----------+------------+------------+---------+------------+\n\n");

    printf("[BENCHMARK FINDINGS & TRADE-OFF SUMMARY]\n");
    printf("  1. Banker's Avoidance (Safety Matrix Check):\n");
    printf("     - Ensures 100%% freedom from deadlocks without process abortion.\n");
    printf("     - Checks O(m * n^2) vector safety on every single request, introducing computational overhead on high concurrency.\n");
    printf("  2. Global Resource Ordering (Prevention):\n");
    printf("     - Breaks Coffman's circular wait condition statically via ascending resource indices (R1 < R2 ...).\n");
    printf("     - Near-zero runtime overhead; completely eliminates deadlocks without runtime safety checks or abortions.\n");
    printf("     - May increase request rejections / reduce flexibility if processes require out-of-order resources.\n");
    printf("  3. Deadlock Detection & Recovery:\n");
    printf("     - Permits maximum initial concurrency and high resource utilization.\n");
    printf("     - Incurs periodic O(m * n^2) detection passes and recovery victim abortion costs when cycles arise.\n");
    printf("========================================================================================================\n\n");
}
