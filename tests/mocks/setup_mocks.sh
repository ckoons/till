#!/bin/bash
#
# Setup mock commands for Till testing
# This script creates a mock environment for testing without real network access
#

set -e

# Colors for output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Mock directory
MOCK_DIR="${TILL_MOCK_DIR:-/tmp/till-mocks}"
MOCK_BIN="$MOCK_DIR/bin"

echo -e "${YELLOW}Setting up Till mock environment...${NC}"

# Create mock directories
mkdir -p "$MOCK_BIN"
mkdir -p "$MOCK_DIR"

# Get the directory where this script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# Copy mock scripts
echo "Installing mock commands..."
cp "$SCRIPT_DIR/mock_ssh.sh" "$MOCK_BIN/ssh"
cp "$SCRIPT_DIR/mock_ping.sh" "$MOCK_BIN/ping"
cp "$SCRIPT_DIR/mock_nc.sh" "$MOCK_BIN/nc"

# Make them executable
chmod +x "$MOCK_BIN"/*

# Create default configuration files
echo "Creating mock configuration..."

# SSH responses
cat > "$MOCK_DIR/ssh_responses.txt" <<EOF
# Format: host:command=response
test-host-1:whoami=tilluser
test-host-1:pwd=/home/tilluser
test-host-2:ls /opt=Tekton
EOF

# Ping responses
cat > "$MOCK_DIR/ping_responses.txt" <<EOF
# Format: host=reachable|unreachable
test-host-1=reachable
test-host-2=reachable
bad-host=unreachable
EOF

# Port responses
cat > "$MOCK_DIR/port_responses.txt" <<EOF
# Format: host:port=open|closed
test-host-1:22=open
test-host-1:80=closed
test-host-2:22=open
test-host-2:2222=open
bad-host:22=closed
EOF

echo -e "${GREEN}Mock environment ready!${NC}"
echo ""
echo "To use mocks, add to PATH:"
echo "  export PATH=\"$MOCK_BIN:\$PATH\""
echo ""
echo "Mock configuration files:"
echo "  - $MOCK_DIR/ssh_responses.txt"
echo "  - $MOCK_DIR/ping_responses.txt"
echo "  - $MOCK_DIR/port_responses.txt"
echo ""
echo "To clean up mocks:"
echo "  rm -rf $MOCK_DIR"