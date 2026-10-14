#include "common.h"
#include "manager.h"
#include "monitor.h"
#include "timeline.h"
#include "trace.h"
#include "timeout.h"
#include "scenario.h"
#include "logger.h"
#include "process_demo.h"
#include "ipc.h"
#include "benchmark.h"

int main(int argc, char *argv[]) {
    if (argc > 1) {
        if (strcmp(argv[1], "--worker") == 0) {
            return process_demo_worker_main(argc - 1, &argv[1]);
        }
        if ((strcmp(argv[1], "--step") == 0 || strcmp(argv[1], "-s") == 0) && argc > 2) {
            SystemManager sm;
            system_manager_init(&sm);
            scenario_step_session_run_interactive(&sm, argv[2]);
            system_manager_cleanup(&sm);
            return 0;
        }
        if (strcmp(argv[1], "--timeline") == 0) {
            SystemManager sm;
            system_manager_init(&sm);
            timeline_show(&sm.timeline, 0, NULL);
            system_manager_cleanup(&sm);
            return 0;
        }
        if (strcmp(argv[1], "--monitor") == 0 || strcmp(argv[1], "-m") == 0) {
            SystemManager sm;
            system_manager_init(&sm);
            monitor_show_process_table(&sm.pm, &sm.rm);
            system_manager_cleanup(&sm);
            return 0;
        }
        if (strcmp(argv[1], "--dashboard") == 0) {
            SystemManager sm;
            system_manager_init(&sm);
            monitor_show_resource_dashboard(&sm.rm, &sm.pm);
            system_manager_cleanup(&sm);
            return 0;
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
        if (strcmp(argv[1], "--timeout-audit") == 0 || strcmp(argv[1], "-to") == 0) {
            double thresh = (argc > 2) ? atof(argv[2]) : 500.0;
            SystemManager sm;
            system_manager_init(&sm);
            TimeoutAuditResult tres;
            timeout_run_audit(&sm, thresh, &tres);
            timeout_print_audit_report(&tres);
            timeout_cleanup_audit(&tres);
            system_manager_cleanup(&sm);
            return 0;
        }
        if (strcmp(argv[1], "--trace") == 0 || strcmp(argv[1], "--strace") == 0 || strcmp(argv[1], "-t") == 0) {
            trace_run_demonstration();
            return 0;
        }
        if (strcmp(argv[1], "--compare") == 0 || strcmp(argv[1], "-c") == 0) {
            int p = (argc > 2) ? atoi(argv[2]) : 10;
            int r = (argc > 3) ? atoi(argv[3]) : 4;
            int ops = (argc > 4) ? atoi(argv[4]) : 80;
            unsigned int s = (argc > 5) ? (unsigned int)atoi(argv[5]) : 42;
            benchmark_compare_algorithms(p, r, ops, s);
            return 0;
        }
        if (strcmp(argv[1], "--stress") == 0) {
            int p = (argc > 2) ? atoi(argv[2]) : 20;
            int r = (argc > 3) ? atoi(argv[3]) : 6;
            int ops = (argc > 4) ? atoi(argv[4]) : 300;
            unsigned int s = (argc > 5) ? (unsigned int)atoi(argv[5]) : 777;
            benchmark_run_stress_test(p, r, ops, s);
            return 0;
        }
        if (strcmp(argv[1], "--benchmark") == 0 || strcmp(argv[1], "-b") == 0) {
            benchmark_run_suite();
            return 0;
        }
        if (strcmp(argv[1], "--strategy-compare") == 0 || strcmp(argv[1], "-sc") == 0) {
            system_manager_print_strategy_comparison();
            return 0;
        }
        if (strcmp(argv[1], "--demo") == 0 || strcmp(argv[1], "-d") == 0) {
            system_manager_run_full_demo();
            return 0;
        }
        if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
            system_manager_print_help();
            return 0;
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
