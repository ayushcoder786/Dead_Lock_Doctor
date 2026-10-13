# DeadlockDoctor: Core Algorithms & Theoretical Analysis

## 1. Banker's Algorithm (Dijkstra)

### 1.1 Safety Algorithm
Determines whether the system is in a safe state:
1. Initialize $\text{Work} = \text{Available}$ and $\text{Finish}[i] = \text{false}$ for all $i \in [0, n-1]$.
2. Find an index $i$ such that:
   $$\text{Finish}[i] == \text{false} \quad \land \quad \text{Need}[i] \le \text{Work}$$
   If no such $i$ exists, go to Step 4.
3. $\text{Work} = \text{Work} + \text{Allocation}[i]$; $\text{Finish}[i] = \text{true}$.
   Append $i$ to safe sequence. Repeat from Step 2.
4. If $\text{Finish}[i] == \text{true}$ for all $i$, system is **SAFE**. Otherwise, **UNSAFE**.
- **Time Complexity**: $O(m \cdot n^2)$ where $n$ is number of processes and $m$ is resource types.

### 1.2 Resource-Request Algorithm
When process $i$ requests vector $\text{Request}[i]$:
1. If $\text{Request}[i] > \text{Need}[i]$, reject (exceeded maximum claim).
2. If $\text{Request}[i] > \text{Available}$, block process (must wait).
3. Tentatively allocate:
   $$\text{Available} = \text{Available} - \text{Request}[i]$$
   $$\text{Allocation}[i] = \text{Allocation}[i] + \text{Request}[i]$$
   $$\text{Need}[i] = \text{Need}[i] - \text{Request}[i]$$
4. Execute Safety Algorithm on tentative state. If SAFE $\to$ commit. If UNSAFE $\to$ rollback tentative state and block process.

---

## 2. Multi-Instance Deadlock Detection Algorithm

1. Let $\text{Work} = \text{Available}$.
2. For $i = 0 \dots n-1$:
   If $\text{Allocation}[i] \ne 0$, $\text{Finish}[i] = \text{false}$; else $\text{Finish}[i] = \text{true}$.
3. Find an index $i$ such that $\text{Finish}[i] == \text{false}$ and $\text{Request}[i] \le \text{Work}$.
   If found: $\text{Work} = \text{Work} + \text{Allocation}[i]$; $\text{Finish}[i] = \text{true}$; repeat Step 3.
4. Any process with $\text{Finish}[i] == \text{false}$ is **DEADLOCKED**.

---

## 3. Deadlock Recovery Cost Function
$$\text{Cost}(P_i) = (\text{HeldUnits} \times 2) + (\text{PendingRequests} \times 1) + \text{Priority}$$
The candidate with the lowest cost is chosen as victim, aborted, and its resources reclaimed to unblock surviving processes.
