#include "common.h"
#include "recovery.h"
#include "manager.h"
#include "detector.h"
#include "banker.h"
#include <assert.h>

static void test_recovery_cost_formula(void) {
    printf("[RUNNING] Test 1: Recovery cost calculation formula\n");
    ProcessManager pm;
    process_manager_init(&pm);

    Process *p = process_create(&pm, 1, "P1");
    assert(p != NULL);
    process_ensure_resource_capacity(p, 3);

    /* Allocate 2 units of R0, 1 unit of R1 -> total held = 3 */
    p->allocated[0] = 2;
    p->allocated[1] = 1;
    /* Request 2 units of R2 -> total pending = 2 */
    p->requested[2] = 2;
    /* Priority = 5 */
    p->priority = 5;

    /* Expected cost: (3 * 2) + (2 * 1) + 5 = 6 + 2 + 5 = 13 */
    int cost = recovery_calculate_cost(p, 3);
    assert(cost == 13);

    process_manager_cleanup(&pm);
    printf("[PASS] Test 1 Passed: Formula (held*2 + pending*1 + priority) verified.\n\n");
}

static void test_priority_weighted_victim_selection(void) {
    printf("[RUNNING] Test 2: Priority-weighted victim selection\n");
    SystemManager sm;
    system_manager_init(&sm);

    /* Setup 2 processes, 2 resources */
    system_manager_create_process(&sm, 1, "P1");
    system_manager_create_process(&sm, 2, "P2");
    system_manager_create_resource(&sm, 1, "R1", 1);
    system_manager_create_resource(&sm, 2, "R2", 1);

    system_manager_set_max(&sm, "P1", "R1", 1);
    system_manager_set_max(&sm, "P1", "R2", 1);
    system_manager_set_max(&sm, "P2", "R1", 1);
    system_manager_set_max(&sm, "P2", "R2", 1);

    /* Create circular wait */
    system_manager_request(&sm, "P1", "R1", 1);
    system_manager_request(&sm, "P2", "R2", 1);
    system_manager_request(&sm, "P1", "R2", 1);
    system_manager_request(&sm, "P2", "R1", 1);

    /* Make P1 high priority (priority = 100), P2 default priority (priority = 1) */
    Process *p1 = process_find_by_id_or_name(&sm.pm, "P1");
    Process *p2 = process_find_by_id_or_name(&sm.pm, "P2");
    assert(p1 && p2);
    process_set_priority(p1, 100);
    process_set_priority(p2, 1);

    /* P1 cost: (1*2) + (1*1) + 100 = 103 */
    /* P2 cost: (1*2) + (1*1) + 1 = 4 */
    /* Lowest cost victim MUST be P2 */
    bool recovered = recovery_run(&sm);
    assert(recovered == true);

    assert(p2->state == PROCESS_STATE_ABORTED);
    assert(p1->state != PROCESS_STATE_BLOCKED);

    /* Deadlock check must now be clean */
    DeadlockReport report;
    deadlock_report_init(&report);
    detector_run(&sm.pm, &sm.rm, &report);
    assert(report.deadlock_detected == false);
    deadlock_report_cleanup(&report);

    system_manager_cleanup(&sm);
    printf("[PASS] Test 2 Passed: Priority-weighted victim selection correctly chose P2.\n\n");
}

static void test_auto_recovery_multicycle(void) {
    printf("[RUNNING] Test 3: Iterative auto-recovery (recovery_auto_recover)\n");
    SystemManager sm;
    system_manager_init(&sm);

    /* Create deadlock */
    system_manager_create_process(&sm, 1, "P1");
    system_manager_create_process(&sm, 2, "P2");
    system_manager_create_resource(&sm, 1, "R1", 1);
    system_manager_create_resource(&sm, 2, "R2", 1);

    system_manager_set_max(&sm, "P1", "R1", 1);
    system_manager_set_max(&sm, "P1", "R2", 1);
    system_manager_set_max(&sm, "P2", "R1", 1);
    system_manager_set_max(&sm, "P2", "R2", 1);

    system_manager_request(&sm, "P1", "R1", 1);
    system_manager_request(&sm, "P2", "R2", 1);
    system_manager_request(&sm, "P1", "R2", 1);
    system_manager_request(&sm, "P2", "R1", 1);

    /* Run auto recovery */
    bool ok = recovery_auto_recover(&sm);
    assert(ok == true);

    DeadlockReport report;
    deadlock_report_init(&report);
    detector_run(&sm.pm, &sm.rm, &report);
    assert(report.deadlock_detected == false);
    deadlock_report_cleanup(&report);

    system_manager_cleanup(&sm);
    printf("[PASS] Test 3 Passed: Auto-recovery resolved deadlock cycle.\n\n");
}

int main(void) {
    printf("==========================================\n");
    printf("      DEADLOCKDOCTOR RECOVERY TESTS       \n");
    printf("==========================================\n\n");

    test_recovery_cost_formula();
    test_priority_weighted_victim_selection();
    test_auto_recovery_multicycle();

    printf("==========================================\n");
    printf("      ALL RECOVERY TEST CASES PASSED      \n");
    printf("==========================================\n");
    return 0;
}
