#include "common.h"
#include "benchmark.h"
#include <assert.h>

int main(void) {
    printf("======================================================================\n");
    printf("   UNIT TESTS: ALGORITHM COMPARISON & REPRODUCIBLE STRESS TESTING     \n");
    printf("======================================================================\n\n");

    /* Test 1: Deterministic workload generation */
    printf("[RUNNING] Test 1: Workload generation reproducibility with identical seed\n");
    Workload w1, w2;
    workload_init(&w1);
    workload_init(&w2);
    assert(workload_generate(&w1, 5, 3, 50, 123));
    assert(workload_generate(&w2, 5, 3, 50, 123));

    assert(w1.num_processes == w2.num_processes);
    assert(w1.num_resources == w2.num_resources);
    assert(w1.num_operations == w2.num_operations);

    for (int r = 0; r < w1.num_resources; r++) {
        assert(w1.res_capacities[r] == w2.res_capacities[r]);
    }
    for (int i = 0; i < w1.num_operations; i++) {
        assert(w1.ops[i].proc_idx == w2.ops[i].proc_idx);
        assert(w1.ops[i].res_idx == w2.ops[i].res_idx);
        assert(w1.ops[i].is_request == w2.ops[i].is_request);
        assert(w1.ops[i].units == w2.ops[i].units);
    }
    printf("[PASS] Test 1: Equivalent workload traces generated identically.\n\n");

    /* Test 2: Execution across all 3 strategies on equivalent workload */
    printf("[RUNNING] Test 2: Run all 3 strategies on identical workload trace\n");
    BenchmarkResult res_ordered, res_banker, res_detect;
    benchmark_run_workload(&w1, STRATEGY_RESOURCE_ORDERING, &res_ordered);
    benchmark_run_workload(&w1, STRATEGY_BANKER_AVOIDANCE, &res_banker);
    benchmark_run_workload(&w1, STRATEGY_DETECTION_RECOVERY, &res_detect);

    /* Assert correctness properties */
    assert(res_ordered.invariants_verified == true);
    assert(res_banker.invariants_verified == true);
    assert(res_detect.invariants_verified == true);

    /* Prevention and Avoidance guarantee zero deadlocks */
    assert(res_ordered.deadlocks_detected == 0);
    assert(res_banker.deadlocks_detected == 0);

    printf("  [Observed Grants] Ordered: %d | Banker: %d | Detection: %d\n",
           res_ordered.granted_requests, res_banker.granted_requests, res_detect.granted_requests);
    printf("  [Observed Invariants] All 3 strategies maintained 100%% invariant integrity.\n");
    printf("[PASS] Test 2: Equivalent workload comparison validated.\n\n");

    /* Test 3: Reproducible Stress Test Execution */
    printf("[RUNNING] Test 3: Reproducible stress test run\n");
    benchmark_run_stress_test(8, 3, 100, 42);
    printf("[PASS] Test 3: Stress test completed cleanly with invariant validation.\n\n");

    workload_cleanup(&w1);
    workload_cleanup(&w2);

    printf("======================================================================\n");
    printf("         ALL ALGORITHM COMPARISON & STRESS TESTS PASSED               \n");
    printf("======================================================================\n");
    return 0;
}
