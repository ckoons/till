#!/bin/bash
#
# Mock nc (netcat) command for testing Till
# Simulates port connectivity testing
#

# Parse arguments
HOST=""
PORT=""
ZERO_SCAN=0
TIMEOUT=1

while [[ $# -gt 0 ]]; do
    case $1 in
        -z)
            ZERO_SCAN=1
            shift
            ;;
        -w)
            TIMEOUT="$2"
            shift 2
            ;;
        -*)
            # Ignore other options
            shift
            ;;
        *)
            if [[ -z "$HOST" ]]; then
                HOST="$1"
            elif [[ -z "$PORT" ]]; then
                PORT="$1"
            fi
            shift
            ;;
    esac
done

# Check mock configuration
MOCK_CONFIG="${TILL_MOCK_DIR:-/tmp/till-mocks}/port_responses.txt"

# Default responses for known host:port combinations
case "${HOST}:${PORT}" in
    "localhost:22"|"127.0.0.1:22")
        exit 0  # Port open
        ;;
    "mock-with-ssh:"*)
        if [[ "$PORT" == "22" ]]; then
            exit 0  # SSH port open
        else
            exit 1  # Other ports closed
        fi
        ;;
    "mock-no-ssh:"*)
        exit 1  # All ports closed
        ;;
    "mock-custom-port:"*)
        if [[ "$PORT" == "2222" ]]; then
            exit 0  # Custom SSH port open
        else
            exit 1  # Other ports closed
        fi
        ;;
    *)
        # Check for custom responses
        if [[ -f "$MOCK_CONFIG" ]]; then
            if grep -q "^${HOST}:${PORT}=open" "$MOCK_CONFIG"; then
                exit 0
            elif grep -q "^${HOST}:${PORT}=closed" "$MOCK_CONFIG"; then
                exit 1
            fi
        fi
        
        # Default: port closed
        exit 1
        ;;
esac