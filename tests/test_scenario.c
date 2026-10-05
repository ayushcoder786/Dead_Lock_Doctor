#include <assert.h>
#include "common.h"
#include "manager.h"
#include "scenario.h"
#include "banker.h"
#include "detector.h"
#include "recovery.h"

static void test_replay_deadlock_scenario(void) {
    printf("[RUNNING] Test 1: Replay scenarios/deadlock.txt\n");

    SystemManager sm;
    system_manager_init(&sm);

    bool ok = scenario_replay_file(&sm, "scenarios/deadlock.txt");
    assert(ok == true);

    /* Verify 2 processes and 2 resources exist */
    assert(sm.pm.count == 2);
    assert(sm.rm.count == 2);

    /* Verify both P1 and P2 are blocked */
    Process *p1 = process_find_by_id_or_name(&sm.pm, "P1");
    Process *p2 = process_find_by_id_or_name(&sm.pm, "P2");
    assert(p1 != NULL);
    assert(p2 != NULL);
    assert(p1->state == PROCESS_STATE_BLOCKED);
    assert(p2->state == PROCESS_STATE_BLOCKED);

    /* Verify detector detects circular wait deadlock */
    DeadlockReport report;
    deadlock_report_init(&report);
    bool detected = detector_run(&sm.pm, &sm.rm, &report);
    assert(detected == true);
    assert(report.deadlock_detected == true);
    assert(report.deadlocked_count == 2);
    assert(report.cycle_len >= 2);

    deadlock_report_cleanup(&report);
    system_manager_cleanup(&sm);
    printf("[PASS] Test 1 Passed: Deadlock scenario replayed and verified.\n\n");
}

static void test_replay_safe_scenario(void) {
    printf("[RUNNING] Test 2: Replay scenarios/safe.txt\n");

    SystemManager sm;
    system_manager_init(&sm);

    bool ok = scenario_replay_file(&sm, "scenarios/safe.txt");
    assert(ok == true);

    /* Verify processes and resources */
    assert(sm.pm.count == 3);
    assert(sm.rm.count == 3);

    /* Verify Banker's Safety */
    BankerSystem bs;
    banker_system_init(&bs);
    banker_init_from_manager(&bs, &sm.pm, &sm.rm);
    int safe_seq[MAX_PROCESSES];
    int seq_len = 0;
    bool is_safe = banker_is_safe(&bs, safe_seq, &seq_len);
    assert(is_safe == true);
    assert(seq_len == 3);

    banker_system_cleanup(&bs);
    system_manager_cleanup(&sm);
    printf("[PASS] Test 2 Passed: Safe scenario replayed and verified.\n\n");
}

static void test_replay_unsafe_scenario(void) {
    printf("[RUNNING] Test 3: Replay scenarios/unsafe.txt\n");

    SystemManager sm;
    system_manager_init(&sm);

    bool ok = scenario_replay_file(&sm, "scenarios/unsafe.txt");
    assert(ok == true);

    /* Verify Banker's Safety indicates unsafe state */
    BankerSystem bs;
    banker_system_init(&bs);
    banker_init_from_manager(&bs, &sm.pm, &sm.rm);
    int safe_seq[MAX_PROCESSES];
    int seq_len = 0;
    bool is_safe = banker_is_safe(&bs, safe_seq, &seq_len);
    assert(is_safe == false);

    banker_system_cleanup(&bs);
    system_manager_cleanup(&sm);
    printf("[PASS] Test 3 Passed: Unsafe scenario replayed and verified.\n\n");
}

static void test_parser_error_handling(void) {
    printf("[RUNNING] Test 4: Parser Error Handling\n");

    SystemManager sm;
    system_manager_init(&sm);

    /* Non-existent file should return false */
    assert(scenario_replay_file(&sm, "scenarios/nonexistent_file_xyz.txt") == false);

    /* Invalid commands via scenario_execute_line */
    assert(scenario_execute_line(&sm, "INVALID_CMD P1", 1) == false);
    assert(scenario_execute_line(&sm, "PROCESS", 2) == false);
    assert(scenario_execute_line(&sm, "RESOURCE", 3) == false);
    assert(scenario_execute_line(&sm, "REQUEST P1 R1", 4) == false);
    assert(scenario_execute_line(&sm, "RELEASE P1 R1", 5) == false);

    /* Missing process/resource */
    system_manager_create_process(&sm, 1, "P1");
    system_manager_create_resource(&sm, 1, "R1", 2);
    assert(scenario_execute_line(&sm, "REQUEST P99 R1 1", 6) == false);
    assert(scenario_execute_line(&sm, "REQUEST P1 R99 1", 7) == false);
    assert(scenario_execute_line(&sm, "RELEASE P99 R1 1", 8) == false);
    assert(scenario_execute_line(&sm, "RELEASE P1 R99 1", 9) == false);

    system_manager_cleanup(&sm);
    printf("[PASS] Test 4 Passed: Parser error handling verified.\n\n");
}

static void test_recovery_on_deadlock_scenario(void) {
    printf("[RUNNING] Test 5: 10-step Deadlock Recovery on scenarios/deadlock.txt\n");

    SystemManager sm;
    system_manager_init(&sm);

    bool replay_ok = scenario_replay_file(&sm, "scenarios/deadlock.txt");
    assert(replay_ok == true);

    /* Verify initial deadlock state */
    DeadlockReport initial_report;
    deadlock_report_init(&initial_report);
    assert(detector_run(&sm.pm, &sm.rm, &initial_report) == true);
    assert(initial_report.deadlock_detected == true);
    assert(initial_report.deadlocked_count == 2);
    deadlock_report_cleanup(&initial_report);

    /* Execute Milestone 13 Recovery */
    bool recovered = recovery_run(&sm);
    assert(recovered == true);

    /* Verify victim (P1) is aborted and P2 unblocked */
    Process *p1 = process_find_by_id_or_name(&sm.pm, "P1");
    Process *p2 = process_find_by_id_or_name(&sm.pm, "P2");
    assert(p1 != NULL && p1->state == PROCESS_STATE_ABORTED);
    assert(p2 != NULL && p2->state != PROCESS_STATE_BLOCKED);

    /* Verify deadlock is completely resolved */
    DeadlockReport final_report;
    deadlock_report_init(&final_report);
    detector_run(&sm.pm, &sm.rm, &final_report);
    assert(final_report.deadlock_detected == false);
    deadlock_report_cleanup(&final_report);

    system_manager_cleanup(&sm);
    printf("[PASS] Test 5 Passed: 10-step Deadlock recovery verified.\n\n");
}

int main(void) {
    printf("==========================================\n");
    printf("  DEADLOCKDOCTOR SCENARIO REPLAY TESTS   \n");
    printf("==========================================\n\n");

    test_replay_deadlock_scenario();
    test_replay_safe_scenario();
    test_replay_unsafe_scenario();
    test_parser_error_handling();
    test_recovery_on_deadlock_scenario();

    printf("==========================================\n");
    printf("  ALL SCENARIO TEST CASES PASSED          \n");
    printf("==========================================\n");
    return 0;
}
