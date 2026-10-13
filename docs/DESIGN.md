# DeadlockDoctor: System Architecture & Design Specification

## 1. Overview
DeadlockDoctor is an educational and simulation Operating Systems platform engineered in C11 and POSIX APIs. It implements core OS kernels concepts including resource management, deadlock prevention, deadlock avoidance (Banker's Algorithm), multi-instance deadlock detection, cost-weighted recovery, POSIX thread synchronization, and Linux IPC.

---

## 2. Architectural Diagram

```
+---------------------------------------------------------------------------------+
|                                 USER INTERFACE                                  |
|     Interactive CLI Shell (GNU Readline)  /  Scripted Replay  /  Unified Demo    |
+---------------------------------------+-----------------------------------------+
                                        |
+---------------------------------------v-----------------------------------------+
|                              SYSTEM MANAGER ENGINE                              |
|   - Process Lifecycle Management (READY, RUNNING, BLOCKED, COMPLETED, ABORTED)  |
|   - Strategy Dispatcher (Detection/Recovery, Resource Ordering, All-or-Nothing) |
|   - External Command Dispatcher (fork, execvp, waitpid, cd, pwd)                |
+--------+------------------+-------------------+------------------+--------------+
         |                  |                   |                  |
+--------v-------+  +-------v-------+   +-------v-------+  +-------v-------+
| RESOURCE &     |  | BANKER        |   | DETECTOR &    |  | RECOVERY      |
| PROCESS ENGINE |  | AVOIDANCE     |   | GRAPH VIEWER  |  | ENGINE        |
| - Alloc Matrix |  | - Safety Alg  |   | - Multi-Inst  |  | - Cost Eval   |
| - Max Matrix   |  | - Request Alg |   |   Reduction   |  | - Victim Kill |
| - Need Matrix  |  | - Safe Seq    |   | - WFG / RAG   |  | - Resource    |
| - Avail Vector |  | - Rollback    |   | - DFS Cycle   |  |   Reclaim     |
+----------------+  +---------------+   +---------------+  +---------------+
```

---

## 3. Core Data Structures & Invariants

### 3.1 Mathematical Invariant
At all times, for process $i$ and resource $j$:
$$	ext{Need}[i][j] = 	ext{Max}[i][j] - 	ext{Allocation}[i][j]$$
$$\sum_{i} 	ext{Allocation}[i][j] + 	ext{Available}[j] = 	ext{TotalInstances}[j]$$

### 3.2 Key Structs
- `Resource`: Stores `id`, `name`, `total_instances`, `available_instances`.
- `Process`: Stores `id`, `name`, `state`, `priority`, dynamic vectors: `allocated[]`, `requested[]`, `max_demand[]`.
- `BankerSystem`: Snapshot representation storing `available[]`, `allocation[][]`, `max[][]`, `need[][]`.
- `WaitForGraph`: Adjacency matrix `adj[u][v]` representing process wait dependencies ($P_u 	o P_v$).
- `DeadlockReport`: Deadlock status flag, detected cycle path, deadlocked process set.
- `SystemManager`: Top-level orchestrator holding `ProcessManager`, `ResourceManager`, `DeadlockStrategy`.

---

## 4. Process State Machine
- **READY**: Created, registered in system, holds zero resources.
- **RUNNING**: Allocated resources, actively executing in simulation.
- **BLOCKED**: Outstanding request pending that cannot be fulfilled immediately.
- **COMPLETED**: Released all resources and terminated normally.
- **ABORTED**: Terminated by deadlock recovery engine to reclaim resources.
