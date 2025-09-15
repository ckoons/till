#!/bin/bash
#
# test_gh_integration.sh - Test GitHub CLI integration
#

echo "=== GitHub CLI Integration Test ==="
echo

# Test 1: Check if gh is available
echo -n "Test 1 - gh CLI available: "
if command -v gh >/dev/null 2>&1; then
    echo "✓ PASS"
else
    echo "✗ FAIL - gh not installed"
    echo "Please install: https://cli.github.com/"
    exit 1
fi

# Test 2: Check if gh is authenticated
echo -n "Test 2 - gh authenticated: "
if gh auth status >/dev/null 2>&1; then
    echo "✓ PASS"
    GH_AUTH=true
else
    echo "⚠ WARNING - gh not authenticated"
    echo "  Run: gh auth login -s gist"
    GH_AUTH=false
fi

# Test 3: Check for gist scope
echo -n "Test 3 - gh has gist scope: "
if [ "$GH_AUTH" = true ]; then
    if gh api user -i 2>/dev/null | grep -q "X-OAuth-Scopes:.*gist"; then
        echo "✓ PASS"
        GIST_SCOPE=true
    else
        echo "⚠ WARNING - missing gist scope"
        echo "  Run: gh auth refresh -s gist"
        GIST_SCOPE=false
    fi
else
    echo "⚠ SKIP - gh not authenticated"
    GIST_SCOPE=false
fi

# Test 4: Join federation with gh token
echo -n "Test 4 - Join with gh token: "
if [ "$GH_AUTH" = true ]; then
    rm -f ~/.till/federation.json 2>/dev/null
    OUTPUT=$(till federate join --named 2>&1)
    if echo "$OUTPUT" | grep -q "Using GitHub token from gh CLI"; then
        echo "✓ PASS"
    else
        echo "✗ FAIL - not using gh token"
    fi
else
    echo "⚠ SKIP - gh not authenticated"
fi

# Test 5: Federation status after join
echo -n "Test 5 - Federation status: "
if [ "$GH_AUTH" = true ]; then
    if till federate status 2>&1 | grep -q "Trust Level: named"; then
        echo "✓ PASS"
    else
        echo "✗ FAIL - incorrect trust level"
    fi
else
    echo "⚠ SKIP - gh not authenticated"
fi

# Test 6: Leave federation
echo -n "Test 6 - Leave federation: "
if [ "$GH_AUTH" = true ]; then
    if till federate leave 2>&1 | grep -q "Left federation"; then
        echo "✓ PASS"
    else
        echo "✗ FAIL - failed to leave"
    fi
else
    echo "⚠ SKIP - gh not authenticated"
fi

echo
echo "=== Streamlined Workflow Test ==="
echo

# Test the complete workflow
echo "Testing: git clone -> make install -> federate join"
echo

# Simulate fresh install
echo "1. Prerequisites check..."
command -v git >/dev/null 2>&1 && echo "   ✓ git installed" || echo "   ✗ git missing"
command -v gh >/dev/null 2>&1 && echo "   ✓ gh installed" || echo "   ✗ gh missing"

echo "2. GitHub authentication..."
if gh auth status >/dev/null 2>&1; then
    echo "   ✓ gh authenticated"
    USER=$(gh api user --jq .login 2>/dev/null)
    echo "   User: $USER"
else
    echo "   ⚠ gh not authenticated"
fi

echo "3. Federation capability..."
if [ "$GIST_SCOPE" = true ]; then
    echo "   ✓ Full federation available"
elif [ "$GH_AUTH" = true ]; then
    echo "   ⚠ Limited federation (missing gist scope)"
else
    echo "   ⚠ Anonymous federation only"
fi

echo
echo "=== Summary ==="
echo
if [ "$GH_AUTH" = true ] && [ "$GIST_SCOPE" = true ]; then
    echo "✅ Complete GitHub integration working!"
    echo "   Users can run: till federate join --named"
    echo "   No manual token needed!"
elif [ "$GH_AUTH" = true ]; then
    echo "⚠️  Partial integration - missing gist scope"
    echo "   Fix: gh auth refresh -s gist"
else
    echo "⚠️  GitHub CLI not configured"
    echo "   Fix: gh auth login -s gist"
fi

echo
echo "=== Test Complete ==="