# Custom Transactional KV Server Requirements for 3FS

This document outlines the requirements and interfaces needed to replace FoundationDB with a custom transactional key-value server for the 3FS metadata service.

## Overview

The 3FS metadata service currently relies on FoundationDB for transactional metadata storage with strong consistency guarantees. To replace it with a custom implementation, we need to maintain compatibility with the existing KV interfaces while providing equivalent ACID properties and performance characteristics.

## Core Interface Requirements

### 1. IKVEngine Interface

**Location**: `src/common/kv/IKVEngine.h:8`

The main entry point for creating transactions:

```cpp
class IKVEngine {
  virtual std::unique_ptr<IReadOnlyTransaction> createReadonlyTransaction() = 0;
  virtual std::unique_ptr<IReadWriteTransaction> createReadWriteTransaction() = 0;
};
```

**Usage Pattern**: Used throughout meta server components (`src/meta/components/`) for creating transaction instances.

### 2. Transaction Interfaces

**Location**: `src/common/kv/ITransaction.h:34`

#### IReadOnlyTransaction Interface

Core read operations with conflict detection support:

```cpp
class IReadOnlyTransaction {
  // Read operations
  virtual CoTryTask<std::optional<String>> snapshotGet(std::string_view key) = 0;
  virtual CoTryTask<std::optional<String>> get(std::string_view key) = 0;
  virtual CoTryTask<GetRangeResult> snapshotGetRange(const KeySelector &begin, const KeySelector &end, int32_t limit) = 0;
  virtual CoTryTask<GetRangeResult> getRange(const KeySelector &begin, const KeySelector &end, int32_t limit) = 0;
  
  // Version management
  virtual void setReadVersion(int64_t version) = 0;
  
  // Transaction control
  virtual CoTryTask<void> cancel() = 0;
  virtual void reset() = 0;
};
```

**Key Differences**:
- `snapshotGet`/`snapshotGetRange`: Read without adding to conflict set
- `get`/`getRange`: Read with conflict detection for serializable isolation

#### IReadWriteTransaction Interface

Extends read-only interface with write operations:

```cpp
class IReadWriteTransaction : public IReadOnlyTransaction {
  // Write operations  
  virtual CoTryTask<void> set(std::string_view key, std::string_view value) = 0;
  virtual CoTryTask<void> clear(std::string_view key) = 0;
  
  // Versionstamp operations
  virtual CoTryTask<void> setVersionstampedKey(std::string_view key, uint32_t offset, std::string_view value) = 0;
  virtual CoTryTask<void> setVersionstampedValue(std::string_view key, std::string_view value, uint32_t offset) = 0;
  
  // Conflict management
  virtual CoTryTask<void> addReadConflict(std::string_view key) = 0;
  virtual CoTryTask<void> addReadConflictRange(std::string_view begin, std::string_view end) = 0;
  
  // Transaction lifecycle
  virtual CoTryTask<void> commit() = 0;
  virtual int64_t getCommittedVersion() = 0;
};
```

### 3. Supporting Data Structures

#### KeySelector
Used for range operations:
```cpp
struct KeySelector {
  std::string_view key;
  bool inclusive;
};
```

#### GetRangeResult
Result structure for range queries:
```cpp
struct GetRangeResult {
  std::vector<KeyValue> kvs;
  bool hasMore;  // Indicates if more results available
};
```

#### Versionstamp
10-byte unique identifier for each transaction:
```cpp
using Versionstamp = std::array<uint8_t, 10>;
// First 8 bytes: commit version (big-endian)
// Last 2 bytes: transaction order within version (big-endian)
```

## Transactional Properties Required

### ACID Compliance

1. **Atomicity**: All operations within a transaction succeed or fail together
2. **Consistency**: Strong consistency across all operations - reads reflect all committed writes
3. **Isolation**: Snapshot isolation for reads, serializable conflict detection for writes  
4. **Durability**: Committed transactions survive server failures

### Concurrency Control

The system uses **optimistic concurrency control** with conflict detection:

- **Read-Write Conflicts**: Detect when a key read by transaction T1 was modified by transaction T2 that committed after T1's read version
- **Write-Write Conflicts**: Detect concurrent writes to the same key
- **Range Conflicts**: Support conflict detection on key ranges for `getRange` operations
- **Conflict Resolution**: Failed transactions should be retried with exponential backoff

