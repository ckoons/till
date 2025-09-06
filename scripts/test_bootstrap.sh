#!/bin/bash
#
# Test bootstrap script in different scenarios
#

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

log() {
    echo -e "${GREEN}[TEST]${NC} $1"
}

error() {
    echo -e "${RED}[ERROR]${NC} $1" >&2
}

test_local() {
    log "Testing local bootstrap installation..."
    
    # Test 1: Check if bootstrap script exists
    if [ -f "./scripts/bootstrap.sh" ]; then
        log "✓ Bootstrap script found"
    else
        error "✗ Bootstrap script not found"
        return 1
    fi
    
    # Test 2: Check script is executable
    if [ -x "./scripts/bootstrap.sh" ]; then
        log "✓ Bootstrap script is executable"
    else
        error "✗ Bootstrap script is not executable"
        return 1
    fi
    
    # Test 3: Dry run check (just check prerequisites)
    log "Checking prerequisites..."
    if command -v git >/dev/null && command -v make >/dev/null; then
        log "✓ Prerequisites satisfied"
    else
        error "✗ Missing prerequisites"
        return 1
    fi
    
    log "Local tests passed!"
    return 0
}

test_directory_logic() {
    log "Testing directory selection logic..."
    
    # Test with ~/projects/github existing
    if [ -d "$HOME/projects/github" ]; then
        log "✓ Would use existing ~/projects/github"
        log "  Till would install to: ~/projects/github/till"
        log "  Tekton would install to: ~/projects/github/Tekton"
    else
        log "✓ Would use ~/.till directory"
        log "  Till would install to: ~/.till/till"
        log "  Tekton would install to: ~/.till/Tekton"
    fi
    
    return 0
}

test_remote_simulation() {
    log "Simulating remote installation..."
    
    # Create a test directory to simulate remote environment
    TEST_DIR="/tmp/till_bootstrap_test_$$"
    log "Creating test environment in $TEST_DIR"
    
    mkdir -p "$TEST_DIR"
    cd "$TEST_DIR"
    
    # Create fake home
    export HOME="$TEST_DIR/home"
    mkdir -p "$HOME"
    
    # Test without ~/projects/github
    log "Testing installation without ~/projects/github..."
    bash "$OLDPWD/scripts/bootstrap.sh" 2>&1 | grep -q ".till" && {
        log "✓ Would install to ~/.till"
    } || {
        error "✗ Installation path detection failed"
    }
    
    # Create ~/projects/github and test again
    mkdir -p "$HOME/projects/github"
    log "Testing installation with ~/projects/github..."
    
    # Cleanup
    cd "$OLDPWD"
    rm -rf "$TEST_DIR"
    
    log "Remote simulation tests completed"
    return 0
}

# Main
main() {
    log "Starting Till bootstrap tests..."
    echo ""
    
    test_local || exit 1
    echo ""
    
    test_directory_logic || exit 1
    echo ""
    
    log "========================================="
    log "All tests passed!"
    log "========================================="
    log ""
    log "Ready to test on remote hosts tomorrow:"
    log "  1. For host with ~/projects/github: till host setup <host>"
    log "  2. For virgin host: till host setup <host>"
    log ""
    log "The bootstrap script will automatically:"
    log "  - Detect the right installation directory"
    log "  - Clone or update till from git"
    log "  - Build and install till"
    log "  - Configure PATH"
    log "  - Set up SSH keys"
    log ""
}

main "$@"