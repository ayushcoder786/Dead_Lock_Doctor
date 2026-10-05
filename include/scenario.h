#ifndef DEADLOCKDOCTOR_SCENARIO_H
#define DEADLOCKDOCTOR_SCENARIO_H

#include "common.h"
#include "manager.h"

bool scenario_replay_file(SystemManager *sm, const char *filepath);

bool scenario_execute_line(SystemManager *sm, const char *line, int step_num);

#endif /* DEADLOCKDOCTOR_SCENARIO_H */