### Transaction Lifecycle

**Pattern used throughout meta server** (`src/common/kv/WithTransaction.h:16`):

```cpp
template<typename RetryStrategy>
class WithTransaction {
  // Simplified transaction execution pattern
  while (true) {
    auto result = co_await handler(txn);
    if (!result.hasError()) {
      auto commitResult = co_await txn.commit();
      CO_RETURN_ON_ERROR(commitResult);
      co_return result;
    }
    // Handle retryable errors
    auto retryResult = co_await strategy.onError(&txn, result.error());
    CO_RETURN_ON_ERROR(retryResult);
  }
}
```

## Key Features Required

### 1. Versioning & Timestamps

- **Monotonic Versions**: 64-bit transaction versions that increase monotonically
- **Read Versions**: Assigned at transaction start for snapshot isolation
- **Commit Versions**: Assigned at commit time, must be > all read versions
- **Versionstamp Support**: 10-byte unique identifiers combining version + transaction order

### 2. Key-Value Operations

- **Binary Safety**: Keys and values are arbitrary byte sequences
- **Key Ordering**: Lexicographic byte ordering for range operations
- **Large Values**: Support values up to several MB (for complex metadata)
- **Prefix Operations**: Efficient operations on key prefixes (heavily used by meta server)

### 3. Range Query Support

Efficient range scans with:
- **Begin/End Selectors**: Inclusive/exclusive boundary specification
- **Limit Support**: Bounded result sets to prevent memory exhaustion  
- **Continuation**: `hasMore` flag for paginated results
- **Empty Range Handling**: Proper handling of empty result sets

### 4. Performance Requirements

Based on 3FS meta server usage patterns:

- **Low Latency**: <10ms for typical metadata operations
- **High Throughput**: Support thousands of concurrent transactions
- **Efficient Range Scans**: Directory listings and garbage collection depend on fast range queries
- **Connection Reuse**: Support connection pooling to reduce overhead

### 5. Error Handling

Your custom server must distinguish between error types for proper retry logic:

#### Retriable Transaction Errors
- Conflicts with other transactions
- Temporary resource unavailability  
- Network timeouts during operation

#### Non-Retriable Errors
- Invalid request format
- Storage system failures
- Authentication/authorization failures

#### Maybe-Committed Errors
- Network failures during commit phase
- Server crashes during commit processing

**Error Classification Pattern** (based on `src/fdb/FDBRetryStrategy.h:23`):
```cpp
enum class TransactionErrorType {
  Retriable,                // Retry with backoff
  RetriableNotCommitted,    // Retry, transaction definitely not committed
  NotRetriable,            // Don't retry, permanent failure
  MaybeCommitted           // Retry based on configuration
};
```

## Implementation Architecture

### Phase 1: Core Transactional Engine

#### Storage Layer
- **MVCC Storage**: Multi-version storage supporting snapshot reads
- **Index Structure**: B+ tree or LSM tree for efficient key operations
- **Persistence**: Write-ahead log for durability
- **Compaction**: Background cleanup of old versions

#### Transaction Manager
- **Version Assignment**: Centralized version assignment for consistency
- **Conflict Detection**: Track read/write sets and detect conflicts at commit
- **Deadlock Prevention**: Timeout-based deadlock prevention
- **Resource Management**: Memory and lock management per transaction

#### Network Layer
- **Protocol**: Binary protocol compatible with 3FS coroutine patterns
- **Connection Handling**: Persistent connections with multiplexing
- **Load Balancing**: Support for multiple server instances

### Phase 2: 3FS Integration

#### Custom KV Engine Implementation
Implement the `IKVEngine` interface:

```cpp
class CustomKVEngine : public IKVEngine {
public:
  CustomKVEngine(const CustomKVConfig& config);
  
  std::unique_ptr<IReadOnlyTransaction> createReadonlyTransaction() override;
  std::unique_ptr<IReadWriteTransaction> createReadWriteTransaction() override;
  
private:
  // Connection pool, configuration, etc.
};
```

