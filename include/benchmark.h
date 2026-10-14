#ifndef DEADLOCKDOCTOR_BENCHMARK_H
#define DEADLOCKDOCTOR_BENCHMARK_H

#include "common.h"
#include "manager.h"
#include <time.h>

typedef struct {
    int proc_idx;
    int res_idx;
    bool is_request;
    int units;
} WorkloadOp;

typedef struct {
    int num_processes;
    int num_resources;
    int num_operations;
    unsigned int seed;
    int *res_capacities;
    int **max_claims;
    WorkloadOp *ops;
} Workload;

typedef struct {
    int num_processes;
    int num_resources;
    DeadlockStrategy strategy;
    unsigned int seed;

    int total_requests;
    int granted_requests;
    int blocked_requests;
    int release_operations;
    int safety_checks;
    int deadlocks_detected;
    int recoveries_executed;

    double detection_time_ms;
    double recovery_time_ms;
    double total_runtime_ms;
    double resource_utilization_pct;
    bool invariants_verified;
} BenchmarkResult;

void workload_init(Workload *w);
bool workload_generate(Workload *w, int num_procs, int num_res, int num_ops, unsigned int seed);
void workload_cleanup(Workload *w);

void benchmark_run_single(int num_processes, int num_resources, int iterations,
                          DeadlockStrategy strategy, unsigned int seed, BenchmarkResult *res);

void benchmark_run_workload(const Workload *w, DeadlockStrategy strategy, BenchmarkResult *res);

void benchmark_compare_algorithms(int num_procs, int num_res, int num_ops, unsigned int seed);

void benchmark_run_stress_test(int num_procs, int num_res, int num_ops, unsigned int seed);

void benchmark_run_suite(void);

#endif /* DEADLOCKDOCTOR_BENCHMARK_H */
