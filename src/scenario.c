#include "scenario.h"
#include "logger.h"
#include "recovery.h"
#include "process_demo.h"
#include "ipc.h"
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

/* POSIX Buffered File Reader implementing Linux file-system concepts */
typedef struct {
    int fd;
    char buffer[4096];
    ssize_t buf_pos;
    ssize_t buf_size;
} PosixFileReader;

static void posix_reader_init(PosixFileReader *reader, int fd) {
    reader->fd = fd;
    reader->buf_pos = 0;
    reader->buf_size = 0;
}

static ssize_t posix_reader_readline(PosixFileReader *reader, char *dest, size_t max_len) {
    if (!reader || !dest || max_len == 0) return -1;
    size_t out_pos = 0;

    while (out_pos + 1 < max_len) {
        if (reader->buf_pos >= reader->buf_size) {
            ssize_t n = read(reader->fd, reader->buffer, sizeof(reader->buffer));
            if (n <= 0) {
                if (out_pos == 0) return n; /* 0 for EOF, negative for error */
                break; /* EOF reached after partial line */
            }
            reader->buf_size = n;
            reader->buf_pos = 0;
        }

        char c = reader->buffer[reader->buf_pos++];
        dest[out_pos++] = c;
        if (c == '\n') break;
    }
    dest[out_pos] = '\0';
    return (ssize_t)out_pos;
}

