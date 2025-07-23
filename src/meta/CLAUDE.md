# Metadata Service

This directory contains the metadata service implementation for 3FS, providing stateless file system metadata operations backed by FoundationDB for strong consistency and high availability.

## Directory Structure

### Core Service
- `service/` - Main metadata service implementation and operators
- `store/` - Metadata storage layer with file system operations
- `components/` - Supporting components (caching, allocation, session management)
- `event/` - Event system for metadata operations and scanning
- `base/` - Base configuration and types

## Key Components

### Service Layer (`service/`)
- **MetaServer.cc/h** - Main metadata server implementation
- **MetaOperator.cc/h** - Request processing and operation dispatch
- **MetaSerdeService.h** - Serialization service definitions
- **MockMeta.h** - Mock implementation for testing

### Storage Layer (`store/`)
Core file system metadata storage:
- **MetaStore.cc/h** - Main metadata storage interface
- **Inode.cc/h** - File/directory inode management
- **DirEntry.cc/h** - Directory entry operations
- **FileSession.cc/h** - File session tracking
- **PathResolve.cc/h** - Path resolution and traversal
- **Operation.h** - Operation type definitions
- **BatchContext.h** - Batch operation support
- **Idempotent.h** - Idempotent operation utilities

#### File System Operations (`store/ops/`)
- **BatchOperation.cc/h** - Batch metadata operations
- **GetRealPath.cc** - Real path resolution
- **HardLink.cc** - Hard link operations
- **List.cc** - Directory listing
- **LockDirectory.cc** - Directory locking
- **Mkdirs.cc** - Directory creation
- **Open.cc** - File open operations
- **PruneSession.cc** - Session cleanup
- **Remove.cc** - File/directory removal
- **Rename.cc** - File/directory renaming
- **SetAttr.cc/h** - Attribute setting
- **Stat.cc** - File/directory stat operations
- **StatFs.cc** - File system statistics
- **Symlink.cc** - Symbolic link operations

### Components (`components/`)
Supporting metadata service components:
- **AclCache.h** - Access control list caching
- **ChainAllocator.h** - Storage chain allocation
- **Distributor.cc/h** - Request distribution logic
- **FileHelper.cc/h** - File operation utilities
- **GcManager.cc/h** - Garbage collection management
- **InodeIdAllocator.cc/h** - Inode ID allocation
- **SessionManager.cc/h** - File session lifecycle management

### Event System (`event/`)
- **Event.cc/h** - Event processing framework
- **Scan.cc/h** - File system scanning operations

## Architecture

### Stateless Design
- Each metadata server is stateless and can handle any request
- All persistent state stored in FoundationDB
- Supports horizontal scaling and fault tolerance

### Transaction Support
- ACID transactions via FoundationDB
- Batch operations for performance
- Idempotent operation support

### Caching Strategy
- ACL caching for access control
- Session caching for open files
- Inode ID allocation caching

## Key Features

### File System Operations
- **Standard POSIX operations**: create, read, update, delete
- **Directory operations**: listing, traversal, locking
- **Extended attributes**: custom metadata support
- **Symbolic/hard links**: link management
- **Access control**: permission checking and enforcement

### Performance Optimizations
- **Batch operations**: Multiple operations in single transaction
- **Path caching**: Efficient path resolution
- **Session management**: Reduce metadata overhead for active files
- **Garbage collection**: Cleanup of orphaned metadata

### High Availability
- **Multi-server deployment**: Multiple metadata servers
- **Automatic failover**: Client-side server selection
- **Consistent state**: FoundationDB ensures consistency

## Configuration

Metadata service configuration in `meta_main.toml`:
- Server endpoints and ports
- FoundationDB connection settings
- Caching and performance parameters
- Logging and monitoring configuration

## Development Guidelines

### Adding New Operations
1. Define operation in `store/Operation.h`
2. Implement in appropriate `store/ops/` file
3. Add serialization support in `fbs/meta/`
4. Update service dispatcher in `MetaOperator.cc`
5. Add tests in `tests/meta/`

### Transaction Patterns
- Use batch operations for multiple related changes
- Implement idempotent operations where possible
- Handle FoundationDB retry logic appropriately
- Consider performance impact of transaction size

### Testing
- Unit tests for individual components
- Integration tests with FoundationDB
- Performance benchmarks for metadata operations