# Host Management User Guide

## Introduction

Till's host management system allows you to control multiple Tekton installations across different machines from a single location. Whether you're managing a laptop, desktop, and server, or coordinating a fleet of development environments, Till makes it simple and secure.

## Quick Start

### Step 1: Initialize Till's SSH Environment

First, set up Till's SSH configuration and generate your federation key:

```bash
till host init
```

This creates:
- Your federation SSH key pair (Ed25519)
- Till's SSH configuration directory
- Initial host database

You'll see output like:
```
Generating federation SSH key...

Federation public key:
==========
ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAA... till-federation@yourname
==========

Add this key to authorized_keys on remote hosts.
```

**Important**: Save this public key - you'll need to add it to each host you want to manage.

### Step 2: Prepare Remote Hosts

On each machine you want to manage, add Till's public key to the authorized keys:

```bash
# On the remote machine
echo "ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAA... till-federation@yourname" >> ~/.ssh/authorized_keys
```

### Step 3: Add Your First Host

Add a remote host to Till's management:

```bash
till host add myserver user@server.example.com
```

For non-standard SSH ports:
```bash
till host add myserver user@server.example.com:2222
```

### Step 4: Test the Connection

Verify Till can connect to the host:

```bash
till host test myserver
```

Success output:
```
Testing connection to 'myserver'...
✓ Connection successful
⚠ Till not found on remote host
  Run: till host setup myserver
```

### Step 5: Install Till on Remote Host

If Till isn't already installed on the remote host:

```bash
till host setup myserver
```

This will:
1. Create necessary directories
2. Clone Till from GitHub
3. Build Till on the remote host
4. Verify installation

### Step 6: Deploy Tekton

Deploy your Tekton installation to the remote host:

```bash
till host deploy myserver
```

To deploy a specific installation:
```bash
till host deploy myserver coder-a.tekton.development.us
```

## Common Scenarios

### Managing Multiple Development Machines

You have a desktop for heavy development and a laptop for mobile work:

```bash
# Add both machines
till host add desktop casey@192.168.1.100
till host add laptop casey@192.168.1.101

# Set up Till on both
till host setup desktop
till host setup laptop

# Deploy primary Tekton to desktop
till host deploy desktop primary.tekton.development.us

# Deploy Coder-A to laptop  
till host deploy laptop coder-a.tekton.development.us

# Keep them synchronized
till host sync  # Syncs all hosts
```

### Team Development Environment

Set up a shared development server for your team:

```bash
# Add the team server
till host add teamdev deploy@dev.company.com

# Setup and deploy
till host setup teamdev
till host deploy teamdev

# Regular synchronization
till host sync teamdev
```

### Production Deployment

Deploy to production servers with careful control:

```bash
# Add production hosts
till host add prod1 deploy@prod1.example.com
till host add prod2 deploy@prod2.example.com

# Test connections first
till host test prod1
till host test prod2

# Deploy one at a time
till host deploy prod1
# Verify deployment...
till host deploy prod2
```

## SSH Aliases

Till creates SSH aliases for easy access to managed hosts:

```bash
# Instead of:
ssh user@server.example.com

# You can use:
ssh till-myserver
```

These aliases use Till's SSH configuration and federation key automatically.

## Synchronization

### Manual Sync

Sync all hosts:
```bash
till host sync
```

Sync specific host:
```bash
till host sync myserver
```

### Automatic Daily Sync

Set up automatic daily synchronization at 3 AM:

```bash
till watch --daily-at 03:00
```

Check sync schedule:
```bash
till watch --status
```

Disable automatic sync:
```bash
till watch --disable
```

## Status and Monitoring

### View All Hosts

```bash
till host status
```

Output:
```
Configured hosts:
Name        Host                      Status         
----        ----                      ------         
desktop     casey@192.168.1.100      till-ready     
laptop      casey@192.168.1.101      till-ready     
teamdev     deploy@dev.company.com   deployed       

SSH aliases: till-<name>
Example: ssh till-desktop
```

### View Specific Host

```bash
till host status desktop
```

Output:
```
Host: desktop
  User: casey
  Host: 192.168.1.100
  Port: 22
  Status: till-ready
  SSH alias: till-desktop
  Last sync: 2025-01-05 15:30:00
  Installations:
    - primary.tekton.development.us
    - coder-a.tekton.development.us
```

## Troubleshooting

### Connection Failed

```
✗ Connection failed
Check:
  1. SSH key is in remote authorized_keys
  2. Host is reachable
  3. SSH service is running
```

**Solutions**:
1. Verify the public key was added correctly:
   ```bash
   ssh user@host 'cat ~/.ssh/authorized_keys | grep till-federation'
   ```

2. Test basic SSH connectivity:
   ```bash
   ssh -v user@host
   ```

3. Check firewall settings allow SSH (port 22)

### Till Not Found on Remote

```
⚠ Till not found on remote host
  Run: till host setup myserver
```

**Solution**: Run the setup command to install Till:
```bash
till host setup myserver
```

### Permission Denied During Setup

```
Error: Failed to build Till
You may need to install build tools on the remote host
```

**Solutions**:
1. Install build tools on remote:
   ```bash
   # Ubuntu/Debian
   ssh till-myserver 'sudo apt-get install build-essential git'
   
   # RHEL/CentOS
   ssh till-myserver 'sudo yum groupinstall "Development Tools"'
   
   # macOS
   ssh till-myserver 'xcode-select --install'
   ```

