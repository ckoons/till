# Makefile for Till - Tekton Lifecycle Manager
# Cross-platform build for Mac and Linux

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c99 -D_POSIX_C_SOURCE=200809L
LDFLAGS = -lpthread 

# Debug build
ifdef DEBUG
    CFLAGS += -g -DDEBUG
endif

# Platform detection
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    CFLAGS += -D__linux__
endif
ifeq ($(UNAME_S),Darwin)
    CFLAGS += -D__APPLE__ -D_DARWIN_C_SOURCE
endif

# Directories
SRC_DIR = src
BUILD_DIR = build
BIN_DIR = .

# Target executable
TARGET = $(BIN_DIR)/till

# Source files
SOURCES = $(SRC_DIR)/till.c $(SRC_DIR)/till_install.c $(SRC_DIR)/till_tekton.c $(SRC_DIR)/till_host.c $(SRC_DIR)/till_hold.c $(SRC_DIR)/till_schedule.c $(SRC_DIR)/till_run.c $(SRC_DIR)/till_common.c $(SRC_DIR)/till_common_extra.c $(SRC_DIR)/till_registry.c $(SRC_DIR)/till_commands.c $(SRC_DIR)/till_platform.c $(SRC_DIR)/till_platform_process.c $(SRC_DIR)/till_platform_schedule.c $(SRC_DIR)/till_security.c $(SRC_DIR)/till_validate.c $(SRC_DIR)/till_progress.c $(SRC_DIR)/cJSON.c
HEADERS = $(SRC_DIR)/till_config.h $(SRC_DIR)/till_install.h $(SRC_DIR)/till_tekton.h $(SRC_DIR)/till_host.h $(SRC_DIR)/till_hold.h $(SRC_DIR)/till_schedule.h $(SRC_DIR)/till_run.h $(SRC_DIR)/till_common.h $(SRC_DIR)/till_registry.h $(SRC_DIR)/till_commands.h $(SRC_DIR)/till_platform.h $(SRC_DIR)/till_security.h $(SRC_DIR)/till_validate.h $(SRC_DIR)/till_progress.h $(SRC_DIR)/cJSON.h

# Object files
OBJECTS = $(BUILD_DIR)/till.o $(BUILD_DIR)/till_install.o $(BUILD_DIR)/till_tekton.o $(BUILD_DIR)/till_host.o $(BUILD_DIR)/till_hold.o $(BUILD_DIR)/till_schedule.o $(BUILD_DIR)/till_run.o $(BUILD_DIR)/till_common.o $(BUILD_DIR)/till_common_extra.o $(BUILD_DIR)/till_registry.o $(BUILD_DIR)/till_commands.o $(BUILD_DIR)/till_platform.o $(BUILD_DIR)/till_platform_process.o $(BUILD_DIR)/till_platform_schedule.o $(BUILD_DIR)/till_security.o $(BUILD_DIR)/till_validate.o $(BUILD_DIR)/till_progress.o $(BUILD_DIR)/cJSON.o

# Default target
all: $(TARGET)

# Create build directory
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

# Build the executable
$(TARGET): $(BUILD_DIR) $(OBJECTS)
	@echo "Linking $(TARGET)..."
	@$(CC) $(OBJECTS) $(LDFLAGS) -o $(TARGET)
	@echo "Build complete: $(TARGET)"
	@echo "Run './till --help' for usage information"

# Compile source files
$(BUILD_DIR)/till.o: $(SRC_DIR)/till.c $(HEADERS)
	@echo "Compiling till.c..."
	@$(CC) $(CFLAGS) -c $(SRC_DIR)/till.c -o $(BUILD_DIR)/till.o

$(BUILD_DIR)/till_install.o: $(SRC_DIR)/till_install.c $(HEADERS)
	@echo "Compiling till_install.c..."
	@$(CC) $(CFLAGS) -c $(SRC_DIR)/till_install.c -o $(BUILD_DIR)/till_install.o

$(BUILD_DIR)/till_tekton.o: $(SRC_DIR)/till_tekton.c $(HEADERS)
	@echo "Compiling till_tekton.c..."
	@$(CC) $(CFLAGS) -c $(SRC_DIR)/till_tekton.c -o $(BUILD_DIR)/till_tekton.o

