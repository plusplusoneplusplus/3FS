# 3FS Cluster Launch Script

This directory contains the configuration files and launch script for setting up a 3FS cluster with multiple storage nodes.

## Usage

### Basic Launch (5 Storage Nodes)
```bash
./launch_cluster.sh
```

### Dry Run (Generate configs without starting services)
```bash
./launch_cluster.sh --dry-run
```

## What the Script Does

1. **Detects the host IP address** automatically using multiple methods
2. **Generates configuration files** with the correct IP addresses and node-specific settings
3. **Starts the following services**:
   - mgmtd (Management service) on port 7000
   - meta (Metadata service) on port 8000  
   - 5 storage nodes on ports 8001-8005 (Serde) and 9001-9005 (Core)
4. **Registers all nodes** with the management service
5. **Monitors all processes** and handles cleanup on exit

## Storage Node Configuration

The script creates 5 storage nodes with the following configuration:

| Node | Node ID | Serde Port | Core Port | Storage Path |
|------|---------|------------|-----------|--------------|
| 1    | 10001   | 8001       | 9001      | /tmp/3fs_storage_test_node1 |
| 2    | 10002   | 8002       | 9002      | /tmp/3fs_storage_test_node2 |
| 3    | 10003   | 8003       | 9003      | /tmp/3fs_storage_test_node3 |
| 4    | 10004   | 8004       | 9004      | /tmp/3fs_storage_test_node4 |
| 5    | 10005   | 8005       | 9005      | /tmp/3fs_storage_test_node5 |

## Configuration Files

The script uses template configuration files with placeholders that are replaced at runtime:

- `__HOST_IP__` - Replaced with the detected host IP address
- `__STORAGE_NODE_ID__` - Replaced with the unique node ID (10001-10005)
- `__STORAGE_SERDE_PORT__` - Replaced with the Serde service port (8001-8005)
- `__STORAGE_CORE_PORT__` - Replaced with the Core service port (9001-9005)
- `__STORAGE_PATH__` - Replaced with the storage path for each node

## Customization

To change the number of storage nodes or base configurations, modify these variables at the top of the script:

```bash
NUM_STORAGE_NODES=5
STORAGE_BASE_NODE_ID=10001
STORAGE_BASE_SERDE_PORT=8001
STORAGE_BASE_CORE_PORT=9001
```

## Cleanup

The script automatically cleans up all processes and temporary files when terminated with Ctrl+C or when any process dies unexpectedly.

## Prerequisites

- Build the project first: `make -j$(nproc) meta_main` in the build directory
- Ensure all required binaries are available in `build/bin/`
- The `nc` (netcat) command must be available for port checking
