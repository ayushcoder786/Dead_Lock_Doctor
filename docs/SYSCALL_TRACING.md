# DeadlockDoctor: System-Call Tracing & Kernel Architecture (CO-1)

This document provides a comprehensive analysis of the Linux kernel system calls leveraged by **DeadlockDoctor**, along with instructions for empirical verification via `strace`.

---

## 1. System Call Architecture Mapping

| DeadlockDoctor Subsystem | Linux System Call | Relevant Source Files | Purpose & Kernel Mechanism |
| :--- | :--- | :--- | :--- |
| **File I/O & Scenarios** | `openat()` | `src/scenario.c`, `src/logger.c` | Obtains file descriptors (`O_RDONLY`, `O_WRONLY \| O_CREAT \| O_APPEND`). |
| **File I/O & Scenarios** | `read()` | `src/scenario.c` (`PosixFileReader`) | Reads 4096-byte buffered scenario chunks directly into userspace. |
| **File I/O & Audit Log** | `write()` | `src/logger.c` | Atomic log emission directly to `logs/deadlockdoctor.log`. |
| **File I/O & Descriptors**| `close()` | `src/scenario.c`, `src/logger.c` | Closes file descriptors to prevent descriptor leaks. |
| **Process Management**   | `clone()` / `clone3()` | `src/process_demo.c` | Implements `fork()` to create concurrent child worker processes. |
| **Process Replacement**  | `execve()` | `src/process_demo.c` | Implements `execvp()` to overlay child process with worker task image. |
| **Process Reaping**      | `wait4()` | `src/process_demo.c` | Implements `waitpid()` to collect child exit codes and prevent zombies. |
| **IPC Channels**         | `pipe2()` / `pipe()` | `src/ipc.c` | Instantiates bidirectional unidirectional ring pipelines for inter-process data transfer. |
| **IPC Named Channels**   | `mknodat()` / `mknod()` | `src/ipc.c` | Creates FIFO special device nodes (`S_IFIFO`) for filesystem-based IPC. |
| **Signal Dispatch**      | `kill()` / `tgkill()` | `src/ipc.c` | Delivers asynchronous signals (`SIGUSR1`, `SIGALRM`, `SIGTERM`) to target PIDs. |
| **Signal Handling**      | `rt_sigaction()` | `src/ipc.c`, `src/manager.c` | Establishes POSIX signal handlers with `sa_flags` and signal masks. |
| **Thread Synchronization**| `futex()` | `demo/thread_deadlock.c` | Fast userspace locking for `pthread_mutex_trylock()`, lock acquisition, and wait queue transitions. |
| **High-Res Timing**      | `clock_gettime()` | `src/process.c`, `src/benchmark.c` | Queries `CLOCK_MONOTONIC` for sub-microsecond wait durations and latency benchmarks. |
| **Terminal I/O**         | `ioctl()`, `select()` | `src/monitor.c`, `src/manager.c` | Configures terminal raw modes, line editing, and non-blocking keypress detection. |

---

## 2. Empirical Verification via strace

When `strace` is installed (`sudo apt-get install -y strace`), use the following commands to trace and analyze runtime behavior:

### A. Trace File I/O during Scenario Execution
```bash
strace -c -e trace=openat,read,write,close ./deadlockdoctor scenarios/deadlock.txt
```
*Expected Result*: Demonstrates buffered 4KB chunk reads of the scenario file and synchronous write calls to `logs/deadlockdoctor.log`.

### B. Trace Process Creation, Execution, and Reaping
```bash
strace -f -e trace=clone,wait4,execve ./deadlockdoctor --process-demo
```
*Expected Result*: 
- `clone()` creates Worker 1 and Worker 2.
- `execve()` replaces the process image with worker tasks.
- `wait4()` successfully reaps child exit status `0`.

### C. Trace Inter-Process Communication (Pipes & Signals)
```bash
strace -f -e trace=pipe2,kill,rt_sigaction ./deadlockdoctor --ipc-demo
```
*Expected Result*:
- `pipe2()` establishes IPC channels.
- `kill()` delivers `SIGUSR1` to child processes.
- Handlers catch signals safely without termination.

### D. Trace Thread Contention and Futex Invocations
```bash
strace -f -e trace=futex ./build/thread_deadlock
```
*Expected Result*:
- Under conflicting lock ordering, `futex()` calls show mutex lock contention and wait-for blocking.
- Under Havender's global lock ordering, zero contending `futex()` wait calls occur.

---

## 3. Built-In Tracing Demonstration

DeadlockDoctor includes an automated tracing probe:
- **CLI Command**: `trace` or `strace`
- **CLI Flag**: `./deadlockdoctor --trace`
