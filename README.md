# DeadlockDoctor: Linux C11 Deadlock Simulation, Prevention, Detection, and Recovery System

DeadlockDoctor is a comprehensive, production-grade Linux systems programming project written in C11 for Ubuntu Operating Systems laboratory coursework. It integrates operating system principles across process management, multi-instance resource allocation, deadlock avoidance (Banker's Algorithm), deadlock detection (Wait-For Graph DFS cycle finding), transparent cost-based recovery, and POSIX multithreading demonstrations.

---

## 1. Project Directory Structure

```
deadlockdoctor/
├── Makefile                          # Unified build system (C11, POSIX, ASan, Valgrind)
├── README.md                         # Comprehensive documentation and viva guide
├── demo/
│   ├── thread_deadlock.c             # Milestone 12: Real pthread mutex deadlock demo (CO-6)
│   └── worker.c                      # Executable worker target for fork/exec demos (CO-2)
├── include/
│   ├── banker.h                      # Banker's safety check and avoidance algorithm
│   ├── common.h                      # Shared definitions, limits, and system headers
│   ├── detector.h                    # Deadlock detection engine and cycle reports
│   ├── graph.h                       # Resource Allocation Graph & Wait-For Graph (ASCII)
│   ├── ipc.h                         # Anonymous pipe, FIFO, and signal handlers (CO-3)
│   ├── logger.h                      # POSIX low-level file descriptor event logger (CO-5)
│   ├── manager.h                     # Central system manager and interactive CLI REPL
│   ├── process.h                     # Dynamic process structures and priority weights
│   ├── process_demo.h                # fork(), execv(), and waitpid() lifecycle demo (CO-2)
│   ├── recovery.h                    # 10-step deadlock recovery and victim selection (CO-4)
│   ├── resource.h                    # Dynamic multi-instance typed resource inventory
│   └── scenario.h                    # Scenario replay parser using POSIX open/read/close
├── logs/
│   └── deadlockdoctor.log            # Persistent system audit log
├── scenarios/
│   ├── deadlock.txt                  # Classical 2-process circular wait scenario
│   ├── graph_demo.txt                # Bipartite RAG and WFG visualization scenario
│   ├── recovery.txt                  # Deadlock followed by manual process recovery
│   ├── safe.txt                      # 3-process, 3-resource safe Banker execution
│   └── unsafe.txt                    # Unsafe request rejection under Banker avoidance
├── src/
│   ├── banker.c                      # Banker's safety matrices and safe sequence solver
│   ├── detector.c                    # Graph-based DFS cycle detection algorithm
│   ├── graph.c                       # ASCII bipartite graph and WFG rendering
│   ├── ipc.c                         # Pipe, mkfifo, and sigaction implementation
│   ├── logger.c                      # POSIX open(), write(), close() audit logging
│   ├── main.c                        # Entry point (CLI dispatcher & scenario runner)
│   ├── manager.c                     # System state coordinator and CLI command processor
│   ├── process.c                     # Dynamic process management & memory lifecycle
│   ├── process_demo.c                # Process creation, execution, and reaping lifecycle
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
    ├── test_recovery.c               # Recovery cost, victim selection, and auto-recovery tests
    └── test_scenario.c               # Scenario replay and error handling tests
```

---

## 2. Course Outcomes (CO-1 to CO-6) Mapping

| Course Outcome | Description | DeadlockDoctor Implementation & Evidence |
| :--- | :--- | :--- |
| **CO-1: Linux CLI & User-Space Environment** | Command-line interface, user-space program conventions, standard streams, exit codes, and POSIX system call usage. | Implemented in `src/main.c` and `src/manager.c`. Provides structured interactive CLI REPL and file-replay mode (`./deadlockdoctor [file]`). Separates `stdout` and `stderr`, handles non-root execution, and exits with standard POSIX codes. |
| **CO-2: Process Lifecycle Management** | Real process creation, execution replacement, and synchronization using `fork()`, `exec()`, and `waitpid()`. | Implemented in `src/process_demo.c` and `demo/worker.c`. Parent forks children, replaces process images via `execv("./build/worker", ...)` with custom arguments, and synchronizes child termination using `waitpid(pid, &status, 0)` with `WIFEXITED(status)` verification to prevent zombie processes. |
| **CO-3: Inter-Process Communication (IPC) & Signals** | Anonymous pipes, named pipes (FIFOs), and asynchronous signal handling. | Implemented in `src/ipc.c`. Demonstrates `pipe()` where worker transmits requests (`REQUEST P1 R1 1`) to the manager, `mkfifo("/tmp/deadlockdoctor.fifo")` for named channel transfers, and `sigaction()` for `SIGUSR1` (worker notification), `SIGALRM` (timer-driven detector trigger), and `SIGTERM` (graceful shutdown) using `volatile sig_atomic_t` flags. |
| **CO-4: Dynamic Memory & Algorithm State** | Dynamic 2D matrix management (`malloc`, `calloc`, `realloc`, `free`), zero memory leaks, and AddressSanitizer/Valgrind compliance. | Dynamic allocation in `process.c`, `resource.c`, `banker.c`, `graph.c`, and `recovery.c`. Supports arbitrary numbers of processes and resources. Verified with `gcc -fsanitize=address` and `valgrind --leak-check=full` with **0 bytes in use at exit, 0 errors, 0 leaks**. |
| **CO-5: POSIX File I/O & Audit Logging** | Low-level file descriptor operations using `open()`, `read()`, `write()`, and `close()`. | Scenario reader in `src/scenario.c` uses `open(path, O_RDONLY)`, fixed-buffer `read()`, newline framing, and `close()`. System audit logger in `src/logger.c` writes timestamped events to `logs/deadlockdoctor.log` using `open(..., O_WRONLY \| O_CREAT \| O_APPEND, 0644)`, `write()`, and `close()`. |
| **CO-6: Multithreading & Synchronization** | POSIX threads (`pthreads`), mutexes, race conditions, and real-time deadlock observation. | Implemented in `demo/thread_deadlock.c`. Spawns threads T1 and T2 competing for mutexes `R1` and `R2` in reverse order. Employs `pthread_mutex_trylock()` with conflict detection so the terminal never hangs, prints the cycle `T1 -> T2 -> T1`, releases mutexes, and joins cleanly. |

---

## 3. Build Instructions

### Prerequisites
- Ubuntu Linux (or WSL2 on Windows)
- GCC compiler supporting C11 (`gcc >= 9.0`)
- GNU Make
- POSIX Pthreads library (`-pthread`)
- Optional for validation: `valgrind`

### Build Targets

```bash
# 1. Clean and build all binaries (deadlockdoctor, worker, thread_deadlock)
make clean all

# 2. Build and run the entire automated test suite (9 test suites)
make test

# 3. Build and execute test suite with GCC AddressSanitizer (-fsanitize=address)
make test_asan

# 4. Run Valgrind full memory leak check on scenario replay
make valgrind
```

---

## 4. Run Commands

### 1. Interactive CLI Mode
```bash
./deadlockdoctor
```

### 2. Scenario File Replay Mode
```bash
# Replay Safe State Scenario
./deadlockdoctor scenarios/safe.txt

# Replay Unsafe State Scenario (Banker avoidance demonstration)
./deadlockdoctor scenarios/unsafe.txt

# Replay Deadlock Scenario (Wait-For Graph cycle discovery)
./deadlockdoctor scenarios/deadlock.txt

# Replay Recovery Scenario (Deadlock detection followed by victim abort)
./deadlockdoctor scenarios/recovery.txt
```

### 3. Standalone Demonstrations
```bash
# Live pthread mutex deadlock demonstration (CO-6)
./build/thread_deadlock
# or
make thread_demo

# Standalone IPC demonstration (CO-3: pipe, FIFO, sigaction)
./build/test_ipc_bin

# Standalone fork/exec/waitpid demonstration (CO-2)
./build/test_process_demo_bin
```

---

## 5. CLI Command Reference

The DeadlockDoctor interactive shell supports structured, beginner-friendly commands:

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
| **detect** | `detect` | Runs WFG DFS cycle detection and prints deadlocked set | `detect` |
| **graph** | `graph` | Renders ASCII Resource Allocation Graph & Wait-For Graph | `graph` |
| **recover** | `recover` | Runs 10-step lowest-cost recovery algorithm | `recover` |
| **priority** | `priority <proc> <val>` | Configures process priority weight for recovery cost | `priority P1 10` |
| **load** | `load <scenario-file>` | Replays commands from a scenario text file | `load scenarios/deadlock.txt` |
| **log** | `log` | Displays the persistent audit log file on the terminal | `log` |
| **demo** | `demo [thread\|fork\|ipc]` | Runs live OS demonstrations (pthread, fork/exec, IPC) | `demo thread` |
| **exit** | `exit` or `quit` | Cleans up all allocations and exits DeadlockDoctor | `exit` |

---

## 6. Core Algorithms & OS Concepts

### 1. Banker's Safety & Avoidance Algorithm
- **Data Structures**:
  - Available Vector: $Available[m]$
  - Max Matrix: $Max[n][m]$
  - Allocation Matrix: $Allocation[n][m]$
  - Need Matrix: $Need[i][j] = Max[i][j] - Allocation[i][j]$
- **Safety Criterion**: Iteratively finds process $P_i$ where $Finish[i] == false$ and $Need_i \le Work$. If all processes finish, system is **SAFE** and the execution sequence is output (e.g. `P2 -> P1 -> P3`).
- **Avoidance Mode (`banker-mode on`)**: Tentatively allocates resources on request. If the resulting state is **UNSAFE**, the transaction is rolled back and the process is marked **BLOCKED**.

### 2. Wait-For Graph (WFG) & Deadlock Detection
- Collapses bipartite Resource Allocation Graph (RAG) into a process-to-process dependency graph:
  $$P_i \to P_j \iff P_i \text{ is waiting on a resource held by } P_j$$
- Depth-First Search (DFS) with 3-color vertex marking (WHITE = unvisited, GRAY = active recursion stack, BLACK = fully visited) discovers directed cycles and isolates the exact deadlocked process set.

### 3. 10-Step Deadlock Recovery
When a deadlock is detected:
1. **Identify deadlocked set** via WFG cycle extraction.
2. **Calculate candidate recovery cost** using transparent formula:
   $$\text{Cost} = (\text{Held Units} \times 2) + (\text{Pending Requests} \times 1) + \text{Priority}$$
3. **Select victim** with the lowest recovery cost.
4. **Abort victim** process in simulation model.
5. **Reclaim all resources** held by the victim.
6. **Update Available vector** and unblock pending requests that can now be satisfied.
7. **Prune victim** from Wait-For Graph.
8. **Re-run deadlock detection** to confirm cycle resolution.
9. **Execute Banker's safety check** to verify system integrity.
10. **Report recovery outcome** and new system state.

### 4. Non-Hanging Pthread Deadlock Demo
- Two threads acquire mutexes in reverse order:
  - Thread 1: Locks `R1`, attempts `R2`.
  - Thread 2: Locks `R2`, attempts `R1`.
- Uses `pthread_mutex_trylock()` with a deterministic observation loop. When both threads are holding one mutex and attempting the other, DeadlockDoctor detects the conflict, prints the cycle `T1 -> T2 -> T1`, releases the locks, and exits without freezing the terminal.

---

## 7. Verification & Test Results

### 1. Automated Test Suites (`make test`)
All 9 test suites pass 100%:
- `test_banker`: Safe sequence generation, unsafe state rejection, tentative request rollback.
- `test_detector`: 2-process cycle, 3-process cycle, safe acyclic state, multi-instance resource detection.
- `test_scenario`: File replay of `safe.txt`, `unsafe.txt`, `deadlock.txt`, and error handling.
- `test_logger`: POSIX `open`/`write`/`read`/`close` verification, log formatting.
- `test_process_demo`: `fork()`, `execv()`, `waitpid()` status verification.
- `test_graph`: Bipartite RAG and WFG ASCII rendering verification.
- `test_ipc`: Anonymous pipe request delivery, named FIFO transfer, `sigaction` signal capture.
- `test_recovery`: Cost formula verification, priority weighting victim selection, auto-recovery.
- `test_thread_deadlock`: Real pthread mutex conflict detection and clean release.

### 2. AddressSanitizer (`make test_asan`)
Compiled with `-fsanitize=address -g`:
```
=== Running AddressSanitizer Scenario Suite ===
./deadlockdoctor scenarios/safe.txt        -> PASSED (0 memory errors)
./deadlockdoctor scenarios/unsafe.txt      -> PASSED (0 memory errors)
./deadlockdoctor scenarios/deadlock.txt    -> PASSED (0 memory errors)
./build/test_scenario_bin                  -> PASSED (0 memory errors)
./build/test_recovery_bin                  -> PASSED (0 memory errors)
./build/thread_deadlock                    -> PASSED (0 memory errors)
=== AddressSanitizer Complete Test Suite Passed ===
```

### 3. Valgrind Memory Leak Check (`make valgrind`)
Executed on scenario replay, recovery, and thread demo:
```
==719== HEAP SUMMARY:
==719==     in use at exit: 0 bytes in 0 blocks
==719==   total heap usage: 38 allocs, 38 frees, 9,918 bytes allocated
==719== All heap blocks were freed -- no leaks are possible
==719== ERROR SUMMARY: 0 errors from 0 contexts (suppressed: 0 from 0)
=== Valgrind Passed Cleanly ===
```

---

## 8. Instructions for Demonstrating in a College Viva

When demonstrating DeadlockDoctor to an external examiner or professor, follow this structured 5-minute walkthrough:

### Step 1: Explain Architecture & Build (`make`)
1. Show clean C11 compilation without warnings:
   ```bash
   make clean all
   ```
2. Explain that DeadlockDoctor models an OS resource manager with typed multi-instance resources, while also demonstrating real Linux OS primitives (pthreads, fork, exec, pipes, FIFOs, signals).

### Step 2: Demonstrate Banker's Algorithm (Safe vs. Unsafe)
1. Run `scenarios/safe.txt`:
   ```bash
   ./deadlockdoctor scenarios/safe.txt
   ```
   *Point to the screen*: Show how Banker finds the safe sequence `P2 -> P1 -> P3`.
2. Run `scenarios/unsafe.txt`:
   ```bash
   ./deadlockdoctor scenarios/unsafe.txt
   ```
   *Point to the screen*: Show that when avoidance is enabled, the request from `P1` is blocked because it leads to an unsafe state where no process can finish.

### Step 3: Demonstrate Deadlock Detection, Graph, & Recovery
1. Run DeadlockDoctor interactively:
   ```bash
   ./deadlockdoctor
   ```
2. Load the deadlock scenario:
   ```
   load scenarios/deadlock.txt
   ```
3. Visualize the bipartite graph:
   ```
   graph
   ```
   *Point to the screen*: Explain the bipartite Resource Allocation Graph (P holds R, P waits on R) and the collapsed Wait-For Graph (`P1 -> P2 -> P1`).
4. Run detection:
   ```
   detect
   ```
   Show the detected cycle and deadlocked set (`P1 P2`).
5. Execute recovery:
   ```
   recover
   ```
   *Point to the screen*: Walk the examiner through the 10 steps:
   - Evaluates cost: `(Held * 2) + (Pending * 1) + Priority`
   - Selects victim `P1`
   - Aborts `P1`, reclaims `R1`
   - Unblocks `P2`
   - Prunes WFG, re-runs detection (No Deadlock)
   - Re-runs Banker (Returns to Safe State)

### Step 4: Demonstrate Real Linux Pthread Deadlock (CO-6)
1. Run the live thread demo:
   ```
   demo thread
   ```
   *Point to the screen*: Explain how threads T1 and T2 acquire `R1` and `R2` in opposite order. Emphasize that `pthread_mutex_trylock()` observes the circular dependency without hanging the terminal.

### Step 5: Demonstrate Linux IPC & Signals (CO-3)
1. Run the IPC demo:
   ```
   demo ipc
   ```
   *Point to the screen*:
   - Anonymous pipe: Child sends `REQUEST P1 R1 1` through `pipe()`, manager executes allocation.
   - Named FIFO: Worker writes to `/tmp/deadlockdoctor.fifo`, manager reads request.
   - Signals: `sigaction()` catches `SIGUSR1`, `SIGALRM` (triggers periodic detection), and `SIGTERM`.

### Step 6: Memory Cleanliness Verification
1. Run Valgrind to prove zero leaks:
   ```bash
   make valgrind
   ```
   Point to `0 bytes in use at exit` and `0 errors`.

---

## 9. Known Limitations & Design Boundaries

1. **Simulation vs. Host Kernel**: Process abortion inside DeadlockDoctor terminates simulated processes (`p->state = PROCESS_STATE_ABORTED`) and reclaims simulated resources. It deliberately does not kill host Linux OS processes.
2. **Deterministic Mutex Demonstration**: The thread deadlock demo in `demo/thread_deadlock.c` uses a safe observation mechanism (`pthread_mutex_trylock`) rather than blocking forever in `pthread_mutex_lock`, ensuring reproducible test suite runs.
3. **Single-Host IPC**: IPC demonstrations (pipe, FIFO) operate on local Linux file descriptors and virtual pipes, suitable for single-host OS laboratory curricula.
