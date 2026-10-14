#include "common.h"
#include "manager.h"
#include "monitor.h"
#include <assert.h>

int main(void) {
    printf("======================================================================\n");
    printf("         UNIT TESTS: LIVE PROCESS MONITOR & RESOURCE DASHBOARD        \n");
    printf("======================================================================\n\n");

    SystemManager sm;
    system_manager_init(&sm);

    /* Test 1: Resource creation and initial dashboard calculation */
    printf("[RUNNING] Test 1: Initialize multi-instance resources & check 0%% utilization\n");
    Resource *r1 = system_manager_create_resource(&sm, 1, "R1", 4);
    Resource *r2 = system_manager_create_resource(&sm, 2, "R2", 2);
    assert(r1 != NULL);
    assert(r2 != NULL);

    monitor_show_resource_dashboard(&sm.rm, &sm.pm);
    assert(r1->total_instances == 4);
    assert(r1->available_instances == 4);
    printf("[PASS] Test 1: Initial resource inventory verified (Total=4/2, Avail=4/2, Alloc=0).\n\n");

    /* Test 2: Process creation and process monitor table */
    printf("[RUNNING] Test 2: Create processes and verify initial state\n");
    Process *p1 = system_manager_create_process(&sm, 1, "P1");
    Process *p2 = system_manager_create_process(&sm, 2, "P2");
    assert(p1 != NULL);
    assert(p2 != NULL);
    assert(p1->state == PROCESS_STATE_READY);
    assert(p2->state == PROCESS_STATE_READY);
    assert(process_get_waiting_duration_ms(p1) == 0.0);
    assert(process_get_waiting_duration_ms(p2) == 0.0);

    monitor_show_process_table(&sm.pm, &sm.rm);
    printf("[PASS] Test 2: Initial process table verified.\n\n");

    /* Test 3: Allocations, blocking, and monotonic wait duration tracking */
    printf("[RUNNING] Test 3: Resource allocation and blocked wait duration verification\n");
    AllocResult ar1 = system_manager_request(&sm, "P1", "R1", 3);
    assert(ar1 == ALLOC_GRANTED);
    assert(p1->state == PROCESS_STATE_RUNNING);
    assert(r1->available_instances == 1);

    /* P2 requests 2 units of R1 (only 1 available) -> must block */
    AllocResult ar2 = system_manager_request(&sm, "P2", "R1", 2);
    assert(ar2 == ALLOC_BLOCKED);
    assert(p2->state == PROCESS_STATE_BLOCKED);

    /* Sleep 20ms to allow monotonic wait duration to accumulate */
    usleep(20000);
    double wait_ms = process_get_waiting_duration_ms(p2);
    printf("  [Observed] Process P2 wait duration: %.2f ms\n", wait_ms);
    assert(wait_ms >= 10.0);

    monitor_show_combined(&sm);
    printf("[PASS] Test 3: Monotonic wait duration verified for blocked process.\n\n");

    /* Test 4: Resource accounting invariant check */
    printf("[RUNNING] Test 4: Verify resource accounting invariants\n");
    int total_r1 = r1->total_instances;
    int avail_r1 = r1->available_instances;
    int alloc_r1 = p1->allocated[0] + p2->allocated[0];
    assert(avail_r1 + alloc_r1 == total_r1);
    assert(alloc_r1 == 3);
    assert(avail_r1 == 1);
    printf("[PASS] Test 4: Invariant Avail(%d) + Alloc(%d) == Total(%d) satisfied.\n\n",
           avail_r1, alloc_r1, total_r1);

    /* Test 5: Release and unblocking resets wait duration */
    printf("[RUNNING] Test 5: Release resources, unblock pending request, check duration reset\n");
    system_manager_release(&sm, "P1", "R1", 2);
    /* After release: P1 holds 1 unit of R1, available became 3 -> P2 pending (2 units) fulfilled! */
    assert(p2->state == PROCESS_STATE_RUNNING);
    assert(p2->allocated[0] == 2);
    assert(p2->requested[0] == 0);
    assert(r1->available_instances == 1); /* 4 - 1 (P1) - 2 (P2) = 1 */
    assert(process_get_waiting_duration_ms(p2) == 0.0);
    assert(p2->accumulated_wait_ms >= 10.0);

    monitor_show_combined(&sm);
    printf("[PASS] Test 5: Unblocked process transitioned to RUNNING and duration reset cleanly.\n\n");

    /* Test 6: Live monitor non-blocking tick limit check */
    printf("[RUNNING] Test 6: Live monitor non-blocking tick limit (max_ticks=2)\n");
    monitor_run_live(&sm, 10, 2);
    printf("[PASS] Test 6: Live monitor loop returned successfully.\n\n");

    system_manager_cleanup(&sm);

    printf("======================================================================\n");
    printf("         ALL PROCESS MONITOR & RESOURCE DASHBOARD TESTS PASSED        \n");
    printf("======================================================================\n");
    return 0;
}
