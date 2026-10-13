# DeadlockDoctor: Empirical Performance Benchmarks

## 1. Methodology
- Time measurement: Monotonic hardware clock via `clock_gettime(CLOCK_MONOTONIC, ...)`.
- Pseudo-random workloads generated deterministically using fixed random seeds.
- Workloads evaluated:
  - Small: 5 processes, 3 resource types, 30-40 requests.
  - Medium: 20 processes, 5 resource types, 80-120 requests.
  - Large: 50 processes, 10 resource types, 170-250 requests.

## 2. Measured Results

| Workload | Strategy | Requests | Granted | Blocked | Detection Time | Recoveries | Utilization | Runtime |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Small (5x3)** | Banker Avoidance | 30 | 18 | 12 | 0.000 ms | 0 | 79.2% | 0.315 ms |
| **Small (5x3)** | Resource Ordering | 30 | 14 | 16 | 0.000 ms | 0 | 54.2% | 0.221 ms |
| **Small (5x3)** | Detection & Recovery | 30 | 18 | 12 | 0.006 ms | 0 | 79.2% | 0.201 ms |
| **Medium (20x5)** | Banker Avoidance | 79 | 26 | 53 | 0.000 ms | 0 | 87.8% | 1.958 ms |
| **Medium (20x5)** | Resource Ordering | 79 | 29 | 50 | 0.000 ms | 0 | 90.2% | 0.468 ms |
| **Medium (20x5)** | Detection & Recovery | 79 | 30 | 49 | 0.034 ms | 0 | 97.6% | 0.573 ms |
| **Large (50x10)** | Banker Avoidance | 169 | 53 | 116 | 0.000 ms | 0 | 72.8% | 15.112 ms |
| **Large (50x10)** | Resource Ordering | 169 | 60 | 109 | 0.000 ms | 0 | 84.8% | 1.205 ms |
| **Large (50x10)** | Detection & Recovery | 169 | 69 | 100 | 0.178 ms | 0 | 96.7% | 1.283 ms |

## 3. Analysis & Trade-offs
1. **Banker's Avoidance**: Guarantees zero deadlocks without aborting processes, but incurs $O(m \cdot n^2)$ overhead on each request.
2. **Resource Ordering**: Zero runtime detection overhead; eliminates circular wait statically. May reject out-of-order requests.
3. **Detection & Recovery**: Yields highest concurrency and utilization (96.7%), with periodic detection and abortion overhead only when contention arises.
