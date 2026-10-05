#include <assert.h>
#include "common.h"
#include "process.h"
#include "resource.h"
#include "graph.h"
#include "detector.h"

static void test_empty_and_single_node_graph(void) {
    printf("[RUNNING] Test 1: Empty and single-node graph visualization\n");

    ProcessManager pm;
    ResourceManager rm;
    process_manager_init(&pm);
    resource_manager_init(&rm);

    /* Empty visualization */
    graph_visualize(&pm, &rm);

    /* Add single process and resource with no edges */
    Process *p1 = process_create(&pm, 1, "P1");
    resource_create(&rm, 1, "R1", 1);
    process_ensure_resource_capacity(p1, 1);

    WaitForGraph wfg;
    wfg_init(&wfg);
    wfg_build_from_system(&wfg, &pm, &rm);
    assert(wfg.num_nodes == 1);
    assert(wfg_has_edge(&wfg, 0, 0) == false);

    graph_visualize(&pm, &rm);

    wfg_cleanup(&wfg);
    process_manager_cleanup(&pm);
    resource_manager_cleanup(&rm);
    printf("[PASS] Test 1 Passed: Empty and single node graphs handled cleanly.\n\n");
}

static void test_circular_wait_graph_2_processes(void) {
    printf("[RUNNING] Test 2: Circular wait graph (P1 <-> P2)\n");

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

    /* P1 holds R1, waits for R2 */
    p1->allocated[0] = 1;
    p1->requested[1] = 1;
    p1->state = PROCESS_STATE_BLOCKED;

    /* P2 holds R2, waits for R1 */
    p2->allocated[1] = 1;
    p2->requested[0] = 1;
    p2->state = PROCESS_STATE_BLOCKED;

    WaitForGraph wfg;
    wfg_init(&wfg);
    wfg_build_from_system(&wfg, &pm, &rm);

    assert(wfg.num_nodes == 2);
    int p1_idx = wfg_get_node_index(&wfg, 1);
    int p2_idx = wfg_get_node_index(&wfg, 2);

    assert(p1_idx >= 0);
    assert(p2_idx >= 0);

    /* Edge P1 -> P2 exists */
    assert(wfg_has_edge(&wfg, p1_idx, p2_idx) == true);
    /* Edge P2 -> P1 exists */
    assert(wfg_has_edge(&wfg, p2_idx, p1_idx) == true);

    int *cycle = (int *)malloc((wfg.num_nodes + 2) * sizeof(int));
    int cycle_len = 0;
    bool has_cycle = wfg_find_cycle_dfs(&wfg, cycle, &cycle_len);
    assert(has_cycle == true);
    assert(cycle_len >= 2);

    printf("Visualizing 2-process circular wait graph:\n");
    graph_visualize(&pm, &rm);

    free(cycle);
    wfg_cleanup(&wfg);
    process_manager_cleanup(&pm);
    resource_manager_cleanup(&rm);
    printf("[PASS] Test 2 Passed: 2-process circular wait graph verified.\n\n");
}

static void test_3_process_cycle_graph(void) {
    printf("[RUNNING] Test 3: 3-process cycle graph (P1 -> P2 -> P3 -> P1)\n");

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

    /* P1 holds R1, waits for R2 */
    p1->allocated[0] = 1;
    p1->requested[1] = 1;
    p1->state = PROCESS_STATE_BLOCKED;

    /* P2 holds R2, waits for R3 */
    p2->allocated[1] = 1;
    p2->requested[2] = 1;
    p2->state = PROCESS_STATE_BLOCKED;

    /* P3 holds R3, waits for R1 */
    p3->allocated[2] = 1;
    p3->requested[0] = 1;
    p3->state = PROCESS_STATE_BLOCKED;

    WaitForGraph wfg;
    wfg_init(&wfg);
    wfg_build_from_system(&wfg, &pm, &rm);

    int p1_idx = wfg_get_node_index(&wfg, 1);
    int p2_idx = wfg_get_node_index(&wfg, 2);
    int p3_idx = wfg_get_node_index(&wfg, 3);

    assert(wfg_has_edge(&wfg, p1_idx, p2_idx) == true);
    assert(wfg_has_edge(&wfg, p2_idx, p3_idx) == true);
    assert(wfg_has_edge(&wfg, p3_idx, p1_idx) == true);

    int *cycle = (int *)malloc((wfg.num_nodes + 2) * sizeof(int));
    int cycle_len = 0;
    bool has_cycle = wfg_find_cycle_dfs(&wfg, cycle, &cycle_len);
    assert(has_cycle == true);
    assert(cycle_len >= 3);

    printf("Visualizing 3-process cycle graph:\n");
    graph_visualize(&pm, &rm);

    free(cycle);
    wfg_cleanup(&wfg);
    process_manager_cleanup(&pm);
    resource_manager_cleanup(&rm);
    printf("[PASS] Test 3 Passed: 3-process cycle graph verified.\n\n");
}

static void test_multi_instance_no_deadlock_graph(void) {
    printf("[RUNNING] Test 4: Multi-instance resource graph without deadlock\n");

    ProcessManager pm;
    ResourceManager rm;
    process_manager_init(&pm);
    resource_manager_init(&rm);

    resource_create(&rm, 1, "Printer", 3);
    rm.resources[0].available_instances = 1;

    Process *p1 = process_create(&pm, 1, "P1");
    Process *p2 = process_create(&pm, 2, "P2");

    process_ensure_resource_capacity(p1, 1);
    process_ensure_resource_capacity(p2, 1);

    p1->allocated[0] = 1;
    p2->allocated[0] = 1;

    WaitForGraph wfg;
    wfg_init(&wfg);
    wfg_build_from_system(&wfg, &pm, &rm);

    int p1_idx = wfg_get_node_index(&wfg, 1);
    int p2_idx = wfg_get_node_index(&wfg, 2);

    /* Neither process is waiting, so no wait-for edges exist */
    assert(wfg_has_edge(&wfg, p1_idx, p2_idx) == false);
    assert(wfg_has_edge(&wfg, p2_idx, p1_idx) == false);

    printf("Visualizing multi-instance safe graph:\n");
    graph_visualize(&pm, &rm);

    wfg_cleanup(&wfg);
    process_manager_cleanup(&pm);
    resource_manager_cleanup(&rm);
    printf("[PASS] Test 4 Passed: Multi-instance safe graph verified.\n\n");
}

int main(void) {
    printf("==========================================\n");
    printf("     DEADLOCKDOCTOR GRAPH TESTS           \n");
    printf("==========================================\n\n");

    test_empty_and_single_node_graph();
    test_circular_wait_graph_2_processes();
    test_3_process_cycle_graph();
    test_multi_instance_no_deadlock_graph();

    printf("==========================================\n");
    printf("     ALL GRAPH TEST CASES PASSED          \n");
    printf("==========================================\n");
    return 0;
}
