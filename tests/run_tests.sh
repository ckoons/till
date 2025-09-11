#!/bin/bash
#
# Till Test Runner
# Runs all functional and integration tests

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Test results
TOTAL_TESTS=0
TOTAL_PASSED=0
TOTAL_FAILED=0
FAILED_SUITES=()

# Till executable location
TILL="${TILL:-./till}"
export TILL

# Parse arguments
RUN_FUNCTIONAL=true
RUN_INTEGRATION=true
VERBOSE=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --functional-only)
            RUN_INTEGRATION=false
            shift
            ;;
        --integration-only)
            RUN_FUNCTIONAL=false
            shift
            ;;
        --verbose|-v)
            VERBOSE=true
            shift
            ;;
        --help|-h)
            echo "Till Test Runner"
            echo "Usage: $0 [options]"
            echo ""
            echo "Options:"
            echo "  --functional-only    Run only functional tests"
            echo "  --integration-only   Run only integration tests"
            echo "  --verbose, -v        Show detailed output"
            echo "  --help, -h           Show this help"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

# Header
echo "=========================================="
echo -e "${CYAN}       Till Test Suite Runner${NC}"
echo "=========================================="
echo "Till executable: $TILL"
echo "Test directory: $(dirname $0)"
date
echo

# Check if till exists
if [ ! -x "$TILL" ]; then
    echo -e "${RED}Error: Till executable not found at $TILL${NC}"
    echo "Please build Till first with: make"
    exit 1
fi

# Function to run a test suite
run_test_suite() {
    local suite_type="$1"
    local test_file="$2"
    local test_name=$(basename "$test_file" .sh)
    
    echo -e "\n${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${YELLOW}Running $suite_type test:${NC} $test_name"
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    
    # Make test executable
    chmod +x "$test_file"
    
    # Run test and capture output
    if [ "$VERBOSE" = true ]; then
        if "$test_file"; then
            echo -e "${GREEN}✓ $test_name passed${NC}"
            return 0
        else
            echo -e "${RED}✗ $test_name failed${NC}"
            FAILED_SUITES+=("$test_name")
            return 1
        fi
    else
        # Run quietly, only show summary
        OUTPUT=$("$test_file" 2>&1)
        EXIT_CODE=$?
        
        # Extract test counts from output
        TESTS_RUN=$(echo "$OUTPUT" | grep "Tests run:" | awk '{print $3}')
        TESTS_PASSED=$(echo "$OUTPUT" | grep "Tests passed:" | awk '{print $3}' | sed 's/\x1b\[[0-9;]*m//g')
        TESTS_FAILED=$(echo "$OUTPUT" | grep "Tests failed:" | awk '{print $3}' | sed 's/\x1b\[[0-9;]*m//g')
        
        if [ -z "$TESTS_RUN" ]; then TESTS_RUN=0; fi
        if [ -z "$TESTS_PASSED" ]; then TESTS_PASSED=0; fi
        if [ -z "$TESTS_FAILED" ]; then TESTS_FAILED=0; fi
        
        # Update totals
        ((TOTAL_TESTS += TESTS_RUN))
        ((TOTAL_PASSED += TESTS_PASSED))
        ((TOTAL_FAILED += TESTS_FAILED))
        
        if [ $EXIT_CODE -eq 0 ]; then
            echo -e "${GREEN}✓${NC} $test_name: $TESTS_PASSED/$TESTS_RUN tests passed"
            return 0
        else
            echo -e "${RED}✗${NC} $test_name: $TESTS_FAILED/$TESTS_RUN tests failed"
            if [ "$TESTS_FAILED" -gt 0 ]; then
                # Show failed test details
                echo "$OUTPUT" | grep "✗" | head -5
                if [ $(echo "$OUTPUT" | grep -c "✗") -gt 5 ]; then
                    echo "  ... and $(($(echo "$OUTPUT" | grep -c "✗") - 5)) more failures"
                fi
            fi
            FAILED_SUITES+=("$test_name")
            return 1
        fi
    fi
}

# Track suite results
FUNCTIONAL_PASSED=0
FUNCTIONAL_TOTAL=0
INTEGRATION_PASSED=0
INTEGRATION_TOTAL=0