bool scenario_execute_line(SystemManager *sm, const char *line, int step_num) {
    if (!sm || !line) return false;

    char cmd[64];
    int scanned = sscanf(line, "%63s", cmd);
    if (scanned <= 0) return true; /* empty line */

    if (strcasecmp(cmd, "PROCESS") == 0) {
        char name[MAX_NAME_LEN];
        int id = -1;
        int count = sscanf(line, "%*s %63s %d", name, &id);
        if (count == 1) {
            /* Try to infer ID from name e.g. P1 -> 1, else use pm.count + 1 */
            if ((name[0] == 'P' || name[0] == 'p') && isdigit((unsigned char)name[1])) {
                id = atoi(&name[1]);
            } else {
                id = sm->pm.count + 1;
            }
        } else if (count < 1) {
            fprintf(stderr, "[Scenario Error] Step %d: Invalid PROCESS syntax. Usage: PROCESS <name> [id]\n", step_num);
            return false;
        }

        Process *p = system_manager_create_process(sm, id, name);
        if (p) {
            printf("[OK] Process created: ID=%d, Name='%s', State=%s\n",
                   p->id, p->name, process_state_to_string(p->state));
        } else {
            fprintf(stderr, "[Scenario Error] Step %d: Failed to create process '%s'.\n", step_num, name);
        }
        return (p != NULL);
    } else if (strcasecmp(cmd, "RESOURCE") == 0) {
        char name[MAX_NAME_LEN];
        int instances = 1;
        int id = -1;
        int count = sscanf(line, "%*s %63s %d %d", name, &instances, &id);
        if (count == 1) {
            instances = 1;
            if ((name[0] == 'R' || name[0] == 'r') && isdigit((unsigned char)name[1])) {
                id = atoi(&name[1]);
            } else {
                id = sm->rm.count + 1;
            }
        } else if (count == 2) {
            /* Try to infer ID from name e.g. R1 -> 1, else use rm.count + 1 */
            if ((name[0] == 'R' || name[0] == 'r') && isdigit((unsigned char)name[1])) {
                id = atoi(&name[1]);
            } else {
                id = sm->rm.count + 1;
            }
        } else if (count < 1) {
            fprintf(stderr, "[Scenario Error] Step %d: Invalid RESOURCE syntax. Usage: RESOURCE <name> [instances] [id]\n", step_num);
            return false;
        }

        Resource *r = system_manager_create_resource(sm, id, name, instances);
        if (r) {
            printf("[OK] Resource created: ID=%d, Name='%s', Total Instances=%d\n",
                   r->id, r->name, r->total_instances);
        } else {
            fprintf(stderr, "[Scenario Error] Step %d: Failed to create resource '%s'.\n", step_num, name);
        }
        return (r != NULL);
    } else if (strcasecmp(cmd, "MAX") == 0 || strcasecmp(cmd, "SET-MAX") == 0) {
        char proc[MAX_NAME_LEN], res[MAX_NAME_LEN];
        int instances = 0;
        if (sscanf(line, "%*s %63s %63s %d", proc, res, &instances) == 3) {
            return system_manager_set_max(sm, proc, res, instances);
        } else {
            fprintf(stderr, "[Scenario Error] Step %d: Invalid MAX syntax. Usage: MAX <proc> <res> <instances>\n", step_num);
            return false;
        }
    } else if (strcasecmp(cmd, "REQUEST") == 0 || strcasecmp(cmd, "REQ") == 0) {
        char proc[MAX_NAME_LEN], res[MAX_NAME_LEN];
        int instances = 0;
        if (sscanf(line, "%*s %63s %63s %d", proc, res, &instances) == 3) {
            AllocResult res_alloc = system_manager_request(sm, proc, res, instances);
            return (res_alloc != ALLOC_INVALID);
        } else {
            fprintf(stderr, "[Scenario Error] Step %d: Invalid REQUEST syntax. Usage: REQUEST <proc> <res> <instances>\n", step_num);
            return false;
        }
    } else if (strcasecmp(cmd, "RELEASE") == 0 || strcasecmp(cmd, "REL") == 0) {
        char proc[MAX_NAME_LEN], res[MAX_NAME_LEN];
        int instances = 0;
        if (sscanf(line, "%*s %63s %63s %d", proc, res, &instances) == 3) {
            AllocResult res_alloc = system_manager_release(sm, proc, res, instances);
            return (res_alloc != ALLOC_INVALID);
        } else {
            fprintf(stderr, "[Scenario Error] Step %d: Invalid RELEASE syntax. Usage: RELEASE <proc> <res> <instances>\n", step_num);
            return false;
        }
    } else if (strcasecmp(cmd, "DETECT") == 0 || strcasecmp(cmd, "DEADLOCK") == 0) {
        system_manager_run_detector(sm);
        return true;
    } else if (strcasecmp(cmd, "BANKER") == 0 || strcasecmp(cmd, "SAFETY") == 0) {
        BankerSystem bs;
        banker_system_init(&bs);
        banker_init_from_manager(&bs, &sm->pm, &sm->rm);
        banker_print_safety_report(&bs);
        banker_system_cleanup(&bs);
        return true;
    } else if (strcasecmp(cmd, "STATUS") == 0) {
        system_manager_show_status(sm);
        return true;
    } else if (strcasecmp(cmd, "GRAPH") == 0 || strcasecmp(cmd, "RAG") == 0 || strcasecmp(cmd, "WFG") == 0) {
        system_manager_show_graph(sm);
        return true;
    } else if (strcasecmp(cmd, "LOG") == 0 || strcasecmp(cmd, "SHOW-LOG") == 0) {
        logger_print_log();
        return true;
    } else if (strcasecmp(cmd, "RECOVER") == 0) {
        int pid = -1;
        if (sscanf(line, "%*s %d", &pid) == 1) {
            return recovery_abort_process(sm, pid);
        } else {
            return recovery_run(sm);
        }
    } else if (strcasecmp(cmd, "ABORT") == 0) {
        int pid = -1;
        if (sscanf(line, "%*s %d", &pid) == 1) {
            return recovery_abort_process(sm, pid);
        } else {
            fprintf(stderr, "[Scenario Error] Step %d: Usage: ABORT <pid>\n", step_num);
            return false;
        }
    } else if (strcasecmp(cmd, "PRIORITY") == 0 || strcasecmp(cmd, "SET-PRIORITY") == 0) {
        char proc[MAX_NAME_LEN];
        int prio = 1;
        if (sscanf(line, "%*s %63s %d", proc, &prio) == 2) {
            Process *p = process_find_by_id_or_name(&sm->pm, proc);
            if (p) {
                process_set_priority(p, prio);
                printf("[OK] Process '%s' priority set to %d.\n", p->name, prio);
                return true;
            }
        }
        fprintf(stderr, "[Scenario Error] Step %d: Usage: PRIORITY <proc> <val>\n", step_num);
        return false;
    } else if (strcasecmp(cmd, "BANKER-MODE") == 0 || strcasecmp(cmd, "MODE") == 0) {
        char opt[32] = "";
        if (sscanf(line, "%*s %31s", opt) == 1) {
            if (strcasecmp(opt, "ON") == 0 || strcasecmp(opt, "AVOIDANCE") == 0) {
                sm->banker_avoidance_enabled = true;
                printf("[OK] Banker avoidance mode ENABLED.\n");
            } else if (strcasecmp(opt, "OFF") == 0 || strcasecmp(opt, "DETECTION") == 0) {
                sm->banker_avoidance_enabled = false;
                printf("[OK] Banker avoidance mode DISABLED (Detection mode).\n");
            } else {
                fprintf(stderr, "[Scenario Error] Step %d: Usage: BANKER-MODE <ON|OFF>\n", step_num);
                return false;
            }
        }
        return true;
    } else if (strcasecmp(cmd, "FORK-DEMO") == 0 || strcasecmp(cmd, "PROCESS-DEMO") == 0) {
        return process_demo_run();
    } else if (strcasecmp(cmd, "IPC") == 0 || strcasecmp(cmd, "IPC-DEMO") == 0) {
        return ipc_demo_run_all(sm);
    } else {
        fprintf(stderr, "[Scenario Error] Step %d: Unknown command '%s'.\n", step_num, cmd);
        return false;
    }
}

