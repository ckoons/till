# Till Installation Guide

## Prerequisites

Before installing Till and Tekton, ensure you have:

1. **Git** - For cloning repositories
2. **GitHub Access** - Authenticated via SSH or gh CLI
3. **C Compiler** - gcc or clang (for building Till)
4. **Make** - For building Till
5. **Available Ports** - 8000-8099 and 45000-45099 (or custom ranges)

### Verify Prerequisites

```bash
# Check Git
git --version

# Check GitHub authentication
gh auth status
# OR
ssh -T git@github.com

# Check compiler
gcc --version

# Check make
make --version

# Check port availability
lsof -i :8000-8099
```

## Installing Till

### Step 1: Clone Till Repository

```bash
cd ~/projects  # or your preferred directory
git clone https://github.com/ckoons/till.git
cd till
```

### Step 2: Build Till

```bash
make
```

Expected output:
```
Compiling till.c...
Compiling till_install.c...
Compiling cJSON.c...
Linking ./till...
Build complete: ./till
Run './till --help' for usage information
```

### Step 3: Verify Installation

```bash
./till --version
# Output: Till version 1.0.0

./till --help
# Shows command usage
```

### Step 4: Installation Options

#### Option A: System-wide Installation (requires sudo)

```bash
sudo make install
# Installs to /usr/local/bin/till

# Now use from anywhere:
till --version
```

#### Option B: User-local Installation (no sudo required)

```bash
make install-user
# Installs to ~/.local/bin/till

# Add to your PATH if not already present:
echo 'export PATH="$HOME/.local/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc

# Now use from anywhere:
till --version
```

## Installing Your First Tekton

### Option 1: Solo Installation (Simplest)

For a private, non-federated Tekton:

```bash
till install tekton --mode solo
```

This will:
1. Clone Tekton to `./Tekton`
2. Create `.env.local` with default ports
3. Set up for local use only

### Option 2: Federation-Ready Installation

For a Tekton that can join the federation:

```bash
till install tekton \
  --name alice.development.us \
  --mode member
```

Replace `alice.development.us` with your chosen name following the pattern:
`name.category.geography`

### Option 3: Custom Installation

For specific requirements:

```bash
till install tekton \
  --name alice.development.us \
  --mode observer \
  --path /opt/tekton \
  --port-base 9000 \
  --ai-port-base 46000
```

## Installing Development Environments

After installing a primary Tekton, add Coder environments for parallel development:

### Coder-A Environment

```bash
till install tekton --mode coder-a
```

Automatically:
- Names it: `[primary].coder-a`
- Installs to: `./Coder-A`
- Assigns ports: 8100-8199, 45100-45199

### Coder-B Environment

```bash
till install tekton --mode coder-b
```

Automatically:
- Names it: `[primary].coder-b`
- Installs to: `./Coder-B`
- Assigns ports: 8200-8299, 45200-45299

### Continue with Coder-C, D, E as needed...

## Post-Installation Setup

### 1. Navigate to Installation

```bash
cd Tekton  # or Coder-A, Coder-B, etc.
```

### 2. Review Configuration

```bash
cat .env.local
# Verify TEKTON_ROOT and ports are correct
```

### 3. Start Tekton

```bash
tekton start
```

### 4. Verify Services

```bash
tekton status
```

## Multiple Installations Example

Complete setup for a development environment:

```bash
# 1. Install Till
git clone https://github.com/ckoons/till.git
cd till
make

# 2. Install primary Tekton
./till install tekton --name alice.development.us --mode member

# 3. Install Coder-A for development
./till install tekton --mode coder-a

# 4. Install Coder-B for testing
./till install tekton --mode coder-b

# 5. Check installations
./till status

# Directory structure:
# ./
# ├── till/
# ├── Tekton/       (alice.development.us)
# ├── Coder-A/      (alice.development.us.coder-a)
# └── Coder-B/      (alice.development.us.coder-b)
```

## Federation Setup

### Join Federation as Observer

```bash
till federate init --mode observer --name alice.development.us
```

### Join Federation as Member

```bash
till federate init --mode member --name alice.development.us
```

This will:
1. Generate keypair
2. Create registration branch
3. Push to GitHub for processing

## Troubleshooting Installation

### Problem: Git Clone Fails

```
Error: Git clone failed (exit code 128)
```

**Solutions:**
1. Check GitHub authentication:
```bash
gh auth login
```

2. Verify SSH keys:
```bash
ssh-add -l
ssh -T git@github.com
```

### Problem: Directory Already Exists

```
Error: Directory Tekton already exists
```

**Solutions:**
1. Use a different path:
```bash
till install tekton --path Tekton2
```

2. Remove existing directory (careful!):
```bash
rm -rf Tekton
```

### Problem: Port Conflicts

```
Error: Port 8000 already in use
```

**Solutions:**
1. Find what's using the port:
```bash
lsof -i :8000
```

2. Use different ports:
```bash
till install tekton --port-base 9000 --ai-port-base 46000
```

### Problem: No Primary Tekton

```
Error: No primary Tekton found. Install primary first.
```

**Solution:** Install a primary Tekton before Coder environments:
```bash
till install tekton --name alice.development.us --mode solo
```

## Uninstalling

### Remove a Tekton Installation

1. Stop Tekton services:
```bash
cd Tekton
tekton stop
```

2. Remove directory:
```bash
cd ..
rm -rf Tekton
```

3. Update Till registry:
```bash
# TODO: Implement till uninstall command
```

### Uninstall Till

```bash
cd till
sudo make uninstall  # If installed system-wide
cd ..
rm -rf till
```

## Configuration Files

### Generated .env.local

Till creates `.env.local` in each Tekton directory:

```bash
# Tekton Root (for this Tekton environment)
TEKTON_ROOT=/Users/alice/projects/Tekton

# Tekton Registry Name
TEKTON_REGISTRY_NAME=alice.development.us

# Base Configuration
TEKTON_PORT_BASE=8000
TEKTON_AI_PORT_BASE=45000

# Component Ports
ENGRAM_PORT=8000
HERMES_PORT=8001
... (all ports explicitly set)
```

### Till Private Configuration

Stored in `.till/tekton/federation/till-private.json`:

```json
{
  "primary_tekton": "alice.development.us",
  "installations": {
    "alice.development.us": {
      "path": "/Users/alice/projects/Tekton",
      "port_base": 8000,
      "mode": "member"
    }
  }
}
```

## Next Steps

1. **Start Using Tekton**: `cd Tekton && tekton start`
2. **Join Federation**: `till federate init`
3. **Add Components**: Use Till to manage updates
4. **Monitor Status**: `till status` and `tekton status`
5. **Develop**: Use Coder environments for parallel work