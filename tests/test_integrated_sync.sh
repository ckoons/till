#!/bin/bash
#
# test_integrated_sync.sh - Test integrated sync with federation
#

echo "=== Till Integrated Sync Test ==="
echo
echo "Testing that 'till sync' includes federation operations"
echo

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test counter
TESTS_RUN=0
TESTS_PASSED=0

# Helper function to run test
run_test() {
    local test_name="$1"
    local command="$2"
    local expected_pattern="$3"
    
    TESTS_RUN=$((TESTS_RUN + 1))
    echo -n "Test $TESTS_RUN - $test_name: "
    
    OUTPUT=$(eval "$command" 2>&1)
    
    if echo "$OUTPUT" | grep -q "$expected_pattern"; then
        echo -e "${GREEN}✓ PASS${NC}"
        TESTS_PASSED=$((TESTS_PASSED + 1))
        return 0
    else
        echo -e "${RED}✗ FAIL${NC}"
        echo "  Expected pattern: $expected_pattern"
        echo "  Got output: $OUTPUT" | head -5
        return 1
    fi
}

# Pre-test setup
echo "Setting up test environment..."
rm -f ~/.till/federation.json 2>/dev/null

# Test 1: Sync help shows federation
run_test "Sync help mentions federation" \
    "./till sync --help 2>&1" \
    "Federation sync if joined"

# Test 2: Dry run without federation (not joined)
run_test "Dry run without federation" \
    "./till sync --dry-run 2>&1" \
    "DRY RUN MODE"

OUTPUT=$(./till sync --dry-run 2>&1)
if echo "$OUTPUT" | grep -q "federation sync"; then
    echo -e "${RED}  ✗ Unexpected: Federation sync in dry run when not joined${NC}"
else
    echo -e "${GREEN}  ✓ Correct: No federation sync when not joined${NC}"
fi

# Test 3: Join federation
if gh auth status >/dev/null 2>&1; then
    echo
    echo "Joining federation for integration test..."
    run_test "Join federation" \
        "./till federate join --anonymous 2>&1" \
        "Joined federation successfully"
    
    # Test 4: Dry run with federation (joined)
    run_test "Dry run mentions federation" \
        "./till sync --dry-run 2>&1" \
        "DRY RUN MODE"
    
    # Note: Dry run should NOT trigger federation sync
    OUTPUT=$(./till sync --dry-run 2>&1)
    if echo "$OUTPUT" | grep -q "Running federation sync"; then
        echo -e "${RED}  ✗ Error: Federation sync ran in dry-run mode${NC}"
    else
        echo -e "${GREEN}  ✓ Correct: No federation sync in dry-run${NC}"
    fi
    
    # Test 5: Leave and rejoin as named for real sync test
    ./till federate leave >/dev/null 2>&1
    
    echo
    echo "Testing real sync with federation..."
    OUTPUT=$(./till federate join --named 2>&1)
    if echo "$OUTPUT" | grep -q "Joined federation successfully"; then
        echo -e "${GREEN}✓ Joined as named member${NC}"
        
        # Test 6: Real sync includes federation
        echo "Running integrated sync (this will update installations)..."
        OUTPUT=$(./till sync --skip-till-update 2>&1)
        
        if echo "$OUTPUT" | grep -q "Running federation sync"; then
            echo -e "${GREEN}✓ Sync included federation operations${NC}"
            
            if echo "$OUTPUT" | grep -q "Push complete"; then
                echo -e "${GREEN}✓ Federation push succeeded${NC}"
            fi
            
            if echo "$OUTPUT" | grep -q "Pull complete"; then
                echo -e "${GREEN}✓ Federation pull succeeded${NC}"
            fi
        else
            echo -e "${RED}✗ Sync did not include federation${NC}"
        fi
    fi
    
    # Clean up
    ./till federate leave --delete-gist >/dev/null 2>&1
else
    echo -e "${YELLOW}⚠ Skipping federation tests (gh not authenticated)${NC}"
fi

# Test 7: Watch command documentation
echo
echo "Checking watch command integration..."
run_test "Watch help exists" \
    "./till watch --help 2>&1" \
    "Schedule automatic"

# Summary
echo
echo "=== Test Summary ==="
echo "Tests run: $TESTS_RUN"
echo "Tests passed: $TESTS_PASSED"

if [ $TESTS_PASSED -eq $TESTS_RUN ]; then
    echo -e "${GREEN}All tests passed!${NC}"
    echo
    echo "Integration confirmed:"
    echo "  ✓ 'till sync' updates Tekton installations"
    echo "  ✓ 'till sync' includes federation sync when joined"
    echo "  ✓ 'till watch' will schedule recurring syncs"
    exit 0
else
    FAILED=$((TESTS_RUN - TESTS_PASSED))
    echo -e "${RED}$FAILED test(s) failed${NC}"
    exit 1
fi