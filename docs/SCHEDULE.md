# Till Schedule System Documentation

## Overview

Till's scheduling system provides automated synchronization of Tekton installations using native OS schedulers. It supports macOS (launchd), Linux (systemd/cron), and other Unix systems (cron).

## Configuration

Schedule settings are stored in `~/.till/schedule.json`.

## Commands

### View Schedule Status
```bash
till watch --status
# or just
till watch
```

Shows:
- Current enabled/disabled status
- Sync interval or daily time
- Next scheduled run
- Last run time and status
- Recent sync history

### Enable Automatic Sync

**Set interval-based sync (every N hours):**
```bash
till watch 24              # Sync every 24 hours
till watch 6               # Sync every 6 hours
```

**Set daily sync at specific time:**
```bash
till watch --daily-at 03:00   # Daily at 3:00 AM
till watch --daily-at 14:30   # Daily at 2:30 PM
```

### Disable Automatic Sync
```bash
till watch --disable
```

### Re-enable Previous Schedule
```bash
till watch --enable
```

## Platform-Specific Implementation

### macOS (launchd)

Till creates a launchd plist at:
```
~/Library/LaunchAgents/com.till.sync.plist
```

The plist specifies:
- Program to run: `till sync`
- Schedule: Daily at specified time or interval
- Log output: `~/.till/logs/sync.log`
- Error output: `~/.till/logs/sync.error.log`

To manually check the launchd job:
```bash
launchctl list | grep till
```

To manually unload:
```bash
launchctl unload ~/Library/LaunchAgents/com.till.sync.plist
```

### Linux with systemd

Till creates systemd user units:
```
~/.config/systemd/user/till-sync.service  # Service definition
~/.config/systemd/user/till-sync.timer    # Timer schedule
```

To check status:
```bash
systemctl --user status till-sync.timer
systemctl --user list-timers
```

To manually control:
```bash
systemctl --user stop till-sync.timer
systemctl --user disable till-sync.timer
```

### Linux/Unix with cron

Till adds a cron entry for the current user:
```bash
# View cron entries
crontab -l | grep till

# Example entry:
0 3 * * * /usr/local/bin/till sync >> ~/.till/logs/cron.log 2>&1
```

To manually remove:
```bash
crontab -e  # Then delete the till line
```

## Schedule Configuration File

The `~/.till/schedule.json` file structure:

```json
{
  "sync": {
    "enabled": true,
    "interval_hours": 24,
    "daily_at": "03:00",
    "next_run": "2024-01-16 03:00:00",
    "last_run": "2024-01-15 03:00:00",
    "last_status": "success",
    "consecutive_failures": 0,
    "history": [
      {
        "timestamp": "2024-01-15 03:00:00",
        "status": "success",
        "duration_seconds": 45,
        "installations_synced": 3,
        "hosts_synced": 2
      }
    ]
  },
  "watch": {
    "enabled": false,
    "pid": null,
    "started": null,
    "daemon_type": null
  }
}
```

## Sync History

Till maintains the last 10 sync attempts in the history array. Each entry contains:
- `timestamp`: When the sync occurred
- `status`: "success" or "failure"
- `duration_seconds`: How long the sync took
- `installations_synced`: Number of Tekton installations updated
- `hosts_synced`: Number of remote hosts synchronized

## Logging

All scheduled sync operations log to:
- Standard output: `~/.till/logs/sync.log`
- Error output: `~/.till/logs/sync.error.log`
- Daily logs: `~/.till/logs/till-YYYY-MM-DD.log`

## Troubleshooting

### Schedule not running

1. Check if enabled:
   ```bash
   till watch --status
   ```

2. Verify system scheduler:
   ```bash
   # macOS
   launchctl list | grep till
   
   # Linux systemd
   systemctl --user status till-sync.timer
   
   # Cron
   crontab -l | grep till
   ```

3. Check logs for errors:
   ```bash
   tail -f ~/.till/logs/sync.error.log
   ```

### Reinstall Schedule

If the system scheduler gets corrupted:

1. Disable current schedule:
   ```bash
   till watch --disable
   ```

2. Re-enable with desired settings:
   ```bash
   till watch --daily-at 03:00
   ```

### Manual Sync

You can always run a manual sync:
```bash
till sync
```

## Best Practices

1. **Choose appropriate intervals**: 
   - Development: Every 6-12 hours
   - Production: Daily at low-traffic times

2. **Monitor failures**: Check `consecutive_failures` in status
   - 3+ failures may indicate network or repository issues

3. **Log rotation**: Periodically clean old logs in `~/.till/logs/`

4. **Time zone awareness**: Daily times use system local time zone

## Implementation Notes

The schedule system is implemented in `till_schedule.c` with platform-specific functions:
- `till_watch_install_launchd()` - macOS
- `till_watch_install_systemd()` - Linux with systemd  
- `till_watch_install_cron()` - Fallback for other systems

The schedule is checked and updated after each sync operation via `till_watch_record_sync()`.