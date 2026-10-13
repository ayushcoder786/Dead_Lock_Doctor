CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Iinclude
LDFLAGS ?= -pthread -lreadline

TARGET = deadlockdoctor
SRC_DIR = src
TEST_DIR = tests
OBJ_DIR = build
WORKER_BIN = $(OBJ_DIR)/worker
THREAD_DEADLOCK_BIN = $(OBJ_DIR)/thread_deadlock

TEST_BANKER_BIN = $(OBJ_DIR)/test_banker_bin
TEST_DETECTOR_BIN = $(OBJ_DIR)/test_detector_bin
TEST_SCENARIO_BIN = $(OBJ_DIR)/test_scenario_bin
TEST_LOGGER_BIN = $(OBJ_DIR)/test_logger_bin
TEST_PROCESS_DEMO_BIN = $(OBJ_DIR)/test_process_demo_bin
TEST_GRAPH_BIN = $(OBJ_DIR)/test_graph_bin
TEST_IPC_BIN = $(OBJ_DIR)/test_ipc_bin
TEST_RECOVERY_BIN = $(OBJ_DIR)/test_recovery_bin

SRCS = $(SRC_DIR)/main.c \
       $(SRC_DIR)/process.c \
       $(SRC_DIR)/resource.c \
       $(SRC_DIR)/manager.c \
       $(SRC_DIR)/banker.c \
       $(SRC_DIR)/detector.c \
       $(SRC_DIR)/recovery.c \
       $(SRC_DIR)/scenario.c \
       $(SRC_DIR)/graph.c \
       $(SRC_DIR)/ipc.c \
       $(SRC_DIR)/logger.c \
       $(SRC_DIR)/process_demo.c \
       $(SRC_DIR)/benchmark.c

OBJS = $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

TEST_BANKER_OBJS = $(OBJ_DIR)/test_banker.o \
                   $(OBJ_DIR)/banker.o \
                   $(OBJ_DIR)/process.o \
                   $(OBJ_DIR)/resource.o \
                   $(OBJ_DIR)/logger.o

TEST_DETECTOR_OBJS = $(OBJ_DIR)/test_detector.o \
                     $(OBJ_DIR)/detector.o \
                     $(OBJ_DIR)/graph.o \
                     $(OBJ_DIR)/process.o \
                     $(OBJ_DIR)/resource.o \
                     $(OBJ_DIR)/logger.o

TEST_SCENARIO_OBJS = $(OBJ_DIR)/test_scenario.o \
                     $(OBJ_DIR)/scenario.o \
                     $(OBJ_DIR)/manager.o \
                     $(OBJ_DIR)/banker.o \
                     $(OBJ_DIR)/detector.o \
                     $(OBJ_DIR)/graph.o \
                     $(OBJ_DIR)/process.o \
                     $(OBJ_DIR)/resource.o \
                     $(OBJ_DIR)/recovery.o \
                     $(OBJ_DIR)/logger.o \
                     $(OBJ_DIR)/process_demo.o \
                     $(OBJ_DIR)/ipc.o \
                     $(OBJ_DIR)/benchmark.o

TEST_LOGGER_OBJS = $(OBJ_DIR)/test_logger.o \
                   $(OBJ_DIR)/logger.o

TEST_PROCESS_DEMO_OBJS = $(OBJ_DIR)/test_process_demo.o \
                         $(OBJ_DIR)/process_demo.o \
                         $(OBJ_DIR)/logger.o

TEST_GRAPH_OBJS = $(OBJ_DIR)/test_graph.o \
                  $(OBJ_DIR)/graph.o \
                  $(OBJ_DIR)/detector.o \
                  $(OBJ_DIR)/process.o \
                  $(OBJ_DIR)/resource.o \
                  $(OBJ_DIR)/logger.o

TEST_IPC_OBJS = $(OBJ_DIR)/test_ipc.o \
                $(OBJ_DIR)/ipc.o \
                $(OBJ_DIR)/manager.o \
                $(OBJ_DIR)/banker.o \
                $(OBJ_DIR)/detector.o \
                $(OBJ_DIR)/graph.o \
                $(OBJ_DIR)/process.o \
                $(OBJ_DIR)/resource.o \
                $(OBJ_DIR)/recovery.o \
                $(OBJ_DIR)/scenario.o \
                $(OBJ_DIR)/logger.o \
                $(OBJ_DIR)/process_demo.o \
                     $(OBJ_DIR)/benchmark.o

TEST_RECOVERY_OBJS = $(OBJ_DIR)/test_recovery.o \
                     $(OBJ_DIR)/recovery.o \
                     $(OBJ_DIR)/manager.o \
                     $(OBJ_DIR)/banker.o \
                     $(OBJ_DIR)/detector.o \
                     $(OBJ_DIR)/graph.o \
                     $(OBJ_DIR)/process.o \
                     $(OBJ_DIR)/resource.o \
                     $(OBJ_DIR)/scenario.o \
                     $(OBJ_DIR)/logger.o \
                     $(OBJ_DIR)/process_demo.o \
                     $(OBJ_DIR)/ipc.o \
                     $(OBJ_DIR)/benchmark.o

