#!/bin/bash
#
# test_federation_gist.sh - Test GitHub Gist operations for Till Federation
#

echo "=== Till Federation Gist Operations Test ==="
echo
echo "Testing GitHub Gist integration for federation sync"
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

# Pre-test cleanup
echo "Preparing test environment..."
rm -f ~/.till/federation.json 2>/dev/null

# Test 1: Check GitHub CLI authentication
run_test "GitHub CLI available" \
    "gh auth status >/dev/null 2>&1 && echo 'authenticated'" \
    "authenticated"

if [ $? -ne 0 ]; then
    echo -e "${YELLOW}Warning: GitHub CLI not authenticated${NC}"
    echo "Some tests will be skipped"
    echo "To authenticate: gh auth login -s gist"
    GH_AUTH=false
else
    GH_AUTH=true
fi

# Test 2: Join federation as named member
if [ "$GH_AUTH" = true ]; then
    run_test "Join federation (named)" \
        "./till federate join --named 2>&1" \
        "Joined federation successfully"
fi

# Test 3: Check federation status
if [ "$GH_AUTH" = true ]; then
    run_test "Federation status after join" \
        "./till federate status 2>&1" \
        "Trust Level: named"
fi

# Test 4: Push to federation (creates gist)
if [ "$GH_AUTH" = true ]; then
    echo
    echo "Testing push operation (this will create a real gist)..."
    OUTPUT=$(./till federate push 2>&1)
    
    if echo "$OUTPUT" | grep -q "Push complete"; then
        echo -e "${GREEN}✓ Push succeeded${NC}"
        
        # Extract gist ID
        GIST_ID=$(echo "$OUTPUT" | grep "Gist:" | sed 's/.*github.com\///')
        if [ -n "$GIST_ID" ]; then
            echo "  Created gist: $GIST_ID"
            
            # Test 5: Verify gist exists
            run_test "Verify gist exists" \
                "gh api gists/$GIST_ID --jq .id 2>&1" \
                "$GIST_ID"
            
            # Test 6: Verify gist content
            run_test "Verify gist content" \
                "gh api gists/$GIST_ID --jq '.files.\"status.json\".content' 2>&1" \
                "site_id"
        fi
    else
        echo -e "${RED}✗ Push failed${NC}"
    fi
fi

# Test 7: Sync operation
if [ "$GH_AUTH" = true ]; then
    run_test "Federation sync" \
        "./till federate sync 2>&1" \
        "Sync complete"
fi

# Test 8: Pull operation
if [ "$GH_AUTH" = true ]; then
    run_test "Federation pull" \
        "./till federate pull 2>&1" \
        "Pull complete"
fi

# Test 9: Leave federation without deleting gist
if [ "$GH_AUTH" = true ]; then
    run_test "Leave federation (keep gist)" \
        "./till federate leave 2>&1" \
        "Left federation successfully"
fi

# Test 10: Rejoin and leave with delete
if [ "$GH_AUTH" = true ] && [ -n "$GIST_ID" ]; then
    echo
    echo "Testing gist deletion on leave..."
    
    # Rejoin
    ./till federate join --named >/dev/null 2>&1
    
    # Leave with delete
    OUTPUT=$(./till federate leave --delete-gist 2>&1)
    
    if echo "$OUTPUT" | grep -q "Gist deleted"; then
        echo -e "${GREEN}✓ Gist deletion succeeded${NC}"
        
        # Verify gist is gone
        if ! gh api gists/$GIST_ID >/dev/null 2>&1; then
            echo "  Verified: Gist no longer exists"
        fi
    else
        echo -e "${YELLOW}⚠ Gist deletion may have failed${NC}"
    fi
fi

# Anonymous member tests
echo
echo "Testing anonymous membership..."

# Test 11: Join as anonymous
run_test "Join as anonymous" \
    "./till federate join --anonymous 2>&1" \
    "Joined federation successfully"

# Test 12: Status shows anonymous
run_test "Status shows anonymous" \
    "./till federate status 2>&1" \
    "Trust Level: anonymous"

# Test 13: Push not available for anonymous
run_test "Push blocked for anonymous" \
    "./till federate push 2>&1" \
    "not available for anonymous"

# Test 14: Sync works for anonymous (pull only)
run_test "Sync for anonymous" \
    "./till federate sync 2>&1" \
    "Push skipped.*anonymous"

# Clean up
./till federate leave >/dev/null 2>&1

# Summary
echo
echo "=== Test Summary ==="
echo "Tests run: $TESTS_RUN"
echo "Tests passed: $TESTS_PASSED"

if [ $TESTS_PASSED -eq $TESTS_RUN ]; then
    echo -e "${GREEN}All tests passed!${NC}"
    exit 0
else
    FAILED=$((TESTS_RUN - TESTS_PASSED))
    echo -e "${RED}$FAILED test(s) failed${NC}"
    exit 1
fi