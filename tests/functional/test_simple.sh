#!/bin/bash
# Simple test to debug the test runner issue

set -e

TILL="${TILL:-./till}"

echo "Starting simple test"
echo "Till path: $TILL"

# Test 1: Version
echo "Test 1: Running version command..."
if $TILL --version </dev/null >/dev/null 2>&1; then
    echo "PASS: Version command works"
    PASSED=1
else
    echo "FAIL: Version command failed"
    PASSED=0
fi

# Summary
echo ""
echo "Test Summary"
echo "Tests run:    1"
echo "Tests passed: $PASSED"
echo "Tests failed: $((1 - PASSED))"

if [ $PASSED -eq 1 ]; then
    exit 0
else
    exit 1
fi