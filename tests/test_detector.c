#include <assert.h>
#include "common.h"
#include "process.h"
#include "resource.h"
#include "detector.h"

static void test_circular_wait_2_processes(void) {
    printf("[RUNNING] Test 1: P1/P2 circular wait\n");

    ProcessManager pm;
    ResourceManager rm;
    process_manager_init(&pm);
    resource_manager_init(&rm);

    resource_create(&rm, 1, "R1", 1);
    resource_create(&rm, 2, "R2", 1);
    rm.resources[0].available_instances = 0;
    rm.resources[1].available_instances = 0;

    Process *p1 = process_create(&pm, 1, "P1");
    Process *p2 = process_create(&pm, 2, "P2");

    process_ensure_resource_capacity(p1, 2);
    process_ensure_resource_capacity(p2, 2);

    /* P1 holds R1, requests R2 */
    p1->allocated[0] = 1;
    p1->requested[1] = 1;
    p1->state = PROCESS_STATE_BLOCKED;

    /* P2 holds R2, requests R1 */
    p2->allocated[1] = 1;
    p2->requested[0] = 1;
    p2->state = PROCESS_STATE_BLOCKED;

    DeadlockReport report;
    deadlock_report_init(&report);
    bool detected = detector_run(&pm, &rm, &report);

    assert(detected == true);
    assert(report.deadlock_detected == true);
    assert(report.deadlocked_count == 2);
    assert(report.cycle_len >= 2);

    detector_print_report(&report);

    deadlock_report_cleanup(&report);
    process_manager_cleanup(&pm);
    resource_manager_cleanup(&rm);
    printf("[PASS] Test 1 Passed.\n\n");
}

static void test_cycle_3_processes(void) {
    printf("[RUNNING] Test 2: P1/P2/P3 3-process cycle\n");

    ProcessManager pm;
    ResourceManager rm;
    process_manager_init(&pm);
    resource_manager_init(&rm);

    resource_create(&rm, 1, "R1", 1);
    resource_create(&rm, 2, "R2", 1);
    resource_create(&rm, 3, "R3", 1);
    rm.resources[0].available_instances = 0;
    rm.resources[1].available_instances = 0;
    rm.resources[2].available_instances = 0;

    Process *p1 = process_create(&pm, 1, "P1");
    Process *p2 = process_create(&pm, 2, "P2");
    Process *p3 = process_create(&pm, 3, "P3");

    process_ensure_resource_capacity(p1, 3);
    process_ensure_resource_capacity(p2, 3);
    process_ensure_resource_capacity(p3, 3);

    /* P1 holds R1, requests R2 */
    p1->allocated[0] = 1;
    p1->requested[1] = 1;
    p1->state = PROCESS_STATE_BLOCKED;

    /* P2 holds R2, requests R3 */
    p2->allocated[1] = 1;
    p2->requested[2] = 1;
    p2->state = PROCESS_STATE_BLOCKED;

    /* P3 holds R3, requests R1 */
    p3->allocated[2] = 1;
    p3->requested[0] = 1;
    p3->state = PROCESS_STATE_BLOCKED;

    DeadlockReport report;
    deadlock_report_init(&report);
    bool detected = detector_run(&pm, &rm, &report);

    assert(detected == true);
    assert(report.deadlock_detected == true);
    assert(report.deadlocked_count == 3);
    assert(report.cycle_len >= 3);

    detector_print_report(&report);

    deadlock_report_cleanup(&report);
    process_manager_cleanup(&pm);
    resource_manager_cleanup(&rm);
    printf("[PASS] Test 2 Passed.\n\n");
}

static void test_no_cycle_safe(void) {
    printf("[RUNNING] Test 3: No-cycle safe situation\n");

    ProcessManager pm;
    ResourceManager rm;
    process_manager_init(&pm);
    resource_manager_init(&rm);

    resource_create(&rm, 1, "R1", 1);
    resource_create(&rm, 2, "R2", 1);
    rm.resources[0].available_instances = 0;
    rm.resources[1].available_instances = 0;

    Process *p1 = process_create(&pm, 1, "P1");
    Process *p2 = process_create(&pm, 2, "P2");

    process_ensure_resource_capacity(p1, 2);
    process_ensure_resource_capacity(p2, 2);

    /* P1 holds R1, requests R2 */
    p1->allocated[0] = 1;
    p1->requested[1] = 1;
    p1->state = PROCESS_STATE_BLOCKED;

    /* P2 holds R2, but has NO requests (can finish and release R2) */
    p2->allocated[1] = 1;
    p2->state = PROCESS_STATE_RUNNING;

    DeadlockReport report;
    deadlock_report_init(&report);
    bool detected = detector_run(&pm, &rm, &report);

    assert(detected == false);
    assert(report.deadlock_detected == false);
    assert(report.deadlocked_count == 0);

    detector_print_report(&report);

    deadlock_report_cleanup(&report);
    process_manager_cleanup(&pm);
    resource_manager_cleanup(&rm);
    printf("[PASS] Test 3 Passed.\n\n");
}

