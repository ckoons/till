# Till Sync Integration Documentation

## Overview

As of the latest update, `till sync` is now a unified command that performs all synchronization operations:

1. **Till self-update** - Updates Till itself to the latest version
2. **Tekton updates** - Updates all registered Tekton installations 
3. **Federation sync** - If joined, syncs with the global federation

## Command Flow

```
till sync
    │
    ├── Check for Till updates (unless --skip-till-update)
    │   └── Self-update if needed
    │
    ├── Update all Tekton installations
    │   ├── Check each installation
    │   ├── Skip held installations
    │   └── Git pull for each active installation
    │
    └── Federation sync (if joined)
        ├── Pull menu-of-the-day directives
        └── Push status to GitHub gist
```

## Usage

### Basic Sync
```bash
# Full sync - updates everything
till sync

# Skip Till self-update
till sync --skip-till-update

# Dry run - see what would be updated
till sync --dry-run
```

### Automatic Scheduling
```bash
# Schedule hourly syncs
till watch enable --interval 1h

# Schedule daily sync at 3 AM
till watch enable --daily-at 03:00

# Check watch status
till watch status
```

## Federation Integration

### How It Works

When you run `till sync`:

1. **Not Joined**: Only Till and Tekton updates occur
2. **Joined as Anonymous**: Pull-only federation sync (no gist updates)
3. **Joined as Named/Trusted**: Full bidirectional sync
   - Pulls menu-of-the-day directives
   - Pushes system status to GitHub gist

### Federation Workflow

```bash
# Join federation
till federate join --named

# Now regular sync includes federation
till sync
# Output will show:
#   Till Sync
#   =========
#   [Tekton updates...]
#   
#   Running federation sync...
#   Step 1: Pulling menu-of-the-day...
#   Step 2: Pushing status...
#   ✓ Sync complete

# Leave federation
till federate leave
```

## Command Comparison

### Before Integration
```bash
till sync              # Only Tekton updates
till update            # Only Till self-update  
till federate sync     # Only federation sync
# User had to run all three separately
```

### After Integration
```bash
till sync              # Everything in one command
```

## Watch Command Integration

The `till watch` command schedules recurring `till sync` operations, which means:

- Scheduled syncs automatically include federation updates
- No need to schedule separate federation syncs
- One cron job handles everything

### Example Cron Entry
```cron
0 3 * * * /usr/local/bin/till sync
```

This single entry will:
- Update Till
- Update all Tekton installations
- Sync with federation (if joined)

## Options and Flags

### till sync

| Flag | Description |
|------|-------------|
| `--dry-run` | Preview what would be updated without making changes |
| `--skip-till-update` | Skip Till self-update check |
| `--help` | Show help message |

### Environment Behavior

- **Dry Run Mode**: Federation sync is skipped in dry-run mode
- **Anonymous Members**: Push operations are skipped
- **Offline Mode**: Federation sync fails gracefully if network unavailable

## Technical Details

### Implementation Location

The integration is implemented in:
- `src/till_commands.c` - Main sync command
- `src/till_federation.c` - Federation sync functions
- `src/till_schedule.c` - Watch scheduling

### Key Functions

```c
// Main sync command
int cmd_sync(int argc, char *argv[]) {
    // ... Update Tekton installations ...
    
    /* Run federation sync if joined */
    if (!dry_run && federation_is_joined()) {
        printf("\nRunning federation sync...\n");
        till_federate_sync();
    }
}
```

## Benefits of Integration

1. **Simplicity**: One command does everything
2. **Consistency**: Federation always stays in sync
3. **Automation**: Watch command handles all updates
4. **Efficiency**: Single command reduces overhead
5. **User Experience**: Less to remember and configure

## Migration Guide

If you had separate cron jobs or scripts:

### Old Way
```bash
# Multiple cron entries or script lines
0 3 * * * till update
5 3 * * * till sync  
10 3 * * * till federate sync
```

### New Way
```bash
# Single entry
0 3 * * * till sync
# Or use watch
till watch enable --daily-at 03:00
```

## Troubleshooting

### Federation sync not running?
```bash
# Check if joined
till federate status

# Join if needed
till federate join --named
```

### Want to sync without federation?
```bash
# Leave federation
till federate leave

# Now sync won't include federation
till sync
```

### Debug federation issues
```bash
# Run federation sync manually
till federate sync

# Check gist
till federate status  # Shows gist ID
```

## Summary

The integrated `till sync` command provides a unified synchronization experience. Whether updating Till itself, syncing Tekton installations, or participating in the global federation, one command handles it all. This integration makes Till simpler to use while maintaining all the power of its distributed architecture.