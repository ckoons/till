# Till Configuration Reference

## Overview

Till uses several configuration files to manage state, settings, and relationships. All configuration files use JSON format for consistency and Git-friendliness.

## Directory Structure

```
~/.till/                           # Till configuration root
├── tekton/                       # Tekton-specific configuration
│   ├── till-private.json        # Installation registry
│   ├── federation/              # Federation settings
│   │   └── relationships.json   # Trust relationships
│   └── installed/               # Component tracking
│       └── components.json      # Installed components
├── ssh/                         # SSH configuration
│   ├── config                  # SSH client configuration
│   ├── known_hosts             # SSH host fingerprints
│   ├── till_federation_ed25519 # Private key
│   ├── till_federation_ed25519.pub # Public key
│   └── control/                # SSH multiplexing sockets
├── hosts-local.json            # Host database
├── schedule.json               # Sync scheduling
└── logs/                       # Operation logs
    └── till.log               # Main log file
```

## Core Configuration Files

### till-private.json

Location: `~/.till/tekton/till-private.json`

Stores all discovered and installed Tekton instances.

```json
{
  "installations": {
    "primary.tekton.development.us": {
      "root": "/Users/casey/projects/github/Tekton",
      "main_root": "/Users/casey/projects/github/Tekton",
      "port_base": 8000,
      "ai_port_base": 45000,
      "mode": "trusted",
      "installed": "2025-01-05T10:00:00Z",
      "last_sync": "2025-01-05T15:00:00Z",
      "hold": false
    },
    "coder-a.tekton.development.us": {
      "root": "/Users/casey/projects/github/Coder-A",
      "main_root": "/Users/casey/projects/github/Tekton",
      "port_base": 8100,
      "ai_port_base": 45100,
      "mode": "anonymous",
      "installed": "2025-01-05T11:00:00Z",
      "last_sync": "2025-01-05T15:00:00Z",
      "hold": false
    }
  },
  "last_discovery": "2025-01-05T15:30:00Z",
  "version": 1
}
```

**Fields**:
- `installations`: Map of FQN to installation details
- `root`: Installation directory path
- `main_root`: Primary Tekton path (for Coder-X)
- `port_base`: Starting port for components
- `ai_port_base`: Starting port for AI components
- `mode`: Installation mode (anonymous/named/trusted)
- `installed`: Installation timestamp
- `last_sync`: Last synchronization timestamp
- `hold`: If true, prevents updates
- `last_discovery`: Last discovery run timestamp
- `version`: Configuration schema version

### hosts-local.json

Location: `~/.till/hosts-local.json`

Database of managed remote hosts.

```json
{
  "hosts": {
    "production": {
      "user": "deploy",
      "host": "prod.example.com",
      "port": 22,
      "status": "deployed",
      "added": "2025-01-05T10:00:00Z",
      "last_sync": "2025-01-05T15:00:00Z",
      "last_test": "2025-01-05T14:30:00Z",
      "tekton_path": "/home/deploy/projects/github/Tekton",
      "till_version": "1.0.0",
      "installations": [
        "primary.tekton.production.us"
      ],
      "metadata": {
        "os": "linux",
        "arch": "x86_64",
        "memory": "16GB",
        "cores": 8
      }
    },
    "development": {
      "user": "casey",
      "host": "192.168.1.100",
      "port": 22,
      "status": "till-ready",
      "added": "2025-01-05T11:00:00Z",
      "last_sync": "2025-01-05T15:00:00Z",
      "last_test": "2025-01-05T15:00:00Z",
      "tekton_path": "/home/casey/Tekton",
      "till_version": "1.0.0",
      "installations": []
    }
  },
  "groups": {
    "production": ["production"],
    "development": ["development", "laptop", "desktop"],
    "all": ["production", "development", "laptop", "desktop"]
  },
  "updated": "1234567890"
}
```

**Host Status Values**:
- `untested`: Newly added, not verified
- `connected`: SSH connection successful
- `till-ready`: Till installed and operational
- `deployed`: Tekton deployed and running
- `unreachable`: Connection failed
- `error`: Configuration or operational error

