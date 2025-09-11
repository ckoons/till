#!/bin/bash
#
# Integration tests for Till
# Tests full workflows with actual Tekton mock installations

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test counters
TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0

# Till executable
TILL="${TILL:-./till}"

# Test directory
TEST_DIR="/tmp/till-integration-test-$$"
MOCK_TEKTON_DIR="$TEST_DIR/mock-tekton"

# Test functions
pass() {
    echo -e "${GREEN}✓${NC} $1"
    ((TESTS_PASSED++))
}

fail() {
    echo -e "${RED}✗${NC} $1"
    if [ ! -z "$2" ]; then
        echo "  Error: $2"
    fi
    ((TESTS_FAILED++))
}

run_test() {
    local test_name="$1"
    echo -e "\n${YELLOW}Running:${NC} $test_name"
    ((TESTS_RUN++))
}

info() {
    echo -e "${BLUE}ℹ${NC} $1"
}

# Create mock Tekton installation
create_mock_tekton() {
    local name="$1"
    local dir="$2"
    
    mkdir -p "$dir/src"
    
    # Create .env.local with installation name
    cat > "$dir/.env.local" <<EOF
TEKTON_NAME=$name
TEKTON_HOST=localhost
TEKTON_PORT=8000
TEKTON_ENV=development
EOF
    
    # Create mock tekton.py
    cat > "$dir/src/tekton.py" <<EOF
#!/usr/bin/env python3
# Mock Tekton installation: $name
print("Tekton $name")
EOF
    
    # Create mock git repo
    cd "$dir"
    git init -q
    git config user.email "test@test.com"
    git config user.name "Test User"
    git add .
    git commit -q -m "Initial mock commit"
    cd - > /dev/null
}

# Clean up function
cleanup() {
    info "Cleaning up test environment..."
    rm -rf "$TEST_DIR"
    rm -rf ~/.till/tekton/test-*
    # Remove any test holds
    if [ -f ~/.till/tekton/till-private.json ]; then
        cp ~/.till/tekton/till-private.json ~/.till/tekton/till-private.json.backup
        cat ~/.till/tekton/till-private.json.backup | \
            python3 -c "import sys, json; d=json.load(sys.stdin); d['holds']={k:v for k,v in d.get('holds',{}).items() if not k.startswith('test.')}; print(json.dumps(d))" \
            > ~/.till/tekton/till-private.json
    fi
}

# Set up
echo "==================================="
echo "Till Integration Tests"
echo "==================================="
echo "Using Till: $TILL"
echo "Test Directory: $TEST_DIR"

# Ensure till exists
if [ ! -x "$TILL" ]; then
    echo -e "${RED}Error: Till executable not found at $TILL${NC}"
    exit 1
fi

# Create test environment
info "Setting up test environment..."
mkdir -p "$MOCK_TEKTON_DIR"

# Create mock Tekton installations
create_mock_tekton "test.primary.tekton" "$MOCK_TEKTON_DIR/Primary"
create_mock_tekton "test.coder-a.tekton" "$MOCK_TEKTON_DIR/Coder-A"
create_mock_tekton "test.coder-b.tekton" "$MOCK_TEKTON_DIR/Coder-B"

# Save current directory
ORIGINAL_DIR=$(pwd)

# Test 1: Discovery
run_test "Discover Tekton installations"
cd "$MOCK_TEKTON_DIR"
if $TILL sync 2>&1 | grep -q "Found.*Tekton installation"; then
    pass "Discovered mock installations"
else
    fail "Failed to discover installations"
fi
cd "$ORIGINAL_DIR"

# Test 2: Verify discovered installations
run_test "Verify discovered installations"
DISCOVERY_OUTPUT=$($TILL sync 2>&1)
if echo "$DISCOVERY_OUTPUT" | grep -q "test.primary.tekton" && \
   echo "$DISCOVERY_OUTPUT" | grep -q "test.coder-a.tekton" && \
   echo "$DISCOVERY_OUTPUT" | grep -q "test.coder-b.tekton"; then
    pass "All mock installations discovered"
else
    fail "Not all installations discovered"
fi

# Test 3: Hold and sync workflow
run_test "Hold component and verify sync skips it"
$TILL hold test.primary.tekton --reason "Integration test hold" > /dev/null 2>&1
SYNC_OUTPUT=$($TILL sync 2>&1)
if echo "$SYNC_OUTPUT" | grep -q "test.primary.tekton.*HELD\|🔒"; then
    pass "Sync shows held component"
else
    fail "Sync doesn't show held component"
fi