.PHONY: all clean run test test_banker test_detector test_scenario test_logger test_process_demo test_graph test_ipc test_recovery test_thread_deadlock thread_demo asan test_asan valgrind

all: $(TARGET) $(WORKER_BIN) $(THREAD_DEADLOCK_BIN)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

$(WORKER_BIN): demo/worker.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) demo/worker.c -o $@

$(THREAD_DEADLOCK_BIN): demo/thread_deadlock.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) demo/thread_deadlock.c $(LDFLAGS) -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: $(TEST_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(TEST_BANKER_BIN): $(TEST_BANKER_OBJS)
	$(CC) $(TEST_BANKER_OBJS) $(LDFLAGS) -o $@

$(TEST_DETECTOR_BIN): $(TEST_DETECTOR_OBJS)
	$(CC) $(TEST_DETECTOR_OBJS) $(LDFLAGS) -o $@

$(TEST_SCENARIO_BIN): $(TEST_SCENARIO_OBJS)
	$(CC) $(TEST_SCENARIO_OBJS) $(LDFLAGS) -o $@

$(TEST_LOGGER_BIN): $(TEST_LOGGER_OBJS)
	$(CC) $(TEST_LOGGER_OBJS) $(LDFLAGS) -o $@

$(TEST_PROCESS_DEMO_BIN): $(TEST_PROCESS_DEMO_OBJS)
	$(CC) $(TEST_PROCESS_DEMO_OBJS) $(LDFLAGS) -o $@

$(TEST_GRAPH_BIN): $(TEST_GRAPH_OBJS)
	$(CC) $(TEST_GRAPH_OBJS) $(LDFLAGS) -o $@

$(TEST_IPC_BIN): $(TEST_IPC_OBJS)
	$(CC) $(TEST_IPC_OBJS) $(LDFLAGS) -o $@

$(TEST_RECOVERY_BIN): $(TEST_RECOVERY_OBJS)
	$(CC) $(TEST_RECOVERY_OBJS) $(LDFLAGS) -o $@

test_banker: $(TEST_BANKER_BIN)
	./$(TEST_BANKER_BIN)

test_detector: $(TEST_DETECTOR_BIN)
	./$(TEST_DETECTOR_BIN)

test_scenario: $(TEST_SCENARIO_BIN)
	./$(TEST_SCENARIO_BIN)

test_logger: $(TEST_LOGGER_BIN)
	./$(TEST_LOGGER_BIN)

test_process_demo: $(TEST_PROCESS_DEMO_BIN) $(WORKER_BIN)
	./$(TEST_PROCESS_DEMO_BIN)

test_graph: $(TEST_GRAPH_BIN)
	./$(TEST_GRAPH_BIN)

test_ipc: $(TEST_IPC_BIN)
	./$(TEST_IPC_BIN)

test_recovery: $(TEST_RECOVERY_BIN)
	./$(TEST_RECOVERY_BIN)

test_thread_deadlock: $(THREAD_DEADLOCK_BIN)
	./$(THREAD_DEADLOCK_BIN)

thread_demo: $(THREAD_DEADLOCK_BIN)
	./$(THREAD_DEADLOCK_BIN)

test: test_banker test_detector test_scenario test_logger test_process_demo test_graph test_ipc test_recovery test_thread_deadlock

# AddressSanitizer Build and Test Suite
asan: CFLAGS += -g -fsanitize=address
asan: LDFLAGS += -fsanitize=address
asan: clean all $(TEST_SCENARIO_BIN) $(TEST_BANKER_BIN) $(TEST_DETECTOR_BIN) $(TEST_GRAPH_BIN) $(TEST_RECOVERY_BIN)
	@echo "=== Running AddressSanitizer Scenario Suite ==="
	./$(TARGET) scenarios/safe.txt
	./$(TARGET) scenarios/unsafe.txt
	./$(TARGET) scenarios/deadlock.txt
	./$(TEST_SCENARIO_BIN)
	./$(TEST_RECOVERY_BIN)
	./$(THREAD_DEADLOCK_BIN)
	@echo "=== AddressSanitizer Scenarios Passed Successfully ==="

test_asan: CFLAGS += -g -fsanitize=address
test_asan: LDFLAGS += -fsanitize=address
test_asan: clean all test_banker test_detector test_scenario test_logger test_graph test_recovery test_thread_deadlock
	@echo "=== AddressSanitizer Complete Test Suite Passed ==="

valgrind: clean all
	@echo "=== Running Valgrind Leak Check on scenarios/deadlock.txt ==="
	valgrind --leak-check=full --error-exitcode=1 ./$(TARGET) scenarios/deadlock.txt
	@echo "=== Valgrind Passed Cleanly ==="

run: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(OBJ_DIR) $(TARGET)

benchmark: $(TARGET)
	./$(TARGET) --benchmark
