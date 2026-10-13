#ifndef DEADLOCKDOCTOR_COMMON_H
#define DEADLOCKDOCTOR_COMMON_H

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <ctype.h>
#include <pthread.h>

#define MAX_PROCESSES 32
#define MAX_RESOURCES 32
#define MAX_NAME_LEN  64
#define MAX_CMD_LEN   1024

typedef enum {
    PROCESS_STATE_READY,
    PROCESS_STATE_RUNNING,
    PROCESS_STATE_BLOCKED,
    PROCESS_STATE_COMPLETED,
    PROCESS_STATE_ABORTED
} ProcessState;

typedef enum {
    ALLOC_GRANTED,
    ALLOC_BLOCKED,
    ALLOC_RELEASED,
    ALLOC_INVALID
} AllocResult;

const char *process_state_to_string(ProcessState state);
const char *alloc_result_to_string(AllocResult res);

#endif /* DEADLOCKDOCTOR_COMMON_H */
