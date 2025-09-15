#!/bin/bash
#
# test_federate.sh - Test cases for till federate commands
#
# Usage: ./test_federate.sh [test_name]
#

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test counters
TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0

# Test data
TEST_TOKEN="ghp_test1234567890abcdef"
FEDERATION_CONFIG="$HOME/.till/federation.json"
FEDERATION_BACKUP="$HOME/.till/federation.json.backup"

# Helper functions
print_test() {
    echo -e "${YELLOW}TEST:${NC} $1"
    ((TESTS_RUN++))
}

print_pass() {
    echo -e "${GREEN}✓ PASS${NC}: $1"
    ((TESTS_PASSED++))
}

print_fail() {
    echo -e "${RED}✗ FAIL${NC}: $1"
    ((TESTS_FAILED++))
}

backup_federation() {
    if [ -f "$FEDERATION_CONFIG" ]; then
        cp "$FEDERATION_CONFIG" "$FEDERATION_BACKUP"
        echo "Backed up existing federation config" >&2
    fi
}

restore_federation() {
    if [ -f "$FEDERATION_BACKUP" ]; then
        mv "$FEDERATION_BACKUP" "$FEDERATION_CONFIG"
        echo "Restored federation config" >&2
    else
        rm -f "$FEDERATION_CONFIG" 2>/dev/null
    fi
}

cleanup() {
    restore_federation
}

# Set up cleanup on exit
trap cleanup EXIT

# Test Cases

test_help() {
    print_test "Federation help command"
    
    # Test help flag (capture all output including discovery)
    OUTPUT=$(till federate --help 2>&1)
    if echo "$OUTPUT" | grep -q "Till Federation Commands"; then
        print_pass "till federate --help shows help"
    else
        print_fail "till federate --help doesn't show help"
    fi
    
    # Test help command
    OUTPUT=$(till federate help 2>&1)
    if echo "$OUTPUT" | grep -q "Till Federation Commands"; then
        print_pass "till federate help shows help"
    else
        print_fail "till federate help doesn't show help"
    fi
    
    # Test no arguments
    OUTPUT=$(till federate 2>&1)
    if echo "$OUTPUT" | grep -q "Till Federation Commands"; then
        print_pass "till federate (no args) shows help"
    else
        print_fail "till federate (no args) doesn't show help"
    fi
}

test_invalid_commands() {
    print_test "Invalid federation commands"
    
    # Test invalid command
    OUTPUT=$(till federate invalid 2>&1)
    if echo "$OUTPUT" | grep -q "Unknown federate command"; then
        print_pass "Invalid command shows error"
    else
        print_fail "Invalid command doesn't show error"
    fi
    
    # Test misused flag as command
    OUTPUT=$(till federate --anonymous 2>&1)
    if echo "$OUTPUT" | grep -q "join option, not a command"; then
        print_pass "Misused --anonymous flag shows helpful error"
    else
        print_fail "Misused --anonymous flag doesn't show helpful error"
    fi
    
    OUTPUT=$(till federate --named 2>&1)
    if echo "$OUTPUT" | grep -q "Did you mean: till federate join --named"; then
        print_pass "Error suggests correct command"
    else
        print_fail "Error doesn't suggest correct command"
    fi
}

test_status_not_joined() {
    print_test "Federation status when not joined"
    
    # Ensure not joined
    rm -f "$FEDERATION_CONFIG" 2>/dev/null
    
    OUTPUT=$(till federate status 2>&1)
    
    if echo "$OUTPUT" | grep -q "Federation Status: Not Joined"; then
        print_pass "Shows 'Not Joined' status"
    else
        print_fail "Doesn't show 'Not Joined' status"
    fi
    
    if echo "$OUTPUT" | grep -q "till federate join --anonymous"; then
        print_pass "Shows join instructions"
    else
        print_fail "Doesn't show join instructions"
    fi
}

test_join_anonymous() {
    print_test "Join federation as anonymous"
    
    # Ensure not joined
    rm -f "$FEDERATION_CONFIG" 2>/dev/null
    
    OUTPUT=$(till federate join --anonymous 2>&1)
    
    if echo "$OUTPUT" | grep -q "Joined federation as anonymous"; then
        print_pass "Successfully joined as anonymous"
    else
        print_fail "Failed to join as anonymous"
    fi
    
    # Check status after joining
    OUTPUT=$(till federate status 2>&1)
    
    if echo "$OUTPUT" | grep -q "Trust Level: anonymous"; then
        print_pass "Status shows anonymous trust level"
    else
        print_fail "Status doesn't show anonymous trust level"
    fi
    
    if echo "$OUTPUT" | grep -q "Site ID:"; then
        print_pass "Status shows Site ID"
    else
        print_fail "Status doesn't show Site ID"
    fi
}

test_leave_federation() {
    print_test "Leave federation"
    
    # First join (suppress output)
    till federate join --anonymous >/dev/null 2>&1
    
    OUTPUT=$(till federate leave 2>&1)
    
    if echo "$OUTPUT" | grep -q "Left federation successfully"; then
        print_pass "Successfully left federation"
    else
        print_fail "Failed to leave federation"
    fi
    
    # Check status after leaving
    OUTPUT=$(till federate status 2>&1)
    
    if echo "$OUTPUT" | grep -q "Federation Status: Not Joined"; then
        print_pass "Status shows not joined after leaving"
    else
        print_fail "Status doesn't show not joined after leaving"
    fi
}

test_join_named_without_token() {
    print_test "Join as named without token"
    
    # Ensure not joined
    rm -f "$FEDERATION_CONFIG" 2>/dev/null
    
    OUTPUT=$(till federate join --named 2>&1)
    
    if echo "$OUTPUT" | grep -q "GitHub personal access token required"; then
        print_pass "Shows token required error"
    else
        print_fail "Doesn't show token required error"
    fi
}

