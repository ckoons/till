#!/bin/bash
#
# Functional tests for Till Hold/Release functionality
# Tests hold and release commands with mock components

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

# Set up mock registry
setup_mock_registry() {
    mkdir -p ~/.till/tekton
    cat > ~/.till/tekton/till-private.json <<EOF
{
    "installations": {
        "test.component.one": {
            "root": "/tmp/test1",
            "main_root": "/tmp/test1",
            "discovered_at": "2024-01-01T00:00:00"
        },
        "test.component.two": {
            "root": "/tmp/test2",
            "main_root": "/tmp/test1",
            "discovered_at": "2024-01-01T00:00:00"
        },
        "test.component.three": {
            "root": "/tmp/test3",
            "main_root": "/tmp/test1",
            "discovered_at": "2024-01-01T00:00:00"
        }
    },
    "holds": {}
}
EOF
}

# Clean up function
cleanup() {
    rm -f ~/.till/tekton/till-private.json
    rm -rf ~/.till/tekton/test-*
}

# Set up
echo "==================================="
echo "Till Hold/Release Functional Tests"
echo "==================================="
echo "Using Till: $TILL"

# Ensure till exists
if [ ! -x "$TILL" ]; then
    echo -e "${RED}Error: Till executable not found at $TILL${NC}"
    exit 1
fi

# Set up mock registry
setup_mock_registry

# Test 1: Hold a single component
run_test "Hold single component"
if $TILL hold test.component.one --reason "Testing hold" </dev/null 2>&1 | grep -q "Successfully held"; then
    pass "Single component held"
else
    fail "Failed to hold single component"
fi

# Test 2: Verify hold status
run_test "Verify hold status"
if $TILL hold </dev/null 2>&1 | grep -q "test.component.one.*HELD\|Currently Held"; then
    pass "Hold status shows held component"
else
    fail "Hold status doesn't show held component"
fi

# Test 3: Hold with duration
run_test "Hold with duration"
if $TILL hold test.component.two --duration 1h --reason "Temporary hold" </dev/null 2>&1 | grep -q "Successfully held"; then
    pass "Component held with duration"
else
    fail "Failed to hold with duration"
fi

# Test 4: Hold with specific end time
run_test "Hold with end time"
END_TIME=$(date -v+1d '+%Y-%m-%d %H:%M' 2>/dev/null || date -d '+1 day' '+%Y-%m-%d %H:%M' 2>/dev/null || echo "2025-12-31 23:59")
if $TILL hold test.component.three --until "$END_TIME" --reason "Scheduled hold" </dev/null 2>&1 | grep -q "Successfully held"; then
    pass "Component held with end time"
else
    fail "Failed to hold with end time"
fi

# Test 5: Attempt to hold already held component
run_test "Hold already held component (should fail)"
if $TILL hold test.component.one --reason "Duplicate hold" </dev/null 2>&1 | grep -q "already held\|Failed"; then
    pass "Duplicate hold properly rejected"
else
    fail "Duplicate hold not properly handled"
fi

# Test 6: Force hold on already held component
run_test "Force hold on held component"
if $TILL hold test.component.one --force --reason "Forced hold" </dev/null 2>&1 | grep -q "Successfully held"; then
    pass "Force hold succeeded"
else
    fail "Force hold failed"
fi

# Test 7: Hold multiple components
run_test "Hold multiple components"
# First release all
$TILL release --all > /dev/null </dev/null 2>&1
if $TILL hold test.component.one,test.component.two --reason "Multi hold" </dev/null 2>&1 | grep -q "Successfully held 2"; then
    pass "Multiple components held"
else
    fail "Failed to hold multiple components"
fi

# Test 8: Release single component
run_test "Release single component"
if $TILL release test.component.one </dev/null 2>&1 | grep -q "Successfully released"; then
    pass "Single component released"
else
    fail "Failed to release component"
fi

# Test 9: Release non-held component
run_test "Release non-held component"
if $TILL release test.component.one </dev/null 2>&1 | grep -q "not held\|Warning"; then
    pass "Release of non-held component handled"
else
    fail "Release of non-held component not properly handled"
fi

# Test 10: Hold all components
run_test "Hold all components"
$TILL release --all > /dev/null </dev/null 2>&1
if $TILL hold --all --reason "Global hold" </dev/null 2>&1 | grep -q "Successfully held"; then
    pass "All components held"
else
    fail "Failed to hold all components"
fi

# Test 11: Verify all held
run_test "Verify all components held"
HOLD_STATUS=$($TILL hold </dev/null 2>&1)
if echo "$HOLD_STATUS" | grep -q "test.component.one.*HELD" && \
   echo "$HOLD_STATUS" | grep -q "test.component.two.*HELD" && \
   echo "$HOLD_STATUS" | grep -q "test.component.three.*HELD"; then
    pass "All components shown as held"
else
    fail "Not all components shown as held"
fi

# Test 12: Release all components
run_test "Release all components"
if $TILL release --all </dev/null 2>&1 | grep -q "Successfully released"; then
    pass "All components released"
else
    fail "Failed to release all components"
fi

# Test 13: Verify all released
run_test "Verify all components released"
if $TILL hold </dev/null 2>&1 | grep -q "No components.*held\|Currently Held: 0"; then
    pass "No components shown as held"
else
    fail "Some components still shown as held"
fi

# Test 14: Hold with very short duration
run_test "Hold with short duration (5 seconds)"
if $TILL hold test.component.one --duration 5s --reason "Short hold" </dev/null 2>&1 | grep -q "Successfully held"; then
    pass "Short duration hold created"
    echo "  Waiting 6 seconds for expiration..."
    sleep 6
    if $TILL hold </dev/null 2>&1 | grep -q "test.component.one.*EXPIRED\|No components.*held"; then
        pass "  Hold expired as expected"
    else
        fail "  Hold did not expire"
    fi
else
    fail "Failed to create short duration hold"
fi

# Test 15: Release expired holds
run_test "Release expired holds"
$TILL hold test.component.one --duration 1s --reason "To expire" > /dev/null </dev/null 2>&1
sleep 2
if $TILL release --expired </dev/null 2>&1 | grep -q "released\|expired"; then
    pass "Expired holds released"
else
    fail "Failed to release expired holds"
fi

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