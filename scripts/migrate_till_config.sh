#!/bin/bash
#
# Migrate Till configuration from ~/.till to till/.till
#

set -e

# Get Till directory
TILL_DIR="$(cd "$(dirname "$0")/.." && pwd)"
TILL_CONFIG_DIR="$TILL_DIR/.till"

echo "Migrating Till configuration..."
echo "Till directory: $TILL_DIR"
echo "Config directory: $TILL_CONFIG_DIR"

# Create new structure
mkdir -p "$TILL_CONFIG_DIR/ssh/control"
mkdir -p "$TILL_CONFIG_DIR/tekton"

# Copy from ~/.till if it exists
if [ -d "$HOME/.till" ]; then
    echo "Found existing ~/.till directory"
    
    # Copy SSH config
    if [ -d "$HOME/.till/ssh" ]; then
        echo "Copying SSH configuration..."
        cp -r "$HOME/.till/ssh/"* "$TILL_CONFIG_DIR/ssh/" 2>/dev/null || true
    fi
    
    # Copy hosts file
    if [ -f "$HOME/.till/hosts-local.json" ]; then
        echo "Copying hosts configuration..."
        cp "$HOME/.till/hosts-local.json" "$TILL_CONFIG_DIR/"
    fi
    
    echo "Migration complete!"
    echo ""
    echo "Old configuration remains at ~/.till"
    echo "You can remove it with: rm -rf ~/.till"
else
    echo "No existing ~/.till directory found"
fi

# Create symlinks in Tekton directories
echo ""
echo "Creating symlinks in Tekton directories..."

for dir in "$TILL_DIR"/../*/; do
    if [ -d "$dir" ] && [ "$dir" != "$TILL_DIR/" ]; then
        name=$(basename "$dir")
        
        # Skip if not a Tekton-like directory
        if [[ "$name" == "Tekton" ]] || [[ "$name" == "Coder-"* ]]; then
            echo "  Linking $name/.till -> ../till/.till"
            
            # Remove existing .till if it's not a symlink
            if [ -e "$dir/.till" ] && [ ! -L "$dir/.till" ]; then
                echo "    Removing existing $dir/.till"
                rm -rf "$dir/.till"
            fi
            
            # Create symlink
            ln -sf "../till/.till" "$dir/.till"
        fi
    fi
done

echo ""
echo "Configuration structure:"
echo "  $TILL_CONFIG_DIR/"
echo "  ├── ssh/              # SSH keys and config"
echo "  ├── tekton/           # Discovery data"
echo "  └── hosts-local.json  # Host database"
echo ""
echo "Tekton directories now use symlinks to share this configuration."