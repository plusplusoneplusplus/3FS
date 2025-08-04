# Storage Main Testing Configuration

This directory contains minimal testing configurations for the storage_main server, similar to the meta_main configurations.

## Files Created

1. **storage_main_launcher.toml** - Launcher-specific settings
2. **storage_main_app.toml** - Application-specific settings  
3. **storage_main.toml** - Main server configuration

## Key Configuration Differences from Production

### Network Configuration
- Uses TCP instead of RDMA for local testing
- StorageSerde service listens on port 8001 (vs 8000 for meta)
- Core service listens on port 9001 (vs 9000 for meta)
- Simplified thread pool with reduced thread counts

### Storage Configuration
- Uses local filesystem storage in `/tmp/3fs_storage_test`
- LevelDB with reduced cache sizes for testing
- Simplified target configuration
- Allows disk without UUID for local testing

### Logging
- Simple console logging instead of file logging
- Reduced to INFO level for clarity

## Running the Storage Server

You can run the storage server using the VS Code task:
- Press Ctrl+Shift+P and search for "Tasks: Run Task"
- Select "run-storage-main"

Or manually from the build directory:
```bash
./bin/storage_main \
  --launcher_cfg ../testing_configs/storage_main_launcher.toml \
  --app_cfg ../testing_configs/storage_main_app.toml \
  --cfg ../testing_configs/storage_main.toml
```

## Prerequisites

1. Build the storage_main binary: `make storage_main` in the build directory
2. Ensure mgmtd is running at 172.19.70.9:7000 (or update the address in configs)
3. The storage directory `/tmp/3fs_storage_test` will be created automatically

## Port Usage

- StorageSerde: 8001
- Core: 9001
- Meta (for reference): 8000, 9000

## Notes

- This is a minimal configuration for local testing only
- For production use, refer to the full configs in the `configs/` directory
- The storage server will connect to mgmtd for cluster coordination
