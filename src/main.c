#include "common.h"
#include "manager.h"
#include "scenario.h"
#include "logger.h"
#include "process_demo.h"
#include "ipc.h"

int main(int argc, char *argv[]) {
    if (argc > 1) {
        if (strcmp(argv[1], "--worker") == 0) {
            return process_demo_worker_main(argc - 1, &argv[1]);
        }
        if (strcmp(argv[1], "--log") == 0 || strcmp(argv[1], "-l") == 0) {
            logger_print_log();
            return 0;
        }
        if (strcmp(argv[1], "--fork-demo") == 0 || strcmp(argv[1], "-f") == 0 ||
            strcmp(argv[1], "--process-demo") == 0) {
            bool ok = process_demo_run();
            return ok ? 0 : 1;
        }
        if (strcmp(argv[1], "--ipc-demo") == 0 || strcmp(argv[1], "-i") == 0) {
            bool ok = ipc_demo_run_all(NULL);
            return ok ? 0 : 1;
        }
    }

    SystemManager sm;
    system_manager_init(&sm);

    if (argc > 1) {
        if (strcmp(argv[1], "--posix-read") == 0 && argc > 2) {
            bool ok = scenario_replay_file(&sm, argv[2]);
            system_manager_cleanup(&sm);
            return ok ? 0 : 1;
        }

        bool ok = scenario_replay_file(&sm, argv[1]);
        system_manager_cleanup(&sm);
        return ok ? 0 : 1;
    }

    printf("======================================================================\n");
    printf("        DeadlockDoctor - Resource & Deadlock Manager (CLI)            \n");
    printf("======================================================================\n");
    printf("Type 'help' for available commands or 'exit' to quit.\n\n");

    system_manager_run_cli(&sm);

    system_manager_cleanup(&sm);
    return 0;
}
