#!/bin/bash
#
# Update all references from ~/.till to use get_till_dir()
#

# This script shows what needs to be changed
# For now, let's just update the SSH config path references

TILL_DIR="$(cd "$(dirname "$0")/.." && pwd)"

echo "Updating paths in Till source code..."
echo "Till directory: $TILL_DIR"

# The SSH config now needs to use relative paths since it's in till/.till/ssh/
sed -i.bak 's|IdentityFile ~/.till/ssh/|IdentityFile |g' "$TILL_DIR/.till/ssh/config"
sed -i.bak 's|UserKnownHostsFile ~/.till/ssh/|UserKnownHostsFile |g' "$TILL_DIR/.till/ssh/config"
sed -i.bak 's|ControlPath ~/.till/ssh/|ControlPath |g' "$TILL_DIR/.till/ssh/config"

# Use absolute paths for now
TILL_CONFIG_DIR="$TILL_DIR/.till"
sed -i.bak "s|IdentityFile |IdentityFile $TILL_CONFIG_DIR/ssh/|g" "$TILL_DIR/.till/ssh/config"
sed -i.bak "s|UserKnownHostsFile |UserKnownHostsFile $TILL_CONFIG_DIR/ssh/|g" "$TILL_DIR/.till/ssh/config"
sed -i.bak "s|ControlPath |ControlPath $TILL_CONFIG_DIR/ssh/|g" "$TILL_DIR/.till/ssh/config"

echo "Updated SSH config to use absolute paths"

# Clean up backup files
rm -f "$TILL_DIR/.till/ssh/config.bak"

echo "Path updates complete"