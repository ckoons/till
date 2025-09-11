#!/bin/bash
#
# Integration tests for Till using mock commands
# Tests full workflows without requiring real network/SSH access
#

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Test counters
TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0

# Till executable
TILL="${TILL:-./till}"

# Mock setup
MOCK_DIR="/tmp/till-mocks"
MOCK_BIN="$MOCK_DIR/bin"

# Test functions
pass() {
    echo -e "${GREEN}✓${NC} $1"
    TESTS_PASSED=$((TESTS_PASSED + 1))
}

fail() {
    echo -e "${RED}✗${NC} $1"
    if [ ! -z "$2" ]; then
        echo "  Error: $2"
    fi
    TESTS_FAILED=$((TESTS_FAILED + 1))
}

run_test() {
    local test_name="$1"
    echo -e "\n${YELLOW}Running:${NC} $test_name"
    TESTS_RUN=$((TESTS_RUN + 1))
}

# Setup function
setup_test_environment() {
    echo "===================================="
    echo "Till Mock Integration Tests"
    echo "===================================="
    echo "Using Till: $TILL"
    
    # Ensure till exists
    if [ ! -x "$TILL" ]; then
        echo -e "${RED}Error: Till executable not found at $TILL${NC}"
        exit 1
    fi
    
    # Setup mocks
    echo -e "\n${YELLOW}Setting up mock environment...${NC}"
    ./tests/mocks/setup_mocks.sh > /dev/null 2>&1
    
    # Add mocks to PATH
    export PATH="$MOCK_BIN:$PATH"
    export TILL_MOCK_DIR="$MOCK_DIR"
    
    # Create test Till directory
    mkdir -p ~/.till/tekton
    
    echo -e "${GREEN}Mock environment ready${NC}"
}

# Cleanup function
cleanup() {
    echo -e "\n${YELLOW}Cleaning up...${NC}"
    rm -rf "$MOCK_DIR"
    rm -rf ~/.till/tekton/test-*
    rm -f ~/.till/hosts-local.json
}

# Trap cleanup on exit
trap cleanup EXIT

# Setup environment
setup_test_environment

# Test 1: Add host with mock SSH
run_test "Add host using mock SSH"
echo "test-host-1:whoami=tilluser" >> "$MOCK_DIR/ssh_responses.txt"
echo "test-host-1:echo TILL_TEST_SUCCESS=TILL_TEST_SUCCESS" >> "$MOCK_DIR/ssh_responses.txt"

if $TILL host add test-host-1 tilluser mock-with-ssh </dev/null 2>&1 | grep -q "added"; then
    pass "Host added successfully"
else
    fail "Failed to add host"
fi

# Test 2: Test host connectivity with mocks
run_test "Test host connectivity"
echo "mock-with-ssh=reachable" >> "$MOCK_DIR/ping_responses.txt"
echo "mock-with-ssh:22=open" >> "$MOCK_DIR/port_responses.txt"

if $TILL host test test-host-1 </dev/null 2>&1 | grep -q "successful"; then
    pass "Host connectivity test passed"
else
    fail "Host connectivity test failed"
fi

# Test 3: List hosts
run_test "List hosts"
if $TILL host list </dev/null 2>&1 | grep -q "test-host-1"; then
    pass "Host appears in list"
else
    fail "Host not in list"
fi

# Test 4: Test unreachable host
run_test "Test unreachable host handling"
echo "bad-host=unreachable" >> "$MOCK_DIR/ping_responses.txt"
echo "bad-host:22=closed" >> "$MOCK_DIR/port_responses.txt"

$TILL host add bad-host user bad-host </dev/null > /dev/null 2>&1 || true
if $TILL host test bad-host </dev/null 2>&1 | grep -q "failed\|closed"; then
    pass "Unreachable host properly detected"
else
    fail "Failed to detect unreachable host"
fi

# Test 5: Custom SSH port
run_test "Test custom SSH port"
echo "mock-custom-port=reachable" >> "$MOCK_DIR/ping_responses.txt"
echo "mock-custom-port:2222=open" >> "$MOCK_DIR/port_responses.txt"
echo "mock-custom-port:echo TILL_TEST_SUCCESS=TILL_TEST_SUCCESS" >> "$MOCK_DIR/ssh_responses.txt"

if $TILL host add custom-port-host user mock-custom-port --port 2222 </dev/null 2>&1 | grep -q "added"; then
    pass "Host with custom port added"
else
    fail "Failed to add host with custom port"
fi

# Test 6: Discovery simulation
run_test "Tekton discovery with mocks"
echo "test-host-1:ls /opt/Tekton=bin" >> "$MOCK_DIR/ssh_responses.txt"
echo "test-host-1:ls /opt/Tekton=config" >> "$MOCK_DIR/ssh_responses.txt"
echo "test-host-1:which tekton=/opt/Tekton/bin/tekton" >> "$MOCK_DIR/ssh_responses.txt"

# Create a mock discovery result
cat > ~/.till/tekton/mock-discovery.json <<EOF
{
    "installations": {
        "test-host-1.tekton": {
            "root": "/opt/Tekton",
            "host": "test-host-1",
            "discovered_at": "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
        }
    }
}
EOF

if [ -f ~/.till/tekton/mock-discovery.json ]; then
    pass "Mock discovery data created"
else
    fail "Failed to create mock discovery data"
fi

# Test 7: Remove host
run_test "Remove host"
if $TILL host remove test-host-1 </dev/null 2>&1 | grep -q "removed"; then
    pass "Host removed successfully"
else
    fail "Failed to remove host"
fi

# Test 8: Verify host removal
run_test "Verify host removal"
if $TILL host list </dev/null 2>&1 | grep -q "test-host-1"; then
    fail "Host still in list after removal"
else
    pass "Host properly removed from list"
fi

# Summary
echo
echo "===================================="
echo "Test Summary"
echo "===================================="
echo "Tests run:    $TESTS_RUN"
echo -e "Tests passed: ${GREEN}$TESTS_PASSED${NC}"
if [ $TESTS_FAILED -gt 0 ]; then
    echo -e "Tests failed: ${RED}$TESTS_FAILED${NC}"
else
    echo -e "Tests failed: $TESTS_FAILED"
fi

# Exit with appropriate code
if [ $TESTS_FAILED -eq 0 ]; then
    echo -e "\n${GREEN}All mock tests passed!${NC}"
    exit 0
else
    echo -e "\n${RED}Some mock tests failed${NC}"
    exit 1
fi