#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include "common.h"
#include "process_demo.h"
#include "logger.h"

#define TEST_DEMO_LOG "logs/test_process_demo.log"

static void test_process_demo_execution(void) {
    printf("[RUNNING] Test 1: Full Linux process demonstration (fork, exec, waitpid)\n");

    /* Ensure isolated logger is active to capture process demo events */
    unlink(TEST_DEMO_LOG);
    logger_init(TEST_DEMO_LOG);

    bool success = process_demo_run();
    assert(success == true);

    logger_close();

    /* Verify log contains process demo entries */
    int fd = open(TEST_DEMO_LOG, O_RDONLY);
    assert(fd >= 0);

    char buf[8192];
    ssize_t bytes = read(fd, buf, sizeof(buf) - 1);
    assert(bytes > 0);
    buf[bytes] = '\0';
    close(fd);
    unlink(TEST_DEMO_LOG);

    assert(strstr(buf, "Linux Process Demo") != NULL);
    assert(strstr(buf, "Created child P1") != NULL);
    assert(strstr(buf, "Created child P2") != NULL);
    assert(strstr(buf, "waitpid") != NULL);

    printf("[PASS] Test 1 Passed: fork(), exec(), and waitpid() execution and logging verified.\n\n");
}

static void test_process_demo_worker_direct(void) {
    printf("[RUNNING] Test 2: Direct worker invocation test\n");

    char *argv[] = { "worker", "TestWorker", "1", NULL };
    int ret = process_demo_worker_main(3, argv);
    assert(ret == 0);

    printf("[PASS] Test 2 Passed: Direct worker routine executed successfully.\n\n");
}

int main(void) {
    printf("==========================================\n");
    printf("     PROCESS DEMO (CO-2) UNIT TESTS       \n");
    printf("==========================================\n\n");

    test_process_demo_execution();
    test_process_demo_worker_direct();

    printf("==========================================\n");
    printf("     ALL PROCESS DEMO TESTS PASSED        \n");
    printf("==========================================\n");
    return 0;
}
