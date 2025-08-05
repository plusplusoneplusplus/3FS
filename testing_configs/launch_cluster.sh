#!/bin/bash

# 3FS Cluster Launch Script
# This script starts mgmtd, meta, and storage services in the background
# and registers the nodes. All processes are killed when the script is terminated.
#
# Usage: ./launch_cluster.sh [--dry-run]
#   --dry-run: Generate config files but don't start services
#
# Environment variables:
#   NUM_STORAGE_NODES: Number of storage nodes to create (default: 5)

set -e

# Parse command line arguments
DRY_RUN=false
if [[ "$1" == "--dry-run" ]]; then
    DRY_RUN=true
fi

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Get the directory where this script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"

# Get the system's IP address dynamically
# Try multiple methods to get the IP address
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

# Configuration
NUM_STORAGE_NODES=${NUM_STORAGE_NODES:-5}  # Can be overridden by environment variable
STORAGE_BASE_NODE_ID=10001
STORAGE_BASE_SERDE_PORT=8001
STORAGE_BASE_CORE_PORT=9001

HOST_IP=$(get_ip_address)

echo -e "${BLUE}3FS Cluster Launch Script${NC}"
echo -e "${BLUE}=========================${NC}"
echo "Project root: $PROJECT_ROOT"
echo "Build directory: $BUILD_DIR"
echo "Host IP address: $HOST_IP"
echo ""

# Check if build directory exists
if [ ! -d "$BUILD_DIR" ]; then
    echo -e "${RED}Error: Build directory not found at $BUILD_DIR${NC}"
    echo "Please build the project first."
    exit 1
fi

# Array to store background process PIDs
declare -a PIDS=()
declare -a STORAGE_PIDS=()
declare -a STORAGE_NODES=()

# Function to cleanup all processes
cleanup() {
    echo -e "\n${YELLOW}Shutting down cluster...${NC}"
    
    # Kill all background processes
    for pid in "${PIDS[@]}"; do
        if kill -0 "$pid" 2>/dev/null; then
            echo "Killing process $pid"
            kill "$pid" 2>/dev/null || true
        fi
    done
    
    # Wait a bit and force kill if necessary
    sleep 2
    for pid in "${PIDS[@]}"; do
        if kill -0 "$pid" 2>/dev/null; then
            echo "Force killing process $pid"
            kill -9 "$pid" 2>/dev/null || true
        fi
    done
    
    # Clean up temporary config files
    if [[ -n "$CONFIG_DIR" && -d "$CONFIG_DIR" ]]; then
        echo "Cleaning up temporary config files..."
        rm -rf "$CONFIG_DIR"
    fi
    
    echo -e "${GREEN}All processes terminated.${NC}"
    echo -e "${YELLOW}Service logs are available in: $BUILD_DIR/logs/${NC}"
    exit 0
}

# Set up signal handlers
trap cleanup SIGINT SIGTERM EXIT

# Function to wait for service to be ready
wait_for_service() {
    local service_name="$1"
    local port="$2"
    local max_attempts=30
    local attempt=1
    
    echo -n "Waiting for $service_name to be ready on port $port..."
    
    while [ $attempt -le $max_attempts ]; do
        if nc -z "$HOST_IP" "$port" 2>/dev/null; then
            echo -e " ${GREEN}Ready!${NC}"
            return 0
        fi
        echo -n "."
        sleep 1
        ((attempt++))
    done
    
    echo -e " ${RED}Failed to start after ${max_attempts}s${NC}"
    return 1
}

# Function to run admin_cli command
run_admin_cli() {
    local cmd="$1"
    echo "Running: $cmd"
    cd "$BUILD_DIR"
    eval "$cmd"
}

echo -e "${YELLOW}Starting services...${NC}"

# Create logs directory
mkdir -p "$BUILD_DIR/logs"

