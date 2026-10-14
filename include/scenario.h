#ifndef DEADLOCKDOCTOR_SCENARIO_H
#define DEADLOCKDOCTOR_SCENARIO_H

#include "common.h"
#include "manager.h"

typedef struct {
    char **lines;
    int total_lines;
    int current_step;
    char filepath[MAX_CMD_LEN];
} ScenarioStepSession;

bool scenario_replay_file(SystemManager *sm, const char *filepath);
bool scenario_execute_line(SystemManager *sm, const char *line, int step_num);

void scenario_step_session_init(ScenarioStepSession *sess);
void scenario_step_session_cleanup(ScenarioStepSession *sess);
bool scenario_step_session_load(ScenarioStepSession *sess, const char *filepath);
bool scenario_step_session_next(ScenarioStepSession *sess, SystemManager *sm);
void scenario_step_session_run_interactive(SystemManager *sm, const char *filepath);

#endif /* DEADLOCKDOCTOR_SCENARIO_H */