# Run functional tests
if [ "$RUN_FUNCTIONAL" = true ]; then
    echo -e "\n${CYAN}══════════════════════════════════════════${NC}"
    echo -e "${CYAN}         FUNCTIONAL TESTS${NC}"
    echo -e "${CYAN}══════════════════════════════════════════${NC}"
    
    if [ -d "$(dirname $0)/functional" ]; then
        for test in $(dirname $0)/functional/test_*.sh; do
            if [ -f "$test" ]; then
                ((FUNCTIONAL_TOTAL++))
                if run_test_suite "Functional" "$test"; then
                    ((FUNCTIONAL_PASSED++))
                fi
            fi
        done
    else
        echo -e "${YELLOW}Warning: No functional tests directory found${NC}"
    fi
fi

# Run integration tests
if [ "$RUN_INTEGRATION" = true ]; then
    echo -e "\n${CYAN}══════════════════════════════════════════${NC}"
    echo -e "${CYAN}         INTEGRATION TESTS${NC}"
    echo -e "${CYAN}══════════════════════════════════════════${NC}"
    
    if [ -d "$(dirname $0)/integration" ]; then
        for test in $(dirname $0)/integration/test_*.sh; do
            if [ -f "$test" ]; then
                ((INTEGRATION_TOTAL++))
                if run_test_suite "Integration" "$test"; then
                    ((INTEGRATION_PASSED++))
                fi
            fi
        done
    else
        echo -e "${YELLOW}Warning: No integration tests directory found${NC}"
    fi
fi

# Final summary
echo
echo -e "${BLUE}══════════════════════════════════════════${NC}"
echo -e "${CYAN}           FINAL SUMMARY${NC}"
echo -e "${BLUE}══════════════════════════════════════════${NC}"

if [ "$RUN_FUNCTIONAL" = true ] && [ $FUNCTIONAL_TOTAL -gt 0 ]; then
    echo -e "Functional Test Suites:  ${GREEN}$FUNCTIONAL_PASSED${NC}/$FUNCTIONAL_TOTAL passed"
fi

if [ "$RUN_INTEGRATION" = true ] && [ $INTEGRATION_TOTAL -gt 0 ]; then
    echo -e "Integration Test Suites: ${GREEN}$INTEGRATION_PASSED${NC}/$INTEGRATION_TOTAL passed"
fi

echo
echo "Individual Test Results:"
echo "  Total tests run:    $TOTAL_TESTS"
echo -e "  Tests passed:       ${GREEN}$TOTAL_PASSED${NC}"
if [ $TOTAL_FAILED -gt 0 ]; then
    echo -e "  Tests failed:       ${RED}$TOTAL_FAILED${NC}"
else
    echo -e "  Tests failed:       $TOTAL_FAILED"
fi

# List failed suites if any
if [ ${#FAILED_SUITES[@]} -gt 0 ]; then
    echo
    echo -e "${RED}Failed test suites:${NC}"
    for suite in "${FAILED_SUITES[@]}"; do
        echo "  - $suite"
    done
fi

# Overall result
echo
if [ $TOTAL_FAILED -eq 0 ] && [ $TOTAL_TESTS -gt 0 ]; then
    echo -e "${GREEN}╔══════════════════════════════════════╗${NC}"
    echo -e "${GREEN}║       ALL TESTS PASSED! 🎉          ║${NC}"
    echo -e "${GREEN}╚══════════════════════════════════════╝${NC}"
    exit 0
elif [ $TOTAL_TESTS -eq 0 ]; then
    echo -e "${YELLOW}╔══════════════════════════════════════╗${NC}"
    echo -e "${YELLOW}║         NO TESTS WERE RUN            ║${NC}"
    echo -e "${YELLOW}╚══════════════════════════════════════╝${NC}"
    exit 1
else
    echo -e "${RED}╔══════════════════════════════════════╗${NC}"
    echo -e "${RED}║         SOME TESTS FAILED            ║${NC}"
    echo -e "${RED}╚══════════════════════════════════════╝${NC}"
    exit 1
fi