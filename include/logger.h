#ifndef DEADLOCKDOCTOR_LOGGER_H
#define DEADLOCKDOCTOR_LOGGER_H

#include "common.h"
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>

#define DEFAULT_LOG_FILE "logs/deadlockdoctor.log"

typedef enum {
    LOG_PROCESS_CREATE,
    LOG_PROCESS_TERM,
    LOG_RESOURCE_CREATE,
    LOG_RESOURCE_REQUEST,
    LOG_RESOURCE_GRANT,
    LOG_RESOURCE_BLOCK,
    LOG_RESOURCE_RELEASE,
    LOG_BANKER_RESULT,
    LOG_DEADLOCK_DETECT,
    LOG_RECOVERY_EVENT,
    LOG_INFO
} LogEventType;

/**
 * Initialize the system logger using POSIX open().
 * Opens the specified filepath with O_WRONLY | O_CREAT | O_APPEND.
 * Ensures the parent directory exists.
 */
bool logger_init(const char *filepath);

/**
 * Close the logger file descriptor using POSIX close().
 */
void logger_close(void);

/**
 * Check whether the logger file descriptor is open and valid.
 */
bool logger_is_open(void);

/**
 * Log an event using POSIX write() with timestamp and event tag.
 */
void logger_log(LogEventType type, const char *fmt, ...);

/**
 * Print the contents of the log file to stdout using POSIX open(), read(), and write().
 */
bool logger_print_log(void);

/**
 * Returns the tag string associated with a LogEventType.
 */
const char *logger_event_type_to_string(LogEventType type);

#endif /* DEADLOCKDOCTOR_LOGGER_H */
