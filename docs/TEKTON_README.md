# Till - Tekton Lifecycle Manager

## Overview

Till is a lightweight lifecycle manager for Tekton installations, providing automated synchronization, multi-instance management, and federation capabilities through a simple C program with no runtime dependencies.

## Features

- **Multi-Instance Support**: Manage multiple Tekton environments (primary, Coder-A, Coder-B, Coder-C)
- **Automatic Discovery**: Finds and registers Tekton installations automatically
- **Smart Port Allocation**: Manages port ranges for each instance (8000-8099, 8100-8199, etc.)
- **Host Federation**: Sync configurations across multiple machines via SSH
- **Scheduled Sync**: Automated updates using native OS schedulers (launchd/systemd/cron)
- **Component Management**: Run commands across Tekton components
- **Zero Dependencies**: Single binary, no runtime requirements

## Installation

```bash
# Clone the repository
git clone https://github.com/yourusername/till
cd till

# Build
make

# Install system-wide (requires sudo)
sudo make install

# OR install for current user only
make install-user
# Add to PATH: export PATH="$HOME/.local/bin:$PATH"
```

## Quick Start

### First Time Setup

```bash
# Discover existing Tekton installations
till status

# Install a new Tekton instance
till install --mode anonymous
# OR interactive mode
till install -i
```

### Daily Usage

```bash
# Check status
till status

# Sync all Tekton installations
till sync

# Dry run - see what would be synced
till

# Run Tekton commands
till run tekton status
till run tekton start
till run tekton stop
```

## Core Commands

### Installation Management

```bash
till install [options]        # Install new Tekton instance
  --mode MODE                # anonymous, named, trusted, coder-[a-c]
  --name NAME                # Registry name (FQN format)
  --path PATH                # Installation directory
  --port-base PORT           # Starting port (8000, 8100, etc.)

till uninstall <name>        # Remove Tekton instance
```

### Synchronization

```bash
till sync                    # Pull updates for all installations
till sync --dry-run         # Check for updates without pulling
till                        # Dry run (same as above)
```

### Scheduling

```bash
till watch                   # Show schedule status
till watch 24               # Sync every 24 hours
till watch --daily-at 03:00 # Daily at 3 AM
till watch --enable         # Enable scheduled sync
till watch --disable        # Disable scheduled sync
```

See [SCHEDULE.md](SCHEDULE.md) for detailed scheduling documentation.

### Host Federation

```bash
# Add remote host
till host add <name> <user>@<hostname>

# Test connectivity
till host test <name>

# Install Till on remote host
till host setup <name>

# Sync from specific host
till host sync <name>

# Sync from all hosts
till host sync

# Remove host
till host remove <name>

# SSH to host
till host ssh <name>

# Execute command on host
till host exec <name> <command>
```

### Component Commands

```bash
# List all available commands
till run

# List commands for a component
till run tekton

# Execute component command
till run tekton status
till run tekton start
till run tekton stop --force
```

### Update Management

```bash
# Update Till itself
till update

# Hold component (prevent updates)
till hold <component>

# Release hold
till release <component>
```

## Configuration

Till stores its configuration in `~/.till/`:

```
~/.till/
├── config/           # Till configuration
├── tekton/          # Tekton registry
│   └── till-private.json
├── hosts/           # Host federation data
│   └── hosts.json
├── logs/            # Operation logs
│   ├── till-YYYY-MM-DD.log
│   └── sync.log
└── schedule.json    # Sync schedule
```

### Registry Format (till-private.json)

```json
{
  "version": "1.0.0",
  "installations": {
    "primary.tekton.development.us": {
      "root": "/path/to/Tekton",
      "main_root": "/path/to/Tekton",
      "port_base": 8000,
      "ai_port_base": 45000,
      "mode": "anonymous"
    }
  }
}
```

## Port Allocation

Till manages port ranges for each Tekton instance:

| Instance | Main Ports | AI Ports |
|----------|------------|----------|
| Primary  | 8000-8099  | 45000-45099 |
| Coder-A  | 8100-8199  | 45100-45199 |
| Coder-B  | 8200-8299  | 45200-45299 |
| Coder-C  | 8300-8399  | 45300-45399 |

## Logging

All operations are logged to `~/.till/logs/`:

- `till-YYYY-MM-DD.log` - Daily operation logs
- `sync.log` - Scheduled sync output
- `sync.error.log` - Sync error messages

## Architecture

### Modular Design

- `till.c` - Main entry point, command dispatch
- `till_install.c` - Installation management
- `till_host.c` - Host federation
- `till_schedule.c` - Scheduling system
- `till_run.c` - Component command execution
- `till_registry.c` - Discovery and registration
- `till_common.c` - Shared utilities, logging, JSON operations

### Key Design Principles

1. **No Runtime Dependencies** - Pure C, single binary
2. **Transparent Operation** - All configuration in JSON
3. **Platform Native** - Uses OS schedulers, not custom daemons
4. **Federation by Choice** - Share only what you want
5. **Safe by Default** - Never overwrites without confirmation

## Development

### Building from Source

```bash
# Debug build
make debug

# Run tests
make test

# Show build configuration
make info

# Clean build artifacts
make clean
```

### Adding Component Commands

1. Create executable in component's `TILL_COMMANDS_DIR/`
2. Make it executable: `chmod +x command_name`
3. Command is now available via `till run <component> <command>`

## Troubleshooting

### Discovery Issues

```bash
# Force rediscovery
rm ~/.till/tekton/till-private.json
till status
```

### Schedule Not Running

```bash
# Check schedule status
till watch --status

# Reinstall scheduler
till watch --disable
till watch --daily-at 03:00
```

### Port Conflicts

```bash
# Check port allocations
cat ~/.till/tekton/till-private.json | jq '.installations'

# Manually edit if needed
vi ~/.till/tekton/till-private.json
```

## Support

- Issues: Create an issue in this repository
- Logs: Check `~/.till/logs/` for detailed information
- Debug: Build with `make debug` for verbose output

## License

MIT License - See LICENSE file for details

## Credits

Built for the Tekton project to simplify multi-instance development and deployment.

---

*"Managing Tekton installations should be simple, reliable, and under your control."*