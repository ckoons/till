# Makefile for Till unit tests

CC = cc
CFLAGS = -Wall -Wextra -g -I../../src
LDFLAGS = 

SRC_DIR = ../../src
BUILD_DIR = ../../build

# Source files needed for tests
SECURITY_SRCS = $(SRC_DIR)/till_security.c $(SRC_DIR)/till_common.c $(SRC_DIR)/till_common_extra.c \
                $(SRC_DIR)/till_platform.c $(SRC_DIR)/till_platform_process.c $(SRC_DIR)/cJSON.c
SECURITY_OBJS = $(BUILD_DIR)/till_security.o $(BUILD_DIR)/till_common.o $(BUILD_DIR)/till_common_extra.o \
                $(BUILD_DIR)/till_platform.o $(BUILD_DIR)/till_platform_process.o $(BUILD_DIR)/cJSON.o

# Test executables
TESTS = test_security

.PHONY: all clean test

all: $(TESTS)

test_security: test_security.c $(SECURITY_OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(SECURITY_OBJS) $(LDFLAGS)

# Run all tests
test: $(TESTS)
	@echo "Running unit tests..."
	@for test in $(TESTS); do \
		echo "Running $$test..."; \
		./$$test || exit 1; \
	done

clean:
	rm -f $(TESTS)

.PHONY: help
help:
	@echo "Till Unit Tests"
	@echo ""
	@echo "Targets:"
	@echo "  make all    - Build all test executables"
	@echo "  make test   - Run all tests"
	@echo "  make clean  - Remove test executables"
	@echo ""
	@echo "Individual tests:"
	@echo "  make test_security - Build security tests"