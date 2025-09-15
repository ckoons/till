# Till Federation Workflows

## Table of Contents
1. [Overview](#overview)
2. [Trust Levels](#trust-levels)
3. [Joining Workflows](#joining-workflows)
4. [Repository and Till Interaction](#repository-and-till-interaction)
5. [Testing Federation Features](#testing-federation-features)
6. [Federation Operations](#federation-operations)
7. [Troubleshooting](#troubleshooting)

## Overview

The Till Federation is a decentralized network of Till installations that share updates, configurations, and operational directives through GitHub infrastructure. Each Till installation can join the federation at different trust levels, determining their participation and data sharing capabilities.

### Architecture Components

```
┌─────────────────────────────────────────────────────────┐
│                   GitHub Infrastructure                  │
├─────────────────────────────────────────────────────────┤
│                                                         │
│  ┌──────────────┐     ┌──────────────┐                │
│  │  Till Repo   │     │ GitHub Gists │                │
│  │              │     │              │                │
│  │ - Directives │     │ - Status     │                │
│  │ - Updates    │     │ - Telemetry  │                │
│  └──────┬───────┘     └──────┬───────┘                │
│         │                     │                        │
└─────────┼─────────────────────┼────────────────────────┘
          │                     │
          ▼                     ▼
    ┌─────────────────────────────────┐
    │        Local Till Instance       │
    │                                  │
    │  ~/.till/federation.json         │
    │  - Site ID                       │
    │  - Trust Level                   │
    │  - GitHub Token (encrypted)       │
    └──────────────────────────────────┘
```

## Trust Levels

### Anonymous (Read-Only)
- **Purpose**: Basic federation participation without identity
- **Requirements**: None
- **Capabilities**:
  - Pull menu-of-the-day directives
  - Receive updates
  - No data sharing
- **Use Case**: Users who want updates but maintain complete privacy

### Named (Standard Member)
- **Purpose**: Standard federation membership with identity
- **Requirements**: GitHub personal access token with gist scope
- **Capabilities**:
  - All anonymous capabilities
  - Create and maintain status gist
  - Share non-sensitive status information
  - Participate in federation statistics
- **Use Case**: Regular users contributing to federation health metrics

### Trusted (Full Participation)
- **Purpose**: Complete federation integration with telemetry
- **Requirements**: GitHub token + approval from federation admins
- **Capabilities**:
  - All named capabilities
  - Share detailed telemetry data
  - Priority directive processing
  - Beta feature access
- **Use Case**: Power users and contributors helping improve Till

## Joining Workflows

### Workflow 1: Anonymous Join

```bash
# Step 1: Check current status
$ till federate status
Federation Status: Not Joined

To join the federation, use:
  till federate join --anonymous    # Read-only access
  till federate join --named        # Standard membership
  till federate join --trusted      # Full participation

# Step 2: Join as anonymous
$ till federate join --anonymous
Joining federation as anonymous (read-only)...
✓ Joined federation as anonymous
  Site ID: M4-1757943273-6807
  You can now use 'till federate pull' to get updates

# Step 3: Verify membership
$ till federate status
Federation Status: Joined
==================
Site ID:     M4-1757943273-6807
Trust Level: anonymous
Last Sync:   Never
Auto Sync:   Enabled

# Step 4: Configuration created
$ cat ~/.till/federation.json
{
  "site_id": "M4-1757943273-6807",
  "gist_id": "",
  "github_token_encrypted": "",
  "trust_level": "anonymous",
  "last_sync": 0,
  "auto_sync": true,
  "last_menu_date": ""
}
```

### Workflow 2: Named Join

```bash
# Step 1: Obtain GitHub Personal Access Token
# Go to: https://github.com/settings/tokens
# Create token with 'gist' scope
# Copy token: ghp_xxxxxxxxxxxxxxxxxxxx

# Step 2: Join as named member
$ till federate join --named --token ghp_xxxxxxxxxxxxxxxxxxxx
Joining federation as named (standard member)...
Creating GitHub Gist...
✓ Joined federation successfully
  Site ID: M4-1757943273-6807
  Trust Level: named
  Gist ID: a1b2c3d4e5f6
  Run 'till federate sync' to start syncing

# Step 3: Verify gist creation
$ till federate status
Federation Status: Joined
==================
Site ID:     M4-1757943273-6807
Trust Level: named
Gist ID:     a1b2c3d4e5f6
Gist URL:    https://gist.github.com/username/a1b2c3d4e5f6
Last Sync:   Never
Auto Sync:   Enabled

# Step 4: Initial sync
$ till federate sync
Syncing with federation...
  Pulling menu-of-the-day...
  Processing directives...
  Updating gist status...
✓ Sync complete
```

### Workflow 3: Trusted Join

```bash
# Step 1: Start with named membership
$ till federate join --named --token ghp_xxxxxxxxxxxxxxxxxxxx

# Step 2: Request trusted status
$ till federate join --trusted --token ghp_xxxxxxxxxxxxxxxxxxxx
Joining federation as trusted (full participation)...
Creating GitHub Gist with telemetry manifest...
✓ Joined federation successfully
  Site ID: M4-1757943273-6807
  Trust Level: trusted
  Gist ID: a1b2c3d4e5f6
  Telemetry: Enabled
  Run 'till federate sync' to start syncing

# Step 3: Verify trusted features
$ till federate status
Federation Status: Joined
==================
Site ID:     M4-1757943273-6807
Trust Level: trusted
Gist ID:     a1b2c3d4e5f6
Telemetry:   Enabled
Beta Access: Enabled
Last Sync:   2025-01-15 09:30:00
Auto Sync:   Enabled
```

### Workflow 4: Trust Level Progression

```bash
# Start anonymous
$ till federate join --anonymous
✓ Joined as anonymous

# Upgrade to named (must leave first)
$ till federate leave
✓ Left federation successfully

$ till federate join --named --token ghp_xxxxxxxxxxxxxxxxxxxx
✓ Joined as named member

# Upgrade to trusted
$ till federate leave --keep-gist
✓ Left federation (gist preserved)

$ till federate join --trusted --token ghp_xxxxxxxxxxxxxxxxxxxx
✓ Joined as trusted member
```

## Repository and Till Interaction

### Menu-of-the-Day Processing

The central Till repository publishes daily directives that federation members can pull and process:

```yaml
# Repository: till/federation/menu-of-the-day.yaml
date: 2025-01-15
directives:
  - id: update-till-001
    type: update
    condition: "version < 1.5.0"
    action: "till update"
    priority: high
    
  - id: security-patch-001
    type: security
    condition: "all"
    action: "apply-patch security-2025-01.patch"
    priority: critical
    
  - id: feature-rollout-001
    type: feature
    condition: "trust_level == trusted"
    action: "enable-feature beta-sync"
    priority: normal
```

### Pull Workflow

```bash
# Manual pull
$ till federate pull
Fetching menu-of-the-day from repository...
Found 3 directives for 2025-01-15

Processing directives:
  [1/3] update-till-001
    Condition: version < 1.5.0
    Current version: 1.4.0
    ✓ Condition met - executing: till update
    
  [2/3] security-patch-001
    Condition: all
    ✓ Condition met - executing: apply-patch
    
  [3/3] feature-rollout-001
    Condition: trust_level == trusted
    Current trust: anonymous
    ✗ Condition not met - skipping

Summary: 2 executed, 1 skipped
```

### Push Workflow (Named/Trusted Only)

```bash
# Manual push
$ till federate push
Updating federation status gist...

Collecting status:
  - Till version: 1.5.0
  - Installations: 5
  - Last sync: 2025-01-15 09:45:00
  - Trust level: named

Updating gist: https://gist.github.com/username/a1b2c3d4e5f6
✓ Status pushed successfully
```

### Sync Workflow (Pull + Process + Push)

```bash
# Full synchronization
$ till federate sync
Starting federation sync...

Step 1: Pull menu-of-the-day
  ✓ Fetched 3 directives

Step 2: Process directives
  ✓ Executed 2 directives
  ✗ Skipped 1 directive (condition not met)

Step 3: Update local state
  ✓ Updated last_menu_date
  ✓ Logged processed directives

Step 4: Push status (named/trusted only)
  ✓ Updated gist with current status

Sync complete at 2025-01-15 09:50:00
Next auto-sync in 24 hours
```

## Testing Federation Features

### Test 1: Basic Federation Flow

```bash
#!/bin/bash
# test_federation_basic.sh

echo "=== Testing Basic Federation Flow ==="

# Clean start
till federate leave 2>/dev/null

# Test join
echo "1. Testing anonymous join..."
till federate join --anonymous
[ $? -eq 0 ] && echo "✓ Join successful" || echo "✗ Join failed"

# Test status
echo "2. Testing status..."
till federate status | grep -q "anonymous"
[ $? -eq 0 ] && echo "✓ Status shows anonymous" || echo "✗ Status incorrect"

# Test pull
echo "3. Testing pull..."
till federate pull
[ $? -eq 0 ] && echo "✓ Pull successful" || echo "✗ Pull failed"

# Test leave
echo "4. Testing leave..."
till federate leave
[ $? -eq 0 ] && echo "✓ Leave successful" || echo "✗ Leave failed"

echo "=== Basic Federation Test Complete ==="
```

### Test 2: Trust Level Progression

```bash
#!/bin/bash
# test_trust_progression.sh

# Test anonymous -> named -> trusted progression
echo "=== Testing Trust Level Progression ==="

# Start fresh
till federate leave 2>/dev/null

# Anonymous
till federate join --anonymous
TRUST=$(till federate status | grep "Trust Level:" | awk '{print $3}')
[ "$TRUST" = "anonymous" ] && echo "✓ Anonymous verified" || echo "✗ Failed"

# Named (requires token)
till federate leave
till federate join --named --token "$GITHUB_TOKEN"
TRUST=$(till federate status | grep "Trust Level:" | awk '{print $3}')
[ "$TRUST" = "named" ] && echo "✓ Named verified" || echo "✗ Failed"

# Trusted
till federate leave
till federate join --trusted --token "$GITHUB_TOKEN"
TRUST=$(till federate status | grep "Trust Level:" | awk '{print $3}')
[ "$TRUST" = "trusted" ] && echo "✓ Trusted verified" || echo "✗ Failed"
```

### Test 3: Gist Management

```bash
#!/bin/bash
# test_gist_management.sh

echo "=== Testing Gist Management ==="

# Join with gist
till federate join --named --token "$GITHUB_TOKEN"

# Get gist ID
GIST_ID=$(till federate status | grep "Gist ID:" | awk '{print $3}')
echo "Gist ID: $GIST_ID"

# Push status
till federate push
echo "✓ Status pushed to gist"

# Verify gist exists
curl -s "https://api.github.com/gists/$GIST_ID" | grep -q "till-status"
[ $? -eq 0 ] && echo "✓ Gist verified" || echo "✗ Gist not found"

# Leave with gist deletion
till federate leave --delete-gist
echo "✓ Left and deleted gist"
```

### Test 4: Directive Processing

```bash
#!/bin/bash
# test_directive_processing.sh

echo "=== Testing Directive Processing ==="

# Create test directive
cat > /tmp/test-directive.yaml << EOF
date: $(date +%Y-%m-%d)
directives:
  - id: test-001
    type: test
    condition: "trust_level == anonymous"
    action: "echo 'Test directive executed'"
    priority: normal
EOF

# Join federation
till federate join --anonymous

# Process directive (when implemented)
till federate pull --directive-file /tmp/test-directive.yaml

# Verify processing
till federate status | grep -q "Last Menu:"
[ $? -eq 0 ] && echo "✓ Directive processed" || echo "✗ Processing failed"
```

## Federation Operations

### Daily Operations

```bash
# Morning sync
$ till federate sync
✓ Synced with federation

# Check for urgent directives
$ till federate pull --urgent-only
No urgent directives found

# Update status
$ till federate push
✓ Status updated
```

### Weekly Maintenance

```bash
# Review federation status
$ till federate status --detailed
Federation Status: Healthy
Member Since: 2025-01-01
Directives Processed: 45
Last 7 days: 7
Success Rate: 100%

# Clean old logs
$ till federate clean --older-than 7d
Cleaned 3 old directive logs

# Verify token validity
$ till federate verify-token
✓ GitHub token is valid
✓ Gist access confirmed
```

### Administrative Tasks

```bash
# Export federation config
$ till federate export > federation-backup.json
✓ Exported federation configuration

# Import federation config
$ till federate import federation-backup.json
✓ Imported federation configuration

# Reset federation
$ till federate reset --confirm
⚠️  This will remove all federation data
Type 'yes' to confirm: yes
✓ Federation reset complete
```

## Troubleshooting

### Common Issues

#### Issue: "Already joined to federation"
```bash
$ till federate join --anonymous
Error: Already joined to federation. Use 'till federate leave' first.

# Solution:
$ till federate leave
$ till federate join --anonymous
```

#### Issue: "GitHub token required"
```bash
$ till federate join --named
Error: GitHub personal access token required for named membership

# Solution:
$ till federate join --named --token ghp_xxxxxxxxxxxxxxxxxxxx
```

#### Issue: "Gist creation failed"
```bash
$ till federate join --named --token ghp_invalid
Error: Failed to create GitHub gist. Check token permissions.

# Solution:
# Ensure token has 'gist' scope at https://github.com/settings/tokens
```

#### Issue: "Federation config corrupted"
```bash
$ till federate status
Error: Failed to load federation configuration

# Solution:
$ rm ~/.till/federation.json
$ till federate join --anonymous  # Rejoin
```

### Debug Mode

```bash
# Enable debug output
$ TILL_DEBUG=1 till federate sync
DEBUG: Loading federation config from /Users/user/.till/federation.json
DEBUG: Fetching menu-of-the-day from https://github.com/ckoons/till/...
DEBUG: Processing 3 directives
DEBUG: Updating gist a1b2c3d4e5f6
DEBUG: Sync complete

# Check logs
$ tail -f ~/.till/logs/till_*.log | grep federate
```

### Recovery Procedures

```bash
# Backup before recovery
$ cp ~/.till/federation.json ~/.till/federation.json.backup

# Try repair
$ till federate repair
Checking federation configuration...
  ✓ Config file exists
  ✓ Site ID valid
  ✗ Gist not accessible
Attempting repair...
  Creating new gist...
  ✓ Repair complete

# If repair fails, reset
$ till federate reset --preserve-site-id
Resetting federation (preserving Site ID)...
✓ Reset complete, rejoin when ready
```

## Best Practices

1. **Start with Anonymous**: Begin with anonymous membership to understand federation behavior
2. **Secure Your Token**: Never commit GitHub tokens to version control
3. **Regular Syncs**: Enable auto-sync or sync daily for latest directives
4. **Monitor Logs**: Check federation logs regularly for issues
5. **Backup Config**: Backup `~/.till/federation.json` before upgrades
6. **Test Directives**: Test directive processing in anonymous mode first
7. **Update Regularly**: Keep Till updated for latest federation features

## Security Considerations

- **Token Storage**: Tokens are encrypted locally using XOR (should upgrade to proper encryption)
- **Gist Privacy**: Federation gists are public by default
- **Directive Validation**: All directives should be validated before execution
- **Trust Verification**: Trusted status requires additional verification
- **Audit Trail**: All federation operations are logged locally

## Future Enhancements

- [ ] Implement actual GitHub API integration
- [ ] Add proper token encryption
- [ ] Implement directive processing engine
- [ ] Add telemetry collection for trusted members
- [ ] Create federation dashboard
- [ ] Add federation health metrics
- [ ] Implement peer discovery
- [ ] Add federation chat/communication channel