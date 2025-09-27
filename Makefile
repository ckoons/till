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
SOURCES = $(SRC_DIR)/till.c $(SRC_DIR)/till_install.c $(SRC_DIR)/till_tekton.c $(SRC_DIR)/till_host.c $(SRC_DIR)/till_hold.c $(SRC_DIR)/till_schedule.c $(SRC_DIR)/till_run.c $(SRC_DIR)/till_common.c $(SRC_DIR)/till_common_extra.c $(SRC_DIR)/till_registry.c $(SRC_DIR)/till_commands.c $(SRC_DIR)/till_platform.c $(SRC_DIR)/till_platform_process.c $(SRC_DIR)/till_platform_schedule.c $(SRC_DIR)/till_security.c $(SRC_DIR)/till_validate.c $(SRC_DIR)/till_progress.c $(SRC_DIR)/till_federation.c $(SRC_DIR)/till_federation_gist.c $(SRC_DIR)/till_federation_admin.c $(SRC_DIR)/till_menu.c $(SRC_DIR)/cJSON.c
HEADERS = $(SRC_DIR)/till_config.h $(SRC_DIR)/till_install.h $(SRC_DIR)/till_tekton.h $(SRC_DIR)/till_host.h $(SRC_DIR)/till_hold.h $(SRC_DIR)/till_schedule.h $(SRC_DIR)/till_run.h $(SRC_DIR)/till_common.h $(SRC_DIR)/till_registry.h $(SRC_DIR)/till_commands.h $(SRC_DIR)/till_platform.h $(SRC_DIR)/till_security.h $(SRC_DIR)/till_validate.h $(SRC_DIR)/till_progress.h $(SRC_DIR)/till_federation.h $(SRC_DIR)/till_menu.h $(SRC_DIR)/cJSON.h

# Object files
OBJECTS = $(BUILD_DIR)/till.o $(BUILD_DIR)/till_install.o $(BUILD_DIR)/till_tekton.o $(BUILD_DIR)/till_host.o $(BUILD_DIR)/till_hold.o $(BUILD_DIR)/till_schedule.o $(BUILD_DIR)/till_run.o $(BUILD_DIR)/till_common.o $(BUILD_DIR)/till_common_extra.o $(BUILD_DIR)/till_registry.o $(BUILD_DIR)/till_commands.o $(BUILD_DIR)/till_platform.o $(BUILD_DIR)/till_platform_process.o $(BUILD_DIR)/till_platform_schedule.o $(BUILD_DIR)/till_security.o $(BUILD_DIR)/till_validate.o $(BUILD_DIR)/till_progress.o $(BUILD_DIR)/till_federation.o $(BUILD_DIR)/till_federation_gist.o $(BUILD_DIR)/till_federation_admin.o $(BUILD_DIR)/till_menu.o $(BUILD_DIR)/cJSON.o

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

$(BUILD_DIR)/till_federation.o: $(SRC_DIR)/till_federation.c $(HEADERS)
	@echo "Compiling till_federation.c..."
	@$(CC) $(CFLAGS) -c $(SRC_DIR)/till_federation.c -o $(BUILD_DIR)/till_federation.o

$(BUILD_DIR)/till_federation_gist.o: $(SRC_DIR)/till_federation_gist.c $(HEADERS)
	@echo "Compiling till_federation_gist.c..."
	@$(CC) $(CFLAGS) -c $(SRC_DIR)/till_federation_gist.c -o $(BUILD_DIR)/till_federation_gist.o

$(BUILD_DIR)/till_federation_admin.o: $(SRC_DIR)/till_federation_admin.c $(HEADERS)
	@echo "Compiling till_federation_admin.c..."
	@$(CC) $(CFLAGS) -c $(SRC_DIR)/till_federation_admin.c -o $(BUILD_DIR)/till_federation_admin.o

$(BUILD_DIR)/till_menu.o: $(SRC_DIR)/till_menu.c $(HEADERS)
	@echo "Compiling till_menu.c..."
	@$(CC) $(CFLAGS) -c $(SRC_DIR)/till_menu.c -o $(BUILD_DIR)/till_menu.o

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

