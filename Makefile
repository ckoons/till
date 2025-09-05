# Makefile for Till - Tekton Lifecycle Manager
# Cross-platform build for Mac and Linux

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c99 -D_POSIX_C_SOURCE=200809L
LDFLAGS = 

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
    CFLAGS += -D__APPLE__
endif

# Directories
SRC_DIR = src
BUILD_DIR = build
BIN_DIR = .

# Target executable
TARGET = $(BIN_DIR)/till

# Source files
SOURCES = $(SRC_DIR)/till.c $(SRC_DIR)/till_install.c $(SRC_DIR)/till_host.c $(SRC_DIR)/till_schedule.c $(SRC_DIR)/till_run.c $(SRC_DIR)/cJSON.c
HEADERS = $(SRC_DIR)/till_config.h $(SRC_DIR)/till_install.h $(SRC_DIR)/till_host.h $(SRC_DIR)/till_schedule.h $(SRC_DIR)/till_run.h $(SRC_DIR)/cJSON.h

# Object files
OBJECTS = $(BUILD_DIR)/till.o $(BUILD_DIR)/till_install.o $(BUILD_DIR)/till_host.o $(BUILD_DIR)/till_schedule.o $(BUILD_DIR)/till_run.o $(BUILD_DIR)/cJSON.o

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

$(BUILD_DIR)/till_host.o: $(SRC_DIR)/till_host.c $(HEADERS)
	@echo "Compiling till_host.c..."
	@$(CC) $(CFLAGS) -c $(SRC_DIR)/till_host.c -o $(BUILD_DIR)/till_host.o

$(BUILD_DIR)/till_schedule.o: $(SRC_DIR)/till_schedule.c $(HEADERS)
	@echo "Compiling till_schedule.c..."
	@$(CC) $(CFLAGS) -c $(SRC_DIR)/till_schedule.c -o $(BUILD_DIR)/till_schedule.o

$(BUILD_DIR)/till_run.o: $(SRC_DIR)/till_run.c $(HEADERS)
	@echo "Compiling till_run.c..."
	@$(CC) $(CFLAGS) -c $(SRC_DIR)/till_run.c -o $(BUILD_DIR)/till_run.o

$(BUILD_DIR)/cJSON.o: $(SRC_DIR)/cJSON.c $(SRC_DIR)/cJSON.h
	@echo "Compiling cJSON.c..."
	@$(CC) $(CFLAGS) -c $(SRC_DIR)/cJSON.c -o $(BUILD_DIR)/cJSON.o

# Clean build files
clean:
	@echo "Cleaning build files..."
	@rm -rf $(BUILD_DIR)
	@rm -f $(TARGET)
	@echo "Clean complete"

# Install (copies to /usr/local/bin) - requires sudo
install: $(TARGET)
	@echo "Installing till to /usr/local/bin..."
	@cp $(TARGET) /usr/local/bin/
	@chmod +x /usr/local/bin/till
	@echo "Installation complete"
	@echo "You can now run 'till' from anywhere"

# Install to user's home directory - no sudo required
install-user: $(TARGET)
	@echo "Installing till to ~/.local/bin..."
	@mkdir -p $(HOME)/.local/bin
	@cp $(TARGET) $(HOME)/.local/bin/
	@chmod +x $(HOME)/.local/bin/till
	@echo "Installation complete"
	@echo ""
	@echo "Add the following to your shell configuration (.bashrc or .zshrc):"
	@echo '  export PATH="$$HOME/.local/bin:$$PATH"'
	@echo ""
	@echo "Or create an alias:"
	@echo "  alias till='$(CURDIR)/till'"

# Uninstall
uninstall:
	@echo "Removing till from /usr/local/bin..."
	@rm -f /usr/local/bin/till
	@echo "Uninstall complete"

# Run tests (to be implemented)
test: $(TARGET)
	@echo "Running tests..."
	@./$(TARGET) --version
	@./$(TARGET) --help > /dev/null
	@echo "Basic tests passed"

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
	@echo "  install     - Install to /usr/local/bin (requires sudo)"
	@echo "  install-user- Install to ~/.local/bin (no sudo)"
	@echo "  uninstall   - Remove from /usr/local/bin"
	@echo "  debug       - Build with debug symbols"
	@echo "  test        - Run basic tests"
	@echo "  info        - Show build configuration"
	@echo "  help        - Show this help message"

.PHONY: all clean install uninstall test debug info help