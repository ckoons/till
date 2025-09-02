#!/bin/bash
# Setup Thunderbolt Bridge for M4-M2 direct connection

echo "Setting up Thunderbolt Bridge for direct wire connection..."

# Assign static IP to bridge0 on M4
ifconfig bridge0 inet 10.10.10.1 netmask 255.255.255.0

# Verify configuration
echo "Bridge configuration:"
ifconfig bridge0

echo ""
echo "On the M2 host, run:"
echo "  sudo ifconfig bridge0 inet 10.10.10.2 netmask 255.255.255.0"
echo ""
echo "Then add to /etc/hosts on both machines:"
echo "  10.10.10.1 m4.local m4"
echo "  10.10.10.2 m2.local m2"