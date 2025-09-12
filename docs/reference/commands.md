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

### till host init

Initialize Till's SSH environment and generate federation keys.

```bash
till host init
```

Creates:
- SSH configuration directory (`~/.till/ssh/`)
- Ed25519 key pair for federation
- Host database
- SSH config file

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

### till host setup

Install Till on a remote host.

```bash
till host setup <name>
```

Process:
1. Creates directory structure
2. Clones Till repository
3. Builds Till
4. Verifies installation

Examples:
```bash
till host setup laptop
```

### till host deploy

Deploy a Tekton installation to a remote host.

```bash
till host deploy <name> [installation]
```

| Argument | Description |
|----------|-------------|
| `name` | Host name |
| `installation` | Specific installation to deploy (optional) |

If no installation specified, deploys the primary Tekton.

Examples:
```bash
till host deploy laptop                          # Deploy primary
till host deploy laptop coder-a.tekton.dev.us   # Deploy specific
```

### till host sync

Synchronize from remote hosts.

```bash
till host sync [name]
```

| Argument | Description |
|----------|-------------|
| `name` | Specific host to sync (optional) |

Without a name, syncs all configured hosts.

Examples:
```bash
till host sync          # Sync all hosts
till host sync laptop   # Sync specific host
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

### till federate init

Join the federation network.

```bash
till federate init --mode <mode> --name <name>
```

| Option | Description | Required |
|--------|-------------|----------|
| `--mode` | Federation mode: named, trusted | Yes |
| `--name` | Federation name (FQN format) | Yes |

Examples:
```bash
till federate init --mode named --name alice.dev.us
till federate init --mode trusted --name team.company.us
```

### till federate status

Show federation status and relationships.

```bash
till federate status [options]
```

| Option | Description |
|--------|-------------|
| `--verbose` | Show detailed information |
| `--json` | Output in JSON format |

### till federate leave

Leave the federation network.

```bash
till federate leave
```

This:
- Deregisters from GitHub
- Removes federation configuration
- Retains local installations

## Discovery Commands

### till discover

Discover existing Tekton installations.

```bash
till discover [options]
```

| Option | Description |
|--------|-------------|
| `--force` | Force rediscovery |
| `--path PATH` | Specific path to search |

Till automatically runs discovery on every command, but you can force it manually.

Examples:
```bash
till discover
till discover --force
till discover --path ~/projects
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

## Configuration Commands

### till config

Manage Till configuration.

```bash
till config [options]
```

| Option | Description |
|--------|-------------|
| `--list` | Show all configuration |
| `--get KEY` | Get specific configuration value |
| `--set KEY VALUE` | Set configuration value |

Examples:
```bash
till config --list
till config --get federation.mode
till config --set sync.auto true
```

## Troubleshooting Commands

### till doctor

Diagnose and fix common issues.

```bash
till doctor [options]
```

| Option | Description |
|--------|-------------|
| `--fix` | Attempt to fix issues automatically |
| `--verbose` | Show detailed diagnostics |

Checks:
- Directory permissions
- Git configuration
- SSH connectivity
- Port availability
- Configuration validity

### till logs

View Till operation logs.

```bash
till logs [options]
```

| Option | Description |
|--------|-------------|
| `--tail N` | Show last N lines |
| `--follow` | Follow log output |
| `--level LEVEL` | Filter by log level |

Examples:
```bash
till logs --tail 50
till logs --follow
till logs --level ERROR
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

## Aliases and Shortcuts

Common command shortcuts:

```bash
# These are equivalent:
till sync
till s

till host status
till h status
till hs

till federate init
till f init
```

## Command Chaining

Till commands can be chained for automation:

```bash
# Setup and deploy in one line
till host add prod user@server && \
till host setup prod && \
till host deploy prod

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