test_join_named_with_token() {
    print_test "Join as named with token"
    
    # Ensure not joined
    rm -f "$FEDERATION_CONFIG" 2>/dev/null
    
    OUTPUT=$(till federate join --named --token "$TEST_TOKEN" 2>&1)
    
    # This will fail with actual GitHub API call, but should show attempt
    if echo "$OUTPUT" | grep -q "Joining federation as named"; then
        print_pass "Attempts to join as named with token"
    else
        print_fail "Doesn't attempt to join as named"
    fi
}

test_join_trusted() {
    print_test "Join as trusted"
    
    # Ensure not joined
    rm -f "$FEDERATION_CONFIG" 2>/dev/null
    
    OUTPUT=$(till federate join --trusted --token "$TEST_TOKEN" 2>&1)
    
    if echo "$OUTPUT" | grep -q "Joining federation as trusted"; then
        print_pass "Attempts to join as trusted"
    else
        print_fail "Doesn't attempt to join as trusted"
    fi
}

test_already_joined() {
    print_test "Join when already joined"
    
    # First join (suppress output)
    till federate join --anonymous >/dev/null 2>&1
    
    OUTPUT=$(till federate join --anonymous 2>&1)
    
    if echo "$OUTPUT" | grep -q "Already joined"; then
        print_pass "Shows already joined error"
    else
        print_fail "Doesn't show already joined error"
    fi
}

test_leave_when_not_joined() {
    print_test "Leave when not joined"
    
    # Ensure not joined
    rm -f "$FEDERATION_CONFIG" 2>/dev/null
    
    OUTPUT=$(till federate leave 2>&1)
    
    if echo "$OUTPUT" | grep -q "Not currently joined"; then
        print_pass "Shows not joined error"
    else
        print_fail "Doesn't show not joined error"
    fi
}

test_pull_command() {
    print_test "Federation pull command"
    
    OUTPUT=$(till federate pull 2>&1)
    
    if echo "$OUTPUT" | grep -q "not yet implemented"; then
        print_pass "Pull shows not implemented"
    else
        print_fail "Pull doesn't show not implemented"
    fi
}

test_push_command() {
    print_test "Federation push command"
    
    OUTPUT=$(till federate push 2>&1)
    
    if echo "$OUTPUT" | grep -q "not yet implemented"; then
        print_pass "Push shows not implemented"
    else
        print_fail "Push doesn't show not implemented"
    fi
}

test_sync_command() {
    print_test "Federation sync command"
    
    OUTPUT=$(till federate sync 2>&1)
    
    if echo "$OUTPUT" | grep -q "not yet implemented"; then
        print_pass "Sync shows not implemented"
    else
        print_fail "Sync doesn't show not implemented"
    fi
}

test_persistence() {
    print_test "Federation config persistence"
    
    # Join federation
    rm -f "$FEDERATION_CONFIG" 2>/dev/null
    till federate join --anonymous >/dev/null 2>&1
    
    # Get site ID
    SITE_ID1=$(till federate status 2>&1 | grep "Site ID:" | awk '{print $3}')
    
    # Run status again (simulating new session)
    SITE_ID2=$(till federate status 2>&1 | grep "Site ID:" | awk '{print $3}')
    
    if [ "$SITE_ID1" = "$SITE_ID2" ]; then
        print_pass "Site ID persists across commands"
    else
        print_fail "Site ID doesn't persist (ID1: $SITE_ID1, ID2: $SITE_ID2)"
    fi
}

test_global_config() {
    print_test "Federation config is global"
    
    # Join from one directory
    rm -f "$FEDERATION_CONFIG" 2>/dev/null
    cd /tmp
    till federate join --anonymous >/dev/null 2>&1
    
    # Check status from another directory
    cd "$HOME"
    OUTPUT=$(till federate status 2>&1)
    
    if echo "$OUTPUT" | grep -q "Federation Status: Joined"; then
        print_pass "Federation status consistent across directories"
    else
        print_fail "Federation status not consistent across directories"
    fi
}

# Main test runner
main() {
    echo "================================"
    echo "Till Federation Test Suite"
    echo "================================"
    echo
    
    # Backup existing federation config
    backup_federation
    
    # Run specific test or all tests
    if [ $# -eq 0 ]; then
        # Run all tests
        test_help
        test_invalid_commands
        test_status_not_joined
        test_join_anonymous
        test_leave_federation
        test_join_named_without_token
        test_join_named_with_token
        test_join_trusted
        test_already_joined
        test_leave_when_not_joined
        test_pull_command
        test_push_command
        test_sync_command
        test_persistence
        test_global_config
    else
        # Run specific test
        test_name="test_$1"
        if declare -f "$test_name" > /dev/null; then
            $test_name
        else
            echo "Unknown test: $1"
            echo "Available tests:"
            declare -F | grep "test_" | sed 's/declare -f test_/  - /'
            exit 1
        fi
    fi
    
    # Print summary
    echo
    echo "================================"
    echo "Test Summary"
    echo "================================"
    echo "Tests run:    $TESTS_RUN"
    echo -e "Tests passed: ${GREEN}$TESTS_PASSED${NC}"
    echo -e "Tests failed: ${RED}$TESTS_FAILED${NC}"
    
    if [ $TESTS_FAILED -eq 0 ]; then
        echo -e "${GREEN}All tests passed!${NC}"
        exit 0
    else
        echo -e "${RED}Some tests failed.${NC}"
        exit 1
    fi
}

# Run main if not sourced
if [ "${BASH_SOURCE[0]}" = "${0}" ]; then
    main "$@"
fi