# Check prerequisites and attempt auto-install if missing
.PHONY: check-prereqs
check-prereqs:
	@echo "Checking and installing prerequisites..."
	@if ! command -v git >/dev/null 2>&1; then \
		echo "Warning: git is not installed. Attempting to install..."; \
		if [ "$$(uname -s)" = "Darwin" ]; then \
			if command -v brew >/dev/null 2>&1; then \
				brew install git || echo "Warning: Could not install git automatically"; \
			else \
				echo "Installing Homebrew first..."; \
				/bin/bash -c "$$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)" && \
				brew install git || echo "Warning: Could not install git automatically"; \
			fi; \
		elif [ "$$(uname -s)" = "Linux" ]; then \
			if command -v apt-get >/dev/null 2>&1; then \
				sudo apt-get update && sudo apt-get install -y git || echo "Warning: Could not install git automatically"; \
			elif command -v yum >/dev/null 2>&1; then \
				sudo yum install -y git || echo "Warning: Could not install git automatically"; \
			fi; \
		fi; \
		if ! command -v git >/dev/null 2>&1; then \
			echo "Error: git installation failed. Please install manually: https://git-scm.com/"; \
			exit 1; \
		fi; \
	fi
	@if ! command -v gh >/dev/null 2>&1; then \
		echo "Warning: GitHub CLI (gh) is not installed. Attempting to install..."; \
		if [ "$$(uname -s)" = "Darwin" ]; then \
			if command -v brew >/dev/null 2>&1; then \
				brew install gh || echo "Warning: Could not install gh automatically"; \
			else \
				echo "Installing Homebrew first..."; \
				/bin/bash -c "$$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)" && \
				brew install gh || echo "Warning: Could not install gh automatically"; \
			fi; \
		elif [ "$$(uname -s)" = "Linux" ]; then \
			if command -v apt-get >/dev/null 2>&1; then \
				curl -fsSL https://cli.github.com/packages/githubcli-archive-keyring.gpg | sudo dd of=/usr/share/keyrings/githubcli-archive-keyring.gpg 2>/dev/null && \
				echo "deb [arch=$$(dpkg --print-architecture) signed-by=/usr/share/keyrings/githubcli-archive-keyring.gpg] https://cli.github.com/packages stable main" | sudo tee /etc/apt/sources.list.d/github-cli.list > /dev/null && \
				sudo apt update && sudo apt install gh -y || echo "Warning: Could not install gh automatically"; \
			elif command -v yum >/dev/null 2>&1; then \
				sudo yum-config-manager --add-repo https://cli.github.com/packages/rpm/gh-cli.repo && \
				sudo yum install gh -y || echo "Warning: Could not install gh automatically"; \
			fi; \
		fi; \
		if ! command -v gh >/dev/null 2>&1; then \
			echo "Warning: gh installation failed. Federation features will be limited."; \
			echo "To install manually: https://cli.github.com"; \
		else \
			echo "✓ GitHub CLI (gh) installed successfully"; \
		fi; \
	else \
		echo "✓ GitHub CLI (gh) already installed"; \
	fi
	@echo "✓ Prerequisites check complete"

# Setup GitHub authentication
.PHONY: setup-github
setup-github:
	@echo "Checking GitHub authentication..."
	@if gh auth status >/dev/null 2>&1; then \
		echo "✓ GitHub CLI authenticated"; \
		if gh api user -i 2>/dev/null | grep -q "X-OAuth-Scopes:.*gist"; then \
			echo "✓ Token has 'gist' scope for federation"; \
		else \
			echo "ℹ️  Token missing 'gist' scope (optional for federation)"; \
			echo "   To add: gh auth refresh -s gist"; \
		fi \
	else \
		echo "ℹ️  GitHub CLI not authenticated (optional)"; \
		echo "   For federation features, run: gh auth login -s gist"; \
	fi

