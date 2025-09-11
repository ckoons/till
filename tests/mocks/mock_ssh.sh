#!/bin/bash
#
# Mock SSH command for testing Till
# Simulates SSH behavior without actual network connections
#

# Parse arguments
USER=""
HOST=""
PORT=22
COMMAND=""

while [[ $# -gt 0 ]]; do
    case $1 in
        -p)
            PORT="$2"
            shift 2
            ;;
        -o)
            # Ignore SSH options
            shift 2
            ;;
        *@*)
            # User@host format
            USER="${1%@*}"
            HOST="${1#*@}"
            shift
            ;;
        *)
            # Remaining arguments are the command
            COMMAND="$*"
            break
            ;;
    esac
done

# Check mock configuration
MOCK_CONFIG="${TILL_MOCK_DIR:-/tmp/till-mocks}/ssh_responses.txt"

# Default responses for common commands
case "$COMMAND" in
    "echo TILL_TEST_SUCCESS")
        echo "TILL_TEST_SUCCESS"
        exit 0
        ;;
    "uname -s")
        echo "Linux"
        exit 0
        ;;
    "which tekton")
        if [[ "$HOST" == "mock-with-tekton" ]]; then
            echo "/usr/local/bin/tekton"
            exit 0
        else
            exit 1
        fi
        ;;
    "ls /opt/Tekton")
        if [[ "$HOST" == "mock-with-tekton" ]]; then
            echo "bin"
            echo "config"
            echo "logs"
            exit 0
        else
            echo "ls: /opt/Tekton: No such file or directory" >&2
            exit 1
        fi
        ;;
    *)
        # Check for custom responses in config file
        if [[ -f "$MOCK_CONFIG" ]]; then
            RESPONSE=$(grep "^$HOST:$COMMAND=" "$MOCK_CONFIG" | cut -d= -f2-)
            if [[ -n "$RESPONSE" ]]; then
                echo "$RESPONSE"
                exit 0
            fi
        fi
        
        # Default: command not found
        echo "bash: ${COMMAND%% *}: command not found" >&2
        exit 127
        ;;
esac