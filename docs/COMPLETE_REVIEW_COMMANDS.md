# DEADLOCKDOCTOR — COMPLETE COLLEGE REVIEW & VIVA PLAYBOOK

This document contains **every command** you can demonstrate during your college project review, organized by demonstration phase with exact syntax, expected terminal output, and theoretical talking points to impress your examiners.

---

## TABLE OF CONTENTS
1. [Phase 0: Build, Test & Memory Safety Verification](#phase-0-build-test--memory-safety-verification)
2. [Phase 1: Core Viva Demonstration (Interactive Shell)](#phase-1-core-viva-demonstration-interactive-shell)
3. [Phase 2: Advanced Upgraded Review Features (6 Core Upgrades)](#phase-2-advanced-upgraded-review-features-6-core-upgrades)
4. [Phase 3: Fast-Track Terminal Commands (No Typing Inside Shell)](#phase-3-fast-track-terminal-commands-no-typing-inside-shell)
5. [Phase 4: Examiner Q&A Talking Points & Formula Cheat Sheet](#phase-4-examiner-qa-talking-points--formula-cheat-sheet)

---

## PHASE 0: BUILD, TEST & MEMORY SAFETY VERIFICATION
*Run these commands directly in your Ubuntu WSL terminal to show that the project compiles cleanly and passes all tests.*

### Command 0.1: Clean Build
```bash
make clean && make -j4
```
- **What it does**: Removes old binaries and compiles the entire C11 project (`-std=c11 -Wall -Wextra -pthread -lreadline`) with **zero warnings**.
- **What to say**: *"The project strictly follows C11 standards with zero compiler warnings and modular architecture separating process management, resource tracking, Banker's algorithm, cycle detection, and recovery."*

### Command 0.2: Run All 14 Automated Test Suites
```bash
make test
```
- **What it does**: Executes all 14 unit test suites (`test_banker`, `test_detector`, `test_scenario`, `test_logger`, `test_process_demo`, `test_graph`, `test_ipc`, `test_recovery`, `test_thread_deadlock`, `test_monitor`, `test_timeline`, `test_bench_compare`, `test_trace`, `test_timeout`).
- **Expected result**: `14 of 14 unit test suites passed (100% pass rate)`.

### Command 0.3: AddressSanitizer Memory Safety & Zero-Leak Proof
```bash
make test_asan
```
- **What it does**: Recompiles and runs the entire suite with GCC AddressSanitizer (`-fsanitize=address -g`).
- **Expected result**: `=== AddressSanitizer Complete Test Suite Passed ===` (0 leaks, 0 buffer overflows, 0 use-after-free).
- **What to say**: *"We validated memory safety across all data structures using GCC AddressSanitizer to guarantee enterprise-grade stability."*

---

## PHASE 1: CORE VIVA DEMONSTRATION (INTERACTIVE SHELL)
*Launch the interactive CLI by typing:*
```bash
./deadlockdoctor
```
*Enter each numbered command below at the `deadlockdoctor>` prompt:*

| Step | Exact Command to Type | What It Demonstrates |
|:---:|:---|:---|
| **1** | `help` | Displays the command guide, categories, and supported options. |
| **2** | `status` | Shows empty system matrices (Available vector, Alloc, Max, Need). |
| **3** | `process create P1`<br>`process create P2` | Creates two simulated processes: `P1` (PID 1) and `P2` (PID 2) in `READY` state. |
| **4** | `resource create R1 1`<br>`resource create R2 1`<br>`max P1 R1 1`<br>`max P1 R2 1`<br>`max P2 R1 1`<br>`max P2 R2 1` | Creates resources `R1` and `R2` (1 instance each) and declares maximum resource claims for Banker's calculations. |
| **5** | `request P1 R1 1`<br>`request P2 R2 1` | Allocates `R1` to `P1` and `R2` to `P2`. Both transition to `RUNNING` state. |
| **6** | `request P1 R2 1`<br>`request P2 R1 1` | `P1` requests held resource `R2` and **blocks**.<br>`P2` requests held resource `R1` and **blocks**.<br>*(Creates a classic Circular Wait condition: P1 -> P2 -> P1)*. |
| **7** | `graph` | Renders ASCII **Resource Allocation Graph (RAG)** and **Wait-For Graph (WFG)**, highlighting the circular wait cycle `P1 ==> P2 ==> P1`. |
| **8** | `detect` | Runs the DFS cycle detection algorithm on the WFG and reports the exact deadlocked set: `P1 P2`. |
| **9** | `recover` | Runs the 10-step recovery algorithm: calculates victim cost for `P1` and `P2`, selects lowest-cost victim `P1`, aborts `P1`, reclaims its resources, and automatically grants `R1` to unblock `P2`! |
| **10** | `status` | Displays the updated matrices showing `P1` is `ABORTED`, `P2` is `RUNNING` with `R1:1 R2:1`, and Need is 0. |
| **11** | `detect` | Re-runs deadlock detection to verify that circular wait has been broken. Displays: `NO DEADLOCK DETECTED`. |
| **12** | `load scenarios/safe.txt`<br>`load scenarios/unsafe.txt` | **Banker's Algorithm Evaluation**:<br>- `safe.txt`: Discovers a valid safe sequence (`SYSTEM STATE: SAFE`).<br>- `unsafe.txt`: Detects an unsafe state and blocks the dangerous request under avoidance mode (`BANKER-MODE ON`). |
| **13** | `load scenarios/deadlock.txt`<br>`load scenarios/multi_instance.txt` | **Multi-Instance vs Single-Instance**:<br>- `deadlock.txt`: Detects crafted circular wait.<br>- `multi_instance.txt`: Proves that a cycle in a multi-instance system does **NOT** equal deadlock if unblocked processes can release resources. |
| **14** | `demo thread` | Runs real POSIX thread demonstration showing ABBA mutex lock inversion on Linux `futex()`, deadlock detection, and cooperative recovery without terminal hang. |
| **15** | `exit` | Safely terminates the DeadlockDoctor interactive shell. |

---

## PHASE 2: ADVANCED UPGRADED REVIEW FEATURES (6 CORE UPGRADES)
*These commands showcase the advanced systems engineering capabilities requested by evaluation rubrics.*

### Feature 1: Live Process Monitor
- **Interactive Command**: `monitor` *(or `live` for continuous auto-refreshing monitor)*
- **Terminal Flag**: `./deadlockdoctor --monitor` or `./deadlockdoctor --live`
- **What it shows**:
  - Real-time process states: `READY`, `RUNNING`, `BLOCKED`, `COMPLETED`, `ABORTED`.
  - Process Priority, Held Resource Vector, Pending Request Vector.
  - Precise waiting duration measured via `clock_gettime(CLOCK_MONOTONIC)` in milliseconds.

### Feature 2: Multi-Instance Resource Utilization Dashboard
- **Interactive Command**: `dashboard`
- **Terminal Flag**: `./deadlockdoctor --dashboard`
- **What it shows**:
  - Total Capacity, Allocated Units ($\sum_i 	ext{Alloc}[i][j]$), Available Units.
  - Utilization percentage and ASCII visual capacity gauge (`[████████░░] 75.0%`).
  - **Invariant Verification**: Audits that $	ext{Total} == 	ext{Available} + 	ext{Allocated}$ across all resources.

### Feature 3: Interactive Step-by-Step Scenario Debugger
- **Interactive Command**: `step scenarios/safe.txt`
- **Terminal Flag**: `./deadlockdoctor --step scenarios/safe.txt`
- **What it shows**:
  - Steps through scenario instructions **one line at a time** (press Enter to advance, 's' to skip to end, 'q' to quit).
  - Displays instantaneous state mutations, Available vector updates, and Need calculations after each operation.

### Feature 4: 3-Way Comparative Algorithm Benchmark
- **Interactive Command**: `compare`
- **Terminal Flag**: `./deadlockdoctor --compare`
- **What it shows**:
  - Generates a single deterministic workload trace (80 operations across 10 processes and 4 resource types).
  - Replays the **exact identical trace** across:
    1. **Resource Ordering (Havender's Rule Prevention)**
    2. **Dijkstra's Banker's Algorithm (Avoidance)**
    3. **Deadlock Detection & Recovery**
  - Compares: Granted Requests, Blocked Requests, Detection Latency, Deadlocks Detected, Recoveries, Utilization %, and Total Runtime.

### Feature 5: Reproducible High-Concurrency Stress Test
- **Interactive Command**: `stress`
- **Terminal Flag**: `./deadlockdoctor --stress`
- **What it shows**:
  - Executes 300 randomized, high-concurrency transactions across 20 processes and 6 resources with seed `777`.
  - Audits 100% accounting invariant integrity after every batch of transactions.

### Feature 6: Kernel System-Call Tracing & strace Demonstration
- **Interactive Command**: `trace`
- **Terminal Flag**: `./deadlockdoctor --trace`
- **What it shows**:
  - Probes for `strace` availability.
  - Displays the kernel mapping table linking DeadlockDoctor functions to real Linux system calls:
    - Mutex contention: `futex()`
    - Process creation: `clone()` / `clone3()`, `execve()`, `wait4()`
    - IPC channels: `pipe2()`, `mknodat()`, `rt_sigaction()`, `kill()`
    - File I/O: `openat()`, `read()`, `write()`, `close()`
    - High-res timing: `clock_gettime()`

### Bonus Feature: Timeout-Based Detection & Contention False-Alarm Audit
- **Interactive Command**: `timeout 500`
- **Terminal Binary**: `./build/test_timeout_bin`
- **What it shows**:
  - Demonstrates that a timeout represents **suspicion only**, not proof of deadlock.
  - Audits normal contention (flagged as `FALSE ALARM`) vs circular wait (flagged as `CONFIRMED DEADLOCK`).

---

## PHASE 3: FAST-TRACK TERMINAL COMMANDS (NO TYPING INSIDE SHELL)
*If your examiner asks for quick, direct execution from the Ubuntu bash terminal, use these one-liners:*

```bash
# 1. Quick Circular Wait & Recovery Demonstration
./deadlockdoctor scenarios/deadlock.txt

# 2. Multi-Instance Matrix Reduction (Cycle without Deadlock)
./deadlockdoctor scenarios/multi_instance.txt

# 3. Banker's Safe State Discovery
./deadlockdoctor scenarios/safe.txt

# 4. Banker's Unsafe State Request Blocking
./deadlockdoctor scenarios/unsafe.txt

# 5. Real POSIX Thread Mutex Deadlock & Futex Recovery
./build/thread_deadlock

# 6. Complete 3-Way Comparative Benchmark
./deadlockdoctor --compare

# 7. High-Concurrency Stress Test Suite
./deadlockdoctor --stress

# 8. System Call Architecture Trace
./deadlockdoctor --trace

# 9. Timeout Classification & Contention False-Alarm Audit
./build/test_timeout_bin

# 10. Complete Multi-Stage OS Demonstration
./deadlockdoctor --demo all
```

---

## PHASE 4: EXAMINER Q&A TALKING POINTS & FORMULA CHEAT SHEET

### Q1: What are Coffman's Four Conditions for Deadlock?
1. **Mutual Exclusion**: At least one resource is held in a non-shareable mode.
2. **Hold and Wait**: A process holds at least one resource while waiting to acquire others.
3. **No Preemption**: Resources cannot be forcibly seized; only released voluntarily.
4. **Circular Wait**: A closed chain of processes exists where each waits for a resource held by the next ($P_0 	o P_1 	o \dots 	o P_0$).

### Q2: How does Deadlock Prevention differ from Avoidance?
- **Prevention**: Statically eliminates at least one Coffman condition before runtime (e.g. Havender's Resource Ordering $R_1 < R_2 < \dots < R_m$ eliminates Circular Wait). Low overhead, but reduces concurrency.
- **Avoidance**: Dynamically inspects resource requests at runtime using Dijkstra's Banker's Algorithm. Only grants requests that leave the system in a **Safe State** (where at least one safe sequence exists).

### Q3: Why does a cycle in a graph NOT always mean deadlock?
- In a **Single-Instance Resource Allocation Graph**, a cycle is both **necessary and sufficient** for deadlock.
- In a **Multiple-Instance Resource System**, a cycle is **necessary but NOT sufficient**. If an unblocked process holding an instance of that resource can finish and release its allocation, the cycle is broken. DeadlockDoctor uses Coffman-Silberschatz matrix reduction ($Work/Finish$ vectors) to determine true deadlocks.

### Q4: What is the Banker's Need Matrix Formula?
$$	ext{Need}[i][j] = 	ext{Max}[i][j] - 	ext{Allocation}[i][j]$$
Safety requirement: A request is safe if:
1. $	ext{Request}[i] \le 	ext{Need}[i]$
2. $	ext{Request}[i] \le 	ext{Available}$
3. Tentative allocation results in $	ext{Need} \le 	ext{Work}$ for all processes in some sequence.

### Q5: How does DeadlockDoctor calculate recovery victim cost?
$$	ext{Cost} = (	ext{Held Units} 	imes 2) + (	ext{Pending Requests} 	imes 1) + 	ext{Priority}$$
The simulator aborts the candidate with the **lowest cost**, freeing maximum resources while penalizing the least critical process.

### Q6: How does the thread deadlock demo avoid hanging indefinitely?
Instead of blocking forever on `pthread_mutex_lock()`, threads coordinate via POSIX barriers, monitor contention with `pthread_mutex_trylock()`, detect the mutual `EBUSY` dependency, and trigger cooperative lock yielding to safely finish execution.
