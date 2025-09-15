#!/bin/bash
#
# test_federate_simple.sh - Simple federation tests
#

echo "=== Simple Federation Tests ==="
echo

# Test 1: Help
echo -n "Test 1 - Help command: "
if till federate help 2>&1 | grep -q "Till Federation Commands"; then
    echo "✓ PASS"
else
    echo "✗ FAIL"
fi

# Test 2: Status when not joined
echo -n "Test 2 - Status not joined: "
rm -f ~/.till/federation.json 2>/dev/null
if till federate status 2>&1 | grep -q "Not Joined"; then
    echo "✓ PASS"
else
    echo "✗ FAIL"
fi

# Test 3: Join anonymous
echo -n "Test 3 - Join anonymous: "
if till federate join --anonymous 2>&1 | grep -q "Joined federation"; then
    echo "✓ PASS"
else
    echo "✗ FAIL"
fi

# Test 4: Status after join
echo -n "Test 4 - Status after join: "
if till federate status 2>&1 | grep -q "Trust Level: anonymous"; then
    echo "✓ PASS"
else
    echo "✗ FAIL"
fi

# Test 5: Already joined error
echo -n "Test 5 - Already joined: "
if till federate join --anonymous 2>&1 | grep -q "Already joined"; then
    echo "✓ PASS"
else
    echo "✗ FAIL"
fi

# Test 6: Leave federation
echo -n "Test 6 - Leave federation: "
if till federate leave 2>&1 | grep -q "Left federation"; then
    echo "✓ PASS"
else
    echo "✗ FAIL"
fi

# Test 7: Leave when not joined
echo -n "Test 7 - Leave not joined: "
if till federate leave 2>&1 | grep -q "Not currently joined"; then
    echo "✓ PASS"
else
    echo "✗ FAIL"
fi

# Test 8: Invalid command
echo -n "Test 8 - Invalid command: "
if till federate invalid 2>&1 | grep -q "Unknown federate command"; then
    echo "✓ PASS"
else
    echo "✗ FAIL"
fi

# Test 9: Misused flag
echo -n "Test 9 - Misused flag: "
if till federate --named 2>&1 | grep -q "join option, not a command"; then
    echo "✓ PASS"
else
    echo "✗ FAIL"
fi

# Test 10: Named requires token
echo -n "Test 10 - Named needs token: "
if till federate join --named 2>&1 | grep -q "token required"; then
    echo "✓ PASS"
else
    echo "✗ FAIL"
fi

echo
echo "=== Tests Complete ==="