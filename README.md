# DeadlockDoctor: Advanced Resource & Deadlock Management System

[![C11](https://img.shields.io/badge/Language-C11-blue.svg)](https://en.wikipedia.org/wiki/C11_(C_standard_revision))
[![POSIX](https://img.shields.io/badge/Standard-POSIX.1--2008-green.svg)](https://pubs.opengroup.org/)
[![AddressSanitizer](https://img.shields.io/badge/Memory%20Safety-AddressSanitizer%20Clean-brightgreen.svg)]()
[![Tests](https://img.shields.io/badge/Test%20Suite-14%2F14%20Passing-success.svg)]()

**DeadlockDoctor** is an advanced, production-grade Operating Systems educational and diagnostic framework written in C11 for Linux/Ubuntu. It integrates complete resource management, Dijkstra's Banker's algorithm, multi-instance matrix reduction deadlock detection, lowest-cost victim recovery, live ASCII visualization, POSIX multithreading contention resolution, empirical benchmark comparisons, timeout-based detection with false-alarm auditing, and full Linux system-call tracing.

---

## 1. Key Architectural Features

1. **Modular Resource Manager**:
   - Arbitrary processes and multi-instance resources with dynamic resizing.
   - Vectors: Allocation matrix, Max matrix, Need matrix, Available vector, Request vector.
   - Process states: `READY`, `RUNNING`, `BLOCKED`, `COMPLETED`, `ABORTED`.
   - Strict runtime invariant validation: $\forall R_j: Total(R_j) == Available(R_j) + \sum_i Allocated(P_i, R_j)$.
2. **Dijkstra's Banker's Algorithm (Avoidance)**:
   - Full safety algorithm computing valid execution sequences.
   - Tentative request evaluation with rollback on unsafe states.
   - Clear distinction between unsafe states and deadlocks.
3. **Multi-Instance Deadlock Detection**:
   - Silberschatz / Coffman matrix reduction algorithm using $Work$ and $Finish$ vectors.
   - Identifies the exact deadlocked process set.
   - Correctly prevents false positives in multi-instance resources when unblocked holders can release units.
4. **Lowest-Cost Deadlock Recovery**:
   - 10-step recovery lifecycle.
   - Candidate cost formula: $Cost = (Held \times 2) + (Pending \times 1) + Priority$.
   - Aborts simulated processes (never kills host OS processes), reclaims resources, unblocks pending requests, and re-runs detection iteratively.
5. **Real POSIX Threads Deadlock Demonstration (`demo/thread_deadlock.c`)**:
   - Two concurrent POSIX threads acquiring mutexes in opposing order.
   - Deterministic barrier coordination (`pthread_barrier_t`).
   - Contention detection via `pthread_mutex_trylock()`, circular wait identification, and cooperative yield recovery without hanging.
   - Havender's global lock ordering demonstration guaranteeing 100% deadlock prevention.
6. **Live Process Monitor & Resource Dashboard**:
   - Real-time terminal tables displaying process state, held units, pending requests, and high-resolution waiting duration (`CLOCK_MONOTONIC`).
   - Resource utilization dashboard with percentage and visual gauges (`[============    ] 75.0%`).
   - Interactive live auto-refresh mode (`live` / `top`).
7. **Event Timeline & Deterministic Step Replay**:
   - Chronological event timeline recording every request, grant, block, release, and snapshot.
   - Interactive step-by-step scenario replay (`step <file>`).
8. **Algorithm Comparison Mode & Reproducible Stress Testing**:
   - Pre-generated identical transaction traces executed across Resource Ordering, Banker's Avoidance, and Detection & Recovery.
   - Configurable stress testing with process count, resource count, operation count, and random seed.
9. **Timeout-Based Deadlock Detection & False-Alarm Audit**:
   - Configurable waiting timeout threshold.
   - Classifies blocked processes into NORMAL, SUSPECTED, CONFIRMED DEADLOCK, and FALSE ALARM (ordinary contention/starvation).
   - Measures false alarm rate against safe interleaving workloads.
10. **Linux System-Programming & Syscall Tracing**:
    - Demonstrations of `fork()`, `execvp()`, `waitpid()`, anonymous pipes, named FIFOs (`mkfifo`), and signal handling (`sigaction`).
    - Dynamic `strace` kernel tracing integration and architecture documentation.

---

## 2. Compilation & Testing

```bash
# Build the target binary and worker executables
make clean && make

# Run the complete test suite (14 test binaries)
make test

# Run tests under AddressSanitizer
make test_asan
```

---

## 3. CLI Command Reference

Launch the interactive shell with `./deadlockdoctor`:

| Command | Syntax | Description |
| :--- | :--- | :--- |
| `help` | `help` | Display command reference |
| `status` | `status` | Display system matrices (Alloc, Max, Need, Available) |
| `monitor` | `monitor` | Live Process Monitor (states, held units, pending, wait duration) |
| `dashboard`| `dashboard` | Resource Utilization Dashboard (total, alloc, avail, %, gauge) |
| `live` | `live` | Interactive auto-refreshing process and resource dashboard |
| `timeline` | `timeline [max\|filter\|clear]` | Chronological event timeline with state snapshots |
| `step` | `step <scenario-file>` | Interactive step-by-step scenario replay |
| `process` | `process create <name> [id]` | Register a new simulated process |
| `process` | `process kill <name\|id>` | Terminate process and reclaim resources |
| `resource` | `resource create <name> <units>`| Register a multi-instance resource |
| `max` | `max <proc> <res> <units>` | Declare process maximum resource demand |
| `request` | `request <proc> <res> <units>` | Request resource units for process |
| `release` | `release <proc> <res> <units>` | Release held resource units |
| `banker` | `banker` | Run Banker's Safety Algorithm to find safe sequence |
| `detect` | `detect` | Run Deadlock Detector (Matrix reduction + WFG cycle search)|
| `graph` | `graph` | Display live Resource Allocation Graph & Wait-For Graph |
| `recover` | `recover [pid]` | Run lowest-cost victim deadlock recovery |
| `timeout` | `timeout [threshold_ms]` | Run timeout-based detection and false-alarm audit |
| `compare` | `compare [p] [r] [ops] [seed]` | Algorithm comparison mode on equivalent workload trace |
| `stress` | `stress [p] [r] [ops] [seed]` | Reproducible stress testing with configurable parameters |
| `trace` | `trace` | System-call tracing demonstration using strace |
| `demo` | `demo [all\|thread\|fork\|ipc]`| Run Linux system-programming demonstrations |
| `exit` | `exit` | Cleanly exit DeadlockDoctor |

---

## 4. Documentation Index

- [Rubric Audit & Compliance Matrix](docs/RUBRIC_AUDIT.md)
- [Design Architecture](docs/DESIGN.md)
- [Strategy Trade-Off Analysis](docs/TRADEOFFS.md)
- [Empirical Benchmarks & Methodology](docs/BENCHMARKS.md)
- [System-Call Tracing Reference](docs/SYSCALL_TRACING.md)
- [Course Outcome (CO) Mapping](docs/CO_MAPPING.md)
- [Testing Guide & Verification](docs/TESTING.md)
- [Viva Voce Questions & Answers](docs/VIVA_QUESTIONS.md)
- [Implementation Status](docs/IMPLEMENTATION_STATUS.md)
