# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

3FS (Fire-Flyer File System) is a high-performance distributed file system designed for AI training and inference workloads. It uses a disaggregated architecture combining thousands of SSDs and RDMA networks to provide shared storage with strong consistency via Chain Replication with Apportioned Queries (CRAQ).

## Build Commands

### Building the Project
```bash
# Configure build with Clang (recommended)
cmake -S . -B build -DCMAKE_CXX_COMPILER=clang++-14 -DCMAKE_C_COMPILER=clang-14 -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# Build with parallel jobs
cmake --build build -j 32
```

### Testing
```bash
# Run all tests
cd build && ctest

# Build and run specific test targets
make test_common -j8                                    # Build common tests
./tests/test_common                                     # Run all common tests
./tests/test_common --gtest_filter="TestCustomKv*"     # Run filtered tests

# Other test targets
make test_meta -j8 && ./tests/test_meta                # Meta service tests
make test_storage -j8 && ./tests/test_storage          # Storage service tests
make test_client -j8 && ./tests/test_client            # Client tests
```

### Formatting and Linting
```bash
# Format code (configured via CMake)
make clang-format

# Run clang-tidy
make clang-tidy
```

### Rust Components
```bash
# Build Rust workspace (chunk_engine, trash_cleaner, usrbio-sys)
cargo build --release

# Run Rust tests
cargo test
```

## Architecture Overview

### Core Components

1. **Cluster Manager (mgmtd/)** - Handles membership changes, distributes cluster configuration, and manages node registration/heartbeats
2. **Metadata Service (meta/)** - Stateless metadata operations backed by transactional key-value store (FoundationDB)
3. **Storage Service (storage/)** - Manages local SSDs, implements CRAQ for strong consistency, provides chunk store interface
4. **FUSE Client (fuse/)** - POSIX-compliant file system interface for applications
5. **Native Client (client/)** - High-performance zero-copy API for performance-critical applications

### Key Directories

- `src/client/` - Client-side components including CLI tools, meta/storage clients, and trash cleaner
- `src/meta/` - Metadata service implementation with file system operations
- `src/storage/` - Storage service with chunk management and CRAQ implementation
- `src/mgmtd/` - Cluster management daemon
- `src/fuse/` - FUSE filesystem implementation
- `src/common/` - Shared utilities, networking (RDMA/TCP), logging, monitoring, and serialization
- `src/lib/api/` - Public APIs including USRBIO (User Block I/O) interface
- `src/fdb/` - FoundationDB integration and hybrid KV engine

### Languages and Technologies

- **C++20** - Main implementation language with coroutines support
- **Rust** - High-performance components (chunk engine, trash cleaner)
- **CMake** - Primary build system
- **FoundationDB** - Transactional metadata storage
- **RDMA** - High-performance networking (InfiniBand/RoCE)
- **FUSE** - Filesystem in userspace interface

## Development Setup

### Prerequisites
- Clang 14+ (preferred) or GCC 10+
- CMake 3.12+
- Rust 1.75+ (recommended 1.85+)
- FoundationDB 7.1+
- libfuse 3.16.1+
- Various system libraries (see README.md)

### Configuration Files
Configuration files are in TOML format located in `configs/` directory:
- Service configurations: `meta_main.toml`, `storage_main.toml`, `mgmtd_main.toml`
- Launcher configurations: `*_launcher.toml` files
- Application configurations: `*_app.toml` files

### Testing Infrastructure
- Unit tests use Google Test framework
- Integration tests in `tests/` directory
- FUSE tests with Python scripts in `tests/fuse/`
- Performance benchmarks in `benchmarks/`

## Important Notes

- The system uses RDMA for high-performance networking - ensure proper RDMA setup
- Metadata operations are stateless and can connect to any metadata service
- Storage uses chunk-based replication with configurable chunk sizes
- FUSE client provides POSIX compatibility but native client offers better performance
- Background operations include garbage collection, chain management, and heartbeat monitoring