#### Custom Transaction Implementation
Implement transaction interfaces with network communication to custom server:

```cpp
class CustomTransaction : public IReadWriteTransaction {
public:
  CustomTransaction(ConnectionHandle conn);
  
  // Implement all required methods
  CoTryTask<std::optional<String>> get(std::string_view key) override;
  CoTryTask<void> set(std::string_view key, std::string_view value) override;
  CoTryTask<void> commit() override;
  // ... etc
};
```

#### Configuration Integration
Mirror FoundationDB configuration patterns in `src/fdb/HybridKvEngineConfig.h`:

```cpp
struct CustomKvEngineConfig : public ConfigBase<CustomKvEngineConfig> {
  CONFIG_ITEM(cluster_endpoints, std::vector<std::string>{});
  CONFIG_ITEM(transaction_timeout_ms, 10000);
  CONFIG_ITEM(max_retry_count, 10);
  CONFIG_ITEM(connection_pool_size, 10);
};
```

### Phase 3: Advanced Features

#### High Availability
- **Multi-Node Deployment**: Distributed consensus for fault tolerance
- **Automatic Failover**: Client-side failover to healthy servers
- **Data Replication**: Synchronous replication for durability

#### Performance Optimization
- **Batching**: Batch multiple operations in single network round-trip
- **Caching**: Client-side caching of frequently accessed metadata
- **Locality**: Data placement awareness for performance

#### Monitoring Integration
Integration with 3FS monitoring system (`src/common/monitor/`):
- **Metrics Collection**: Transaction latency, throughput, conflict rates
- **Health Monitoring**: Server health and performance monitoring  
- **Alerting**: Automated alerting on error conditions

## Usage Patterns in 3FS Meta Server

### Primary Access Patterns

Based on analysis of `src/meta/components/`:

1. **Single-Key Operations** (most common):
   - Inode metadata: `inode/{inode_id}` 
   - Directory entries: `direntry/{parent_id}/{name}`
   - File sessions: `session/{inode_id}/{session_id}`

2. **Range Scans**:
   - Directory listings: scan `direntry/{parent_id}/` prefix
   - Session cleanup: scan `session/` ranges
   - Garbage collection: scan inode ranges

3. **Batch Operations**:
   - Bulk metadata updates during file operations
   - Session pruning operations
   - Chain allocation updates

4. **Background Operations**:
   - Garbage collection of deleted files
   - Session timeout cleanup
   - Metrics and monitoring updates

### Performance Characteristics

- **Read Heavy**: ~80% read operations vs 20% writes
- **Small Values**: Most values <1KB, some metadata up to 10KB  
- **Hot Keys**: Root directory and frequently accessed paths
- **Range Queries**: Directory listings are latency-sensitive

## Migration Strategy

### 1. Compatibility Layer
Implement custom KV engine that can be swapped in via configuration:

```toml
[kv_engine]
type = "custom"  # vs "fdb" or "memory"
endpoints = ["server1:9000", "server2:9000"]
```

### 2. Testing Strategy
- **Unit Tests**: Test transaction interface compliance
- **Integration Tests**: Run existing meta server tests against custom KV
- **Load Tests**: Verify performance under realistic workloads
- **Consistency Tests**: Verify ACID properties under concurrent load

### 3. Rollout Plan
1. **Development**: Implement and test against synthetic workloads
2. **Staging**: Deploy in test environments with realistic data
3. **Canary**: Gradual rollout with fallback to FoundationDB
4. **Production**: Full deployment after validation

## Monitoring and Observability

### Key Metrics
- **Transaction Latency**: P50, P95, P99 latencies per operation type
- **Throughput**: Transactions per second, operations per second
- **Conflict Rate**: Percentage of transactions that fail due to conflicts
- **Error Rate**: Breakdown by error type and retry behavior
- **Resource Usage**: Memory, CPU, disk usage per server

### Health Checks
- **Server Health**: Basic connectivity and response time checks
- **Data Consistency**: Periodic consistency validation
- **Performance Regression**: Automated performance regression detection

This specification provides a comprehensive foundation for implementing a custom transactional KV server that can replace FoundationDB while maintaining full compatibility with the 3FS metadata service architecture.