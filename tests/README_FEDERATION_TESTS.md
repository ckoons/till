# Till Federation Test Guide

## Overview
This guide explains how to test the Till federation functionality, including both automated tests and manual testing procedures.

## Automated Tests

### Running All Tests
```bash
cd tests
chmod +x test_federate.sh
./test_federate.sh
```

### Running Specific Tests
```bash
# Run a specific test by name (without test_ prefix)
./test_federate.sh help
./test_federate.sh join_anonymous
./test_federate.sh persistence
```

### Available Test Cases
- `help` - Tests help command variations
- `invalid_commands` - Tests error handling for invalid commands
- `status_not_joined` - Tests status when not in federation
- `join_anonymous` - Tests joining as anonymous
- `leave_federation` - Tests leaving the federation
- `join_named_without_token` - Tests named join without token (should fail)
- `join_named_with_token` - Tests named join with token
- `join_trusted` - Tests trusted join
- `already_joined` - Tests joining when already a member
- `leave_when_not_joined` - Tests leaving when not a member
- `pull_command` - Tests pull command (currently unimplemented)
- `push_command` - Tests push command (currently unimplemented)
- `sync_command` - Tests sync command (currently unimplemented)
- `persistence` - Tests config persistence
- `global_config` - Tests that config works globally

## Manual Testing Procedures

### 1. Basic Federation Flow
```bash
# Check initial status
till federate status

# Join as anonymous
till federate join --anonymous

# Verify joined
till federate status

# Leave federation
till federate leave

# Verify left
till federate status
```

### 2. Trust Level Progression
```bash
# Start anonymous
till federate join --anonymous
till federate status  # Should show "anonymous"

# Leave and rejoin as named (requires GitHub token)
till federate leave
till federate join --named --token ghp_YOUR_TOKEN_HERE
till federate status  # Should show "named"

# Leave and rejoin as trusted
till federate leave
till federate join --trusted --token ghp_YOUR_TOKEN_HERE
till federate status  # Should show "trusted"
```

### 3. Error Handling Tests
```bash
# Test invalid commands
till federate invalid
till federate --anonymous  # Should suggest: till federate join --anonymous
till federate --named      # Should suggest: till federate join --named

# Test joining without required token
till federate join --named  # Should error: token required

# Test double join
till federate join --anonymous
till federate join --anonymous  # Should error: already joined

# Test leave when not joined
till federate leave
till federate leave  # Should error: not joined
```

### 4. Global Configuration Test
```bash
# Join from till directory
cd ~/projects/github/till
till federate join --anonymous

# Check from different directory
cd /tmp
till federate status  # Should show joined

# Check from home
cd ~
till federate status  # Should show joined

# Leave from anywhere
till federate leave
```

### 5. Federation Config Persistence
```bash
# Join and note the Site ID
till federate join --anonymous
till federate status  # Note the Site ID

# Simulate new shell session
exec bash
till federate status  # Site ID should be the same

# Check config file directly
cat ~/.till/federation.json | jq .
```

### 6. Help System Test
```bash
# Various ways to get help
till federate --help
till federate help
till federate  # No arguments

# All should show:
# - Command list
# - Join options
# - Examples
```

## Expected Behaviors

### Successful Operations
1. **Anonymous Join**: Creates Site ID, no token needed
2. **Named Join**: Requires token, creates gist (when implemented)
3. **Trusted Join**: Requires token, enables telemetry (when implemented)
4. **Leave**: Removes local config, optionally deletes gist
5. **Status**: Shows current membership and details

### Error Conditions
1. **No Token**: Named/trusted join fails without token
2. **Already Joined**: Can't join when already a member
3. **Not Joined**: Can't leave or check status details when not a member
4. **Invalid Command**: Shows helpful error with suggestions

## Test Coverage Areas

### Unit Tests (test_federate.sh)
- Command parsing ✓
- Error messages ✓
- State transitions ✓
- Config persistence ✓
- Global configuration ✓

### Integration Tests (Manual)
- GitHub API integration (when implemented)
- Gist creation/deletion (when implemented)
- Menu-of-the-day processing (when implemented)
- Telemetry collection (when implemented)

### Not Yet Implemented
- `till federate pull` - Pull menu-of-the-day
- `till federate push` - Push status to gist
- `till federate sync` - Full synchronization
- GitHub API integration
- Gist management
- Directive processing

## Debugging Tips

### Check Federation Config
```bash
# View raw config
cat ~/.till/federation.json

# Pretty print with jq
cat ~/.till/federation.json | jq .

# Check if joined
[ -f ~/.till/federation.json ] && echo "Joined" || echo "Not joined"
```

### Enable Debug Output
```bash
# Set debug environment variable
export TILL_DEBUG=1
till federate status

# Check logs
tail -f ~/.till/logs/till_*.log
```

### Reset Federation State
```bash
# Complete reset
rm -f ~/.till/federation.json
till federate status  # Should show "Not Joined"
```

## Common Issues and Solutions

### Issue: "Already joined to federation"
**Solution**: Run `till federate leave` first, then join again

### Issue: Token not accepted
**Solution**: Ensure token has gist scope permissions

### Issue: Discovery runs on every command
**Solution**: This is expected behavior to ensure registry is current

### Issue: Federation status inconsistent
**Solution**: Check `~/.till/federation.json` exists and is readable

## Test Output Example
```
================================
Till Federation Test Suite
================================

TEST: Federation help command
✓ PASS: till federate --help shows help
✓ PASS: till federate help shows help
✓ PASS: till federate (no args) shows help

TEST: Invalid federation commands
✓ PASS: Invalid command shows error
✓ PASS: Misused --anonymous flag shows helpful error
✓ PASS: Error suggests correct command

[... more tests ...]

================================
Test Summary
================================
Tests run:    15
Tests passed: 14
Tests failed: 1
Some tests failed.
```