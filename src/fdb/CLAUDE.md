# FoundationDB Integration

This directory contains the FoundationDB integration layer for 3FS, providing transactional key-value storage with strong consistency guarantees for metadata and cluster state management.

## Directory Structure

### Core Components
- **FDB.cc/h** - Main FoundationDB client interface and initialization
- **FDBContext.cc/h** - FoundationDB context management and connection handling
- **FDBTransaction.cc/h** - Transaction wrapper with 3FS-specific functionality
- **FDBKVEngine.h** - Key-value engine implementation using FoundationDB
- **FDBConfig.h** - FoundationDB-specific configuration structures
- **FDBRetryStrategy.h** - Retry logic for FoundationDB operations
- **HybridKvEngine.cc/h** - Hybrid storage engine combining FoundationDB with local storage
- **HybridKvEngineConfig.h** - Configuration for hybrid engine
- **error_definitions.h** - FoundationDB error code definitions and mappings

## Key Components

### FoundationDB Client (`FDB.cc/h`)
Main interface to FoundationDB:
- **Network initialization**: Setup FoundationDB network threads
- **Database connections**: Manage database handles and connections
- **Client configuration**: Configure timeouts, retry policies, and performance parameters
- **Error handling**: Convert FoundationDB errors to 3FS status codes
- **Monitoring integration**: Metrics and logging for FoundationDB operations

### Context Management (`FDBContext.cc/h`)
- **Connection pooling**: Manage multiple database connections
- **Transaction context**: Track transaction state and lifecycle
- **Configuration management**: Handle FoundationDB cluster configuration
- **Resource cleanup**: Proper cleanup of FoundationDB resources

### Transaction Layer (`FDBTransaction.cc/h`)
Enhanced transaction wrapper providing:
- **Automatic retry**: Handle FoundationDB transaction conflicts
- **Timeout management**: Configurable transaction timeouts
- **Batch operations**: Efficient batch reads and writes
- **Conflict resolution**: Handle transaction conflicts gracefully
- **Consistency levels**: Support for different consistency requirements

### Key-Value Engine (`FDBKVEngine.h`)
Implementation of the 3FS key-value interface:
- **CRUD operations**: Create, read, update, delete operations
- **Range operations**: Efficient range queries and scans
- **Atomic operations**: Compare-and-swap and atomic counters
- **Snapshot isolation**: Consistent snapshot reads
- **Transaction support**: Multi-key transactions with ACID properties

### Hybrid Engine (`HybridKvEngine.cc/h`)
Advanced storage engine combining FoundationDB with local storage:
- **Hot/cold data separation**: Frequently accessed data in memory/local storage
- **Write-through caching**: Writes go to both FoundationDB and local cache
- **Read optimization**: Serve reads from fastest available source
- **Consistency management**: Maintain consistency across storage tiers
- **Background synchronization**: Sync data between storage layers

## Architecture

### Strong Consistency
- **ACID transactions**: Full ACID compliance for all operations
- **Snapshot isolation**: Consistent reads across all operations
- **Conflict detection**: Automatic detection and handling of write conflicts
- **Atomic operations**: Multi-key atomic operations

### High Availability
- **Automatic failover**: FoundationDB handles node failures transparently
- **Geographic distribution**: Support for multi-region deployments
- **No single point of failure**: Distributed architecture
- **Self-healing**: Automatic recovery from failures

### Scalability
- **Horizontal scaling**: Add capacity by adding nodes
- **Load distribution**: Automatic data and load distribution
- **Performance scaling**: Linear performance scaling with cluster size
- **Operational simplicity**: Minimal operational overhead

## Key Features

### Transaction Management
- **Automatic retry**: Handle transient failures and conflicts
- **Configurable timeouts**: Different timeout policies for different operations
- **Deadlock detection**: Automatic deadlock detection and resolution
- **Performance monitoring**: Track transaction performance metrics

### Data Operations
- **Efficient range queries**: Optimized for metadata scanning operations
- **Batch operations**: Reduce network overhead with batching
- **Large value support**: Handle large values with streaming
- **Key prefix operations**: Efficient operations on key ranges

### Performance Optimizations
- **Connection pooling**: Reuse connections across operations
- **Pipeline operations**: Pipeline multiple operations for efficiency
- **Locality optimization**: Use FoundationDB locality information
- **Memory management**: Efficient memory usage patterns

### Monitoring and Observability
- **Performance metrics**: Transaction latency, throughput, conflict rates
- **Error tracking**: Monitor and alert on error patterns
- **Resource usage**: Track FoundationDB resource consumption
- **Health monitoring**: Monitor cluster health and performance

## Configuration

FoundationDB configuration parameters:
- **Cluster file**: Path to FoundationDB cluster file
- **Transaction timeouts**: Configure timeout policies
- **Retry policies**: Configure retry behavior
- **Performance tuning**: Network threads, batch sizes, etc.

Example configuration in service TOML files:
```toml
[fdb]
cluster_file = "/etc/foundationdb/fdb.cluster"
transaction_timeout_ms = 10000
max_retry_delay_ms = 1000
```

## Usage Patterns

### Simple Key-Value Operations
```cpp
// Basic read/write operations
auto engine = std::make_shared<FDBKVEngine>(fdb_config);
auto transaction = engine->createTransaction();
transaction->set("key", "value");
auto value = transaction->get("key");
transaction->commit();
```

### Batch Operations
```cpp
// Efficient batch operations
auto batch_context = transaction->createBatchContext();
for (const auto& [key, value] : batch_data) {
    batch_context->set(key, value);
}
batch_context->commit();
```

### Range Queries
```cpp
// Scan key ranges efficiently
auto range = transaction->getRange("prefix/", "prefix0");
for (const auto& [key, value] : range) {
    // Process key-value pairs
}
```

## Development Guidelines

### Transaction Design
- **Keep transactions short**: Minimize transaction duration to reduce conflicts
- **Use appropriate isolation**: Choose the right consistency level for operations
- **Handle conflicts gracefully**: Implement proper retry logic
- **Batch related operations**: Group related operations in single transactions

### Error Handling
- **Distinguish error types**: Handle transient vs permanent errors differently
- **Implement retries**: Use exponential backoff for transient errors
- **Monitor error rates**: Track and alert on high error rates
- **Graceful degradation**: Provide fallback behavior where possible

### Performance Optimization
- **Use batch operations**: Reduce network overhead with batching
- **Optimize key design**: Design keys for efficient range operations
- **Monitor performance**: Track latency and throughput metrics
- **Profile operations**: Identify and optimize slow operations

### Testing
- **Unit tests**: Test individual components in isolation
- **Integration tests**: Test with actual FoundationDB cluster
- **Fault injection**: Test error handling and recovery
- **Performance tests**: Benchmark operations under load
- **Consistency tests**: Verify ACID properties and consistency

## Operational Considerations

### Deployment
- **Cluster sizing**: Size FoundationDB cluster appropriately
- **Network configuration**: Ensure proper network connectivity
- **Storage configuration**: Configure appropriate storage devices
- **Monitoring setup**: Deploy monitoring and alerting

### Maintenance
- **Backup strategies**: Regular backups of FoundationDB data
- **Upgrade procedures**: Safe upgrade procedures for FoundationDB
- **Capacity planning**: Monitor and plan for capacity growth
- **Performance tuning**: Regular performance analysis and tuning