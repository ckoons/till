# Till User Guide

## What is Till?

Till is the lifecycle manager for Tekton. It handles:
- Installing Tekton environments
- Managing updates and components
- Coordinating multiple Tekton instances
- Enabling federation between Tektons

## Quick Start

### Installation

1. **Clone Till**
```bash
git clone https://github.com/ckoons/till.git
cd till
```

2. **Build Till**
```bash
make
```

3. **Test Installation**
```bash
./till --version
./till --help
```

4. **Optional: Install System-wide**
```bash
sudo make install
# Now you can use 'till' from anywhere
```

## Basic Usage

### Check Status (Dry Run)
```bash
till
# Shows what would happen without making changes
```

### Install Your First Tekton

#### Solo Installation (Private)
```bash
till install tekton --mode solo
# Installs in ./Tekton with default ports
```

#### Named Installation (Can Join Federation)
```bash
till install tekton --name alice.development.us --mode member
# Installs with federation capability
```

### Install Development Environments

After installing a primary Tekton, add Coder environments:

```bash
till install tekton --mode coder-a
# Automatically named: alice.development.us.coder-a
# Ports: 8100-8199, AI: 45100-45199

till install tekton --mode coder-b
# Automatically named: alice.development.us.coder-b
# Ports: 8200-8299, AI: 45200-45299
```

## Common Tasks

### Managing Hosts

Add remote Tekton instances:
```bash
till host add laptop localhost /Users/alice/Tekton
till host add desktop 192.168.1.100 /home/alice/Tekton
till host list
```

### Component Management

Prevent updates to a component:
```bash
till hold numa
```

Allow updates again:
```bash
till release numa
```

Check for updates:
```bash
till update check
```

Apply updates:
```bash
till update apply
```

### Federation

Join the federation:
```bash
till federate init --mode observer --name alice.development.us
```

Check federation status:
```bash
till federate status
```

Leave federation:
```bash
till federate leave
```

## Directory Structure

After installation:
```
your-project/
├── Tekton/          # Primary installation
├── Coder-A/         # Development environment A
├── Coder-B/         # Development environment B
└── till/            # Till itself
    └── .till/       # Local configuration (private)
```

## Port Allocation

Default port ranges:
- **Primary Tekton**: 8000-8099 (components), 45000-45099 (AI)
- **Coder-A**: 8100-8199, 45100-45199
- **Coder-B**: 8200-8299, 45200-45299
- **Coder-C**: 8300-8399, 45300-45399

## Configuration Files

### .env.local
Generated in each Tekton directory with:
- TEKTON_ROOT: Installation path
- TEKTON_REGISTRY_NAME: Federation name
- Port assignments for all components

### till-private.json
Stored in `.till/tekton/federation/`:
- Tracks installed Tektons
- Stores federation relationships
- Records port allocations

## Troubleshooting

### Git Authentication Failed
```
Error: Git clone failed
```
**Solution**: Ensure you're authenticated to GitHub:
```bash
gh auth login
# or
ssh-add ~/.ssh/id_rsa
```

### Port Already in Use
Check what's using the port:
```bash
lsof -i :8000
```

Install with different ports:
```bash
till install tekton --port-base 9000 --ai-port-base 46000
```

### Can't Find Primary Tekton
When installing Coder-A but no primary exists:
```
Error: No primary Tekton found. Install primary first.
```
**Solution**: Install a primary Tekton first (without --mode coder-x)

## Command Reference

### Core Commands
- `till` - Dry run, show what would happen
- `till sync` - Synchronize with GitHub
- `till watch [hours]` - Set automatic sync frequency
- `till status` - Show Till status

### Installation
- `till install tekton [options]` - Install new Tekton
  - `--mode [solo|observer|member|coder-x]`
  - `--name <name>` - Federation name
  - `--path <path>` - Installation directory
  - `--port-base <port>` - Starting port number
  - `--ai-port-base <port>` - AI starting port

### Management
- `till hold <component>` - Prevent updates
- `till release <component>` - Allow updates
- `till update check` - Check for updates
- `till update apply` - Apply updates

### Federation
- `till federate init` - Join federation
- `till federate status` - Check status
- `till federate leave` - Leave federation

### Hosts
- `till host add <name> <address> <path>` - Add host
- `till host remove <name>` - Remove host
- `till host list` - List hosts

## Best Practices

1. **Install Primary First**: Always install a primary Tekton before Coder environments
2. **Use Meaningful Names**: Follow name.category.geography pattern
3. **Document Custom Ports**: If not using defaults, document why
4. **Regular Syncs**: Run `till sync` periodically for updates
5. **Backup Configuration**: Keep copies of `.till/` directory

## Getting Help

- Run `till --help` for command syntax
- Check `till/<command> --help` for specific commands
- Review logs in `.till/logs/` (when implemented)
- Check GitHub issues for known problems

## Next Steps

1. Install your first Tekton
2. Start Tekton with `cd Tekton && tekton start`
3. Explore federation by joining as an observer
4. Add Coder environments for development
5. Customize your installation as needed