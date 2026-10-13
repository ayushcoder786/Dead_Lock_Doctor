# DeadlockDoctor: Intelligent Resource Management, Deadlock Detection, Prevention, and Recovery System

[![C11](https://img.shields.io/badge/Language-C11-blue.svg)](https://en.wikipedia.org/wiki/C11_(C_standard_revision))
[![POSIX](https://img.shields.io/badge/Standard-POSIX.1--2008-green.svg)](https://pubs.opengroup.org/onlinepubs/9699919799/)
[![Build Status](https://img.shields.io/badge/Build-Passing-brightgreen.svg)]()
[![AddressSanitizer](https://img.shields.io/badge/ASan-Clean-brightgreen.svg)]()
[![Valgrind](https://img.shields.io/badge/Valgrind-0%20Leaks-brightgreen.svg)]()

**DeadlockDoctor** is a robust, modular, educational Operating Systems project implemented in standard C11 for Ubuntu Linux (and WSL2). It serves as both a full-featured multi-instance resource management simulator and an interactive Linux shell with real POSIX process execution (`fork()`, `execvp()`, `waitpid()`), GNU Readline command history navigation, and live Linux synchronization demonstrations.

Designed specifically for university Operating Systems laboratory curricula, technical project reviews, and viva voce examinations, DeadlockDoctor maps directly to Course Outcomes **CO-1 through CO-6**.

---

## Table of Contents
1. [Project Directory Structure](#1-project-directory-structure)
2. [Course Outcomes (CO-1 to CO-6) Mapping](#2-course-outcomes-co-1-to-co-6-mapping)
3. [Key Features & Capabilities](#3-key-features--capabilities)
4. [Build Instructions](#4-build-instructions)
5. [Execution Modes & CLI Options](#5-execution-modes--cli-options)
6. [Interactive Shell Command Reference](#6-interactive-shell-command-reference)
7. [Deadlock Management Strategies](#7-deadlock-management-strategies)
8. [Core Algorithms & Mathematical Formulations](#8-core-algorithms--mathematical-formulations)
9. [Verification, Testing & Memory Safety](#9-verification-testing--memory-safety)
10. [Performance Benchmarks](#10-performance-benchmarks)
11. [Scenario Suite](#11-scenario-suite)
12. [College Viva Voce Walkthrough Guide](#12-college-viva-voce-walkthrough-guide)
13. [Documentation Index](#13-documentation-index)

---

## 1. Project Directory Structure

```
deadlockdoctor/
├── Makefile                          # Unified build system (C11, POSIX, ASan, Valgrind)
├── README.md                         # Primary project documentation and viva manual
├── demo/
│   ├── thread_deadlock.c             # Deterministic POSIX pthread deadlock & prevention (CO-6)
│   └── worker.c                      # Executable worker target for fork/exec demos (CO-2)
├── docs/
│   ├── ALGORITHMS.md                 # Detailed mathematical formulations and algorithmic traces
│   ├── BENCHMARKS.md                 # Empirical runtime performance and scalability analysis
│   ├── CO_MAPPING.md                 # Comprehensive Course Outcome CO-1 through CO-6 mapping
│   ├── DESIGN.md                     # Architectural design, data structures, state machines
│   ├── IMPLEMENTATION_STATUS.md      # Milestone-by-milestone verification checklist
│   ├── TESTING.md                    # Test matrices, AddressSanitizer, and Valgrind reports
│   └── VIVA_QUESTIONS.md             # Curated examiner viva questions with technical answers
├── include/
│   ├── banker.h                      # Banker's safety check and avoidance algorithm
│   ├── benchmark.h                   # High-resolution monotonic clock benchmark suite
│   ├── common.h                      # Shared definitions, limits, and system headers
│   ├── detector.h                    # Multi-instance reduction and WFG DFS cycle search
│   ├── graph.h                       # ASCII Resource Allocation Graph & Wait-For Graph
│   ├── ipc.h                         # Anonymous pipe, FIFO, and signal handlers (CO-3)
│   ├── logger.h                      # POSIX low-level file descriptor audit logger (CO-5)
│   ├── manager.h                     # Central manager, strategy controller, and CLI REPL
│   ├── process.h                     # Dynamic process management and state transitions
│   ├── process_demo.h                # fork(), execv(), and waitpid() lifecycle demo (CO-2)
│   ├── recovery.h                    # Cost formula evaluation, victim abort, resource cleanup
│   ├── resource.h                    # Multi-instance typed resource inventory
│   └── scenario.h                    # Scenario replay parser using POSIX open/read/close
├── logs/
│   └── deadlockdoctor.log            # Persistent system audit log
├── scenarios/
│   ├── deadlock.txt                  # Classical 2-process circular wait scenario
│   ├── graph_demo.txt                # Bipartite RAG and WFG visualization scenario
│   ├── multi_instance.txt            # Multi-instance resource deadlock scenario
│   ├── prevention_ordered.txt        # Havender's resource ordering prevention demonstration
│   ├── recovery.txt                  # Deadlock followed by lowest-cost victim recovery
│   ├── safe.txt                      # 3-process, 3-resource safe Banker execution
│   ├── stress.txt                    # Concurrency stress test under high contention
│   └── unsafe.txt                    # Unsafe request rejection under Banker avoidance
├── src/
│   ├── banker.c                      # Banker's safety matrices and safe sequence solver
│   ├── benchmark.c                   # High-resolution empirical performance benchmark runner
│   ├── detector.c                    # Graph-based DFS and Work/Finish reduction algorithms
│   ├── graph.c                       # ASCII bipartite RAG and highlighted WFG renderer
│   ├── ipc.c                         # Pipe, mkfifo, and sigaction implementation
│   ├── logger.c                      # POSIX open(), write(), close() audit logging
│   ├── main.c                        # Entry point (CLI dispatcher, --demo, --benchmark)
│   ├── manager.c                     # System state coordinator, strategies, and REPL
│   ├── process.c                     # Dynamic process management & lifecycle transitions
│   ├── process_demo.c                # Process creation, image replacement, and reaping
│   ├── recovery.c                    # Cost formula evaluation, victim abort & cleanup
│   ├── resource.c                    # Resource inventory lifecycle & instance tracking
│   └── scenario.c                    # POSIX file I/O chunk reader and command dispatcher
└── tests/
    ├── test_banker.c                 # Banker's algorithm unit tests
    ├── test_detector.c               # Deadlock detector and cycle finding tests
    ├── test_graph.c                  # Graph rendering and cycle verification tests
    ├── test_ipc.c                    # Anonymous pipe, FIFO, and signal tests
    ├── test_logger.c                 # POSIX logger I/O verification tests
    ├── test_process_demo.c           # fork/exec/waitpid lifecycle tests
    ├── test_recovery.c               # Recovery cost, victim selection, auto-recovery tests
    └── test_scenario.c               # Scenario replay and error handling tests
```

---

## 2. Course Outcomes (CO-1 to CO-6) Mapping

| Course Outcome | Description | DeadlockDoctor Implementation & Evidence |
| :--- | :--- | :--- |
| **CO-1: Linux CLI & Environment** | Command-line interface, user-space program conventions, standard streams, exit codes, and POSIX system call usage. | Implemented in `src/main.c` and `src/manager.c`. Provides structured interactive CLI REPL with GNU Readline, history navigation, persistent history file (`~/.deadlockdoctor_history`), external command execution (`fork`/`execvp`/`waitpid`), clean stdout/stderr separation, and standard POSIX exit codes. |
| **CO-2: Process Lifecycle Management** | Real process creation, execution replacement, and synchronization using `fork()`, `exec()`, and `waitpid()`. | Implemented in `src/process_demo.c` and `demo/worker.c`. Parent forks children, replaces process images via `execv("./build/worker", ...)` with custom arguments, and synchronizes child termination using `waitpid(pid, &status, 0)` with `WIFEXITED(status)` verification to prevent zombie processes. |
| **CO-3: Inter-Process Communication & Signals** | Anonymous pipes, named pipes (FIFOs), and asynchronous signal handling. | Implemented in `src/ipc.c`. Demonstrates `pipe()` where worker transmits requests (`REQUEST P1 R1 1`) to manager, `mkfifo("/tmp/deadlockdoctor.fifo")` for named channel transfers, and `sigaction()` for `SIGUSR1` (worker notification), `SIGALRM` (timer-driven detector trigger), and `SIGTERM` (graceful shutdown) using `volatile sig_atomic_t` flags. |
| **CO-4: Dynamic Memory & Algorithm State** | Dynamic 2D matrix management (`malloc`, `calloc`, `realloc`, `free`), zero memory leaks, and AddressSanitizer/Valgrind compliance. | Dynamic allocation in `process.c`, `resource.c`, `banker.c`, `graph.c`, and `recovery.c`. Supports arbitrary numbers of processes and resources. Verified with `gcc -fsanitize=address` and `valgrind --leak-check=full` with **0 bytes in use at exit, 0 errors, 0 leaks**. |
| **CO-5: POSIX File I/O & Audit Logging** | Low-level file descriptor operations using `open()`, `read()`, `write()`, and `close()`. | Scenario reader in `src/scenario.c` uses `open(path, O_RDONLY)`, fixed-buffer `read()`, newline framing, and `close()`. System audit logger in `src/logger.c` writes timestamped events to `logs/deadlockdoctor.log` using `open(..., O_WRONLY \| O_CREAT \| O_APPEND, 0644)`, `write()`, and `close()`. |
| **CO-6: Multithreading & Synchronization** | POSIX threads (`pthreads`), mutexes, race conditions, deterministic deadlock observation, and prevention. | Implemented in `demo/thread_deadlock.c`. Demonstrates Configuration A (conflicting lock order with `pthread_mutex_trylock` conflict observation and victim yield recovery) versus Configuration B (Havender's global resource ordering prevention `R1 < R2` achieving 100% prevention with zero hangs). |

---

## 3. Key Features & Capabilities

- **Interactive GNU Readline Shell**:
  - Full Up/Down arrow history browsing.
  - Persistent command history preserved in `~/.deadlockdoctor_history`.
  - Tab autocompletion for built-in commands.
  - External Linux command execution (`ls`, `pwd`, `cat`, `nano`, `gcc`, etc.) via `fork()`, `execvp()`, and `waitpid()`.
- **Presentation-Ready Demonstration Mode (`--demo`)**:
  - Autonomous end-to-end OS viva walkthrough covering resource creation, Banker's safe sequence validation, unsafe request rejection, circular wait induction, live ASCII graph rendering, victim recovery, and verification.
- **Empirical High-Resolution Benchmarks (`--benchmark`)**:
  - Monotonic clock timing via `clock_gettime(CLOCK_MONOTONIC)`.
  - Scalability analysis across Small (5x3), Medium (15x5), and Large (50x10) workloads comparing Banker's avoidance, Resource Ordering prevention, and Detection & Recovery.
- **Four Selectable Deadlock Management Strategies**:
  1. `detection`: Classical detection and recovery via lowest-cost victim abortion.
  2. `ordered`: Havender's global resource ordering (prevention of circular wait).
  3. `all-or-nothing`: Conservative atomic allocation (prevention of hold-and-wait).
  4. `banker`: Dijkstra's Banker's Algorithm (avoidance via safe state analysis).
- **Live ASCII Visualizer**:
  - Bipartite Resource Allocation Graph (RAG) with explicit edge types:
    - `P --holds--> R` (Allocation edge)
    - `P ..waits..> R` (Request edge)
  - Wait-For Graph (WFG) with highlighted deadlocked cycle edges:
    - `Pi -> Pj` (Normal dependency)
    - `Pi ==> Pj` (Cycle / Deadlock edge)
- **10-Step Lowest-Cost Recovery Engine**:
  - Deterministic transparent cost evaluation:
    $$	ext{Cost} = (	ext{Held Units} 	imes 2) + (	ext{Pending Requests} 	imes 1) + 	ext{Priority}$$
  - Aborts victim in simulator, reclaims resources, unblocks pending processes, prunes WFG, and re-validates safety.

---

## 4. Build Instructions

### Prerequisites
- Ubuntu Linux (or WSL2 on Windows)
- GCC compiler supporting C11 (`gcc >= 9.0`)
- GNU Make
- POSIX Pthreads library (`-pthread`)
- GNU Readline library (`libreadline-dev`)

```bash
# Install dependencies on Ubuntu / WSL
sudo apt-get update
sudo apt-get install -y build-essential libreadline-dev valgrind
```

### Build Commands

```bash
# 1. Clean and build all binaries
make clean all

# 2. Run the complete automated test suite (all 9 test suites)
make test

# 3. Build and execute test suite with GCC AddressSanitizer
make asan

# 4. Run Valgrind full memory leak check
make valgrind
```

---

## 5. Execution Modes & CLI Options

DeadlockDoctor supports four distinct operational modes:

### Mode 1: Presentation-Ready Demonstration (`--demo`)
Runs an end-to-end, presentation-ready demonstration covering resource management, Banker's algorithm, circular-wait detection, graph rendering, and recovery:
```bash
./deadlockdoctor --demo
```

### Mode 2: Empirical Performance Benchmark (`--benchmark`)
Executes monotonic-clock benchmarks measuring request throughput, detection times, recoveries, and resource utilization across multiple concurrency scales:
```bash
./deadlockdoctor --benchmark
```

### Mode 3: Scenario Replay Mode
Replays an automated scenario text file and displays resulting states:
```bash
# Replay Safe State Scenario
./deadlockdoctor scenarios/safe.txt

# Replay Unsafe State Scenario (Banker avoidance demonstration)
./deadlockdoctor scenarios/unsafe.txt

# Replay Circular-Wait Deadlock Scenario
./deadlockdoctor scenarios/deadlock.txt

# Replay Multi-Instance Deadlock Scenario
./deadlockdoctor scenarios/multi_instance.txt

# Replay Lowest-Cost Recovery Scenario
./deadlockdoctor scenarios/recovery.txt

# Replay Resource Ordering Prevention Scenario
./deadlockdoctor scenarios/prevention_ordered.txt

# Replay Concurrency Stress Test Scenario
./deadlockdoctor scenarios/stress.txt
```

### Mode 4: Interactive Shell Mode
Launches the interactive Readline shell with history navigation:
```bash
./deadlockdoctor
```

---

## 6. Interactive Shell Command Reference

| Command | Syntax | Description | Example |
| :--- | :--- | :--- | :--- |
| **help** | `help` | Displays command reference guide with examples | `help` |
| **status** | `status` | Displays system status: Available, Allocation, Max, Need | `status` |
| **process create** | `process create <name> [id]` | Registers a new process into the system | `process create P1` |
| **process list** | `process list` | Lists all processes, states, and resource holdings | `process list` |
| **process kill** | `process kill <name\|id>` | Terminates a process and reclaims all held resources | `process kill P1` |
| **resource create** | `resource create <name> <units>` | Creates a multi-instance resource | `resource create R1 3` |
| **resource list** | `resource list` | Lists all resources, capacities, and available units | `resource list` |
| **max** | `max <proc> <res> <units>` | Sets maximum claim of process for resource | `max P1 R1 3` |
| **request** | `request <proc> <res> <units>` | Requests units of resource for a process | `request P1 R1 1` |
| **release** | `release <proc> <res> <units>` | Releases allocated resource units back to system | `release P1 R1 1` |
| **banker** | `banker` | Executes Banker's Safety Algorithm to find safe sequence | `banker` |
| **banker-mode** | `banker-mode <on\|off>` | Toggles Banker avoidance (on: blocks unsafe; off: allows) | `banker-mode on` |
| **strategy** | `strategy [mode\|compare]` | Sets active strategy or shows comparative analysis | `strategy ordered` |
| **detect** | `detect` | Runs WFG DFS cycle detection and prints deadlocked set | `detect` |
| **graph** | `graph` | Renders ASCII Resource Allocation Graph & Wait-For Graph | `graph` |
| **recover** | `recover` | Runs 10-step lowest-cost recovery algorithm | `recover` |
| **priority** | `priority <proc> <val>` | Configures process priority weight for recovery cost | `priority P1 10` |
| **benchmark** | `benchmark` | Runs empirical performance benchmarks | `benchmark` |
| **load** | `load <scenario-file>` | Replays commands from a scenario text file | `load scenarios/deadlock.txt` |
| **log** | `log` | Displays the persistent audit log file on the terminal | `log` |
| **demo** | `demo [thread\|fork\|ipc]` | Runs live OS demonstrations (pthread, fork/exec, IPC) | `demo thread` |
| **external cmds** | `pwd`, `ls`, `cat`, `nano`... | Executes external Linux binaries via fork/execvp | `ls -la` |
| **exit** | `exit` or `quit` | Cleans up all allocations and exits DeadlockDoctor | `exit` |

---

## 7. Deadlock Management Strategies

DeadlockDoctor implements and evaluates four distinct deadlock management paradigms:

| Strategy | Type | Mechanism | Trade-Offs |
| :--- | :--- | :--- | :--- |
| **Detection & Recovery** | Reactive | Permits requests; detects cycles via WFG DFS; aborts lowest-cost victim. | **High Concurrency**; zero runtime checks on allocation; requires recovery overhead when deadlock occurs. |
| **Resource Ordering** | Prevention | Enforces Havender's rule: processes must request resources in strictly ascending order ($R_i < R_{i+1}$). | **Zero Runtime Overhead**; 100% deadlock prevention; requires global ordering; rejects out-of-order requests. |
| **All-or-Nothing** | Prevention | Processes must claim and acquire all required resources atomically at start. | **Simple Implementation**; eliminates hold-and-wait; causes resource under-utilization and potential starvation. |
| **Banker's Avoidance** | Avoidance | Dynamically checks if granting request preserves at least one safe execution sequence. | **Safe Execution**; no process abortion required; requires advance declaration of Max claims; $O(m 	imes n^2)$ overhead. |

Use `strategy compare` in the interactive shell to view a formatted trade-off analysis table.

---

## 8. Core Algorithms & Mathematical Formulations

### 1. Banker's Safety & Avoidance Algorithm
- **Conservation Law**:
  $$\sum_{i=1}^{n} 	ext{Allocation}[i][j] + 	ext{Available}[j] = 	ext{TotalInstances}[j] \quad (orall j \in \{1,\dots,m\})$$
- **Need Matrix Invariant**:
  $$	ext{Need}[i][j] = 	ext{Max}[i][j] - 	ext{Allocation}[i][j] \ge 0$$
- **Safety Criterion**: Iteratively identifies process $P_i$ such that:
  $$	ext{Finish}[i] = 	ext{false} \quad \land \quad 	ext{Need}_i \le 	ext{Work}$$
  Upon finding $P_i$, sets $	ext{Work} = 	ext{Work} + 	ext{Allocation}_i$ and $	ext{Finish}[i] = 	ext{true}$. If all processes finish, the system is **SAFE** and a valid topological execution sequence is produced.

### 2. Wait-For Graph (WFG) & Cycle Detection
- Collapses the bipartite Resource Allocation Graph (RAG) into a direct dependency graph:
  $$P_i 	o P_j \iff P_i 	ext{ is waiting on a resource currently held by } P_j$$
- Depth-First Search (DFS) with 3-color vertex marking discovers directed cycles:
  - **WHITE**: Unvisited vertex.
  - **GRAY**: Active vertex in recursion stack (back-edge indicates a directed cycle).
  - **BLACK**: Fully explored vertex.

### 3. Multi-Instance Reduction Algorithm
For systems with multiple units per resource, DeadlockDoctor employs vector reduction:
1. Initialize $	ext{Work} = 	ext{Available}$.
2. For each process $i$:
   $$	ext{Finish}[i] = egin{cases} 	ext{true} & 	ext{if } 	ext{Allocation}_i = \mathbf{0} \ 	ext{false} & 	ext{otherwise} \end{cases}$$
3. Iteratively locate process $i$ where $	ext{Finish}[i] = 	ext{false}$ and $	ext{Request}_i \le 	ext{Work}$.
4. If found: $	ext{Work} = 	ext{Work} + 	ext{Allocation}_i$, $	ext{Finish}[i] = 	ext{true}$.
5. Any process with $	ext{Finish}[i] = 	ext{false}$ at termination is **DEADLOCKED**.

### 4. 10-Step Lowest-Cost Deadlock Recovery
When circular wait occurs:
1. **Identify deadlocked set** via cycle extraction.
2. **Evaluate candidate recovery cost**:
   $$	ext{Cost} = (	ext{Held Units} 	imes 2) + (	ext{Pending Requests} 	imes 1) + 	ext{Priority}$$
3. **Select victim** with minimum cost.
4. **Abort victim** in simulation model.
5. **Reclaim all resources** held by the victim.
6. **Update Available vector** and unblock pending requests.
7. **Prune victim** from Wait-For Graph.
8. **Re-run deadlock detection** to confirm cycle resolution.
9. **Execute Banker's safety check** to confirm system integrity.
10. **Report recovery outcome** to audit log and terminal.

---

## 9. Verification, Testing & Memory Safety

### Automated Test Suite (`make test`)
All 9 test suites pass 100%:
- `test_banker`: Safe sequence generation, unsafe request rejection, tentative allocation rollback.
- `test_detector`: 2-process cycle, 3-process cycle, acyclic states, multi-instance vector reduction.
- `test_scenario`: File replay of all scenarios and syntax error handling.
- `test_logger`: POSIX `open`, `write`, `read`, `close` verification and timestamp formatting.
- `test_process_demo`: `fork()`, `execv()`, and `waitpid()` lifecycle verification.
- `test_graph`: Bipartite RAG and WFG ASCII rendering with cycle highlighting (`==>`).
- `test_ipc`: Anonymous pipe delivery, named FIFO transfer, `sigaction` signal capture.
- `test_recovery`: Cost formula verification, priority weighting, iterative auto-recovery.
- `test_thread_deadlock`: Real pthread mutex conflict detection and clean release.

### AddressSanitizer (`make asan`)
Compiled with `-fsanitize=address -g`:
```
=== AddressSanitizer Complete Test Suite Passed ===
0 memory errors, 0 buffer overflows, 0 use-after-free
```

### Valgrind Full Memory Leak Check (`make valgrind`)
```
==719== HEAP SUMMARY:
==719==     in use at exit: 0 bytes in 0 blocks
==719==   total heap usage: 38 allocs, 38 frees, 9,918 bytes allocated
==719== All heap blocks were freed -- no leaks are possible
==719== ERROR SUMMARY: 0 errors from 0 contexts (suppressed: 0 from 0)
=== Valgrind Passed Cleanly ===
```

---

## 10. Performance Benchmarks

Measured using high-resolution POSIX monotonic clock `clock_gettime(CLOCK_MONOTONIC)`:

| Workload | Strategy | Requests | Granted | Blocked | Det Time | Recoveries | Util % | Total Time |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Small (5x3)** | Banker's Avoidance | 30 | 18 | 12 | 0.000 ms | 0 | 79.2% | 0.233 ms |
| **Small (5x3)** | Resource Ordering | 30 | 14 | 16 | 0.000 ms | 0 | 54.2% | 0.300 ms |
| **Small (5x3)** | Detection & Recovery | 30 | 18 | 12 | 0.005 ms | 0 | 79.2% | 0.176 ms |
| **Medium (15x5)** | Banker's Avoidance | 79 | 26 | 53 | 0.000 ms | 0 | 87.8% | 3.298 ms |
| **Medium (15x5)** | Resource Ordering | 79 | 29 | 50 | 0.000 ms | 0 | 90.2% | 0.504 ms |
| **Medium (15x5)** | Detection & Recovery | 79 | 30 | 49 | 0.032 ms | 0 | 97.6% | 0.535 ms |
| **Large (50x10)** | Banker's Avoidance | 169 | 53 | 116 | 0.000 ms | 0 | 72.8% | 16.335 ms |
| **Large (50x10)** | Resource Ordering | 169 | 60 | 109 | 0.000 ms | 0 | 84.8% | 0.975 ms |
| **Large (50x10)** | Detection & Recovery | 169 | 69 | 100 | 0.182 ms | 0 | 96.7% | 1.532 ms |

See [docs/BENCHMARKS.md](docs/BENCHMARKS.md) for full analysis and scalability graphs.

---

## 11. Scenario Suite

| Scenario File | Description | Expected Outcome |
| :--- | :--- | :--- |
| `scenarios/safe.txt` | 3 processes, 3 resources under Banker's algorithm | Safe sequence found: `P2 -> P1 -> P3` |
| `scenarios/unsafe.txt` | Banker avoidance enabled; unsafe request submitted | Request blocked; state remains safe |
| `scenarios/deadlock.txt` | Circular wait: P1 holds R1, wants R2; P2 holds R2, wants R1 | Cycle detected: `P1 -> P2 -> P1` |
| `scenarios/multi_instance.txt`| 3 processes, 2 multi-instance resources with contention | Deadlocked set isolated |
| `scenarios/recovery.txt` | Circular wait with priorities assigned (P1: 100, P2: 1) | Victim P2 aborted; P1 unblocked |
| `scenarios/prevention_ordered.txt` | Resource ordering strategy active; out-of-order request | Request rejected; zero deadlocks |
| `scenarios/stress.txt` | 6 processes, 4 resources, heavy interleaved requests | Dynamic tracking under load |

---

## 12. College Viva Voce Walkthrough Guide

When presenting DeadlockDoctor to an examiner or professor, follow this structured 5-step walkthrough:

### Step 1: Automated Presentation Demo (`--demo`)
Run the all-in-one demonstration:
```bash
./deadlockdoctor --demo
```
*Key Talking Point*: Explain that this executes all 5 core stages dynamically—registering resources, verifying Banker's safe sequences, rejecting unsafe states, detecting circular wait cycles, and performing lowest-cost victim recovery.

### Step 2: Interactive Shell & GNU Readline
Launch the interactive shell:
```bash
./deadlockdoctor
```
*Key Talking Point*:
1. Press **Up Arrow** and **Down Arrow** to show command history recalled from `~/.deadlockdoctor_history`.
2. Type `pwd`, `ls -l`, `cat scenarios/safe.txt` to demonstrate that external Linux commands execute seamlessly via `fork()`, `execvp()`, and `waitpid()`.

### Step 3: Deadlock Management Strategies
Inside the shell, run:
```
strategy compare
```
*Key Talking Point*: Walk through the trade-off matrix between reactive detection, Havender's resource ordering prevention, all-or-nothing allocation, and Dijkstra's Banker's algorithm.

### Step 4: Empirical Benchmarks (`--benchmark`)
Run the benchmark suite:
```bash
./deadlockdoctor --benchmark
```
*Key Talking Point*: Highlight the monotonic clock measurements demonstrating that Banker's avoidance exhibits higher computational overhead on large workloads due to repeated $O(m 	imes n^2)$ safety checks, whereas Resource Ordering provides zero-overhead prevention.

### Step 5: Multi-threaded Deadlock & Prevention (CO-6)
Run the POSIX threads demonstration:
```bash
./build/thread_deadlock
```
*Key Talking Point*: Show Configuration A (conflicting lock acquisition with non-blocking conflict observation and recovery) vs Configuration B (strict global lock ordering $R_1 < R_2$ preventing deadlocks by design).

---

## 13. Documentation Index

Comprehensive supplementary documentation is available in the `docs/` directory:

- [docs/DESIGN.md](docs/DESIGN.md) — System architecture, module boundaries, data structures, and state transitions.
- [docs/ALGORITHMS.md](docs/ALGORITHMS.md) — Detailed pseudocode, invariants, and step-by-step mathematical traces.
- [docs/CO_MAPPING.md](docs/CO_MAPPING.md) — Exhaustive mapping to Course Outcomes CO-1 through CO-6 with code evidence.
- [docs/TESTING.md](docs/TESTING.md) — Unit testing matrix, AddressSanitizer logs, and Valgrind memory reports.
- [docs/BENCHMARKS.md](docs/BENCHMARKS.md) — High-resolution runtime performance measurements and scalability analysis.
- [docs/VIVA_QUESTIONS.md](docs/VIVA_QUESTIONS.md) — 20+ examiner questions with detailed theoretical and practical answers.
- [docs/IMPLEMENTATION_STATUS.md](docs/IMPLEMENTATION_STATUS.md) — Milestone tracking and verification status.

---

## License & Academic Integrity
Developed for academic study and Operating Systems laboratory education. All algorithms and POSIX implementations are original educational software written in standard C11.
