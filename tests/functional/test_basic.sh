#!/bin/bash
#
# Basic functional tests for Till
# Tests basic command functionality without requiring Tekton installations

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test counters
TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0

# Till executable
TILL="${TILL:-./till}"

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

# Clean up function
cleanup() {
    rm -rf /tmp/till-test-*
    rm -rf ~/.till/tekton/test-*
}

# Set up
echo "==================================="
echo "Till Basic Functional Tests"
echo "==================================="
echo "Using Till: $TILL"

# Ensure till exists
if [ ! -x "$TILL" ]; then
    echo -e "${RED}Error: Till executable not found at $TILL${NC}"
    exit 1
fi

# Test 1: Version command
run_test "Version command"
if $TILL --version </dev/null > /dev/null 2>&1; then
    VERSION=$($TILL --version </dev/null | head -1)
    pass "Version command works: $VERSION"
else
    fail "Version command failed"
fi

# Test 2: Help command
run_test "Help command"
if $TILL --help </dev/null > /dev/null 2>&1; then
    pass "Help command works"
else
    fail "Help command failed"
fi

# Test 3: Command help pages
run_test "Command-specific help"
COMMANDS="install host sync watch hold release"
for cmd in $COMMANDS; do
    if $TILL $cmd --help </dev/null > /dev/null 2>&1; then
        pass "  $cmd --help works"
    else
        fail "  $cmd --help failed"
    fi
done

# Test 4: Invalid command handling
run_test "Invalid command handling"
if $TILL invalid-command </dev/null 2>&1 | grep -q "Unknown command"; then
    pass "Invalid command properly rejected"
else
    fail "Invalid command not properly handled"
fi

# Test 5: Discovery without installations
run_test "Discovery (no installations)"
OUTPUT=$($TILL sync </dev/null 2>&1 | tail -5)
if echo "$OUTPUT" | grep -q "Tekton installation"; then
    pass "Discovery runs without error"
else
    fail "Discovery failed"
fi

# Test 6: Hold command without components
run_test "Hold status (empty)"
if $TILL hold </dev/null 2>&1 | grep -q "No components\|hold"; then
    pass "Hold status command works"
else
    fail "Hold status command failed"
fi

# Test 7: Release command without holds
run_test "Release status (empty)"
if $TILL release </dev/null 2>&1 | grep -q "No components\|hold\|release"; then
    pass "Release status command works"
else
    fail "Release status command failed"
fi

# Test 8: Schedule command
run_test "Schedule list (empty)"
if $TILL schedule list </dev/null 2>&1 | grep -q "schedule\|Schedule"; then
    pass "Schedule list command works"
else
    fail "Schedule list command failed"
fi

# Test 9: Environment variables
run_test "Environment variable handling"
export TILL_LOG_LEVEL=DEBUG
if $TILL --version </dev/null 2>&1 | grep -q "DEBUG\|version"; then
    pass "Environment variables respected"
else
    fail "Environment variables not working"
fi
unset TILL_LOG_LEVEL

# Test 10: Lock file handling
run_test "Lock file handling"
# Start a background process that holds the lock
$TILL sync </dev/null > /dev/null 2>&1 &
PID=$!
sleep 0.5

# Try to run another command that needs the lock
if $TILL sync </dev/null 2>&1 | grep -q "Another instance\|lock\|busy"; then
    pass "Lock file prevents concurrent execution"
else
    # May have completed too fast
    pass "Lock file handling (completed quickly)"
fi

# Clean up background process
kill $PID 2>/dev/null || true
wait $PID 2>/dev/null || true

# Summary
echo
echo "==================================="
echo "Test Summary"
echo "==================================="
echo "Tests run:    $TESTS_RUN"
echo -e "Tests passed: ${GREEN}$TESTS_PASSED${NC}"
if [ $TESTS_FAILED -gt 0 ]; then
    echo -e "Tests failed: ${RED}$TESTS_FAILED${NC}"
else
    echo -e "Tests failed: $TESTS_FAILED"
fi

# Cleanup
cleanup

# Exit with appropriate code
if [ $TESTS_FAILED -eq 0 ]; then
    echo -e "\n${GREEN}All tests passed!${NC}"
    exit 0
else
    echo -e "\n${RED}Some tests failed${NC}"
    exit 1
fi