install: $(TARGET) man check-prereqs setup-github
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
	@# Create .till directory and initial hosts file
	@echo "Initializing Till configuration..."
	@mkdir -p .till
	@if [ ! -f .till/hosts-local.json ]; then \
		echo "Creating initial hosts-local.json..."; \
		HOSTNAME=$$(hostname); \
		TILL_PATH=$$(pwd); \
		echo "{\"hosts\":{\"local\":{\"user\":\"$$USER\",\"host\":\"localhost\",\"port\":22,\"status\":\"ready\",\"hostname\":\"$$HOSTNAME\",\"till_configured\":\"yes\",\"till_path\":\"$$TILL_PATH\"}},\"updated\":\"$$(date +%s)\"}" | python3 -m json.tool > .till/hosts-local.json 2>/dev/null || \
		echo '{"hosts":{"local":{"user":"'$$USER'","host":"localhost","port":22,"status":"ready","hostname":"'$$HOSTNAME'","till_configured":"yes","till_path":"'$$TILL_PATH'"}},"updated":"'$$(date +%s)'"}' > .till/hosts-local.json; \
		echo "Initial hosts file created"; \
	else \
		echo "Using existing hosts-local.json"; \
	fi
	@# Create default federation.json in global ~/.till if it doesn't exist
	@mkdir -p $(HOME)/.till
	@if [ ! -f $(HOME)/.till/federation.json ]; then \
		echo "Creating default federation.json..."; \
		HOSTNAME=$$(hostname); \
		HEX_TIME=$$(printf "%lx" $$(date +%s)); \
		SITE_ID="$$HOSTNAME.$$HEX_TIME.till"; \
		echo '{"federation_mode":"anonymous","site_id":"'$$SITE_ID'","joined_date":null,"last_sync":null,"sync_enabled":true,"menu_version":null,"menu_last_processed":null}' | python3 -m json.tool > $(HOME)/.till/federation.json 2>/dev/null || \
		echo '{"federation_mode":"anonymous","site_id":"'$$SITE_ID'","joined_date":null,"last_sync":null,"sync_enabled":true,"menu_version":null,"menu_last_processed":null}' > $(HOME)/.till/federation.json; \
		echo "Created federation config with site_id: $$SITE_ID"; \
	else \
		echo "Using existing federation.json"; \
	fi
	@echo ""
	@echo "Checking dependencies..."
	@if command -v git >/dev/null 2>&1; then \
		echo "  ✓ git is installed"; \
	else \
		echo "  ✗ git is NOT installed (required)"; \
		echo "    Install: https://git-scm.com"; \
	fi
	@if command -v gh >/dev/null 2>&1; then \
		echo "  ✓ GitHub CLI (gh) is installed"; \
		if gh auth status >/dev/null 2>&1; then \
			echo "  ✓ GitHub CLI is authenticated"; \
		else \
			echo "  ⚠ GitHub CLI is not authenticated"; \
			echo "    Run: gh auth login -s gist"; \
		fi \
	else \
		echo "  ✗ GitHub CLI (gh) is NOT installed"; \
		echo "    Required for named/trusted federation modes"; \
		echo "    Install: https://cli.github.com"; \
		echo "    macOS:  brew install gh"; \
		echo "    Linux:  See https://github.com/cli/cli#installation"; \
	fi
	@echo ""
	@echo "========================================="
	@echo "✓ Till installation complete!"
	@echo "========================================="
	@echo ""
	@echo "You can now run 'till' from anywhere"
	@echo ""
	@if gh auth status >/dev/null 2>&1; then \
		echo "Federation Quick Start:"; \
		echo "  till federate status          # Check federation status"; \
		echo "  till federate join named      # Join as named member"; \
	else \
		echo "To enable federation features:"; \
		echo "  1. Install gh if not installed"; \
		echo "  2. gh auth login -s gist       # Authenticate GitHub CLI"; \
		echo "  3. till federate join named    # Join federation"; \
	fi
	@echo ""
	@echo "For help: till --help"

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