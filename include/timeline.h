#ifndef DEADLOCKDOCTOR_TIMELINE_H
#define DEADLOCKDOCTOR_TIMELINE_H

#include "common.h"
#include <time.h>

typedef enum {
    TIMELINE_PROCESS_CREATE,
    TIMELINE_PROCESS_TERM,
    TIMELINE_RESOURCE_CREATE,
    TIMELINE_RESOURCE_REQUEST,
    TIMELINE_RESOURCE_GRANT,
    TIMELINE_RESOURCE_BLOCK,
    TIMELINE_RESOURCE_RELEASE,
    TIMELINE_BANKER_CHECK,
    TIMELINE_DEADLOCK_DETECT,
    TIMELINE_RECOVERY_ABORT,
    TIMELINE_STRATEGY_CHANGE,
    TIMELINE_SCENARIO_STEP,
    TIMELINE_INFO
} TimelineEventType;

typedef struct {
    int seq_num;
    struct timespec timestamp;
    double offset_ms;
    TimelineEventType type;
    char process_name[MAX_NAME_LEN];
    char resource_name[MAX_NAME_LEN];
    int units;
    char description[256];
    char state_snapshot[128];
} TimelineEvent;

typedef struct {
    TimelineEvent *events;
    int count;
    int capacity;
    struct timespec start_time;
} EventTimeline;

void timeline_init(EventTimeline *tl);
void timeline_cleanup(EventTimeline *tl);
void timeline_record(EventTimeline *tl, TimelineEventType type,
                     const char *proc_name, const char *res_name, int units,
                     const char *snapshot, const char *fmt, ...);
void timeline_show(const EventTimeline *tl, int max_events, const char *filter);
void timeline_clear(EventTimeline *tl);
const char *timeline_event_type_to_string(TimelineEventType type);

#endif /* DEADLOCKDOCTOR_TIMELINE_H */
