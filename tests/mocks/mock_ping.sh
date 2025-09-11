#!/bin/bash
#
# Mock ping command for testing Till
# Simulates ping behavior without actual network connections
#

# Parse arguments
HOST=""
COUNT=3
TIMEOUT=1

while [[ $# -gt 0 ]]; do
    case $1 in
        -c)
            COUNT="$2"
            shift 2
            ;;
        -W)
            TIMEOUT="$2"
            shift 2
            ;;
        -*)
            # Ignore other options
            shift
            ;;
        *)
            HOST="$1"
            shift
            ;;
    esac
done

# Check mock configuration
MOCK_CONFIG="${TILL_MOCK_DIR:-/tmp/till-mocks}/ping_responses.txt"

# Default responses for known hosts
case "$HOST" in
    "localhost"|"127.0.0.1")
        echo "PING $HOST: 64 bytes: icmp_seq=0 ttl=64 time=0.033 ms"
        exit 0
        ;;
    "mock-reachable"*)
        echo "PING $HOST: 64 bytes: icmp_seq=0 ttl=64 time=1.234 ms"
        exit 0
        ;;
    "mock-unreachable"*)
        echo "ping: cannot resolve $HOST: Unknown host" >&2
        exit 1
        ;;
    "mock-timeout"*)
        sleep "$TIMEOUT"
        echo "ping: $HOST: Request timeout" >&2
        exit 1
        ;;
    *)
        # Check for custom responses
        if [[ -f "$MOCK_CONFIG" ]]; then
            if grep -q "^$HOST=reachable" "$MOCK_CONFIG"; then
                echo "PING $HOST: 64 bytes: icmp_seq=0 ttl=64 time=10.0 ms"
                exit 0
            elif grep -q "^$HOST=unreachable" "$MOCK_CONFIG"; then
                echo "ping: $HOST: No route to host" >&2
                exit 1
            fi
        fi
        
        # Default: unreachable
        echo "ping: cannot resolve $HOST: Unknown host" >&2
        exit 1
        ;;
esac