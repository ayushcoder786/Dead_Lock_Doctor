#include "common.h"
#include "trace.h"
#include <assert.h>

int main(void) {
    printf("======================================================================\n");
    printf("            UNIT TESTS: SYSTEM-CALL TRACING DEMONSTRATION             \n");
    printf("======================================================================\n\n");

    printf("[RUNNING] Test 1: Check strace availability check function\n");
    bool has_strace = trace_is_strace_available();
    printf("  [Observed] trace_is_strace_available() returned: %s\n", has_strace ? "TRUE" : "FALSE");
    printf("[PASS] Test 1: Availability probe executed without crashing.\n\n");

    printf("[RUNNING] Test 2: Run trace demonstration function\n");
    trace_run_demonstration();
    printf("[PASS] Test 2: trace_run_demonstration() completed successfully.\n\n");

    printf("======================================================================\n");
    printf("            ALL SYSTEM-CALL TRACING TESTS PASSED                      \n");
    printf("======================================================================\n");
    return 0;
}
