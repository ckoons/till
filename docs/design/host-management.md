# Host Management Architecture

## Overview

Till provides comprehensive SSH-based host management following "The Till Way" - simple, works, hard to screw up. This system enables Till to manage any number of remote Tekton installations through secure SSH connections.

## Core Philosophy

### The Till Way
1. **Simple is better**: Only SSH and rsync for remote operations
2. **Till manages its own SSH**: Separate configuration from user's SSH
3. **Federation-ready**: Ed25519 keys for secure communication
4. **Automatic deployment**: Full lifecycle management from setup to updates

## Architecture

### Two-Layer Federation Model

Till implements a two-layer federation approach:

1. **Infrastructure Layer (Host Management)**
   - Direct SSH-based host control
   - Till manages remote Till instances
   - Full automation capabilities
   - No GitHub dependency
   - Complete sovereignty over managed hosts

2. **Application Layer (GitHub Federation)**
   - Applications have tribal membership
   - Observers and members at application level
   - GitHub-based discovery and registration
   - Component and solution sharing

### Key Distinction
- **Hosts**: Infrastructure managed by Till through SSH (full control)
- **Applications**: Federated through GitHub (tribal membership)
- **Result**: Local control with global collaboration

## SSH Configuration

### Directory Structure
```
~/.till/
├── ssh/
│   ├── config                    # Till's SSH configuration
│   ├── known_hosts               # Known host fingerprints
│   ├── till_federation_ed25519  # Private key
│   ├── till_federation_ed25519.pub  # Public key
│   └── control/                  # SSH multiplexing sockets
├── hosts-local.json              # Host database
└── schedule.json                 # Sync scheduling
```

### SSH Configuration Template
```ssh
# Till SSH Configuration - Auto-generated
Host till-*
    StrictHostKeyChecking accept-new
    UserKnownHostsFile ~/.till/ssh/known_hosts
    ControlMaster auto
    ControlPath ~/.till/ssh/control/%h-%p-%r
    ControlPersist 10m
    ServerAliveInterval 60
    ServerAliveCountMax 3

Host till-hostname
    HostName actual.host.name
    User username
    Port 22
    IdentityFile ~/.till/ssh/till_federation_ed25519
    IdentitiesOnly yes
```

### SSH Multiplexing
- Single SSH connection per host
- Reused for multiple operations
- 10-minute persistence
- Automatic cleanup

## Host Database

### hosts-local.json Structure
```json
{
  "hosts": {
    "production": {
      "user": "deploy",
      "host": "prod.example.com",
      "port": 22,
      "status": "till-ready",
      "added": "2025-01-05T10:00:00Z",
      "last_sync": "2025-01-05T15:00:00Z",
      "tekton_path": "/home/deploy/Tekton",
      "installations": ["primary.tekton.us"]
    }
  },
  "updated": "1234567890"
}
```

### Host Status Progression
1. **untested**: Newly added, not verified
2. **connected**: SSH connection successful
3. **till-ready**: Till installed and operational
4. **deployed**: Tekton deployed and running

## Security Model

### Authentication
- Ed25519 keys (modern, secure, fast)
- Separate from user's SSH keys
- One key pair for all federation operations
- No password authentication

### Authorization
- Host operator adds Till's public key to authorized_keys
- SSH configuration limits Till to specific operations
- No root access required (except optional for system-wide install)

### Trust Boundaries
- Till only manages explicitly added hosts
- Each host relationship is independent
- No transitive trust (A trusts B, B trusts C ≠ A trusts C)
- Revocation by removing from authorized_keys

## Deployment Workflow

### Initial Setup
```mermaid
graph LR
    A[till host init] --> B[Generate SSH Key]
    B --> C[Create SSH Config]
    C --> D[Initialize Database]
```

### Adding a Host
```mermaid
graph LR
    A[till host add] --> B[Parse user@host]
    B --> C[Add to Database]
    C --> D[Update SSH Config]
    D --> E[Create SSH Alias]
```

### Deployment Process
```mermaid
graph LR
    A[till host test] --> B[Verify SSH]
    B --> C[till host setup]
    C --> D[Install Till]
    D --> E[till host deploy]
    E --> F[Sync Tekton]
    F --> G[Configure Remote]
```

## Remote Operations