static void test_multiple_instance_resource(void) {
    printf("[RUNNING] Test 4: Multiple-instance resource situation\n");

    /* Subcase 4A: Multi-instance with unblocked holder avoids false deadlock */
    {
        ProcessManager pm;
        ResourceManager rm;
        process_manager_init(&pm);
        resource_manager_init(&rm);

        /* R1 has 2 instances, R2 has 1 instance. All allocated. */
        resource_create(&rm, 1, "R1", 2);
        resource_create(&rm, 2, "R2", 1);
        rm.resources[0].available_instances = 0;
        rm.resources[1].available_instances = 0;

        Process *p1 = process_create(&pm, 1, "P1");
        Process *p2 = process_create(&pm, 2, "P2");
        Process *p3 = process_create(&pm, 3, "P3");

        process_ensure_resource_capacity(p1, 2);
        process_ensure_resource_capacity(p2, 2);
        process_ensure_resource_capacity(p3, 2);

        /* P1 holds 1 of R1, requests 1 of R2 */
        p1->allocated[0] = 1;
        p1->requested[1] = 1;
        p1->state = PROCESS_STATE_BLOCKED;

        /* P2 holds 1 of R2, requests 1 of R1 */
        p2->allocated[1] = 1;
        p2->requested[0] = 1;
        p2->state = PROCESS_STATE_BLOCKED;

        /* P3 holds 1 of R1, but has NO requests (running, can release) */
        p3->allocated[0] = 1;
        p3->state = PROCESS_STATE_RUNNING;

        DeadlockReport report;
        deadlock_report_init(&report);
        bool detected = detector_run(&pm, &rm, &report);

        assert(detected == false);
        assert(report.deadlock_detected == false);
        printf("  Subcase 4A (Unblocked holder frees instances): ");
        detector_print_report(&report);

        deadlock_report_cleanup(&report);
        process_manager_cleanup(&pm);
        resource_manager_cleanup(&rm);
    }

    /* Subcase 4B: True multi-instance deadlock when all holders are blocked in circular wait */
    {
        ProcessManager pm;
        ResourceManager rm;
        process_manager_init(&pm);
        resource_manager_init(&rm);

        resource_create(&rm, 1, "R1", 2);
        resource_create(&rm, 2, "R2", 1);
        rm.resources[0].available_instances = 0;
        rm.resources[1].available_instances = 0;

        Process *p1 = process_create(&pm, 1, "P1");
        Process *p2 = process_create(&pm, 2, "P2");
        Process *p3 = process_create(&pm, 3, "P3");

        process_ensure_resource_capacity(p1, 2);
        process_ensure_resource_capacity(p2, 2);
        process_ensure_resource_capacity(p3, 2);

        /* P1 holds 1 of R1, requests 1 of R2 */
        p1->allocated[0] = 1;
        p1->requested[1] = 1;
        p1->state = PROCESS_STATE_BLOCKED;

        /* P2 holds 1 of R2, requests 1 of R1 */
        p2->allocated[1] = 1;
        p2->requested[0] = 1;
        p2->state = PROCESS_STATE_BLOCKED;

        /* P3 also holds 1 of R1, but now also requests R2 */
        p3->allocated[0] = 1;
        p3->requested[1] = 1;
        p3->state = PROCESS_STATE_BLOCKED;

        DeadlockReport report;
        deadlock_report_init(&report);
        bool detected = detector_run(&pm, &rm, &report);

        assert(detected == true);
        assert(report.deadlock_detected == true);
        assert(report.deadlocked_count == 3);
        printf("  Subcase 4B (True circular wait across multi-instance): ");
        detector_print_report(&report);

        deadlock_report_cleanup(&report);
        process_manager_cleanup(&pm);
        resource_manager_cleanup(&rm);
    }

    printf("[PASS] Test 4 Passed.\n\n");
}

int main(void) {
    printf("==========================================\n");
    printf("  DEADLOCKDOCTOR DEADLOCK DETECTOR TESTS   \n");
    printf("==========================================\n\n");

    test_circular_wait_2_processes();
    test_cycle_3_processes();
    test_no_cycle_safe();
    test_multiple_instance_resource();

    printf("==========================================\n");
    printf("  ALL DETECTOR TEST CASES PASSED          \n");
    printf("==========================================\n");
    return 0;
}