$(BUILD_DIR)/till_host.o: $(SRC_DIR)/till_host.c $(HEADERS)
	@echo "Compiling till_host.c..."
	@$(CC) $(CFLAGS) -c $(SRC_DIR)/till_host.c -o $(BUILD_DIR)/till_host.o

$(BUILD_DIR)/till_hold.o: $(SRC_DIR)/till_hold.c $(HEADERS)
	@echo "Compiling till_hold.c..."
	@$(CC) $(CFLAGS) -c $(SRC_DIR)/till_hold.c -o $(BUILD_DIR)/till_hold.o

$(BUILD_DIR)/till_schedule.o: $(SRC_DIR)/till_schedule.c $(HEADERS)
	@echo "Compiling till_schedule.c..."
	@$(CC) $(CFLAGS) -c $(SRC_DIR)/till_schedule.c -o $(BUILD_DIR)/till_schedule.o

$(BUILD_DIR)/till_run.o: $(SRC_DIR)/till_run.c $(HEADERS)
	@echo "Compiling till_run.c..."
	@$(CC) $(CFLAGS) -c $(SRC_DIR)/till_run.c -o $(BUILD_DIR)/till_run.o

$(BUILD_DIR)/till_common.o: $(SRC_DIR)/till_common.c $(HEADERS)
	@echo "Compiling till_common.c..."
	@$(CC) $(CFLAGS) -c $(SRC_DIR)/till_common.c -o $(BUILD_DIR)/till_common.o

$(BUILD_DIR)/till_common_extra.o: $(SRC_DIR)/till_common_extra.c $(HEADERS)
	@echo "Compiling till_common_extra.c..."
	@$(CC) $(CFLAGS) -c $(SRC_DIR)/till_common_extra.c -o $(BUILD_DIR)/till_common_extra.o

$(BUILD_DIR)/till_registry.o: $(SRC_DIR)/till_registry.c $(HEADERS)
	@echo "Compiling till_registry.c..."
	@$(CC) $(CFLAGS) -c $(SRC_DIR)/till_registry.c -o $(BUILD_DIR)/till_registry.o

$(BUILD_DIR)/till_commands.o: $(SRC_DIR)/till_commands.c $(HEADERS)
	@echo "Compiling till_commands.c..."
	@$(CC) $(CFLAGS) -c $(SRC_DIR)/till_commands.c -o $(BUILD_DIR)/till_commands.o

$(BUILD_DIR)/till_platform.o: $(SRC_DIR)/till_platform.c $(HEADERS)
	@echo "Compiling till_platform.c..."
	@$(CC) $(CFLAGS) -c $(SRC_DIR)/till_platform.c -o $(BUILD_DIR)/till_platform.o

$(BUILD_DIR)/till_platform_process.o: $(SRC_DIR)/till_platform_process.c $(HEADERS)
	@echo "Compiling till_platform_process.c..."
	@$(CC) $(CFLAGS) -c $(SRC_DIR)/till_platform_process.c -o $(BUILD_DIR)/till_platform_process.o

$(BUILD_DIR)/till_platform_schedule.o: $(SRC_DIR)/till_platform_schedule.c $(HEADERS)
	@echo "Compiling till_platform_schedule.c..."
	@$(CC) $(CFLAGS) -c $(SRC_DIR)/till_platform_schedule.c -o $(BUILD_DIR)/till_platform_schedule.o

$(BUILD_DIR)/till_security.o: $(SRC_DIR)/till_security.c $(HEADERS)
	@echo "Compiling till_security.c..."
	@$(CC) $(CFLAGS) -c $(SRC_DIR)/till_security.c -o $(BUILD_DIR)/till_security.o

$(BUILD_DIR)/till_validate.o: $(SRC_DIR)/till_validate.c $(HEADERS)
	@echo "Compiling till_validate.c..."
	@$(CC) $(CFLAGS) -c $(SRC_DIR)/till_validate.c -o $(BUILD_DIR)/till_validate.o

