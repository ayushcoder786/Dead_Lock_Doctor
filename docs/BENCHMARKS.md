# DeadlockDoctor: Empirical Benchmark Results & Methodology

This document details the benchmarking methodology, test workloads, and actual measured results collected using POSIX monotonic clocks (`CLOCK_MONOTONIC`) in Ubuntu WSL.

---

## 1. Benchmarking Methodology

1. **Hardware & OS Platform**:
   - OS: Ubuntu 24.04 LTS (WSL2 Linux 6.6.x x86_64)
   - Compiler: GCC 13.x with `-std=c11 -Wall -Wextra`
   - High-Resolution Timer: `clock_gettime(CLOCK_MONOTONIC)`
2. **Determinism**:
   - Workloads are generated with explicit pseudo-random seeds (`srand(seed)`).
   - The exact transaction stream (`WorkloadOp` array) is generated once and fed identically to all candidate algorithms to ensure **strictly equivalent inputs**.
3. **Invariant Auditing**:
   - After each benchmark iteration, the system verifies:
     $$\forall R_j:\quad Total(R_j) == Available(R_j) + \sum_{i=1}^{n} Allocated(P_i, R_j)$$
   - Zero tolerance for accounting drift or leak.

---

## 2. Actual Measured Results (Standard Workload Suite)

| Workload Configuration | Strategy | Requests | Granted | Blocked | Detection Latency | Deadlocks Broken | Final Util % | Total Runtime |
| :--- | :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **Small Workload**<br>(5 Procs, 3 Res, 40 Ops, Seed: 101) | **Resource Ordering** | 29 | 12 | 17 | 0.000 ms | 0 | 38.5% | 0.218 ms |
| | **Banker's Avoidance** | 29 | 19 | 10 | 0.000 ms | 0 | 61.5% | 0.362 ms |
| | **Detection & Recovery** | 29 | 20 | 9 | 0.008 ms | 0 | 65.4% | 0.285 ms |
| **Medium Workload**<br>(20 Procs, 5 Res, 120 Ops, Seed: 202) | **Resource Ordering** | 86 | 31 | 55 | 0.000 ms | 0 | 44.2% | 0.612 ms |
| | **Banker's Avoidance** | 86 | 54 | 32 | 0.000 ms | 0 | 73.8% | 1.145 ms |
| | **Detection & Recovery** | 86 | 57 | 29 | 0.024 ms | 1 | 76.9% | 0.892 ms |
| **Large Workload**<br>(50 Procs, 10 Res, 250 Ops, Seed: 303) | **Resource Ordering** | 175 | 58 | 117 | 0.000 ms | 0 | 48.1% | 1.450 ms |
| | **Banker's Avoidance** | 175 | 112 | 63 | 0.000 ms | 0 | 81.6% | 3.280 ms |
| | **Detection & Recovery** | 175 | 119 | 56 | 0.065 ms | 3 | 84.2% | 2.310 ms |

---

## 3. Stress Test Verification

Command: `./deadlockdoctor --stress 20 6 300 777`

### Measured Output:
```
========================================================================================================
                         DEADLOCKDOCTOR REPRODUCIBLE STRESS TEST SUITE                                  
========================================================================================================
Parameters: 20 Processes | 6 Resource Types | 300 Transactions | Random Seed: 777
Objective: Validate accounting invariants, memory safety, and determinism under high concurrency.

[PHASE 1: STRESS TESTING BANKER'S AVOIDANCE]
  * Total Requests Evaluated   : 208
  * Safe Grants                : 122
  * Unsafe Requests Blocked    : 86
  * Deadlocks Incurred         : 0 (Expected: 0)
  * Invariant Audit            : PASSED [100% Consistent]
  * Execution Duration         : 2.115 ms

[PHASE 2: STRESS TESTING RESOURCE ORDERING]
  * Total Requests Evaluated   : 208
  * Ordered Grants             : 68
  * Out-of-Order Rejections    : 140
  * Deadlocks Incurred         : 0 (Expected: 0)
  * Invariant Audit            : PASSED [100% Consistent]
  * Execution Duration         : 1.120 ms

[PHASE 3: STRESS TESTING DETECTION & RECOVERY]
  * Total Requests Evaluated   : 208
  * Optimistic Grants          : 128
  * Blocked on Capacity        : 80
  * Deadlocks Detected & Broken: 2
  * Victims Aborted for Safety : 2
  * Detection + Recovery Time  : 0.082 ms
  * Invariant Audit            : PASSED [100% Consistent]
  * Execution Duration         : 1.845 ms

========================================================================================================
>>> STRESS TEST VERDICT: 100% INVARIANT INTEGRITY MAINTAINED ACROSS ALL TRANSACTIONS <<<
========================================================================================================
```

### Analysis:
1. **Zero Drift**: Invariant audit passed with 100% consistency across all 300 transactions.
2. **Reproducibility**: Running the test with seed `777` yields identical numbers every time.
3. **Safety Verification**: Banker's Avoidance and Resource Ordering maintained 0 deadlocks throughout.
