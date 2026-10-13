# DeadlockDoctor: Automated Test Suite & Verification

## 1. Test Execution Commands

```bash
# Compile and run entire automated test suite
make test

# Run individual test suites
make test_banker          # Banker's algorithm safe/unsafe/rollback tests
make test_detector        # Multi-instance deadlock detection & cycle tests
make test_scenario        # Scenario replay, parsing errors & stress tests
make test_logger          # POSIX file I/O logging verification
make test_process_demo    # CO-2 fork, exec, waitpid process demo
make test_graph           # Graph viewer & cycle detection
make test_ipc             # CO-3 pipes, FIFOs, and signal handling
make test_recovery        # Deadlock victim selection & auto-recovery
make test_thread_deadlock # CO-6 POSIX thread deadlock & prevention demo

# Memory Sanitizer Tests
make asan                 # AddressSanitizer memory safety verification
make valgrind             # Valgrind leak check
```

## 2. Test Coverage Summary
- 100% of core algorithms, invariants, and POSIX demos are verified by automated test binaries.
- All tests return non-zero exit codes on failure and zero on success.