$(BUILD_DIR)/till_progress.o: $(SRC_DIR)/till_progress.c $(HEADERS)
	@echo "Compiling till_progress.c..."
	@$(CC) $(CFLAGS) -c $(SRC_DIR)/till_progress.c -o $(BUILD_DIR)/till_progress.o

$(BUILD_DIR)/cJSON.o: $(SRC_DIR)/cJSON.c $(SRC_DIR)/cJSON.h
	@echo "Compiling cJSON.c..."
	@$(CC) $(CFLAGS) -c $(SRC_DIR)/cJSON.c -o $(BUILD_DIR)/cJSON.o

# Clean build files
clean:
	@echo "Cleaning build files..."
	@rm -rf $(BUILD_DIR)
	@rm -f $(TARGET)
	@echo "Clean complete"

# Install - smart install that tries system-wide first, falls back to user
# Generate man page from help text
man: $(TARGET)
	@echo "Generating man page..."
	@echo ".TH TILL 1 \"$$(date +'%B %Y')\" \"Till 1.0.0\" \"Till Manual\"" > till.1
	@echo ".SH NAME" >> till.1
	@echo "till \\- Tekton Installation Lifecycle Manager" >> till.1
	@echo ".SH SYNOPSIS" >> till.1
	@echo ".B till" >> till.1
	@echo "[\\fICOMMAND\\fR] [\\fIOPTIONS\\fR]" >> till.1
	@echo ".SH DESCRIPTION" >> till.1
	@echo "Till manages Tekton installations across local and remote systems." >> till.1
	@echo ".SH FILES" >> till.1
	@echo ".TP" >> till.1
	@echo ".I ~/.till/" >> till.1
	@echo "Till configuration directory" >> till.1
	@echo "Man page generated"

install: $(TARGET) man
	@if [ -w /usr/local/bin ]; then \
		echo "Installing till to /usr/local/bin..."; \
		cp $(TARGET) /usr/local/bin/; \
		chmod +x /usr/local/bin/till; \
		echo "Installation complete in /usr/local/bin"; \
	else \
		echo "Installing till to ~/.local/bin (no sudo access)..."; \
		mkdir -p $(HOME)/.local/bin; \
		cp $(TARGET) $(HOME)/.local/bin/; \
		chmod +x $(HOME)/.local/bin/till; \
		echo "Installation complete in ~/.local/bin"; \
		if ! echo $$PATH | grep -q "$$HOME/.local/bin"; then \
			echo ""; \
			echo "WARNING: ~/.local/bin is not in your PATH"; \
			echo "Add this to your shell configuration:"; \
			echo '  export PATH="$$HOME/.local/bin:$$PATH"'; \
		fi; \
	fi
	@echo "You can now run 'till' from anywhere"

# Uninstall - removes from both locations
uninstall:
	@echo "Removing till..."
	@rm -f /usr/local/bin/till 2>/dev/null || true
	@rm -f $(HOME)/.local/bin/till 2>/dev/null || true
	@echo "Uninstall complete"

# Run tests
test: $(TARGET)
	@echo "Running Till test suite..."
	@chmod +x tests/run_tests.sh tests/*/*.sh
	@./tests/run_tests.sh

# Debug build
debug:
	@$(MAKE) DEBUG=1

# Show configuration
info:
	@echo "Till Build Configuration"
	@echo "========================"
	@echo "Platform: $(UNAME_S)"
	@echo "Compiler: $(CC)"
	@echo "CFLAGS: $(CFLAGS)"
	@echo "LDFLAGS: $(LDFLAGS)"
	@echo "Target: $(TARGET)"

# Help
help:
	@echo "Till Makefile"
	@echo "============="
	@echo "Targets:"
	@echo "  all         - Build till (default)"
	@echo "  clean       - Remove build files"
	@echo "  install     - Install till (auto-detects best location)"
	@echo "  uninstall   - Remove till from system"
	@echo "  debug       - Build with debug symbols"
	@echo "  test        - Run basic tests"
	@echo "  info        - Show build configuration"
	@echo "  help        - Show this help message"

.PHONY: all clean install uninstall test debug info help man