### Supported Commands
1. **SSH**: Command execution on remote hosts
2. **rsync**: File synchronization with compression
3. **git**: Through SSH for repository operations

### Operation Types

#### Testing
```bash
ssh -F ~/.till/ssh/config till-hostname 'echo TILL_TEST_SUCCESS'
```

#### Setup
```bash
# Create directories
ssh till-hostname 'mkdir -p ~/projects/github'

# Clone Till
ssh till-hostname 'git clone https://github.com/tillfed/till.git'

# Build Till
ssh till-hostname 'cd ~/projects/github/till && make'
```

#### Deployment
```bash
# Sync files
rsync -avz --delete \
  --exclude='.git' \
  --exclude='*.pyc' \
  --exclude='logs/' \
  -e 'ssh -F ~/.till/ssh/config' \
  /local/tekton/ till-hostname:~/projects/github/Tekton/
```

#### Synchronization
```bash
# Pull updates on remote
ssh till-hostname 'cd ~/projects/github/Tekton && git pull'

# Run Till sync
ssh till-hostname 'till sync'
```

## Automation

### Daily Sync Schedule
Till implements automatic synchronization through platform-specific schedulers:

#### Linux (systemd)
```ini
[Unit]
Description=Till Sync Timer

[Timer]
OnCalendar=daily
Persistent=true

[Install]
WantedBy=timers.target
```

#### macOS (launchd)
```xml
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
</dict>
</plist>
```

### Schedule Management
```json
{
  "sync": {
    "enabled": true,
    "interval_hours": 24,
    "daily_at": "03:00",
    "next_run": "2025-01-06T03:00:00Z",
    "last_run": "2025-01-05T03:00:00Z",
    "status": "success"
  }
}
```

## Scalability

### Connection Pooling
- SSH ControlMaster for connection reuse
- Parallel operations on multiple hosts
- Automatic retry with exponential backoff

### Host Groups (Future)
```json
{
  "groups": {
    "production": ["prod1", "prod2", "prod3"],
    "development": ["dev1", "dev2"],
    "all": ["prod1", "prod2", "prod3", "dev1", "dev2"]
  }
}
```

### Batch Operations
```bash
# Sync all production hosts
till host sync --group production

# Deploy to all development
till host deploy --group development
```

## Error Handling

### Connection Failures
1. Retry with exponential backoff
2. Mark host as "unreachable" after 3 failures
3. Continue with other hosts
4. Report summary at end

### Partial Deployments
- Track deployment state per host
- Resume capability for interrupted deployments
- Rollback option for failed deployments

## Integration Points

### With Till Sync
- `till sync` includes remote hosts
- Parallel synchronization
- Consolidated reporting

### With Federation
- Hosts provide infrastructure
- Federation provides application layer
- GitHub federation independent of host management

### With Tekton
- Deploy multiple Tekton instances
- Coordinate updates across hosts
- Maintain configuration consistency

## Implementation Status

### Completed
- [x] SSH key generation (Ed25519)
- [x] SSH configuration management
- [x] Host database (JSON)
- [x] Basic CRUD operations
- [x] SSH connectivity testing
- [x] Remote Till installation
- [x] Tekton deployment
- [x] Remote synchronization

### In Progress
- [ ] Schedule management
- [ ] Platform-specific schedulers
- [ ] Watch daemon implementation

### Future Enhancements
- [ ] Host groups
- [ ] Batch operations
- [ ] Parallel execution
- [ ] Progress reporting
- [ ] Deployment rollback
- [ ] Health monitoring
- [ ] Metrics collection

## Best Practices

### Security
1. Use dedicated SSH keys for Till
2. Limit SSH access to specific commands (future)
3. Regular key rotation (annual)
4. Monitor authorized_keys on hosts

### Operations
1. Test connectivity before deployment
2. Use meaningful host names
3. Document custom configurations
4. Regular sync schedule (daily recommended)
5. Monitor sync logs for issues

### Troubleshooting
1. Verify SSH key in authorized_keys
2. Check network connectivity
3. Confirm Till installation on remote
4. Review SSH config syntax
5. Check disk space on remote

## Command Reference

See [Commands Reference](../reference/commands.md#host-management) for detailed command documentation.

## Configuration Reference

See [Configuration Reference](../reference/configuration.md#host-configuration) for detailed configuration file formats.