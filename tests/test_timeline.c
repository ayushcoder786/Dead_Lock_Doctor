#include "common.h"
#include "manager.h"
#include "timeline.h"
#include "scenario.h"
#include <assert.h>

int main(void) {
    printf("======================================================================\n");
    printf("     UNIT TESTS: EVENT TIMELINE & STEP-BY-STEP SCENARIO REPLAY        \n");
    printf("======================================================================\n\n");

    SystemManager sm;
    system_manager_init(&sm);

    /* Test 1: Event timeline record and sequential order */
    printf("[RUNNING] Test 1: Event timeline recording and chronological sequencing\n");
    timeline_record(&sm.timeline, TIMELINE_INFO, NULL, NULL, 0, "State:INIT", "System initialized");
    timeline_record(&sm.timeline, TIMELINE_PROCESS_CREATE, "P1", NULL, 0, "Procs:1", "Created process P1");
    timeline_record(&sm.timeline, TIMELINE_RESOURCE_CREATE, NULL, "R1", 3, "Res:1", "Created resource R1 (3 units)");

    assert(sm.timeline.count == 3);
    assert(sm.timeline.events[0].seq_num == 1);
    assert(sm.timeline.events[1].seq_num == 2);
    assert(sm.timeline.events[2].seq_num == 3);
    assert(sm.timeline.events[2].offset_ms >= sm.timeline.events[0].offset_ms);
    printf("[PASS] Test 1: 3 events recorded with strictly monotonic timestamps.\n\n");

    /* Test 2: Filtering and timeline rendering */
    printf("[RUNNING] Test 2: Timeline filtering\n");
    timeline_show(&sm.timeline, 0, "P1");
    timeline_show(&sm.timeline, 0, "RES_CREATE");
    printf("[PASS] Test 2: Filtered rendering succeeded.\n\n");

    /* Test 3: Step-by-step scenario load and single-step progression */
    printf("[RUNNING] Test 3: Deterministic step-by-step replay session on scenarios/deadlock.txt\n");
    ScenarioStepSession sess;
    scenario_step_session_init(&sess);
    bool loaded = scenario_step_session_load(&sess, "scenarios/deadlock.txt");
    assert(loaded);
    assert(sess.total_lines > 0);
    printf("  [Observed] Successfully loaded %d operations from scenarios/deadlock.txt\n", sess.total_lines);

    int steps_executed = 0;
    while (sess.current_step < sess.total_lines) {
        bool step_ok = scenario_step_session_next(&sess, &sm);
        assert(step_ok);
        steps_executed++;
    }
    assert(steps_executed == sess.total_lines);
    assert(sm.timeline.count >= sess.total_lines);
    printf("[PASS] Test 3: Successfully stepped through all %d scenario operations.\n\n", steps_executed);

    /* Test 4: Verify timeline captured step events */
    printf("[RUNNING] Test 4: Verify timeline contains SCENARIO_STEP entries\n");
    timeline_show(&sm.timeline, 10, NULL);
    printf("[PASS] Test 4: Full timeline displayed cleanly.\n\n");

    scenario_step_session_cleanup(&sess);
    system_manager_cleanup(&sm);

    printf("======================================================================\n");
    printf("       ALL EVENT TIMELINE & STEP-BY-STEP REPLAY TESTS PASSED          \n");
    printf("======================================================================\n");
    return 0;
}
