# DeadlockDoctor: Testing Guide & Acceptance Verification

This guide outlines how to build, test, and verify **DeadlockDoctor** across all 14 automated test suites and runtime demonstrations.

---

## 1. Quick Build & Test Commands

```bash
# Clean build all targets
make clean && make

# Run all 14 unit test suites
make test

# Run the complete test suite under GCC AddressSanitizer (Memory Safety)
make test_asan

# Run scenario test suite under AddressSanitizer
make asan

# Run standard performance benchmarks
make benchmark
```

---

## 2. Test Suite Descriptions

| Target | Test Binary | Subsystem Tested | Key Verifications |
| :--- | :--- | :--- | :--- |
| `test_banker` | `build/test_banker_bin` | Banker's Algorithm | Safe sequence discovery, unsafe detection, request rollback, invariant $Need = Max - Alloc$. |
| `test_detector` | `build/test_detector_bin` | Deadlock Detector | Single-unit circular wait, 3-process cycle, multi-instance matrix reduction, unblocked holder cycle resolution. |
| `test_scenario` | `build/test_scenario_bin` | Scenario Replayer | Parses and executes `safe.txt`, `unsafe.txt`, `deadlock.txt`, `multi_instance.txt`, `recovery.txt`. |
| `test_logger` | `build/test_logger_bin` | POSIX Event Logger | `open()`, atomic append `write()`, timestamp formatting, log reading. |
| `test_process_demo` | `build/test_process_demo_bin` | Linux Processes | `fork()`, worker execution via `execvp()`, exit code harvest via `waitpid()`. |
| `test_graph` | `build/test_graph_bin` | Graph Visualizer | WFG node/edge management, DFS cycle traversal, cycle extraction. |
| `test_ipc` | `build/test_ipc_bin` | Inter-Process Comm | Anonymous pipe I/O, named FIFO `mkfifo()`, POSIX signal handling. |
| `test_recovery` | `build/test_recovery_bin` | Deadlock Recovery | Cost formula calculation, victim abortion, resource reclaim, auto-recovery loop. |
| `test_thread_deadlock`| `build/thread_deadlock` | POSIX Threads | Live thread mutex contention, barrier coordination, cooperative yield, global order prevention. |
| `test_monitor` | `build/test_monitor_bin` | Monitor & Dashboard | Process state, monotonic wait duration, resource utilization %, visual gauge, invariant audit. |
| `test_timeline` | `build/test_timeline_bin` | Event Timeline | Chronological event sequencing, state snapshots, step-by-step scenario execution. |
| `test_bench_compare`| `build/test_bench_compare_bin`| Benchmarks & Stress | Workload determinism, 3-strategy comparison on equivalent trace, multi-instance stress test. |
| `test_trace` | `build/test_trace_bin` | Syscall Tracing | System-call tracing probe and fallback architecture output. |
| `test_timeout` | `build/test_timeout_bin` | Timeout Detection | Configurable threshold, contention false-alarm classification, confirmed deadlock isolation. |

---

## 3. Live Demonstration Commands for Evaluator / Viva

```bash
# 1. Interactive CLI Shell
./deadlockdoctor

# 2. Replay Scenarios Directly
./deadlockdoctor scenarios/deadlock.txt
./deadlockdoctor scenarios/safe_interleaving.txt
./deadlockdoctor scenarios/multi_instance.txt

# 3. Interactive Step-by-Step Scenario Stepping
./deadlockdoctor --step scenarios/deadlock.txt

# 4. View Live Process Monitor & Resource Dashboard
./deadlockdoctor --monitor
./deadlockdoctor --dashboard

# 5. Algorithm Comparison Mode on Equivalent Workload Trace
./deadlockdoctor --compare 10 4 80 42

# 6. Reproducible Stress Test Suite
./deadlockdoctor --stress 20 6 300 777

# 7. Timeout-Based Detection & False-Alarm Audit
./deadlockdoctor --timeout-audit 500

# 8. POSIX Thread Live Deadlock & Prevention Demonstration
./build/thread_deadlock

# 9. Linux Kernel System-Programming Demonstrations
./deadlockdoctor --process-demo
./deadlockdoctor --ipc-demo
./deadlockdoctor --trace
```
