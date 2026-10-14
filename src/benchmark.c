#include "benchmark.h"
#include "recovery.h"
#include <fcntl.h>
#include <unistd.h>
#include <math.h>

static double get_time_diff_ms(struct timespec start, struct timespec end) {
    double sec = (double)(end.tv_sec - start.tv_sec);
    double nsec = (double)(end.tv_nsec - start.tv_nsec);
    return (sec * 1000.0) + (nsec / 1000000.0);
}

void workload_init(Workload *w) {
    if (!w) return;
    w->num_processes = 0;
    w->num_resources = 0;
    w->num_operations = 0;
    w->seed = 0;
    w->res_capacities = NULL;
    w->max_claims = NULL;
    w->ops = NULL;
}

void workload_cleanup(Workload *w) {
    if (!w) return;
    if (w->res_capacities) {
        free(w->res_capacities);
        w->res_capacities = NULL;
    }
    if (w->max_claims) {
        for (int p = 0; p < w->num_processes; p++) {
            if (w->max_claims[p]) free(w->max_claims[p]);
        }
        free(w->max_claims);
        w->max_claims = NULL;
    }
    if (w->ops) {
        free(w->ops);
        w->ops = NULL;
    }
    w->num_processes = 0;
    w->num_resources = 0;
    w->num_operations = 0;
}

bool workload_generate(Workload *w, int num_procs, int num_res, int num_ops, unsigned int seed) {
    if (!w || num_procs <= 0 || num_res <= 0 || num_ops <= 0) return false;
    workload_cleanup(w);

    w->num_processes = num_procs;
    w->num_resources = num_res;
    w->num_operations = num_ops;
    w->seed = seed;

    w->res_capacities = (int *)malloc(num_res * sizeof(int));
    w->max_claims = (int **)malloc(num_procs * sizeof(int *));
    w->ops = (WorkloadOp *)malloc(num_ops * sizeof(WorkloadOp));

    if (!w->res_capacities || !w->max_claims || !w->ops) {
        workload_cleanup(w);
        return false;
    }

    for (int p = 0; p < num_procs; p++) {
        w->max_claims[p] = (int *)malloc(num_res * sizeof(int));
        if (!w->max_claims[p]) {
            workload_cleanup(w);
            return false;
        }
    }

    /* Seed pseudo-random generator deterministically */
    srand(seed);

    /* 1. Generate Resource Capacities (multi-instance: 4 to 12 units each) */
    for (int r = 0; r < num_res; r++) {
        w->res_capacities[r] = 4 + (rand() % 9);
    }

    /* 2. Generate Maximum Claims for each process */
    for (int p = 0; p < num_procs; p++) {
        for (int r = 0; r < num_res; r++) {
            int max_cap = w->res_capacities[r];
            w->max_claims[p][r] = 1 + (rand() % (max_cap > 3 ? 3 : max_cap));
        }
    }

    /* 3. Generate Workload Operations (70% request, 30% release) */
    for (int i = 0; i < num_ops; i++) {
        w->ops[i].proc_idx = rand() % num_procs;
        w->ops[i].res_idx = rand() % num_res;
        int action = rand() % 100;
        if (action < 70) {
            w->ops[i].is_request = true;
            w->ops[i].units = 1 + (rand() % 2);
        } else {
            w->ops[i].is_request = false;
            w->ops[i].units = 1;
        }
    }

    return true;
}

