#!/bin/bash
#
# Till Bootstrap Script
# Installs Till and prepares system for Tekton installations
#
# Usage:
#   curl -sSL https://raw.githubusercontent.com/ckoons/till/main/scripts/bootstrap.sh | bash
#   or
#   ssh user@host 'bash -s' < bootstrap.sh
#

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Configuration
# Use public HTTPS URL that doesn't require authentication for cloning
TILL_REPO="https://github.com/ckoons/till.git"
TILL_REPO_SSH="git@github.com:ckoons/till.git"

# Functions
log() {
    echo -e "${GREEN}[TILL]${NC} $1"
}

error() {
    echo -e "${RED}[ERROR]${NC} $1" >&2
}

warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

check_command() {
    if ! command -v $1 &> /dev/null; then
        return 1
    fi
    return 0
}

# Main installation
main() {
    log "Starting Till bootstrap installation..."
    
    # 1. Check prerequisites
    log "Checking prerequisites..."
    
    local missing_deps=""
    
    if ! check_command git; then
        missing_deps="$missing_deps git"
    fi
    
    if ! check_command make; then
        missing_deps="$missing_deps make"
    fi
    
    if ! check_command gcc && ! check_command clang; then
        missing_deps="$missing_deps gcc/clang"
    fi
    
    if [ -n "$missing_deps" ]; then
        error "Missing required dependencies:$missing_deps"
        error "Please install these tools and try again."
        exit 1
    fi
    
    log "Prerequisites satisfied"
    
    # 2. Determine installation root
    if [ -d "$HOME/projects/github" ]; then
        INSTALL_ROOT="$HOME/projects/github"
        log "Using existing directory structure: $INSTALL_ROOT"
    else
        INSTALL_ROOT="$HOME/.till"
        log "Creating Till directory structure: $INSTALL_ROOT"
        mkdir -p "$INSTALL_ROOT"
    fi
    
    # 3. Clone or update Till repository
    TILL_DIR="$INSTALL_ROOT/till"
    
    if [ -d "$TILL_DIR/.git" ]; then
        log "Till repository exists, updating..."
        cd "$TILL_DIR"
        
        # Stash any local changes
        if ! git diff --quiet || ! git diff --cached --quiet; then
            warning "Local changes detected, stashing..."
            git stash push -m "Till bootstrap update $(date +%Y%m%d-%H%M%S)"
        fi
        
        # Pull latest changes
        if ! git pull --ff-only origin main 2>/dev/null; then
            if ! git pull --ff-only origin master 2>/dev/null; then
                warning "Could not pull updates, using existing version"
                # Don't fail, just continue with existing code
            fi
        fi
        
        log "Till repository updated"
    else
        # Check if directory exists but is not a git repo
        if [ -d "$TILL_DIR" ]; then
            log "Till directory exists but is not a git repository"
            log "Converting to git repository..."
            cd "$TILL_DIR"
            
            # Initialize as git repo and add remote
            git init
            git remote add origin "$TILL_REPO"
            
            # Try to fetch and setup
            if git fetch origin 2>/dev/null; then
                # Reset to match remote
                git reset --hard origin/main 2>/dev/null || git reset --hard origin/master 2>/dev/null
                log "Converted to git repository and synced with remote"
            else
                warning "Could not fetch from remote, will use existing files"
            fi
        else
            log "Cloning Till repository..."
            cd "$INSTALL_ROOT"
            
            # Try SSH first, fall back to HTTPS
            if git clone "$TILL_REPO_SSH" till 2>/dev/null; then
                log "Cloned via SSH"
            elif git clone "$TILL_REPO" till 2>/dev/null; then
                log "Cloned via HTTPS"
            else
                error "Failed to clone Till repository"
                error "Please check your internet connection and GitHub access"
                exit 1
            fi
            
            cd "$TILL_DIR"
            log "Till repository cloned successfully"
        fi
    fi
    
    # 4. Build Till
    log "Building Till..."
    
    # Clean build to ensure fresh start
    make clean >/dev/null 2>&1 || true
    
    if ! make; then
        error "Failed to build Till"
        error "Please check the build output above"
        exit 1
    fi
    
    log "Till built successfully"
    
    # 5. Install Till binary
    log "Installing Till..."
    
    # Create ~/.local/bin if it doesn't exist
    mkdir -p "$HOME/.local/bin"
    
    # Copy binary
    cp till "$HOME/.local/bin/till"
    chmod +x "$HOME/.local/bin/till"
    
    log "Till installed to ~/.local/bin/till"
    
    # 6. Set up Till configuration directory
    log "Setting up Till configuration..."
    
    mkdir -p "$HOME/.till/ssh/control"
    mkdir -p "$HOME/.till/config"
    
    # Create initial config if it doesn't exist
    if [ ! -f "$HOME/.till/config.json" ]; then
        cat > "$HOME/.till/config.json" << EOF
{
    "version": "1.0.0",
    "install_root": "$INSTALL_ROOT",
    "installed": "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
}
EOF
        log "Created initial configuration"
    else
        log "Configuration already exists"
    fi
    
    # 7. Update PATH in shell configuration
    log "Configuring PATH..."
    
    PATH_LINE='export PATH="$HOME/.local/bin:$PATH"'
    
    # Function to add PATH to a shell rc file
    add_to_rc() {
        local rc_file="$1"
        if [ -f "$rc_file" ]; then
            if ! grep -q "/.local/bin" "$rc_file"; then
                echo "" >> "$rc_file"
                echo "# Added by Till bootstrap" >> "$rc_file"
                echo "$PATH_LINE" >> "$rc_file"
                log "Added PATH to $rc_file"
            else
                log "PATH already configured in $rc_file"
            fi
        fi
    }
    
    # Add to appropriate shell config files
    add_to_rc "$HOME/.bashrc"
    add_to_rc "$HOME/.zshrc"
    add_to_rc "$HOME/.profile"
    
    # 8. Generate SSH keys if needed
    if [ ! -f "$HOME/.till/ssh/till_federation_ed25519" ]; then
        log "Generating Till SSH keys..."
        ssh-keygen -t ed25519 -f "$HOME/.till/ssh/till_federation_ed25519" -N "" -C "till@$(hostname)"
        log "SSH keys generated"
    else
        log "SSH keys already exist"
    fi
    
    # 9. Test installation
    log "Testing Till installation..."
    
    # Export PATH for current session
    export PATH="$HOME/.local/bin:$PATH"
    
    if till --version >/dev/null 2>&1; then
        TILL_VERSION=$(till --version | head -n1)
        log "Till installation successful: $TILL_VERSION"
    elif "$HOME/.local/bin/till" --version >/dev/null 2>&1; then
        TILL_VERSION=$("$HOME/.local/bin/till" --version | head -n1)
        log "Till installation successful: $TILL_VERSION"
        warning "Till not in PATH yet. Please restart your shell or run:"
        warning "  export PATH=\"\$HOME/.local/bin:\$PATH\""
    else
        error "Till installation verification failed"
        exit 1
    fi
    
    # 10. Summary
    echo ""
    log "========================================="
    log "Till Bootstrap Complete!"
    log "========================================="
    log ""
    log "Installation root: $INSTALL_ROOT"
    log "Till location: $TILL_DIR"
    log "Binary location: $HOME/.local/bin/till"
    log "Config location: $HOME/.till/"
    log ""
    log "Next steps:"
    log "  1. Restart your shell or run: source ~/.bashrc"
    log "  2. Verify with: till --version"
    log "  3. Initialize: till init"
    log "  4. Install Tekton: till install tekton"
    log ""
    log "For remote hosts:"
    log "  till host add <name> <user@host>"
    log ""
}

# Run main function
main "$@"