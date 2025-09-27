# Till Command Reference

## Overview

This document provides a complete reference for all Till commands, options, and flags.

## Global Options

These options can be used with any Till command:

| Option | Short | Description |
|--------|-------|-------------|
| `--help` | `-h` | Show help message |
| `--version` | `-v` | Show Till version |
| `--interactive` | `-i` | Run in interactive mode (prompts for missing values) |

## Core Commands

### till

Run without arguments for a dry-run status check.

```bash
till
```

Shows:
- Till version and configuration
- Available updates
- Installed Tekton instances
- Federation status
- Next scheduled sync

### till sync

Synchronize all Tekton installations with their git repositories.

```bash
till sync [options]
```

| Option | Description |
|--------|-------------|
| `--dry-run`, `-n` | Show what would be synced without making changes |
| `--force` | Force sync even if no updates detected |
| `--parallel` | Sync all installations in parallel |

Examples:
```bash
till sync                    # Sync all installations
till sync --dry-run         # Preview sync operation
till sync --force           # Force sync even if up-to-date
```

### till watch

Configure automatic synchronization schedule.

```bash
till watch [options] [interval]
```

| Option | Description |
|--------|-------------|
| `interval` | Hours between syncs (default: 24) |
| `--daily-at TIME` | Run daily at specific time (24h format) |
| `--status` | Show current schedule |
| `--disable` | Disable automatic sync |
| `--enable` | Enable automatic sync |

Examples:
```bash
till watch 24               # Sync every 24 hours
till watch --daily-at 03:00 # Sync daily at 3 AM
till watch --status         # Show current schedule
till watch --disable        # Stop automatic syncs
```

### till status

Show Till system status.

```bash
till status [options]
```

| Option | Description |
|--------|-------------|
| `--verbose`, `-v` | Show detailed information |
| `--json` | Output in JSON format |

Shows:
- Till version
- Configuration location
- Installed Tektons
- Host connections
- Federation status
- Sync schedule

## Installation Commands

### till install

Install a new Tekton instance.

```bash
till install tekton [options]
```

| Option | Description | Default |
|--------|-------------|---------|
| `--mode MODE` | Installation mode: anonymous, named, trusted, coder-[a-c] | anonymous |
| `--name NAME` | Federation name (FQN format) | auto-generated |
| `--path PATH` | Installation directory | ./Tekton or ./Coder-X |
| `--port-base PORT` | Starting port for components | auto-allocated |
| `--ai-port-base PORT` | Starting port for AI components | auto-allocated |

Installation Modes:
- **anonymous**: Private installation with read-only federation access
- **named**: Standard federation participant with identity
- **trusted**: Full federation participant with telemetry
- **coder-a/b/c**: Development environment

Examples:
```bash
till install tekton --mode anonymous
till install tekton --name alice.dev.us --mode trusted
till install tekton --mode coder-a
till install tekton --path ~/myproject/Tekton --port-base 9000
```

### till uninstall

Remove a Tekton installation.

```bash
till uninstall <name>
```

**Warning**: This removes the installation from Till's registry but does not delete files.

Examples:
```bash
till uninstall coder-a.tekton.development.us
```

## Host Management Commands

### till host add

Add a remote host to Till's management.

```bash
till host add <name> <user>@<host>[:port]
```

| Argument | Description |
|----------|-------------|
| `name` | Local name for the host |
| `user@host` | SSH connection string |
| `:port` | Optional SSH port (default: 22) |

Examples:
```bash
till host add laptop casey@192.168.1.100
till host add server deploy@server.example.com:2222
```

### till host test

Test SSH connectivity to a host.

```bash
till host test <name>
```

Checks:
- SSH connection
- Till installation on remote
- Tekton installations

Examples:
```bash
till host test laptop
```

### till host update

Update Till on a remote host (installs if not present).

```bash
till host update [name]
```

| Argument | Description |
|----------|-------------|
| `name` | Specific host to update (optional) |

Without a name, updates all configured hosts.

Examples:
```bash
till host update          # Update all hosts
till host update laptop   # Update specific host
```


### till host exec

Execute command on remote host.

```bash
till host exec <name> <command>
```

| Argument | Description |
|----------|-------------|
| `name` | Host name |
| `command` | Command to execute |

Examples:
```bash
till host exec laptop "till status"
till host exec server "ls -la"
```

### till host ssh

Open SSH session to remote host.

```bash
till host ssh <name> [command]
```

| Argument | Description |
|----------|-------------|
| `name` | Host name |
| `command` | Optional command to execute |

Examples:
```bash
till host ssh laptop              # Interactive SSH session
till host ssh laptop "till sync"  # Execute command and exit
```

### till host remove

Remove host from configuration.

```bash
till host remove <name> [--clean-remote]
```

| Argument | Description |
|----------|-------------|
| `name` | Host name to remove |
| `--clean-remote` | Also remove Till from remote host |

Examples:
```bash
till host remove laptop
till host remove laptop --clean-remote
```

### till host sync

Run 'till sync' on remote host(s) to sync their Tekton installations.

```bash
till host sync [name]
```