### schedule.json

Location: `~/.till/schedule.json`

Automatic synchronization schedule.

```json
{
  "sync": {
    "enabled": true,
    "interval_hours": 24,
    "daily_at": "03:00",
    "next_run": "2025-01-06T03:00:00Z",
    "last_run": "2025-01-05T03:00:00Z",
    "last_status": "success",
    "consecutive_failures": 0,
    "history": [
      {
        "timestamp": "2025-01-05T03:00:00Z",
        "status": "success",
        "duration_seconds": 45,
        "installations_synced": 4,
        "hosts_synced": 2
      }
    ]
  },
  "watch": {
    "enabled": true,
    "pid": 12345,
    "started": "2025-01-05T00:00:00Z",
    "daemon_type": "systemd",
    "log_file": "/home/casey/.till/logs/watch.log"
  }
}
```

**Fields**:
- `sync.enabled`: Whether automatic sync is enabled
- `sync.interval_hours`: Hours between syncs
- `sync.daily_at`: Specific time for daily sync (24h format)
- `sync.next_run`: Next scheduled sync time
- `sync.last_run`: Last sync execution time
- `sync.last_status`: Result of last sync (success/failure)
- `sync.consecutive_failures`: Failed syncs in a row
- `sync.history`: Recent sync history
- `watch.enabled`: Whether watch daemon is enabled
- `watch.pid`: Process ID of watch daemon
- `watch.daemon_type`: Type of scheduler (systemd/launchd/cron)

## SSH Configuration

### SSH Config File

Location: `~/.till/ssh/config`

SSH client configuration for Till-managed hosts.

```ssh
# Till SSH Configuration - Auto-generated
# This file is managed by Till. Manual changes will be preserved.

# Default settings for all Till hosts
Host till-*
    StrictHostKeyChecking accept-new
    UserKnownHostsFile ~/.till/ssh/known_hosts
    ControlMaster auto
    ControlPath ~/.till/ssh/control/%h-%p-%r
    ControlPersist 10m
    ServerAliveInterval 60
    ServerAliveCountMax 3
    Compression yes
    TCPKeepAlive yes

# Host: production (added 2025-01-05T10:00:00Z)
Host till-production
    HostName prod.example.com
    User deploy
    Port 22
    IdentityFile ~/.till/ssh/till_federation_ed25519
    IdentitiesOnly yes
    LogLevel ERROR

# Host: development (added 2025-01-05T11:00:00Z)  
Host till-development
    HostName 192.168.1.100
    User casey
    Port 22
    IdentityFile ~/.till/ssh/till_federation_ed25519
    IdentitiesOnly yes
    LogLevel ERROR
```

**Important SSH Settings**:
- `StrictHostKeyChecking accept-new`: Auto-accept new hosts
- `ControlMaster auto`: Enable connection multiplexing
- `ControlPersist 10m`: Keep connections alive for 10 minutes
- `Compression yes`: Enable compression for all transfers
- `IdentitiesOnly yes`: Only use specified key

## Federation Configuration

### relationships.json

Location: `~/.till/tekton/federation/relationships.json`

Trust relationships with other Tekton instances.

```json
{
  "mode": "trusted",
  "identity": {
    "fqn": "alice.development.us",
    "public_key": "ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAA...",
    "endpoint": "https://alice.example.com:8000",
    "registered": "2025-01-05T10:00:00Z"
  },
  "relationships": {
    "bob.development.uk": {
      "role": "trusted",
      "trust_level": "high",
      "accept_updates": true,
      "accept_components": ["numa", "rhetor"],
      "public_key": "ssh-ed25519 AAAAC3NzaC1lZDI1NTE5BBBB...",
      "endpoint": "https://bob.example.com:8000",
      "last_contact": "2025-01-05T14:00:00Z",
      "metadata": {
        "owner": "Bob Smith",
        "organization": "Example Corp",
        "purpose": "Collaboration"
      }
    },
    "charlie.research.de": {
      "role": "named",
      "trust_level": "medium",
      "accept_updates": false,
      "accept_components": [],
      "public_key": "ssh-ed25519 AAAAC3NzaC1lZDI1NTE5CCCC...",
      "endpoint": null,
      "last_contact": null,
      "metadata": {}
    }
  },
  "pending": {
    "dave.testing.au": {
      "requested": "2025-01-05T15:00:00Z",
      "role": "named",
      "public_key": "ssh-ed25519 AAAAC3NzaC1lZDI1NTE5DDDD..."
    }
  }
}
```

