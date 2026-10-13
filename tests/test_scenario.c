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


static void test_presentation_demo_verification(void) {
    printf("[RUNNING] Test 6: Presentation Demo Complete Stages Verification\n");

    /* 1. Verify Stage 1 & 2: Safe state and Banker validation */
    SystemManager sm_safe;
    system_manager_init(&sm_safe);
    assert(scenario_replay_file(&sm_safe, "scenarios/safe.txt") == true);

    BankerSystem bs;
    banker_system_init(&bs);
    assert(banker_init_from_manager(&bs, &sm_safe.pm, &sm_safe.rm) == true);
    int safe_seq[MAX_PROCESSES];
    int seq_len = 0;
    bool is_safe = banker_is_safe(&bs, safe_seq, &seq_len);
    assert(is_safe == true);
    assert(seq_len == 3);
    /* Safe sequence must be P2 -> P1 -> P3 */
    assert(strcmp(bs.process_names[safe_seq[0]], "P2") == 0);
    assert(strcmp(bs.process_names[safe_seq[1]], "P1") == 0);
    assert(strcmp(bs.process_names[safe_seq[2]], "P3") == 0);
    banker_system_cleanup(&bs);
    system_manager_cleanup(&sm_safe);

    /* 2. Verify Stage 2: Unsafe state and Avoidance blocking */
    SystemManager sm_unsafe;
    system_manager_init(&sm_unsafe);
    assert(scenario_replay_file(&sm_unsafe, "scenarios/unsafe.txt") == true);
    banker_system_init(&bs);
    assert(banker_init_from_manager(&bs, &sm_unsafe.pm, &sm_unsafe.rm) == true);
    assert(banker_is_safe(&bs, safe_seq, &seq_len) == false);
    banker_system_cleanup(&bs);
    system_manager_cleanup(&sm_unsafe);

    /* 3. Verify Stage 3 & 4: Exact deadlocked set and recovery */
    SystemManager sm_deadlock;
    system_manager_init(&sm_deadlock);
    assert(scenario_replay_file(&sm_deadlock, "scenarios/deadlock.txt") == true);

    DeadlockReport report;
    deadlock_report_init(&report);
    assert(detector_run(&sm_deadlock.pm, &sm_deadlock.rm, &report) == true);
    assert(report.deadlock_detected == true);
    assert(report.deadlocked_count == 2);
    /* Exact deadlocked set verification */
    bool has_p1 = false, has_p2 = false;
    for (int i = 0; i < report.deadlocked_count; i++) {
        if (strcmp(report.deadlocked_names[i], "P1") == 0) has_p1 = true;
        if (strcmp(report.deadlocked_names[i], "P2") == 0) has_p2 = true;
    }
    assert(has_p1 == true && has_p2 == true);
    deadlock_report_cleanup(&report);

    /* Test prioritized victim selection and recovery */
    Process *p1 = process_find_by_id_or_name(&sm_deadlock.pm, "P1");
    Process *p2 = process_find_by_id_or_name(&sm_deadlock.pm, "P2");
    assert(p1 != NULL && p2 != NULL);
    process_set_priority(p1, 100);
    process_set_priority(p2, 1);

    bool recovered = recovery_run(&sm_deadlock);
    assert(recovered == true);
    assert(p2->state == PROCESS_STATE_ABORTED);
    assert(p1->state != PROCESS_STATE_BLOCKED);

    deadlock_report_init(&report);
    detector_run(&sm_deadlock.pm, &sm_deadlock.rm, &report);
    assert(report.deadlock_detected == false);
    deadlock_report_cleanup(&report);
    system_manager_cleanup(&sm_deadlock);

    /* 4. Verify system_manager_run_full_demo executes cleanly */
    system_manager_run_full_demo();

    printf("[PASS] Test 6 Passed: Complete presentation demo stages verified.\n\n");
}


static void test_replay_multi_instance_scenario(void) {
    printf("[RUNNING] Test 7: Replay scenarios/multi_instance.txt\n");
    SystemManager sm;
    system_manager_init(&sm);

    bool ok = scenario_replay_file(&sm, "scenarios/multi_instance.txt");
    assert(ok == true);
    assert(sm.pm.count == 3);
    assert(sm.rm.count == 3);

    system_manager_cleanup(&sm);
    printf("[PASS] Test 7 Passed: Multi-instance scenario verified.\n\n");
}

static void test_replay_prevention_ordered_scenario(void) {
    printf("[RUNNING] Test 8: Replay scenarios/prevention_ordered.txt\n");
    SystemManager sm;
    system_manager_init(&sm);

    bool ok = scenario_replay_file(&sm, "scenarios/prevention_ordered.txt");
    assert(ok == true);
    assert(sm.strategy == STRATEGY_RESOURCE_ORDERING);

    system_manager_cleanup(&sm);
    printf("[PASS] Test 8 Passed: Prevention ordered scenario verified.\n\n");
}

static void test_replay_stress_scenario(void) {
    printf("[RUNNING] Test 9: Replay scenarios/stress.txt\n");
    SystemManager sm;
    system_manager_init(&sm);

    bool ok = scenario_replay_file(&sm, "scenarios/stress.txt");
    assert(ok == true);
    assert(sm.pm.count == 5);
    assert(sm.rm.count == 4);

    system_manager_cleanup(&sm);
    printf("[PASS] Test 9 Passed: Stress scenario verified.\n\n");
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
    test_presentation_demo_verification();
    test_replay_multi_instance_scenario();
    test_replay_prevention_ordered_scenario();
    test_replay_stress_scenario();

    printf("==========================================\n");
    printf("  ALL SCENARIO TEST CASES PASSED          \n");
    printf("==========================================\n");
    return 0;
}