# Update configuration files with dynamic IP address
echo -e "${BLUE}Updating configuration files with IP: $HOST_IP${NC}"
update_config_files() {
    local temp_dir="$BUILD_DIR/temp_configs"
    mkdir -p "$temp_dir"
    
    # Copy config files to temp directory and update IP addresses
    for config_file in "$SCRIPT_DIR"/*.toml; do
        if [[ -f "$config_file" ]]; then
            local filename=$(basename "$config_file")
            local temp_file="$temp_dir/$filename"
            
            # Replace hardcoded IP with dynamic IP
            sed "s/__HOST_IP__/$HOST_IP/g" "$config_file" > "$temp_file"
            echo "Updated $filename with IP $HOST_IP" >&2
        fi
    done
    
    echo "$temp_dir"
}

# Function to create storage node config files
create_storage_config() {
    local node_num="$1"
    local node_id=$((STORAGE_BASE_NODE_ID + node_num - 1))
    local serde_port=$((STORAGE_BASE_SERDE_PORT + node_num - 1))
    local core_port=$((STORAGE_BASE_CORE_PORT + node_num - 1))
    local storage_path="/tmp/3fs_storage_test_node$node_num"
    
    # Create storage-specific config files
    local temp_dir="$BUILD_DIR/temp_configs"
    
    # App config
    sed -e "s/__STORAGE_NODE_ID__/$node_id/g" \
        "$temp_dir/storage_main_app.toml" > "$temp_dir/storage_main_app_node$node_num.toml"
    
    # Main config
    sed -e "s/__STORAGE_SERDE_PORT__/$serde_port/g" \
        -e "s/__STORAGE_CORE_PORT__/$core_port/g" \
        -e "s|__STORAGE_PATH__|$storage_path|g" \
        "$temp_dir/storage_main.toml" > "$temp_dir/storage_main_node$node_num.toml"
    
    # Launcher config (no changes needed, just copy)
    cp "$temp_dir/storage_main_launcher.toml" "$temp_dir/storage_main_launcher_node$node_num.toml"
    
    echo "Created config files for storage node $node_num (ID: $node_id, Serde Port: $serde_port, Core Port: $core_port, Path: $storage_path)" >&2
    
    # Store node info for later use
    STORAGE_NODES+=("$node_id:$serde_port:$core_port:$storage_path")
}

CONFIG_DIR=$(update_config_files)

# Create storage node configurations
echo -e "${BLUE}Creating storage node configurations...${NC}"
for ((i=1; i<=NUM_STORAGE_NODES; i++)); do
    create_storage_config $i
done

if [[ "$DRY_RUN" == "true" ]]; then
    echo -e "${GREEN}✓ Dry run completed! Configuration files generated in: $CONFIG_DIR${NC}"
    echo -e "${YELLOW}Storage nodes that would be created:${NC}"
    for ((i=1; i<=NUM_STORAGE_NODES; i++)); do
        node_info="${STORAGE_NODES[$((i-1))]}"
        IFS=':' read -r node_id serde_port core_port storage_path <<< "$node_info"
        echo "  - Storage node $i: ID=$node_id, Serde Port=$serde_port, Core Port=$core_port, Path=$storage_path"
    done
    echo ""
    echo -e "${BLUE}Sample generated config file (storage_main_app_node1.toml):${NC}"
    cat "$CONFIG_DIR/storage_main_app_node1.toml"
    echo ""
    echo -e "${BLUE}Sample generated config file (storage_main_node1.toml - excerpt):${NC}"
    head -n 20 "$CONFIG_DIR/storage_main_node1.toml"
    echo "..."
    # Don't run cleanup in dry-run mode
    trap - SIGINT SIGTERM EXIT
    exit 0
fi

# 1. Start mgmtd
echo -e "${BLUE}1. Starting mgmtd...${NC}"
cd "$BUILD_DIR"
./bin/mgmtd_main \
    --launcher_cfg "$CONFIG_DIR/mgmtd_main_launcher.toml" \
    --app-cfg "$CONFIG_DIR/mgmtd_main_app.toml" \
    --cfg "$CONFIG_DIR/mgmtd_main.toml" > "$BUILD_DIR/logs/mgmtd.log" 2>&1 &
MGMTD_PID=$!
PIDS+=($MGMTD_PID)
echo "Started mgmtd with PID: $MGMTD_PID (logs: $BUILD_DIR/logs/mgmtd.log)"

# Wait for mgmtd to be ready
wait_for_service "mgmtd" 7000

# 2. Start meta
echo -e "${BLUE}2. Starting meta...${NC}"
cd "$BUILD_DIR"
./bin/meta_main \
    --launcher_cfg "$CONFIG_DIR/meta_main_launcher.toml" \
    --app_cfg "$CONFIG_DIR/meta_main_app.toml" \
    --cfg "$CONFIG_DIR/meta_main.toml" > "$BUILD_DIR/logs/meta.log" 2>&1 &
META_PID=$!
PIDS+=($META_PID)
echo "Started meta with PID: $META_PID (logs: $BUILD_DIR/logs/meta.log)"

# Wait for meta to be ready
wait_for_service "meta" 8000

# 3. Register meta node
echo -e "${BLUE}3. Registering meta node...${NC}"
sleep 2  # Give meta a moment to fully initialize
run_admin_cli "./bin/admin_cli --config.log 'INFO' --config.client.force_use_tcp true --config.ib_devices.allow_no_usable_devices true --config.cluster_id \"stage\" --config.mgmtd_client.mgmtd_server_addresses '[\"TCP://${HOST_IP}:7000\"]' \"register-node\" 100 META"

# 4. Start storage nodes
echo -e "${BLUE}4. Starting $NUM_STORAGE_NODES storage nodes...${NC}"
for ((i=1; i<=NUM_STORAGE_NODES; i++)); do
    node_info="${STORAGE_NODES[$((i-1))]}"
    IFS=':' read -r node_id serde_port core_port storage_path <<< "$node_info"
    
    echo -e "${BLUE}4.$i. Starting storage node $i (ID: $node_id, Serde Port: $serde_port)...${NC}"
    
    # Create storage directory
    mkdir -p "$storage_path"
    
    cd "$BUILD_DIR"
    ./bin/storage_main \
        --launcher_cfg "$CONFIG_DIR/storage_main_launcher_node$i.toml" \
        --app_cfg "$CONFIG_DIR/storage_main_app_node$i.toml" \
        --cfg "$CONFIG_DIR/storage_main_node$i.toml" > "$BUILD_DIR/logs/storage_node$i.log" 2>&1 &
    STORAGE_PID=$!
    PIDS+=($STORAGE_PID)
    STORAGE_PIDS+=($STORAGE_PID)
    echo "Started storage node $i with PID: $STORAGE_PID (logs: $BUILD_DIR/logs/storage_node$i.log)"

    # Wait for storage to be ready
    wait_for_service "storage node $i" $serde_port
done

# 5. Register storage nodes
echo -e "${BLUE}5. Registering $NUM_STORAGE_NODES storage nodes...${NC}"
for ((i=1; i<=NUM_STORAGE_NODES; i++)); do
    node_info="${STORAGE_NODES[$((i-1))]}"
    IFS=':' read -r node_id serde_port core_port storage_path <<< "$node_info"
    
    echo -e "${BLUE}5.$i. Registering storage node $i (ID: $node_id)...${NC}"
    sleep 1  # Give storage a moment to fully initialize
    run_admin_cli "./bin/admin_cli --config.log 'INFO' --config.client.force_use_tcp true --config.ib_devices.allow_no_usable_devices true --config.cluster_id \"stage\" --config.mgmtd_client.mgmtd_server_addresses '[\"TCP://${HOST_IP}:7000\"]' \"register-node\" $node_id STORAGE"
done

# 6. Verify cluster status
echo -e "${BLUE}6. Checking cluster status...${NC}"
sleep 1
run_admin_cli "./bin/admin_cli --config.log 'INFO' --config.client.force_use_tcp true --config.ib_devices.allow_no_usable_devices true --config.cluster_id \"stage\" --config.mgmtd_client.mgmtd_server_addresses '[\"TCP://${HOST_IP}:7000\"]' \"list-nodes\""

# 7. Create root user
# echo -e "${BLUE}7. Creating root user...${NC}"
# sleep 1
# run_admin_cli "./bin/admin_cli --config.log 'INFO' --config.client.force_use_tcp true --config.ib_devices.allow_no_usable_devices true --config.cluster_id \"stage\" --config.mgmtd_client.mgmtd_server_addresses '[\"TCP://${HOST_IP}:7000\"]' \"user-add --root --admin 0 root\""

echo ""
echo -e "${GREEN}✓ All services started successfully!${NC}"
echo ""
echo -e "${YELLOW}Cluster is running with the following processes:${NC}"
echo "  - mgmtd (PID: $MGMTD_PID) - Management service on port 7000"
echo "  - meta  (PID: $META_PID) - Metadata service on port 8000"
for ((i=1; i<=NUM_STORAGE_NODES; i++)); do
    node_info="${STORAGE_NODES[$((i-1))]}"
    IFS=':' read -r node_id serde_port core_port storage_path <<< "$node_info"
    storage_pid="${STORAGE_PIDS[$((i-1))]}"
    echo "  - storage node $i (PID: $storage_pid) - Storage service on port $serde_port (ID: $node_id)"
done
echo ""
echo -e "${YELLOW}To interact with the cluster, use:${NC}"
echo "  cd $BUILD_DIR"
echo "  ./bin/admin_cli --config.log 'INFO' --config.client.force_use_tcp true --config.ib_devices.allow_no_usable_devices true --config.cluster_id \"stage\" --config.mgmtd_client.mgmtd_server_addresses '[\"TCP://${HOST_IP}:7000\"]' \"list-nodes\""
echo ""
echo -e "${YELLOW}To view service logs:${NC}"
echo "  tail -f $BUILD_DIR/logs/mgmtd.log     # Management service logs"
echo "  tail -f $BUILD_DIR/logs/meta.log      # Metadata service logs"
echo "  tail -f $BUILD_DIR/logs/storage_node*.log  # Storage service logs"
echo ""
echo -e "${RED}Press Ctrl+C to stop all services and exit${NC}"

# Keep the script running and monitor processes
while true; do
    # Check if any process died unexpectedly
    for i in "${!PIDS[@]}"; do
        pid="${PIDS[$i]}"
        if ! kill -0 "$pid" 2>/dev/null; then
            echo -e "${RED}Process $pid died unexpectedly!${NC}"
            cleanup
        fi
    done
    sleep 5
done