**Trust Levels**:
- `high`: Accept updates and components automatically
- `medium`: Require confirmation for updates
- `low`: Observer only, no automatic actions
- `blocked`: Reject all interactions

## Component Configuration

### components.json

Location: `~/.till/tekton/installed/components.json`

Tracks installed components and their versions.

```json
{
  "components": {
    "numa": {
      "version": "1.0.0",
      "installed": "2025-01-05T10:00:00Z",
      "updated": "2025-01-05T15:00:00Z",
      "source": "github",
      "hold": false,
      "metadata": {
        "description": "Memory management system",
        "dependencies": ["engram"]
      }
    },
    "rhetor": {
      "version": "1.2.0",
      "installed": "2025-01-05T10:00:00Z",
      "updated": "2025-01-05T14:00:00Z",
      "source": "federation:bob.development.uk",
      "hold": true,
      "metadata": {
        "description": "Prompt management system",
        "dependencies": []
      }
    }
  },
  "registry": {
    "github": "https://github.com/tekton-components",
    "federation": {
      "bob.development.uk": "https://bob.example.com:8000/components"
    }
  },
  "updated": "2025-01-05T15:00:00Z"
}
```

## Environment Files

### .env.local

Location: Each Tekton installation directory

Environment configuration for Tekton instances.

```bash
# Tekton Environment Configuration
# Generated by Till

# Core Settings
TEKTON_ROOT="/Users/casey/projects/github/Tekton"
TEKTON_MAIN_ROOT="/Users/casey/projects/github/Tekton"
TEKTON_REGISTRY_NAME="primary.tekton.development.us"
TEKTON_MODE="trusted"

# Port Configuration
TEKTON_PORT_BASE=8000
TEKTON_AI_PORT_BASE=45000

# Component Ports
HERMES_PORT=8000
HERMES_AI_PORT=45000
ENGRAM_PORT=8001
ENGRAM_AI_PORT=45001
RHETOR_PORT=8002
RHETOR_AI_PORT=45002
NUMA_PORT=8003
NUMA_AI_PORT=45003
APOLLO_PORT=8012
APOLLO_AI_PORT=45012

# Federation
TEKTON_FEDERATION_MODE="trusted"
TEKTON_FEDERATION_NAME="primary.tekton.development.us"
TEKTON_FEDERATION_KEY="/Users/casey/.till/ssh/till_federation_ed25519"

# Feature Flags
ENABLE_AI=true
ENABLE_FEDERATION=true
ENABLE_METRICS=true
DEBUG_MODE=false
```

## Log Files

### till.log

Location: `~/.till/logs/till.log`

Main Till operation log.

```
2025-01-05 10:00:00 [START] till install tekton --mode trusted
2025-01-05 10:00:01 [INFO] Cloning Tekton from GitHub
2025-01-05 10:00:15 [INFO] Generating .env.local
2025-01-05 10:00:16 [INFO] Allocating ports: 8000-8099, 45000-45099
2025-01-05 10:00:17 [INFO] Registration: primary.tekton.development.us
2025-01-05 10:00:18 [SUCCESS] Installation complete
2025-01-05 10:00:18 [END] till install tekton --mode trusted

2025-01-05 15:00:00 [START] till sync
2025-01-05 15:00:01 [DISCOVER] Searching in /Users/casey/projects/github
2025-01-05 15:00:02 [FOUND] primary.tekton.development.us at /Users/casey/projects/github/Tekton
2025-01-05 15:00:03 [FOUND] coder-a.tekton.development.us at /Users/casey/projects/github/Coder-A
2025-01-05 15:00:04 [INFO] Syncing primary.tekton.development.us
2025-01-05 15:00:10 [UPDATE] Updated primary.tekton.development.us
2025-01-05 15:00:11 [INFO] Syncing coder-a.tekton.development.us
2025-01-05 15:00:15 [INFO] Already up to date
2025-01-05 15:00:16 [END] till sync
```

