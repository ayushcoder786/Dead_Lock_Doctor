#include "timeline.h"
#include <stdarg.h>

const char *timeline_event_type_to_string(TimelineEventType type) {
    switch (type) {
        case TIMELINE_PROCESS_CREATE:  return "PROC_CREATE";
        case TIMELINE_PROCESS_TERM:    return "PROC_TERM";
        case TIMELINE_RESOURCE_CREATE: return "RES_CREATE";
        case TIMELINE_RESOURCE_REQUEST:return "RES_REQUEST";
        case TIMELINE_RESOURCE_GRANT:  return "RES_GRANT";
        case TIMELINE_RESOURCE_BLOCK:  return "RES_BLOCK";
        case TIMELINE_RESOURCE_RELEASE:return "RES_RELEASE";
        case TIMELINE_BANKER_CHECK:    return "BANKER_CHECK";
        case TIMELINE_DEADLOCK_DETECT: return "DEADLOCK_DET";
        case TIMELINE_RECOVERY_ABORT:  return "RECOVERY";
        case TIMELINE_STRATEGY_CHANGE: return "STRATEGY";
        case TIMELINE_SCENARIO_STEP:   return "SCENARIO_STEP";
        case TIMELINE_INFO:            return "INFO";
        default:                       return "EVENT";
    }
}

void timeline_init(EventTimeline *tl) {
    if (!tl) return;
    tl->events = NULL;
    tl->count = 0;
    tl->capacity = 0;
    clock_gettime(CLOCK_MONOTONIC, &tl->start_time);
}

void timeline_cleanup(EventTimeline *tl) {
    if (!tl) return;
    if (tl->events) {
        free(tl->events);
        tl->events = NULL;
    }
    tl->count = 0;
    tl->capacity = 0;
}

void timeline_clear(EventTimeline *tl) {
    if (!tl) return;
    tl->count = 0;
    clock_gettime(CLOCK_MONOTONIC, &tl->start_time);
}

void timeline_record(EventTimeline *tl, TimelineEventType type,
                     const char *proc_name, const char *res_name, int units,
                     const char *snapshot, const char *fmt, ...) {
    if (!tl) return;

    if (tl->count >= tl->capacity) {
        int new_cap = tl->capacity == 0 ? 32 : tl->capacity * 2;
        TimelineEvent *new_arr = (TimelineEvent *)realloc(tl->events, new_cap * sizeof(TimelineEvent));
        if (!new_arr) return;
        tl->events = new_arr;
        tl->capacity = new_cap;
    }

    TimelineEvent *ev = &tl->events[tl->count];
    ev->seq_num = tl->count + 1;
    clock_gettime(CLOCK_MONOTONIC, &ev->timestamp);

    double sec = (double)(ev->timestamp.tv_sec - tl->start_time.tv_sec);
    double nsec = (double)(ev->timestamp.tv_nsec - tl->start_time.tv_nsec);
    ev->offset_ms = (sec * 1000.0) + (nsec / 1000000.0);
    if (ev->offset_ms < 0.0) ev->offset_ms = 0.0;

    ev->type = type;

    if (proc_name) {
        strncpy(ev->process_name, proc_name, sizeof(ev->process_name) - 1);
        ev->process_name[sizeof(ev->process_name) - 1] = '\0';
    } else {
        ev->process_name[0] = '\0';
    }

    if (res_name) {
        strncpy(ev->resource_name, res_name, sizeof(ev->resource_name) - 1);
        ev->resource_name[sizeof(ev->resource_name) - 1] = '\0';
    } else {
        ev->resource_name[0] = '\0';
    }

    ev->units = units;

    if (snapshot) {
        strncpy(ev->state_snapshot, snapshot, sizeof(ev->state_snapshot) - 1);
        ev->state_snapshot[sizeof(ev->state_snapshot) - 1] = '\0';
    } else {
        ev->state_snapshot[0] = '\0';
    }

    if (fmt) {
        va_list args;
        va_start(args, fmt);
        vsnprintf(ev->description, sizeof(ev->description), fmt, args);
        va_end(args);
    } else {
        ev->description[0] = '\0';
    }

    tl->count++;
}

void timeline_show(const EventTimeline *tl, int max_events, const char *filter) {
    if (!tl) return;

    printf("========================================================================================================================\n");
    printf("                                            SYSTEM EVENT TIMELINE (CHRONOLOGICAL)                                      \n");
    printf("========================================================================================================================\n");
    printf("+-------+-----------+---------------+--------------------+------------------------------------+------------------------+\n");
    printf("| Seq # | Time (ms) | Event Type    | Entity / Target    | Details / Outcome                  | System State Snapshot  |\n");
    printf("+-------+-----------+---------------+--------------------+------------------------------------+------------------------+\n");

    if (tl->count == 0) {
        printf("| %-118s |\n", "  [No events recorded in timeline]");
    } else {
        int start_idx = 0;
        int displayed = 0;

        if (max_events > 0 && tl->count > max_events && !filter) {
            start_idx = tl->count - max_events;
        }

        for (int i = start_idx; i < tl->count; i++) {
            const TimelineEvent *ev = &tl->events[i];

            if (filter && filter[0] != '\0') {
                bool match = false;
                if (strcasestr(timeline_event_type_to_string(ev->type), filter)) match = true;
                if (strcasestr(ev->process_name, filter)) match = true;
                if (strcasestr(ev->resource_name, filter)) match = true;
                if (strcasestr(ev->description, filter)) match = true;
                if (!match) continue;
            }

            char entity[MAX_NAME_LEN * 2 + 32] = "";
            if (ev->process_name[0] != '\0' && ev->resource_name[0] != '\0') {
                snprintf(entity, sizeof(entity), "%s -> %s (%d)", ev->process_name, ev->resource_name, ev->units);
            } else if (ev->process_name[0] != '\0') {
                snprintf(entity, sizeof(entity), "Proc: %s", ev->process_name);
            } else if (ev->resource_name[0] != '\0') {
                snprintf(entity, sizeof(entity), "Res: %s (%d)", ev->resource_name, ev->units);
            } else {
                strncpy(entity, "-", sizeof(entity) - 1);
            }

            printf("| #%-4d | %8.2f  | %-13s | %-18s | %-34.34s | %-22.22s |\n",
                   ev->seq_num, ev->offset_ms,
                   timeline_event_type_to_string(ev->type),
                   entity,
                   ev->description,
                   ev->state_snapshot[0] != '\0' ? ev->state_snapshot : "-");

            displayed++;
            if (max_events > 0 && filter && displayed >= max_events) break;
        }
    }

    printf("+-------+-----------+---------------+--------------------+------------------------------------+------------------------+\n");
    printf("Total Events Recorded: %d | Monotonic Timeline Origin: clock_gettime(CLOCK_MONOTONIC)\n", tl->count);
    printf("========================================================================================================================\n");
}
