#ifndef DEADLOCKDOCTOR_BENCHMARK_H
#define DEADLOCKDOCTOR_BENCHMARK_H

#include "common.h"
#include "manager.h"
#include <time.h>

typedef struct {
    int num_processes;
    int num_resources;
    DeadlockStrategy strategy;
    unsigned int seed;

    int total_requests;
    int granted_requests;
    int blocked_requests;
    int safety_checks;
    int deadlocks_detected;
    int recoveries_executed;

    double detection_time_ms;
    double recovery_time_ms;
    double total_runtime_ms;
    double resource_utilization_pct;
} BenchmarkResult;

void benchmark_run_single(int num_processes, int num_resources, int iterations,
                          DeadlockStrategy strategy, unsigned int seed, BenchmarkResult *res);

void benchmark_run_suite(void);

#endif /* DEADLOCKDOCTOR_BENCHMARK_H */