# Test 4: Multi-component hold workflow
run_test "Hold multiple components and sync"
$TILL release --all > /dev/null 2>&1
$TILL hold test.coder-a.tekton,test.coder-b.tekton --duration 1h --reason "Multi-hold test" > /dev/null 2>&1
SYNC_OUTPUT=$($TILL sync 2>&1)
HELD_COUNT=$(echo "$SYNC_OUTPUT" | grep -c "HELD\|🔒" || true)
if [ "$HELD_COUNT" -eq 2 ]; then
    pass "Multiple components held in sync"
else
    fail "Expected 2 held components, found $HELD_COUNT"
fi

# Test 5: Schedule creation (mock)
run_test "Create schedule for component"
if $TILL schedule add test.primary.tekton --at "02:00" 2>&1 | grep -q "schedule\|Schedule\|Added"; then
    pass "Schedule created"
else
    fail "Failed to create schedule"
fi

# Test 6: List schedules
run_test "List schedules"
if $TILL schedule list 2>&1 | grep -q "test.primary.tekton\|02:00"; then
    pass "Schedule listed correctly"
else
    fail "Schedule not listed"
fi

# Test 7: Remove schedule
run_test "Remove schedule"
if $TILL schedule remove test.primary.tekton 2>&1 | grep -q "Removed\|removed\|deleted"; then
    pass "Schedule removed"
else
    fail "Failed to remove schedule"
fi

# Test 8: Git operations in mock repos
run_test "Git operations in discovered repos"
cd "$MOCK_TEKTON_DIR/Primary"
echo "test file" > test.txt
git add test.txt
git commit -q -m "Test commit"

# Run sync to update
cd "$ORIGINAL_DIR"
SYNC_OUTPUT=$($TILL sync 2>&1)
if echo "$SYNC_OUTPUT" | grep -q "test.primary.tekton"; then
    pass "Sync after git changes works"
else
    fail "Sync after git changes failed"
fi

# Test 9: Hold with expiration and cleanup
run_test "Hold with expiration"
$TILL release --all > /dev/null 2>&1
$TILL hold test.primary.tekton --duration 2s --reason "Expire test" > /dev/null 2>&1
sleep 3
if $TILL release --expired 2>&1 | grep -q "released\|expired\|Released"; then
    pass "Expired hold cleaned up"
else
    fail "Expired hold not cleaned up"
fi

# Test 10: Complex workflow - hold, sync, release, sync
run_test "Complex workflow (hold->sync->release->sync)"
# Hold all
$TILL hold --all --reason "Complex test" > /dev/null 2>&1
# Verify sync shows all held
SYNC1=$($TILL sync 2>&1)
HELD1=$(echo "$SYNC1" | grep -c "HELD\|🔒" || true)
# Release all
$TILL release --all > /dev/null 2>&1
# Verify sync shows none held
SYNC2=$($TILL sync 2>&1)
HELD2=$(echo "$SYNC2" | grep -c "HELD\|🔒" || true)

if [ "$HELD1" -ge 3 ] && [ "$HELD2" -eq 0 ]; then
    pass "Complex workflow completed successfully"
else
    fail "Complex workflow failed (held before: $HELD1, after: $HELD2)"
fi

# Test 11: Registry persistence
run_test "Registry persistence"
# Get current registry
REGISTRY_BEFORE=$($TILL sync 2>&1 | grep "Found.*installation")
# Simulate new till session by clearing any in-memory state
unset TILL_CACHE
# Check registry again
REGISTRY_AFTER=$($TILL sync 2>&1 | grep "Found.*installation")
if [ "$REGISTRY_BEFORE" = "$REGISTRY_AFTER" ]; then
    pass "Registry persisted correctly"
else
    fail "Registry not persisted"
fi

# Test 12: Concurrent operations (if possible)
run_test "Concurrent operation handling"
# Start background sync
$TILL sync > /dev/null 2>&1 &
PID1=$!
# Try another operation immediately
$TILL hold test.primary.tekton --reason "Concurrent test" > /dev/null 2>&1 &
PID2=$!

# Wait for both
wait $PID1 2>/dev/null
RESULT1=$?
wait $PID2 2>/dev/null
RESULT2=$?

if [ $RESULT1 -eq 0 ] || [ $RESULT2 -eq 0 ]; then
    pass "Concurrent operations handled"
else
    fail "Both concurrent operations failed"
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
    echo -e "\n${GREEN}All integration tests passed!${NC}"
    exit 0
else
    echo -e "\n${RED}Some integration tests failed${NC}"
    exit 1
fi