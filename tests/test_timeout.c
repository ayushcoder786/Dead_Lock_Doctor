#include "common.h"
#include "manager.h"
#include "timeout.h"
#include <assert.h>

int main(void) {
    printf("======================================================================\n");
    printf("      UNIT TESTS: TIMEOUT-BASED DETECTION & FALSE-ALARM AUDIT         \n");
    printf("======================================================================\n\n");

    /* Test 1: Normal wait (under threshold) */
    printf("[RUNNING] Test 1: Blocked process under timeout threshold classified as NORMAL\n");
    SystemManager sm1;
    system_manager_init(&sm1);
    system_manager_create_resource(&sm1, 1, "R1", 1);
    system_manager_create_process(&sm1, 1, "P1");
    system_manager_create_process(&sm1, 2, "P2");

    system_manager_request(&sm1, "P1", "R1", 1);
    system_manager_request(&sm1, "P2", "R1", 1); /* P2 blocked */

    TimeoutAuditResult res1;
    timeout_run_audit(&sm1, 500.0, &res1);
    assert(res1.total_blocked == 1);
    assert(res1.suspected_count == 0);
    assert(res1.records[0].verdict == WAIT_NORMAL);
    printf("[PASS] Test 1: Normal wait verified (0 suspected, 0 deadlocks).\n\n");
    timeout_cleanup_audit(&res1);
    system_manager_cleanup(&sm1);

    /* Test 2: False Alarm Verification (Contention exceeds threshold, but NO deadlock) */
    printf("[RUNNING] Test 2: Injected wait exceeding threshold without cycle flags FALSE ALARM\n");
    SystemManager sm2;
    system_manager_init(&sm2);
    system_manager_create_resource(&sm2, 1, "R1", 1);
    system_manager_create_process(&sm2, 1, "P1");
    system_manager_create_process(&sm2, 2, "P2");

    system_manager_request(&sm2, "P1", "R1", 1);
    system_manager_request(&sm2, "P2", "R1", 1); /* P2 blocked */

    Process *p2 = process_find_by_id(&sm2.pm, 2);
    assert(p2 != NULL);
    timeout_inject_wait_time(p2, 600.0); /* P2 has waited 600ms */

    TimeoutAuditResult res2;
    timeout_run_audit(&sm2, 500.0, &res2);
    timeout_print_audit_report(&res2);

    assert(res2.total_blocked == 1);
    assert(res2.suspected_count == 1);
    assert(res2.confirmed_deadlocks == 0);
    assert(res2.false_alarms == 1);
    assert(res2.false_alarm_rate_pct == 100.0);
    assert(res2.records[0].verdict == WAIT_ORDINARY_CONTENTION);
    printf("[PASS] Test 2: Correctly distinguished ordinary contention (false alarm) from deadlock.\n\n");
    timeout_cleanup_audit(&res2);
    system_manager_cleanup(&sm2);

    /* Test 3: Confirmed Deadlock Verification (Wait > threshold AND circular wait cycle) */
    printf("[RUNNING] Test 3: Circular wait exceeding threshold flags CONFIRMED DEADLOCK\n");
    SystemManager sm3;
    system_manager_init(&sm3);
    system_manager_create_resource(&sm3, 1, "R1", 1);
    system_manager_create_resource(&sm3, 2, "R2", 1);
    system_manager_create_process(&sm3, 1, "P1");
    system_manager_create_process(&sm3, 2, "P2");

    /* Create true circular wait */
    system_manager_request(&sm3, "P1", "R1", 1); /* P1 holds R1 */
    system_manager_request(&sm3, "P2", "R2", 1); /* P2 holds R2 */
    system_manager_request(&sm3, "P1", "R2", 1); /* P1 blocks on R2 */
    system_manager_request(&sm3, "P2", "R1", 1); /* P2 blocks on R1 (Deadlock cycle formed) */

    Process *p1 = process_find_by_id(&sm3.pm, 1);
    p2 = process_find_by_id(&sm3.pm, 2);
    assert(p1 != NULL);
    assert(p2 != NULL);
    assert(p1->state == PROCESS_STATE_BLOCKED);
    assert(p2->state == PROCESS_STATE_BLOCKED);

    timeout_inject_wait_time(p1, 600.0);
    timeout_inject_wait_time(p2, 600.0);

    TimeoutAuditResult res3;
    timeout_run_audit(&sm3, 500.0, &res3);
    timeout_print_audit_report(&res3);

    assert(res3.total_blocked == 2);
    assert(res3.suspected_count == 2);
    assert(res3.confirmed_deadlocks == 2);
    assert(res3.false_alarms == 0);
    assert(res3.false_alarm_rate_pct == 0.0);
    assert(res3.records[0].verdict == WAIT_CONFIRMED_DEADLOCK);
    assert(res3.records[1].verdict == WAIT_CONFIRMED_DEADLOCK);
    printf("[PASS] Test 3: True circular wait correctly confirmed as CONFIRMED DEADLOCK.\n\n");
    timeout_cleanup_audit(&res3);
    system_manager_cleanup(&sm3);

    printf("======================================================================\n");
    printf("         ALL TIMEOUT-BASED DETECTION TESTS PASSED                     \n");
    printf("======================================================================\n");
    return 0;
}
