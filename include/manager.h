#ifndef DEADLOCKDOCTOR_MANAGER_H
#define DEADLOCKDOCTOR_MANAGER_H

#include "common.h"
#include "process.h"
#include "resource.h"
#include "banker.h"
#include "graph.h"
#include "detector.h"

typedef struct SystemManager {
    ProcessManager pm;
    ResourceManager rm;
    bool is_running;
    bool banker_avoidance_enabled;
} SystemManager;

void system_manager_init(SystemManager *sm);
void system_manager_cleanup(SystemManager *sm);

Process *system_manager_create_process(SystemManager *sm, int id, const char *name);
Resource *system_manager_create_resource(SystemManager *sm, int id, const char *name, int total_instances);
bool system_manager_terminate_process(SystemManager *sm, int id);

AllocResult system_manager_request(SystemManager *sm, const char *proc_ident, const char *res_ident, int units);
AllocResult system_manager_release(SystemManager *sm, const char *proc_ident, const char *res_ident, int units);
bool system_manager_set_max(SystemManager *sm, const char *proc_ident, const char *res_ident, int max_units);
void system_manager_check_pending_requests(SystemManager *sm);

void system_manager_run_detector(const SystemManager *sm);
void system_manager_show_graph(const SystemManager *sm);
void system_manager_show_status(const SystemManager *sm);
void system_manager_run_process_demo(void);
void system_manager_run_ipc_demo(SystemManager *sm);
void system_manager_run_thread_demo(void);
void system_manager_print_help(void);
void system_manager_run_cli(SystemManager *sm);

#endif /* DEADLOCKDOCTOR_MANAGER_H */
