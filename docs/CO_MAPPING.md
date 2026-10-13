# DeadlockDoctor: Course Outcome (CO) Mapping

| Course Outcome | Description | DeadlockDoctor Implementation & Evidence |
| :--- | :--- | :--- |
| **CO-1** | OS Architecture, System Calls & Shell | Built-in CLI shell (`src/manager.c`) executing standard Linux commands via `fork()`, `execvp()`, `waitpid()`, handling signals, environment variables, `$?` status, and GNU Readline with persistent history (`~/.deadlockdoctor_history`). |
| **CO-2** | Process Creation & Synchronization | Linux process management demo (`src/process_demo.c`) verifying parent-child relationships, worker task dispatch via `execvp()`, exit codes, and reaping via `waitpid()`. |
| **CO-3** | Inter-Process Communication (IPC) | Anonymous pipes (`pipe()`), named pipes (`mkfifo()`), and POSIX signals (`sigaction()` with `SIGUSR1`, `SIGALRM`, `SIGTERM`) in `src/ipc.c`. |
| **CO-4** | Dynamic Memory & Debugging | Zero-leak dynamic memory architecture validated via Valgrind and GCC AddressSanitizer (`make asan`). Transparent victim-cost evaluation and recovery in `src/recovery.c`. |
| **CO-5** | File I/O & File Descriptors | Buffered POSIX file I/O reader (`open()`, `read()`, `write()`, `close()`) in `src/scenario.c` and event logging in `src/logger.c`. |
| **CO-6** | Threads, Synchronization & Deadlocks | Multi-instance Banker's algorithm (`src/banker.c`), WFG DFS cycle detector (`src/detector.c`), and POSIX thread synchronization demo (`demo/thread_deadlock.c`) with `pthread_barrier_t` and `pthread_mutex_trylock()`. |