**Log Levels**:
- `START`: Command start
- `END`: Command completion
- `INFO`: Informational message
- `DISCOVER`: Discovery operation
- `FOUND`: Discovery result
- `UPDATE`: Change made
- `WARNING`: Non-critical issue
- `ERROR`: Operation failure
- `SUCCESS`: Successful completion

## Platform-Specific Configuration

### systemd (Linux)

Location: `/etc/systemd/system/till-sync.timer`

```ini
[Unit]
Description=Till Sync Timer
Requires=till-sync.service

[Timer]
OnCalendar=daily
OnCalendar=03:00
Persistent=true

[Install]
WantedBy=timers.target
```

Location: `/etc/systemd/system/till-sync.service`

```ini
[Unit]
Description=Till Sync Service

[Service]
Type=oneshot
User=casey
ExecStart=/usr/local/bin/till sync
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
```

### launchd (macOS)

Location: `~/Library/LaunchAgents/com.till.sync.plist`

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" 
  "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>Label</key>
    <string>com.till.sync</string>
    <key>ProgramArguments</key>
    <array>
        <string>/usr/local/bin/till</string>
        <string>sync</string>
    </array>
    <key>StartCalendarInterval</key>
    <dict>
        <key>Hour</key>
        <integer>3</integer>
        <key>Minute</key>
        <integer>0</integer>
    </dict>
    <key>StandardOutPath</key>
    <string>/Users/casey/.till/logs/sync.log</string>
    <key>StandardErrorPath</key>
    <string>/Users/casey/.till/logs/sync.error.log</string>
</dict>
</plist>
```

### cron (Unix)

Location: User crontab

```cron
# Till daily sync
0 3 * * * /usr/local/bin/till sync >> ~/.till/logs/cron.log 2>&1
```

## Configuration Precedence

When multiple configurations exist, Till follows this precedence:

1. Command-line arguments (highest priority)
2. Environment variables
3. Configuration files
4. Default values (lowest priority)

Example:
```bash
# Command line overrides config file
till install tekton --port-base 9000  # Uses 9000 regardless of config

# Environment variable overrides config file
TILL_CONFIG=~/custom/.till till status  # Uses custom config location

# Config file overrides defaults
# If schedule.json specifies daily_at: "04:00", uses 4 AM not default 3 AM
```

## Migration and Upgrades

### Configuration Version

Each configuration file includes a version field for migration:

```json
{
  "version": 1,
  "...": "..."
}
```

Till automatically migrates configuration files when upgrading:
- Backs up existing configuration
- Applies migration transformations
- Validates new configuration
- Rolls back on error

### Backup Location

Configuration backups are stored in:
```
~/.till/backups/
├── 2025-01-05-100000-till-private.json
├── 2025-01-05-100000-hosts-local.json
└── 2025-01-05-100000-schedule.json
```

## Security Considerations

### File Permissions

Ensure proper permissions on sensitive files:

```bash
# Private key must be readable only by owner
chmod 600 ~/.till/ssh/till_federation_ed25519

# Configuration directory should be owner-only
chmod 700 ~/.till

# Log files can be more permissive
chmod 644 ~/.till/logs/*.log
```

### Sensitive Data

Never store in configuration:
- Passwords
- API tokens
- Private keys (except SSH keys in designated location)
- Credentials

Use environment variables or secure key stores for sensitive data.

## See Also

- [Command Reference](commands.md)
- [Host Management Guide](../train/host-management-guide.md)
- [Installation Guide](../train/installation-guide.md)