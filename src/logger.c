#include "logger.h"
#include <stdarg.h>
#include <errno.h>

static int s_log_fd = -1;
static char s_log_path[MAX_CMD_LEN] = DEFAULT_LOG_FILE;
static pthread_mutex_t s_log_mutex = PTHREAD_MUTEX_INITIALIZER;

const char *logger_event_type_to_string(LogEventType type) {
    switch (type) {
        case LOG_PROCESS_CREATE:  return "PROCESS_CREATE";
        case LOG_PROCESS_TERM:    return "PROCESS_TERM";
        case LOG_RESOURCE_CREATE: return "RESOURCE_CREATE";
        case LOG_RESOURCE_REQUEST:return "RESOURCE_REQUEST";
        case LOG_RESOURCE_GRANT:  return "RESOURCE_GRANT";
        case LOG_RESOURCE_BLOCK:  return "RESOURCE_BLOCK";
        case LOG_RESOURCE_RELEASE:return "RESOURCE_RELEASE";
        case LOG_BANKER_RESULT:   return "BANKER_RESULT";
        case LOG_DEADLOCK_DETECT: return "DEADLOCK_DETECT";
        case LOG_RECOVERY_EVENT:  return "RECOVERY_EVENT";
        case LOG_INFO:            return "SYSTEM_INFO";
        default:                  return "UNKNOWN_EVENT";
    }
}

bool logger_init(const char *filepath) {
    pthread_mutex_lock(&s_log_mutex);

    if (s_log_fd >= 0) {
        close(s_log_fd);
        s_log_fd = -1;
    }

    if (filepath && strlen(filepath) > 0) {
        strncpy(s_log_path, filepath, sizeof(s_log_path) - 1);
        s_log_path[sizeof(s_log_path) - 1] = '\0';
    } else {
        strncpy(s_log_path, DEFAULT_LOG_FILE, sizeof(s_log_path) - 1);
        s_log_path[sizeof(s_log_path) - 1] = '\0';
    }

    /* Ensure directory exists if in a subdirectory (e.g. logs/) */
    char dir_copy[MAX_CMD_LEN];
    strncpy(dir_copy, s_log_path, sizeof(dir_copy) - 1);
    dir_copy[sizeof(dir_copy) - 1] = '\0';
    char *slash = strrchr(dir_copy, '/');
    if (slash) {
        *slash = '\0';
        mkdir(dir_copy, 0755);
    }

    /* POSIX open(): open for append, create with mode 0644 if non-existent */
    s_log_fd = open(s_log_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (s_log_fd < 0) {
        fprintf(stderr, "[Logger Error] Failed to open log file '%s' (POSIX open() errno=%d: %s)\n",
                s_log_path, errno, strerror(errno));
        pthread_mutex_unlock(&s_log_mutex);
        return false;
    }

    pthread_mutex_unlock(&s_log_mutex);
    return true;
}

void logger_close(void) {
    pthread_mutex_lock(&s_log_mutex);
    if (s_log_fd >= 0) {
        close(s_log_fd);
        s_log_fd = -1;
    }
    pthread_mutex_unlock(&s_log_mutex);
}

bool logger_is_open(void) {
    pthread_mutex_lock(&s_log_mutex);
    bool open_state = (s_log_fd >= 0);
    pthread_mutex_unlock(&s_log_mutex);
    return open_state;
}

void logger_log(LogEventType type, const char *fmt, ...) {
    if (!fmt) return;

    pthread_mutex_lock(&s_log_mutex);

    if (s_log_fd < 0) {
        /* Auto-initialize if not yet open */
        s_log_fd = open(s_log_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (s_log_fd < 0) {
            pthread_mutex_unlock(&s_log_mutex);
            return;
        }
    }

    /* Timestamp formatting */
    time_t now = time(NULL);
    struct tm tm_buf;
    localtime_r(&now, &tm_buf);
    char time_str[32];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &tm_buf);

    /* Format message content */
    char msg_buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg_buf, sizeof(msg_buf), fmt, args);
    va_end(args);

    /* Construct complete log line */
    char log_line[1280];
    int line_len = snprintf(log_line, sizeof(log_line), "[%s] [%s] %s\n",
                            time_str, logger_event_type_to_string(type), msg_buf);

    if (line_len > 0) {
        /* POSIX write() with full-write loop */
        ssize_t total_written = 0;
        while (total_written < line_len) {
            ssize_t written = write(s_log_fd, log_line + total_written, line_len - total_written);
            if (written < 0) {
                if (errno == EINTR) continue;
                break;
            }
            total_written += written;
        }
    }

    pthread_mutex_unlock(&s_log_mutex);
}

bool logger_print_log(void) {
    pthread_mutex_lock(&s_log_mutex);
    int fd = open(s_log_path, O_RDONLY);
    pthread_mutex_unlock(&s_log_mutex);

    if (fd < 0) {
        printf("[Logger Error] Cannot open log file '%s' for reading (errno=%d: %s).\n",
               s_log_path, errno, strerror(errno));
        return false;
    }

    printf("\n======================================================================\n");
    printf("         DEADLOCKDOCTOR SYSTEM LOG (%s)                       \n", s_log_path);
    printf("======================================================================\n\n");
    fflush(stdout);

    char buf[1024];
    ssize_t bytes_read;
    while ((bytes_read = read(fd, buf, sizeof(buf))) > 0) {
        ssize_t total_written = 0;
        while (total_written < bytes_read) {
            ssize_t w = write(STDOUT_FILENO, buf + total_written, bytes_read - total_written);
            if (w < 0) {
                if (errno == EINTR) continue;
                break;
            }
            total_written += w;
        }
    }

    fflush(stdout);
    printf("\n======================================================================\n");
    printf("         END OF SYSTEM LOG                                            \n");
    printf("======================================================================\n\n");
    fflush(stdout);

    /* POSIX close() */
    close(fd);
    return true;
}