bool scenario_replay_file(SystemManager *sm, const char *filepath) {
    if (!sm || !filepath) {
        fprintf(stderr, "[Scenario Error] Invalid arguments.\n");
        return false;
    }

    /* Linux POSIX open() */
    int fd = open(filepath, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "[Scenario Error] Cannot open scenario file '%s' (POSIX open() errno=%d: %s).\n",
                filepath, errno, strerror(errno));
        return false;
    }

    printf("\n======================================================================\n");
    printf(">>> REPLAYING SCENARIO (POSIX file I/O fd=%d): %s\n", fd, filepath);
    printf("======================================================================\n\n");

    logger_log(LOG_INFO, "POSIX file I/O: Reading scenario file '%s' (descriptor fd=%d).", filepath, fd);

    PosixFileReader reader;
    posix_reader_init(&reader, fd);

    char line[MAX_CMD_LEN];
    int step_num = 1;
    int error_count = 0;

    /* Linux POSIX read() loop via buffered line reader */
    while (posix_reader_readline(&reader, line, sizeof(line)) > 0) {
        /* Strip leading whitespace */
        char *ptr = line;
        while (*ptr == ' ' || *ptr == '\t') ptr++;

        /* Strip inline comments */
        char *comment = strchr(ptr, '#');
        if (comment) {
            *comment = '\0';
        }

        /* Strip trailing newline and spaces */
        size_t len = strlen(ptr);
        while (len > 0 && (ptr[len - 1] == '\n' || ptr[len - 1] == '\r' || ptr[len - 1] == ' ' || ptr[len - 1] == '\t')) {
            ptr[--len] = '\0';
        }

        /* Skip blank lines */
        if (len == 0) {
            continue;
        }

        /* Print operation banner matching requirement: [1] PROCESS P1 */
        printf("[%d] %s\n", step_num, ptr);
        if (!scenario_execute_line(sm, ptr, step_num)) {
            error_count++;
        }
        printf("\n");
        step_num++;
    }

    /* Linux POSIX close() */
    close(fd);

    logger_log(LOG_INFO, "POSIX file I/O: Finished replaying '%s' (%d operations, fd=%d closed).",
               filepath, step_num - 1, fd);

    if (error_count > 0) {
        printf(">>> SCENARIO COMPLETED: %d operations executed (%d warning(s)/error(s)) <<<\n\n",
               step_num - 1, error_count);
    } else {
        printf(">>> SCENARIO COMPLETED: %d operations successfully executed <<<\n\n",
               step_num - 1);
    }
    return (error_count == 0);
}
