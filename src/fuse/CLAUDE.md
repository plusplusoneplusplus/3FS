# FUSE Filesystem Interface

This directory contains the FUSE (Filesystem in Userspace) implementation for 3FS, providing POSIX-compliant filesystem access to applications through the standard filesystem interface.

## Directory Structure

### Core Components
- **FuseApplication.cc/h** - Main FUSE application and lifecycle management
- **FuseMainLoop.cc/h** - Main event loop and request processing
- **FuseOps.cc/h** - FUSE operation implementations (open, read, write, etc.)
- **FuseClients.cc/h** - Client connections to backend services
- **hf3fs_fuse.cpp** - FUSE entry point and initialization

### Configuration and Setup
- **FuseAppConfig.cc/h** - Application configuration management
- **FuseLauncherConfig.cc/h** - Launcher configuration
- **FuseConfigFetcher.cc/h** - Dynamic configuration fetching
- **FuseConfig.h** - Configuration structures
- **UserConfig.cc/h** - User-specific configuration

### I/O Optimization
- **IoRing.cc/h** - io_uring integration for high-performance I/O
- **IovTable.cc/h** - I/O vector table management
- **PioV.cc/h** - Parallel I/O vector operations

## Key Components

### FUSE Operations (`FuseOps.cc/h`)
Implementation of standard POSIX filesystem operations:
- **File operations**: open, read, write, close, fsync
- **Directory operations**: opendir, readdir, mkdir, rmdir
- **Metadata operations**: getattr, setattr, chmod, chown
- **Link operations**: link, unlink, symlink, readlink
- **Extended attributes**: getxattr, setxattr, listxattr, removexattr
- **Access control**: access, permission checking

### Application Framework (`FuseApplication.cc/h`)
- **Service initialization**: Setup client connections to meta, storage, mgmtd services
- **Configuration management**: Load and manage FUSE-specific configuration
- **Signal handling**: Graceful shutdown and signal processing
- **Error handling**: Convert 3FS errors to POSIX errno codes

### Client Management (`FuseClients.cc/h`)
Manages connections to backend services:
- **MetaClient**: For filesystem metadata operations
- **StorageClient**: For data read/write operations
- **MgmtdClient**: For cluster management and routing
- **Connection pooling**: Efficient connection reuse
- **Failover handling**: Automatic failover to backup servers

### High-Performance I/O
#### io_uring Integration (`IoRing.cc/h`)
- **Async I/O**: Non-blocking I/O operations using Linux io_uring
- **Batch operations**: Group multiple I/O operations for efficiency
- **Zero-copy**: Minimize data copying between user and kernel space
- **Completion handling**: Efficient completion event processing

#### I/O Vector Management (`IovTable.cc/h`, `PioV.cc/h`)
- **Scatter-gather I/O**: Support for vectored I/O operations
- **Memory management**: Efficient buffer allocation and reuse
- **Parallel I/O**: Concurrent I/O operations to multiple storage targets

## Architecture

### POSIX Compliance
- **Standard interface**: Full POSIX filesystem semantics
- **Error handling**: Proper errno codes for all error conditions
- **File locking**: Support for advisory and mandatory locking
- **Memory mapping**: Support for mmap operations

### Performance Optimizations
- **Metadata caching**: Cache frequently accessed metadata
- **Read-ahead**: Prefetch data for sequential access patterns
- **Write-behind**: Asynchronous write operations
- **Direct I/O**: Bypass page cache for large I/O operations

### Integration with 3FS Backend
- **Transparent access**: Applications see standard filesystem interface
- **Strong consistency**: All operations maintain 3FS consistency guarantees
- **High availability**: Automatic failover to available servers
- **Load balancing**: Distribute requests across multiple backend servers

## Key Features

### High-Performance I/O
- **io_uring support**: Latest Linux async I/O interface
- **RDMA integration**: High-speed network I/O to storage backend
- **Parallel operations**: Concurrent operations to multiple targets
- **Zero-copy paths**: Minimize data copying overhead

### POSIX Compatibility
- **Standard operations**: All standard POSIX filesystem operations
- **Extended attributes**: Support for custom metadata
- **File locking**: Both advisory and mandatory locking
- **Symbolic links**: Full symbolic link support

### Reliability
- **Error recovery**: Robust error handling and recovery
- **Connection management**: Automatic reconnection on failures
- **Consistency**: Strong consistency guarantees from backend
- **Monitoring**: Comprehensive logging and metrics

## Configuration

FUSE client configuration in `hf3fs_fuse_main.toml`:
- **Mount options**: Filesystem mount parameters
- **Backend servers**: Meta, storage, and mgmtd server endpoints
- **Performance tuning**: I/O parameters and caching settings
- **Security settings**: Authentication and authorization

## Usage

### Mounting the Filesystem
```bash
# Mount 3FS via FUSE
./hf3fs_fuse --config hf3fs_fuse_main.toml --mountpoint /mnt/3fs

# Mount with specific options
./hf3fs_fuse --config config.toml --mountpoint /mnt/3fs --allow-other
```

### Performance Tuning
- **I/O size**: Adjust I/O block sizes for workload
- **Concurrency**: Configure number of I/O threads
- **Caching**: Tune metadata and data caching parameters
- **Network**: Optimize network buffer sizes

## Development Guidelines

### Adding New FUSE Operations
1. Implement operation in `FuseOps.cc`
2. Add error handling and logging
3. Update configuration if needed
4. Add tests in `tests/fuse/`
5. Document performance characteristics

### Performance Optimization
- **Profile hot paths**: Use profiling tools to identify bottlenecks
- **Minimize system calls**: Batch operations where possible
- **Optimize data paths**: Reduce memory copies and context switches
- **Cache strategically**: Cache frequently accessed data and metadata

### Error Handling
- **Map 3FS errors**: Convert internal errors to appropriate errno codes
- **Graceful degradation**: Handle backend failures gracefully
- **Logging**: Comprehensive logging for debugging
- **Recovery**: Implement retry logic for transient failures

### Testing
- **POSIX compliance**: Test against POSIX filesystem test suites
- **Performance testing**: Benchmark I/O operations
- **Stress testing**: Test under high load and failure conditions
- **Integration testing**: Test with real applications