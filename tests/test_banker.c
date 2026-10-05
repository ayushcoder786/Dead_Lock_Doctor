#include <assert.h>
#include "common.h"
#include "banker.h"

/* Helper to setup the classic Silberschatz 5-process, 3-resource Banker's example */
static void setup_classic_banker_system(BankerSystem *bs) {
    banker_system_init(bs);
    banker_system_allocate(bs, 5, 3);

    snprintf(bs->resource_names[0], MAX_NAME_LEN, "R0");
    snprintf(bs->resource_names[1], MAX_NAME_LEN, "R1");
    snprintf(bs->resource_names[2], MAX_NAME_LEN, "R2");

    bs->available[0] = 3;
    bs->available[1] = 3;
    bs->available[2] = 2;

    int alloc[5][3] = {
        {0, 1, 0}, /* P0 */
        {2, 0, 0}, /* P1 */
        {3, 0, 2}, /* P2 */
        {2, 1, 1}, /* P3 */
        {0, 0, 2}  /* P4 */
    };

    int max[5][3] = {
        {7, 5, 3}, /* P0 */
        {3, 2, 2}, /* P1 */
        {9, 0, 2}, /* P2 */
        {2, 2, 2}, /* P3 */
        {4, 3, 3}  /* P4 */
    };

    for (int i = 0; i < 5; i++) {
        bs->process_ids[i] = i;
        snprintf(bs->process_names[i], MAX_NAME_LEN, "P%d", i);
        for (int j = 0; j < 3; j++) {
            bs->allocation[i][j] = alloc[i][j];
            bs->max[i][j] = max[i][j];
            bs->need[i][j] = max[i][j] - alloc[i][j];
        }
    }
}

static void test_safe_state(void) {
    printf("[RUNNING] Test 1: Safe state detection & safe sequence generation\n");
    BankerSystem bs;
    setup_classic_banker_system(&bs);

    int *safe_seq = (int *)malloc(bs.num_processes * sizeof(int));
    int seq_len = 0;
    bool is_safe = banker_is_safe(&bs, safe_seq, &seq_len);

    assert(is_safe == true);
    assert(seq_len == 5);

    printf("  Discovered safe sequence: ");
    for (int i = 0; i < seq_len; i++) {
        printf("%s%s", bs.process_names[safe_seq[i]], (i == seq_len - 1) ? "\n" : " -> ");
    }

    /* Verify validity of sequence step by step */
    int work[3] = {3, 3, 2};
    for (int i = 0; i < seq_len; i++) {
        int p = safe_seq[i];
        for (int j = 0; j < 3; j++) {
            assert(bs.need[p][j] <= work[j]);
            work[j] += bs.allocation[p][j];
        }
    }

    free(safe_seq);
    banker_system_cleanup(&bs);
    printf("[PASS] Test 1 Passed.\n\n");
}

static void test_unsafe_state(void) {
    printf("[RUNNING] Test 2: Unsafe state detection\n");
    BankerSystem bs;
    setup_classic_banker_system(&bs);

    /* Constrain available resources so no process can satisfy its need */
    bs.available[0] = 0;
    bs.available[1] = 0;
    bs.available[2] = 0;

    int *safe_seq = (int *)malloc(bs.num_processes * sizeof(int));
    int seq_len = 0;
    bool is_safe = banker_is_safe(&bs, safe_seq, &seq_len);

    assert(is_safe == false);
    assert(seq_len < 5);

    printf("  Expected unsafe state verified. (Satisfiable processes: %d / %d)\n", seq_len, bs.num_processes);
    free(safe_seq);
    banker_system_cleanup(&bs);
    printf("[PASS] Test 2 Passed.\n\n");
}

static void test_safe_request(void) {
    printf("[RUNNING] Test 3: Safe request verification\n");
    BankerSystem bs;
    setup_classic_banker_system(&bs);

    /* P1 requests [1, 0, 2] */
    int *req = (int *)calloc(bs.num_resources, sizeof(int));
    req[0] = 1;
    req[1] = 0;
    req[2] = 2;

    BankerSystem new_state;
    banker_system_init(&new_state);
    int *safe_seq = (int *)malloc(bs.num_processes * sizeof(int));
    int seq_len = 0;

    bool is_safe = banker_check_request_safety(&bs, 1, req, &new_state, safe_seq, &seq_len);

    assert(is_safe == true);
    assert(seq_len == 5);

    /* Check updated state values */
    assert(new_state.available[0] == 2);
    assert(new_state.available[1] == 3);
    assert(new_state.available[2] == 0);
    assert(new_state.allocation[1][0] == 3);
    assert(new_state.allocation[1][2] == 2);
    assert(new_state.need[1][0] == 0);
    assert(new_state.need[1][2] == 0);

    printf("  Request P1 [1, 0, 2] is SAFE. Resulting sequence: ");
    for (int i = 0; i < seq_len; i++) {
        printf("%s%s", new_state.process_names[safe_seq[i]], (i == seq_len - 1) ? "\n" : " -> ");
    }

    free(req);
    free(safe_seq);
    banker_system_cleanup(&new_state);
    banker_system_cleanup(&bs);
    printf("[PASS] Test 3 Passed.\n\n");
}

static void test_unsafe_request(void) {
    printf("[RUNNING] Test 4: Unsafe request blocking & rollback\n");
    BankerSystem bs;
    setup_classic_banker_system(&bs);

    /* P4 requests [3, 3, 0].
     * Although [3, 3, 0] <= Available [3, 3, 2] and [3, 3, 0] <= Need [4, 3, 1],
     * granting it leaves Available = [0, 0, 2], which cannot satisfy ANY process's Need.
     */
    int *req = (int *)calloc(bs.num_resources, sizeof(int));
    req[0] = 3;
    req[1] = 3;
    req[2] = 0;

    BankerSystem new_state;
    banker_system_init(&new_state);
    int *safe_seq = (int *)malloc(bs.num_processes * sizeof(int));
    int seq_len = 0;

    bool is_safe = banker_check_request_safety(&bs, 4, req, &new_state, safe_seq, &seq_len);

    assert(is_safe == false);

    /* Ensure original system state remained unmodified */
    assert(bs.available[0] == 3);
    assert(bs.available[1] == 3);
    assert(bs.available[2] == 2);
    assert(bs.allocation[4][0] == 0);

    printf("  Request P4 [3, 3, 0] properly detected as UNSAFE and blocked.\n");
    free(req);
    free(safe_seq);
    banker_system_cleanup(&new_state);
    banker_system_cleanup(&bs);
    printf("[PASS] Test 4 Passed.\n\n");
}

int main(void) {
    printf("========================================\n");
    printf("  DEADLOCKDOCTOR BANKER'S ALGORITHM TESTS \n");
    printf("========================================\n\n");

    test_safe_state();
    test_unsafe_state();
    test_safe_request();
    test_unsafe_request();

    printf("========================================\n");
    printf("  ALL BANKER TEST CASES PASSED SUCCESSFULLY\n");
    printf("========================================\n");
    return 0;
}
