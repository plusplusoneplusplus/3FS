#!/bin/bash

# VS Code IP Configuration Update Script
# This script updates TOML config files with the current system IP address
# Similar to the functionality in launch_cluster.sh but for VS Code debugging

set -e

# Get the directory where this script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"

# Get the system's IP address dynamically (same logic as launch_cluster.sh)
get_ip_address() {
    # Method 1: Try to get IP from default route interface
    local ip=$(ip route get 8.8.8.8 2>/dev/null | grep -oP 'src \K\S+' | head -1)
    if [[ -n "$ip" && "$ip" != "127.0.0.1" ]]; then
        echo "$ip"
        return 0
    fi
    
    # Method 2: Get IP from hostname
    ip=$(hostname -I 2>/dev/null | awk '{print $1}')
    if [[ -n "$ip" && "$ip" != "127.0.0.1" ]]; then
        echo "$ip"
        return 0
    fi
    
    # Method 3: Try getting from common network interfaces
    for interface in eth0 ens33 ens160 enp0s3 wlan0; do
        ip=$(ip addr show "$interface" 2>/dev/null | grep -oP 'inet \K[\d.]+' | head -1)
        if [[ -n "$ip" && "$ip" != "127.0.0.1" ]]; then
            echo "$ip"
            return 0
        fi
    done
    
    # Fallback to localhost
    echo "127.0.0.1"
}

HOST_IP=$(get_ip_address)
echo "Detected IP address: $HOST_IP"

# Create build directory if it doesn't exist
mkdir -p "$BUILD_DIR"

# Create temp configs directory for VS Code debugging
TEMP_CONFIG_DIR="$BUILD_DIR/vscode_configs"
mkdir -p "$TEMP_CONFIG_DIR"

echo "Updating configuration files with IP: $HOST_IP"

# Copy and update all TOML config files
for config_file in "$SCRIPT_DIR"/*.toml; do
    if [[ -f "$config_file" ]]; then
        filename=$(basename "$config_file")
        temp_file="$TEMP_CONFIG_DIR/$filename"
        
        # Replace __HOST_IP__ placeholder with actual IP
        sed "s/__HOST_IP__/$HOST_IP/g" "$config_file" > "$temp_file"
        echo "Updated $filename with IP $HOST_IP"
    fi
done

echo "Configuration files updated in: $TEMP_CONFIG_DIR"
echo "VS Code can now use these updated config files for debugging"