2. Then retry setup:
   ```bash
   till host setup myserver
   ```

### Deployment Fails

```
Error: Deployment failed
```

**Check**:
1. Sufficient disk space on remote:
   ```bash
   ssh till-myserver 'df -h'
   ```

2. Git is installed and configured:
   ```bash
   ssh till-myserver 'git --version'
   ```

3. Network connectivity to GitHub:
   ```bash
   ssh till-myserver 'git ls-remote https://github.com/tillfed/till.git'
   ```

## Advanced Usage

### Custom SSH Options

Add custom SSH options by editing `~/.till/ssh/config`:

```ssh
Host till-myserver
    HostName server.example.com
    User casey
    Port 2222
    IdentityFile ~/.till/ssh/till_federation_ed25519
    IdentitiesOnly yes
    # Custom options:
    Compression yes
    ProxyJump jumphost.example.com
```

### Parallel Operations

Deploy to multiple hosts simultaneously:

```bash
# In separate terminals or with GNU parallel
till host deploy desktop &
till host deploy laptop &
wait
```

### Backup Before Deploy

Create a backup before deploying:

```bash
ssh till-myserver 'cd ~/projects/github && tar -czf tekton-backup.tar.gz Tekton/'
till host deploy myserver
```

### Health Checks

Check if Tekton is running on remote hosts:

```bash
for host in desktop laptop teamdev; do
  echo "Checking $host..."
  ssh till-$host 'cd ~/projects/github/Tekton && tekton status'
done
```

## Security Best Practices

### Key Management

1. **Protect your private key**:
   ```bash
   chmod 600 ~/.till/ssh/till_federation_ed25519
   ```

2. **Use ssh-agent** for convenience:
   ```bash
   ssh-add ~/.till/ssh/till_federation_ed25519
   ```

3. **Rotate keys annually**:
   ```bash
   # Generate new key
   mv ~/.till/ssh/till_federation_ed25519 ~/.till/ssh/till_federation_ed25519.old
   till host init
   
   # Update authorized_keys on all hosts
   ```

### Access Control

1. **Limit SSH access** on production servers:
   ```bash
   # In remote's ~/.ssh/authorized_keys, add restrictions:
   command="till-only",no-port-forwarding,no-X11-forwarding ssh-ed25519 AAAA...
   ```

2. **Use dedicated deployment user**:
   ```bash
   # On remote host
   sudo useradd -m -s /bin/bash deploy
   sudo -u deploy mkdir -p /home/deploy/.ssh
   # Add Till's key to deploy user
   ```

3. **Monitor access logs**:
   ```bash
   ssh till-myserver 'sudo grep till /var/log/auth.log'
   ```

## Integration with CI/CD

### GitHub Actions

```yaml
name: Deploy Tekton
on:
  push:
    branches: [main]

jobs:
  deploy:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      
      - name: Setup Till
        run: |
          git clone https://github.com/tillfed/till.git
          cd till && make
          
      - name: Configure SSH
        run: |
          mkdir -p ~/.till/ssh
          echo "${{ secrets.TILL_SSH_KEY }}" > ~/.till/ssh/till_federation_ed25519
          chmod 600 ~/.till/ssh/till_federation_ed25519
          
      - name: Deploy to Production
        run: |
          ./till/till host add prod ${{ secrets.PROD_HOST }}
          ./till/till host deploy prod
```

### Local Automation

Create a deployment script:

```bash
#!/bin/bash
# deploy-all.sh

set -e

echo "Deploying to all hosts..."

for host in $(till host status | grep till-ready | awk '{print $1}'); do
  echo "Deploying to $host..."
  till host deploy $host
done

echo "All deployments complete!"
```

## FAQ

### Q: Can I manage hosts behind NAT/firewall?

A: Yes, use SSH ProxyJump or reverse SSH tunnels:
```bash
# Edit ~/.till/ssh/config
Host till-behind-nat
    ProxyJump public-server.com
    HostName 192.168.1.100
    User casey
```

### Q: How do I remove a host?

A: Currently manual removal is required:
1. Edit `~/.till/hosts-local.json` to remove the host entry
2. Remove the Host section from `~/.till/ssh/config`

### Q: Can I use existing SSH keys instead of generating new ones?

A: Yes, but not recommended. If needed:
```bash
ln -s ~/.ssh/id_ed25519 ~/.till/ssh/till_federation_ed25519
ln -s ~/.ssh/id_ed25519.pub ~/.till/ssh/till_federation_ed25519.pub
```

### Q: How do I handle different Tekton configurations per host?

A: Deploy different installations:
```bash
# Development gets Coder-A
till host deploy dev-server coder-a.tekton.development.us

# Production gets primary
till host deploy prod-server primary.tekton.development.us
```

### Q: What if my SSH connection is slow?

A: Enable compression and connection multiplexing:
```bash
# Already enabled by default in Till's SSH config
# For additional optimization, edit ~/.till/ssh/config:
Host till-slowhost
    Compression yes
    CompressionLevel 9
```

## Next Steps

1. Set up your first remote host
2. Deploy Tekton to multiple machines
3. Configure automatic daily synchronization
4. Explore federation across your infrastructure
5. Integrate with your CI/CD pipeline

For more information:
- [Host Management Architecture](../design/host-management.md)
- [Command Reference](../reference/commands.md#host-management)
- [Federation Guide](federation-guide.md)