| Argument | Description |
|----------|-------------|
| `name` | Specific host to sync (optional) |

Without a name, runs sync on all configured hosts.

Examples:
```bash
till host sync          # Sync Tekton on all hosts
till host sync laptop   # Sync Tekton on specific host
```

### till host status

Show host configuration and status.

```bash
till host status [name]
```

| Argument | Description |
|----------|-------------|
| `name` | Specific host to query (optional) |

Examples:
```bash
till host status        # Show all hosts
till host status laptop # Show specific host
```

## Component Management Commands

### till run

Execute component-specific commands.

```bash
till run [component] [command] [args...]
```

| Argument | Description |
|----------|-----------|
| `component` | Component name (e.g., tekton, numa, rhetor) |
| `command` | Command to execute (e.g., start, stop, status) |
| `args` | Additional arguments passed to the command |

Without arguments, lists available components with discoverable commands.

Till discovers commands in component `.tillrc/commands/` directories. Each executable file in this directory becomes an available command.

Examples:
```bash
till run                        # List available components
till run tekton                 # List commands for tekton
till run tekton status          # Run tekton status command
till run tekton start           # Start tekton
till run tekton stop --force    # Stop tekton with --force flag
till run numa build            # Run numa build command
```

### till hold

Prevent a component from being updated.

```bash
till hold <component>
```

Examples:
```bash
till hold numa          # Prevent numa updates
till hold rhetor        # Prevent rhetor updates
```

### till release

Allow a component to be updated again.

```bash
till release <component>
```

Examples:
```bash
till release numa       # Allow numa updates
```


## Federation Commands

### till federate join

Join the federation network.

```bash
till federate join <gist_id> [--mode <mode>]
```

| Argument | Description |
|----------|-------------|
| `gist_id` | GitHub Gist ID of the federation to join |
| `--mode` | Federation mode: anonymous, named, trusted (default: anonymous) |

Examples:
```bash
till federate join abc123def456
till federate join abc123def456 --mode named
till federate join abc123def456 --mode trusted
```

### till federate status

Show federation status and configuration.

```bash
till federate status
```

Shows:
- Federation mode (anonymous, named, trusted)
- Site ID
- Gist ID
- Last sync time
- Auto-sync status

### till federate leave

Leave the federation network.

```bash
till federate leave
```

This removes federation configuration but retains local installations.

### till federate set

Set federation configuration values.

```bash
till federate set <key> <value>
```

Available settings:
- `auto_sync` - Enable/disable automatic sync (true/false)

Examples:
```bash
till federate set auto_sync true
till federate set auto_sync false
```

### till federate menu

Manage federation menu components.

```bash
till federate menu <action> [args...]
```

Actions:
- `add` - Add component to menu
- `remove` - Remove component from menu
- `list` - List menu components

Examples:
```bash
till federate menu add Numa https://github.com/org/numa.git
till federate menu remove Numa
till federate menu list
```


## Update Commands

### till update

Check for and apply Till updates.

```bash
till update [options]
```

| Option | Description |
|--------|-------------|
| `--check` | Check for updates only |
| `--apply` | Apply available updates |
| `--auto` | Enable automatic updates |

Examples:
```bash
till update --check     # Check for Till updates
till update --apply     # Update Till to latest version
```


## Troubleshooting Commands

### till repair

Check and repair Till configuration and installations.

```bash
till repair
```

Checks and fixes:
- Directory permissions
- Configuration validity
- Installation registry
- Federation configuration

Examples:
```bash
till repair
```

## Environment Variables

Till respects these environment variables:

| Variable | Description | Default |
|----------|-------------|---------|
| `TILL_CONFIG` | Configuration directory | ~/.till |
| `TILL_LOG_LEVEL` | Logging verbosity | INFO |
| `TILL_SSH_CONFIG` | SSH config file | ~/.till/ssh/config |
| `TILL_REPO_URL` | Till repository URL | https://github.com/tillfed/till |

## Exit Codes

Till uses standard exit codes:

| Code | Meaning |
|------|---------|
| 0 | Success |
| 1 | General error |
| 2 | Usage error (invalid arguments) |
| 3 | File error (not found, permissions) |
| 4 | Network error |
| 5 | Configuration error |

## Interactive Mode

Many commands support interactive mode with `-i` or `--interactive`:

```bash
till -i install tekton
# Prompts for all required values

till -i host add
# Prompts for host details
```

## JSON Output

Commands that support `--json` output format their results as JSON:

```bash
till status --json | jq '.installations'
till host status --json | jq '.hosts[].status'
```


## Command Chaining

Till commands can be chained for automation:

```bash
# Add host and update Till on it
till host add prod user@server && \
till host update prod && \
till host sync prod

# Check and sync if needed
till status --json | grep -q updates && till sync
```

## Getting Help

For any command, use `--help`:

```bash
till --help
till install --help
till host --help
till host add --help
```

## See Also

- [Configuration Reference](configuration.md)
- [Host Management Guide](../train/host-management-guide.md)
- [Federation Guide](../train/federation-guide.md)
- [Installation Guide](../train/installation-guide.md)
