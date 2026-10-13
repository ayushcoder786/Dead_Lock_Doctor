# DeadlockDoctor: Operating Systems Viva Questions & Model Answers

### Q1: What is the difference between Deadlock Prevention and Deadlock Avoidance?
- **Prevention**: Eliminates at least one of Coffman's four conditions (Mutual Exclusion, Hold and Wait, No Preemption, Circular Wait) statically before runtime (e.g. Havender's Resource Ordering $R_1 < R_2 < \dots$).
- **Avoidance**: Dynamically evaluates the system state before granting requests using Banker's Algorithm, ensuring the system never enters an unsafe state.

### Q2: Is an Unsafe State always a Deadlock?
- **No**. An unsafe state means the system cannot guarantee that all processes will finish if each requests its declared maximum claim simultaneously. However, if processes finish without requesting their full maximum claim, the system may complete without deadlocking. Every deadlocked state is unsafe, but not every unsafe state is deadlocked.

### Q3: Why does a cycle in a Resource Allocation Graph (RAG) not always mean a deadlock?
- For **single-instance resources**, a cycle is both necessary and sufficient for a deadlock.
- For **multiple-instance resources**, a cycle is necessary but NOT sufficient. If a process holding an instance of a resource in the cycle is unblocked and can finish, it releases its instances and breaks the cycle.

### Q4: How does DeadlockDoctor calculate recovery cost?
$$\text{Cost} = (\text{Held Units} \times 2) + (\text{Pending Requests} \times 1) + \text{Priority}$$
Processes with lower priority and fewer held/pending units have a lower cost and are selected as victims first.

### Q5: How does the POSIX thread deadlock demo prevent hanging the terminal?
- It uses `pthread_mutex_trylock()` with bounded retries rather than blocking `pthread_mutex_lock()`.
- Contention (`EBUSY`) is tracked alongside lock ownership to detect the circular wait. Once reported, a recovery signal coordinates unlocking and graceful thread joining.
