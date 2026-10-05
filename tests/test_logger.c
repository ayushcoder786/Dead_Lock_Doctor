#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include "common.h"
#include "logger.h"

#define TEST_LOG_FILE "logs/test_logger.log"

static void test_logger_lifecycle_and_write(void) {
    printf("[RUNNING] Test 1: Logger init, write, and close\n");

    /* Remove old test log if present */
    unlink(TEST_LOG_FILE);

    bool init_ok = logger_init(TEST_LOG_FILE);
    assert(init_ok == true);
    assert(logger_is_open() == true);

    /* Log various event types */
    logger_log(LOG_PROCESS_CREATE, "Process 'Worker1' (PID 1) created.");
    logger_log(LOG_RESOURCE_CREATE, "Resource 'Printer' (ID 1) created with 2 instances.");
    logger_log(LOG_RESOURCE_REQUEST, "Process 'Worker1' requested 1 instances of 'Printer'.");
    logger_log(LOG_RESOURCE_GRANT, "Process 'Worker1' granted 1 instances of 'Printer'.");
    logger_log(LOG_RESOURCE_BLOCK, "Process 'Worker2' blocked on 'Printer'.");
    logger_log(LOG_RESOURCE_RELEASE, "Process 'Worker1' released 1 instances of 'Printer'.");
    logger_log(LOG_BANKER_RESULT, "Banker safety check: SYSTEM STATE: SAFE.");
    logger_log(LOG_DEADLOCK_DETECT, "Deadlock check completed: NO DEADLOCK DETECTED.");
    logger_log(LOG_RECOVERY_EVENT, "Recovery action executed: Aborted PID 2.");
    logger_log(LOG_PROCESS_TERM, "Process 'Worker1' (PID 1) terminated.");

    logger_close();
    assert(logger_is_open() == false);

    /* Verify the file exists on disk using POSIX open() and read() */
    int fd = open(TEST_LOG_FILE, O_RDONLY);
    assert(fd >= 0);

    char content[4096];
    ssize_t bytes = read(fd, content, sizeof(content) - 1);
    assert(bytes > 0);
    content[bytes] = '\0';
    close(fd);

    /* Check that all event tags are present in the log */
    assert(strstr(content, "[PROCESS_CREATE]") != NULL);
    assert(strstr(content, "[RESOURCE_CREATE]") != NULL);
    assert(strstr(content, "[RESOURCE_REQUEST]") != NULL);
    assert(strstr(content, "[RESOURCE_GRANT]") != NULL);
    assert(strstr(content, "[RESOURCE_BLOCK]") != NULL);
    assert(strstr(content, "[RESOURCE_RELEASE]") != NULL);
    assert(strstr(content, "[BANKER_RESULT]") != NULL);
    assert(strstr(content, "[DEADLOCK_DETECT]") != NULL);
    assert(strstr(content, "[RECOVERY_EVENT]") != NULL);
    assert(strstr(content, "[PROCESS_TERM]") != NULL);

    unlink(TEST_LOG_FILE);
    printf("[PASS] Test 1 Passed: Logger lifecycle and event formatting verified.\n\n");
}

static void test_logger_print(void) {
    printf("[RUNNING] Test 2: Logger print log via POSIX read/write\n");

    bool ok = logger_init(TEST_LOG_FILE);
    assert(ok == true);
    logger_log(LOG_INFO, "Testing logger_print_log() functionality.");
    logger_close();

    bool print_ok = logger_print_log();
    assert(print_ok == true);

    unlink(TEST_LOG_FILE);
    printf("[PASS] Test 2 Passed: Logger print log verified.\n\n");
}

int main(void) {
    printf("==========================================\n");
    printf("     DEADLOCKDOCTOR LOGGER TESTS          \n");
    printf("==========================================\n\n");

    test_logger_lifecycle_and_write();
    test_logger_print();

    printf("==========================================\n");
    printf("     ALL LOGGER TEST CASES PASSED         \n");
    printf("==========================================\n");
    return 0;
}
