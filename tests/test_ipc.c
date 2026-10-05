#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include "common.h"
#include "manager.h"
#include "ipc.h"

static void test_ipc_anonymous_pipe(void) {
    printf("[RUNNING] Test 1: IPC Anonymous Pipe resource request\n");

    SystemManager sm;
    system_manager_init(&sm);

    bool ok = ipc_demo_pipe(&sm);
    assert(ok == true);

    Process *p1 = process_find_by_id_or_name(&sm.pm, "P1");
    Resource *r1 = resource_find_by_id_or_name(&sm.rm, "R1");
    assert(p1 != NULL);
    assert(r1 != NULL);

    /* Verify P1 received 1 instance of R1 via pipe request */
    int r1_idx = resource_get_index_by_id(&sm.rm, r1->id);
    assert(r1_idx >= 0);
    assert(p1->allocated[r1_idx] == 1);

    system_manager_cleanup(&sm);
    printf("[PASS] Test 1 Passed: Anonymous pipe resource request verified.\n\n");
}

static void test_ipc_named_pipe_fifo(void) {
    printf("[RUNNING] Test 2: IPC Named Pipe (FIFO) resource request\n");

    SystemManager sm;
    system_manager_init(&sm);

    bool ok = ipc_demo_fifo(&sm);
    assert(ok == true);

    Process *p2 = process_find_by_id_or_name(&sm.pm, "P2");
    Resource *r2 = resource_find_by_id_or_name(&sm.rm, "R2");
    assert(p2 != NULL);
    assert(r2 != NULL);

    /* Verify P2 received 1 instance of R2 via FIFO request */
    int r2_idx = resource_get_index_by_id(&sm.rm, r2->id);
    assert(r2_idx >= 0);
    assert(p2->allocated[r2_idx] == 1);

    system_manager_cleanup(&sm);
    printf("[PASS] Test 2 Passed: Named pipe (FIFO) resource request verified.\n\n");
}

static void test_ipc_signal_handling(void) {
    printf("[RUNNING] Test 3: Asynchronous signal handling (SIGUSR1, SIGALRM, SIGTERM)\n");

    SystemManager sm;
    system_manager_init(&sm);

    bool ok = ipc_demo_signals(&sm);
    assert(ok == true);

    system_manager_cleanup(&sm);
    printf("[PASS] Test 3 Passed: Safe signal handling verified.\n\n");
}

static void test_ipc_run_all_standalone(void) {
    printf("[RUNNING] Test 4: Full standalone IPC demo suite\n");

    bool ok = ipc_demo_run_all(NULL);
    assert(ok == true);

    printf("[PASS] Test 4 Passed: Standalone IPC suite verified.\n\n");
}

int main(void) {
    printf("==========================================\n");
    printf("     DEADLOCKDOCTOR IPC (CO-3) TESTS      \n");
    printf("==========================================\n\n");

    test_ipc_anonymous_pipe();
    test_ipc_named_pipe_fifo();
    test_ipc_signal_handling();
    test_ipc_run_all_standalone();

    printf("==========================================\n");
    printf("     ALL IPC TEST CASES PASSED            \n");
    printf("==========================================\n");
    return 0;
}
