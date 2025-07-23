# Storage Service

This directory contains the storage service implementation for 3FS, providing high-performance chunk-based storage with strong consistency through Chain Replication with Apportioned Queries (CRAQ).

## Directory Structure

### Core Service
- `service/` - Main storage service implementation and components
- `store/` - Storage engines and chunk management
- `worker/` - Background workers for various storage operations
- `aio/` - Asynchronous I/O operations and batch processing
- `sync/` - Data synchronization and replication
- `update/` - Update processing and coordination

### Rust Components
- `chunk_engine/` - High-performance Rust-based chunk storage engine

## Key Components

### Service Layer (`service/`)
- **StorageServer.cc/h** - Main storage server implementation
- **StorageService.cc/h** - Service interface and request handling
- **StorageOperator.cc/h** - Operation processing and dispatch
- **Components.cc/h** - Service component initialization and management
- **BufferPool.cc/h** - Memory buffer management for I/O operations
- **TargetMap.cc/h** - Storage target mapping and routing
- **ReliableUpdate.cc/h** - Reliable update processing with CRAQ
- **ReliableForwarding.cc/h** - Request forwarding in replication chains

### Storage Layer (`store/`)
Core storage implementation:
- **ChunkStore.cc/h** - Main chunk storage interface
- **ChunkEngine.cc/h** - High-performance chunk engine (Rust bridge)
- **ChunkFileStore.cc/h** - File-based chunk storage
- **ChunkFileView.cc/h** - Memory-mapped file views
- **ChunkMetaStore.cc/h** - Chunk metadata management
- **ChunkMetadata.cc/h** - Chunk metadata structures
- **ChunkReplica.cc/h** - Replication management
- **StorageTarget.cc/h** - Individual storage target implementation
- **StorageTargets.cc/h** - Storage target collection management
- **GlobalFileStore.cc/h** - Global file storage coordination
- **PhysicalConfig.h** - Physical storage configuration

### Background Workers (`worker/`)
- **AllocateWorker.cc/h** - Chunk allocation worker
- **CheckWorker.cc/h** - Consistency checking worker
- **DumpWorker.cc/h** - Data dump operations worker
- **PunchHoleWorker.cc/h** - Storage reclamation worker
- **SyncMetaKvWorker.cc/h** - Metadata synchronization worker

### Asynchronous I/O (`aio/`)
- **AioReadWorker.cc/h** - Async read operation worker
- **AioStatus.cc/h** - I/O operation status tracking
- **BatchReadJob.cc/h** - Batch read operation handling

### Synchronization (`sync/`)
- **ResyncWorker.cc/h** - Data resynchronization worker

### Update Processing (`update/`)
- **UpdateWorker.cc/h** - Update operation processing
- **UpdateJob.h** - Update job definitions

### Rust Chunk Engine (`chunk_engine/`)
High-performance storage engine written in Rust:
- **Core Engine** (`src/core/`) - Main engine implementation
- **Allocation** (`src/alloc/`) - Space allocation and management
- **Metadata** (`src/meta/`) - Chunk metadata storage
- **File Management** (`src/file/`) - File system integration
- **Utilities** (`src/utils/`) - Common utilities and helpers

## Architecture

### Chain Replication with Apportioned Queries (CRAQ)
- **Strong Consistency**: All writes go through chain replication
- **Read Optimization**: Reads can be served from any replica with version checking
- **Fault Tolerance**: Automatic chain reconstruction on failures
- **Performance**: Parallel reads while maintaining consistency

### Chunk-Based Storage
- **Fixed-size chunks**: Configurable chunk sizes (typically 4MB)
- **Content addressing**: Chunks identified by hash
- **Deduplication**: Automatic deduplication at chunk level
- **Compression**: Optional chunk compression

### Multi-tier Storage
- **SSD Primary**: High-performance SSD storage
- **Cold Storage**: Optional archival storage tiers
- **Memory Caching**: In-memory chunk caching

## Key Features

### High-Performance I/O
- **RDMA networking**: High-speed network I/O
- **Async operations**: Non-blocking I/O operations
- **Batch processing**: Batch read/write operations
- **Zero-copy**: Memory-efficient data transfer

### Reliability
- **Checksums**: Data integrity verification
- **Replication**: Configurable replication factors
- **Repair**: Automatic data repair on corruption
- **Backup**: Snapshot and backup capabilities

### Scalability
- **Horizontal scaling**: Add storage nodes dynamically
- **Load balancing**: Distribute load across storage targets
- **Sharding**: Data distribution across multiple nodes

## Configuration

Storage service configuration in `storage_main.toml`:
- Storage device configuration
- Replication settings
- Network and RDMA configuration
- Performance tuning parameters

## Development Guidelines

### Adding New Operations
1. Define operation in `fbs/storage/Service.h`
2. Implement in `StorageOperator.cc`
3. Add worker if background processing needed
4. Update replication logic if necessary
5. Add tests in `tests/storage/`

### Performance Considerations
- Use async I/O for all disk operations
- Minimize memory copies with zero-copy techniques
- Batch operations when possible
- Profile and optimize hot paths

### Rust Integration
- The chunk engine is implemented in Rust for performance
- C++ bridge provides seamless integration
- Build system handles cross-language compilation

### Testing
- Unit tests for individual components
- Integration tests with replication scenarios
- Performance benchmarks for I/O operations
- Fault injection tests for reliability