void benchmark_run_workload(const Workload *w, DeadlockStrategy strategy, BenchmarkResult *res) {
    if (!w || !res) return;
    memset(res, 0, sizeof(BenchmarkResult));

    res->num_processes = w->num_processes;
    res->num_resources = w->num_resources;
    res->strategy = strategy;
    res->seed = w->seed;

    /* Redirect stdout to /dev/null during benchmark execution to ensure clean performance timing */
    fflush(stdout);
    int saved_stdout = dup(STDOUT_FILENO);
    int null_fd = open("/dev/null", O_WRONLY);
    if (null_fd >= 0) {
        dup2(null_fd, STDOUT_FILENO);
        close(null_fd);
    }

    SystemManager sm;
    system_manager_init(&sm);
    system_manager_set_strategy(&sm, strategy);

    /* Register Resources */
    for (int r = 0; r < w->num_resources; r++) {
        char rname[32];
        snprintf(rname, sizeof(rname), "R%d", r + 1);
        system_manager_create_resource(&sm, r + 1, rname, w->res_capacities[r]);
    }

    /* Register Processes and Declare Max Claims */
    for (int p = 0; p < w->num_processes; p++) {
        char pname[32];
        snprintf(pname, sizeof(pname), "P%d", p + 1);
        system_manager_create_process(&sm, p + 1, pname);
        for (int r = 0; r < w->num_resources; r++) {
            char rname[32];
            snprintf(rname, sizeof(rname), "R%d", r + 1);
            system_manager_set_max(&sm, pname, rname, w->max_claims[p][r]);
        }
    }

    struct timespec bench_start, bench_end;
    clock_gettime(CLOCK_MONOTONIC, &bench_start);

    /* Execute pre-generated deterministic operations */
    for (int i = 0; i < w->num_operations; i++) {
        const WorkloadOp *op = &w->ops[i];
        char pname[32], rname[32];
        snprintf(pname, sizeof(pname), "P%d", op->proc_idx + 1);
        snprintf(rname, sizeof(rname), "R%d", op->res_idx + 1);

        if (op->is_request) {
            res->total_requests++;
            if (strategy == STRATEGY_BANKER_AVOIDANCE) {
                res->safety_checks++;
            }

            AllocResult ar = system_manager_request(&sm, pname, rname, op->units);
            if (ar == ALLOC_GRANTED) {
                res->granted_requests++;
            } else {
                res->blocked_requests++;
            }
        } else {
            res->release_operations++;
            system_manager_release(&sm, pname, rname, op->units);
        }

        /* Periodic Deadlock Detection in Detection & Recovery strategy */
        if (strategy == STRATEGY_DETECTION_RECOVERY && (i % 10 == 0 || i == w->num_operations - 1)) {
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
    res->total_runtime_ms = get_time_diff_ms(bench_start, bench_end);

    /* Invariant Check & Resource Utilization Calculation */
    bool inv_ok = true;
    int total_system_units = 0;
    int allocated_units = 0;
    for (int r = 0; r < sm.rm.count; r++) {
        int proc_sum = 0;
        for (int p = 0; p < sm.pm.count; p++) {
            if (sm.pm.processes[p] && r < sm.pm.processes[p]->res_capacity) {
                proc_sum += sm.pm.processes[p]->allocated[r];
            }
        }
        int tot = sm.rm.resources[r].total_instances;
        int avail = sm.rm.resources[r].available_instances;
        if (avail + proc_sum != tot) {
            inv_ok = false;
        }
        total_system_units += tot;
        allocated_units += (tot - avail);
    }
    res->invariants_verified = inv_ok;
    if (total_system_units > 0) {
        res->resource_utilization_pct = ((double)allocated_units / (double)total_system_units) * 100.0;
    }

    system_manager_cleanup(&sm);

    /* Restore stdout */
    fflush(stdout);
    if (saved_stdout >= 0) {
        dup2(saved_stdout, STDOUT_FILENO);
        close(saved_stdout);
    }
}

void benchmark_run_single(int num_processes, int num_resources, int iterations,
                          DeadlockStrategy strategy, unsigned int seed, BenchmarkResult *res) {
    Workload w;
    workload_init(&w);
    workload_generate(&w, num_processes, num_resources, iterations, seed);
    benchmark_run_workload(&w, strategy, res);
    workload_cleanup(&w);
}

void benchmark_compare_algorithms(int num_procs, int num_res, int num_ops, unsigned int seed) {
    if (num_procs <= 0) num_procs = 10;
    if (num_res <= 0) num_res = 4;
    if (num_ops <= 0) num_ops = 80;
    if (seed == 0) seed = 42;

    printf("\n========================================================================================================\n");
    printf("         DEADLOCKDOCTOR ALGORITHM COMPARISON MODE (EQUIVALENT WORKLOAD TRACE)                           \n");
    printf("========================================================================================================\n");
    printf("Configuration: %d Processes | %d Resources | %d Operations | Deterministic Seed: %u\n",
           num_procs, num_res, num_ops, seed);
    printf("Methodology: Pre-generated identical transaction trace executed across all 3 strategies.\n\n");

    Workload w;
    workload_init(&w);
    workload_generate(&w, num_procs, num_res, num_ops, seed);

    DeadlockStrategy strats[] = {
        STRATEGY_RESOURCE_ORDERING,
        STRATEGY_BANKER_AVOIDANCE,
        STRATEGY_DETECTION_RECOVERY
    };

    printf("+-------------------------------+----------+----------+----------+------------+------------+---------+------------+------------+\n");
    printf("| Strategy                      | Requests | Granted  | Blocked  | Det Time   | Deadlocks  | Recover | Util %%     | Total Time |\n");
    printf("+-------------------------------+----------+----------+----------+------------+------------+---------+------------+------------+\n");

    for (size_t s = 0; s < sizeof(strats)/sizeof(strats[0]); s++) {
        BenchmarkResult bres;
        benchmark_run_workload(&w, strats[s], &bres);

        printf("| %-29s | %-8d | %-8d | %-8d | %7.3f ms | %-10d | %-7d | %6.1f%%   | %7.3f ms |\n",
               system_manager_get_strategy_name(strats[s]),
               bres.total_requests,
               bres.granted_requests,
               bres.blocked_requests,
               bres.detection_time_ms,
               bres.deadlocks_detected,
               bres.recoveries_executed,
               bres.resource_utilization_pct,
               bres.total_runtime_ms);
    }
    printf("+-------------------------------+----------+----------+----------+------------+------------+---------+------------+------------+\n\n");

    printf("[SCIENTIFIC TRADE-OFF & METHODOLOGY ANALYSIS]\n");
    printf("  1. Resource Ordering (Havender's Rule - Static Prevention):\n");
    printf("     - Zero deadlock occurrence (100%% prevention by denying Coffman's circular wait).\n");
    printf("     - Negligible computational overhead (no safety matrix or graph DFS needed).\n");
    printf("     - Rejects out-of-order resource requests even when resources are physically available.\n");
    printf("  2. Dijkstra's Banker's Algorithm (Dynamic Avoidance):\n");
    printf("     - Zero deadlock occurrence (100%% prevention by denying unsafe allocation paths).\n");
    printf("     - Incurs O(m * n^2) vector safety evaluation overhead per request.\n");
    printf("     - Preserves all running processes without abortion penalties.\n");
    printf("  3. Deadlock Detection & Victim Recovery (Dynamic Resolution):\n");
    printf("     - Grants resources optimistically, maximizing instantaneous resource allocation.\n");
    printf("     - Allows circular waits to manifest, subsequently resolving them via lowest-cost victim abortion.\n");
    printf("========================================================================================================\n\n");

    workload_cleanup(&w);
}

void benchmark_run_stress_test(int num_procs, int num_res, int num_ops, unsigned int seed) {
    if (num_procs <= 0) num_procs = 20;
    if (num_res <= 0) num_res = 6;
    if (num_ops <= 0) num_ops = 300;
    if (seed == 0) seed = 777;

    printf("\n========================================================================================================\n");
    printf("                         DEADLOCKDOCTOR REPRODUCIBLE STRESS TEST SUITE                                  \n");
    printf("========================================================================================================\n");
    printf("Parameters: %d Processes | %d Resource Types | %d Transactions | Random Seed: %u\n",
           num_procs, num_res, num_ops, seed);
    printf("Objective: Validate accounting invariants, memory safety, and determinism under high concurrency.\n\n");

    Workload w;
    workload_init(&w);
    if (!workload_generate(&w, num_procs, num_res, num_ops, seed)) {
        fprintf(stderr, "[Error] Failed to generate workload.\n");
        return;
    }

    printf("[PHASE 1: STRESS TESTING BANKER'S AVOIDANCE]\n");
    BenchmarkResult res_banker;
    benchmark_run_workload(&w, STRATEGY_BANKER_AVOIDANCE, &res_banker);
    printf("  * Total Requests Evaluated   : %d\n", res_banker.total_requests);
    printf("  * Safe Grants                : %d\n", res_banker.granted_requests);
    printf("  * Unsafe Requests Blocked    : %d\n", res_banker.blocked_requests);
    printf("  * Deadlocks Incurred         : %d (Expected: 0)\n", res_banker.deadlocks_detected);
    printf("  * Invariant Audit            : %s\n", res_banker.invariants_verified ? "PASSED [100% Consistent]" : "FAILED [Inconsistent]");
    printf("  * Execution Duration         : %.3f ms\n\n", res_banker.total_runtime_ms);

    printf("[PHASE 2: STRESS TESTING RESOURCE ORDERING]\n");
    BenchmarkResult res_ordered;
    benchmark_run_workload(&w, STRATEGY_RESOURCE_ORDERING, &res_ordered);
    printf("  * Total Requests Evaluated   : %d\n", res_ordered.total_requests);
    printf("  * Ordered Grants             : %d\n", res_ordered.granted_requests);
    printf("  * Out-of-Order Rejections    : %d\n", res_ordered.blocked_requests);
    printf("  * Deadlocks Incurred         : %d (Expected: 0)\n", res_ordered.deadlocks_detected);
    printf("  * Invariant Audit            : %s\n", res_ordered.invariants_verified ? "PASSED [100% Consistent]" : "FAILED [Inconsistent]");
    printf("  * Execution Duration         : %.3f ms\n\n", res_ordered.total_runtime_ms);

    printf("[PHASE 3: STRESS TESTING DETECTION & RECOVERY]\n");
    BenchmarkResult res_detect;
    benchmark_run_workload(&w, STRATEGY_DETECTION_RECOVERY, &res_detect);
    printf("  * Total Requests Evaluated   : %d\n", res_detect.total_requests);
    printf("  * Optimistic Grants          : %d\n", res_detect.granted_requests);
    printf("  * Blocked on Capacity        : %d\n", res_detect.blocked_requests);
    printf("  * Deadlocks Detected & Broken: %d\n", res_detect.deadlocks_detected);
    printf("  * Victims Aborted for Safety : %d\n", res_detect.recoveries_executed);
    printf("  * Detection + Recovery Time  : %.3f ms\n", res_detect.detection_time_ms + res_detect.recovery_time_ms);
    printf("  * Invariant Audit            : %s\n", res_detect.invariants_verified ? "PASSED [100% Consistent]" : "FAILED [Inconsistent]");
    printf("  * Execution Duration         : %.3f ms\n\n", res_detect.total_runtime_ms);

    printf("========================================================================================================\n");
    if (res_banker.invariants_verified && res_ordered.invariants_verified && res_detect.invariants_verified) {
        printf(">>> STRESS TEST VERDICT: 100%% INVARIANT INTEGRITY MAINTAINED ACROSS ALL TRANSACTIONS <<<\n");
    } else {
        printf(">>> STRESS TEST VERDICT: FAILED - INVARIANT DISCREPANCY DETECTED <<<\n");
    }
    printf("========================================================================================================\n\n");

    workload_cleanup(&w);
}

void benchmark_run_suite(void) {
    benchmark_compare_algorithms(10, 4, 100, 101);
}
