# Transactional KV Service Adapter Design Guide

This document provides a comprehensive guide for implementing new transactional key-value service adapters for the 3FS metadata server.

## Overview

The 3FS metadata service currently supports two KV backends:
- **FoundationDB** - Production transactional database with ACID guarantees
- **In-Memory** - Testing implementation with basic transactional semantics

This guide outlines how to add support for additional transactional KV services (e.g., etcd, TiKV, CockroachDB, etc.).

## Current Architecture

### Core Interfaces

The KV abstraction layer consists of:

1. **IKVEngine** (`src/common/kv/IKVEngine.h`) - Factory interface for creating transactions
2. **IReadOnlyTransaction/IReadWriteTransaction** (`src/common/kv/ITransaction.h`) - Transaction interface
3. **HybridKvEngine** (`src/fdb/HybridKvEngine.cc`) - Engine selector and load balancer

### Existing Implementations

- **FoundationDB Adapter** (`src/fdb/FDBKVEngine.h`, `src/fdb/FDBTransaction.cc`)
- **In-Memory Adapter** (`src/common/kv/mem/MemKVEngine.h`, `src/common/kv/mem/MemTransaction.h`)

## Required Components for New KV Adapter

### 1. KV Engine Implementation

Create `YourKVEngine.h` inheriting from `IKVEngine`:

```cpp
namespace hf3fs::kv {

class YourKVEngine : public IKVEngine {
public:
  explicit YourKVEngine(const YourKVConfig &config);
  ~YourKVEngine() override;

  std::unique_ptr<IReadOnlyTransaction> createReadonlyTransaction() override;
  std::unique_ptr<IReadWriteTransaction> createReadWriteTransaction() override;

private:
  // Your KV client/connection pool
  std::unique_ptr<YourKVClient> client_;
};

}  // namespace hf3fs::kv
```

### 2. Transaction Implementation

Create `YourTransaction.h` inheriting from `IReadWriteTransaction`:

```cpp
namespace hf3fs::kv {

class YourTransaction : public IReadWriteTransaction {
public:
  explicit YourTransaction(YourKVClient &client);
  ~YourTransaction() override;

  // Read operations
  CoTryTask<std::optional<String>> snapshotGet(std::string_view key) override;
  CoTryTask<GetRangeResult> snapshotGetRange(const KeySelector &begin, 
                                           const KeySelector &end, 
                                           int32_t limit) override;
  CoTryTask<std::optional<String>> get(std::string_view key) override;
  CoTryTask<GetRangeResult> getRange(const KeySelector &begin, 
                                   const KeySelector &end, 
                                   int32_t limit) override;

  // Conflict tracking
  CoTryTask<void> addReadConflict(std::string_view key) override;
  CoTryTask<void> addReadConflictRange(std::string_view begin, 
                                     std::string_view end) override;

  // Write operations
  CoTryTask<void> set(std::string_view key, std::string_view value) override;
  CoTryTask<void> clear(std::string_view key) override;

  // Versionstamp operations (for ordering guarantees)
  CoTryTask<void> setVersionstampedKey(std::string_view key, 
                                     uint32_t offset, 
                                     std::string_view value) override;
  CoTryTask<void> setVersionstampedValue(std::string_view key, 
                                       std::string_view value, 
                                       uint32_t offset) override;

  // Transaction control
  CoTryTask<void> commit() override;
  CoTryTask<void> cancel() override;
  void reset() override;

  // Version management
  void setReadVersion(int64_t version) override;
  int64_t getCommittedVersion() override;

private:
  YourKVClient &client_;
  YourKVTransaction transaction_;
  std::unordered_set<String> readKeys_;  // For conflict detection
  std::vector<std::pair<String, String>> readRanges_;
  // ... other transaction state
};

}  // namespace hf3fs::kv
```

### 3. Configuration Support

Add configuration in `src/fdb/HybridKvEngineConfig.h`:

```cpp
struct YourKVConfig : public ConfigBase<YourKVConfig> {
  CONFIG_ITEM(endpoints, std::vector<std::string>{});
  CONFIG_ITEM(timeout_ms, 5000);
  CONFIG_ITEM(retry_max_attempts, 3);
  // Add other configuration parameters
};

struct HybridKvEngineConfig : public ConfigBase<HybridKvEngineConfig> {
  CONFIG_ITEM(use_memkv, false);
  CONFIG_OBJ(fdb, kv::fdb::FDBConfig);
  CONFIG_OBJ(your_kv, YourKVConfig);  // Add this
  
  // Add enum for KV engine type selection
  CONFIG_ITEM(engine_type, std::string("fdb")); // "fdb", "mem", "your_kv"
};
```

### 4. Integration with HybridKvEngine

Update `src/fdb/HybridKvEngine.cc`:

```cpp
#include "YourKVEngine.h"

// Add factory method
std::shared_ptr<HybridKvEngine> HybridKvEngine::fromYourKV(const YourKVConfig &config) {
  std::shared_ptr<HybridKvEngine> p(new HybridKvEngine);
  // Initialize your KV engines (potentially multiple for load balancing)
  for (int i = 0; i < desired_connection_count; ++i) {
    p->kvEngines_.push_back(std::make_unique<YourKVEngine>(config));
  }
  return p;
}

// Update factory method to handle your KV type
std::shared_ptr<HybridKvEngine> HybridKvEngine::from(const HybridKvEngineConfig &config) {
  if (config.engine_type() == "your_kv") {
    return fromYourKV(config.your_kv());
  } else if (config.use_memkv()) {
    return fromMem();
  } else {
    return fromFdb(config.fdb());
  }
}
```

## Key Requirements

### Transactional Properties

Your KV implementation must provide:

1. **ACID Compliance**
   - **Atomicity**: All operations in a transaction succeed or fail together
   - **Consistency**: Transactions maintain data integrity constraints
   - **Isolation**: Concurrent transactions don't interfere (snapshot isolation preferred)
   - **Durability**: Committed transactions survive failures

2. **Conflict Detection**
   - Track read and write sets for conflict detection
   - Handle read-write and write-write conflicts
   - Implement optimistic concurrency control

3. **Version Management**
   - Support read version setting for snapshot reads
   - Return commit versions for ordering
   - Handle versionstamped operations for monotonic ordering

### API Compatibility

1. **Coroutine Support**
   - All operations must return `CoTryTask<T>`
   - Use `co_return` and `co_await` appropriately
   - Handle error propagation with `CO_RETURN_ON_ERROR` macros

2. **String Handling**
   - Use 3FS `String` type for keys and values
   - Support binary-safe keys and values
   - Handle empty keys/values correctly

3. **Range Operations**
   - Support efficient prefix scans
   - Implement inclusive/exclusive key selectors
   - Handle pagination with limits and continuation

4. **Error Handling**
   - Map your KV errors to appropriate `TransactionCode` values:
     - `kConflict` - Transaction conflicts (retry with backoff)
     - `kMaybeCommitted` - Uncertain commit status
     - `kThrottled` - Rate limiting
     - `kTooOld` - Read version too old
     - `kNetworkError` - Connection failures
     - `kFailed` - Permanent failures

### Performance Considerations

1. **Connection Management**
   - Implement connection pooling
   - Handle connection failures gracefully
   - Support concurrent transactions

2. **Batch Operations**
   - Optimize multiple operations within single transaction
   - Minimize network round trips
   - Use bulk APIs where available

3. **Monitoring Integration**
   - Add metrics following `FDBTransaction.cc` patterns:
     ```cpp
     monitor::CountRecorder totalRecorder{"your_kv_total_count_get"};
     monitor::CountRecorder failedRecorder{"your_kv_failed_count_get"};
     monitor::LatencyRecorder latencyRecorder{"your_kv_latency_get"};
     ```

4. **Memory Management**
   - Avoid unnecessary memory allocations
   - Use move semantics where appropriate
   - Clean up resources in destructors

## Implementation Steps

### Phase 1: Basic Structure
1. Create KV client wrapper library
2. Implement basic `YourKVEngine` and `YourTransaction` classes
3. Add configuration structures
4. Write unit tests for core functionality

### Phase 2: Transaction Semantics
1. Implement ACID transaction support
2. Add conflict detection logic
3. Handle error mapping and retry logic
4. Test transactional behavior

### Phase 3: Integration
1. Update `HybridKvEngine` factory methods
2. Add configuration support to meta server
3. Update build system and dependencies
4. Write integration tests

### Phase 4: Production Readiness
1. Add comprehensive monitoring and metrics
2. Implement connection pooling and failover
3. Add performance optimizations
4. Load testing and benchmarking

## Testing Strategy

# Existing test cases
```
./build/tests/test_common --gtest_filter="TestMemTransaction.*"
./build/tests/test_meta --gtest_filter="TestInode*"
```

### Unit Tests
Follow patterns in `tests/common/kv/fdb/`:
- Transaction lifecycle (begin, commit, rollback)
- CRUD operations (get, set, clear, range queries)
- Conflict detection and resolution
- Error handling and retry logic

### Integration Tests
- Test with actual metadata operations
- Multi-node cluster testing
- Failure injection and recovery
- Performance benchmarks

### Example Test Structure
```cpp
class TestYourKVEngine : public ::testing::Test {
protected:
  void SetUp() override {
    config_.endpoints({"127.0.0.1:2379"});  // Your KV endpoints
    engine_ = std::make_shared<YourKVEngine>(config_);
  }

  YourKVConfig config_;
  std::shared_ptr<YourKVEngine> engine_;
};

TEST_F(TestYourKVEngine, BasicReadWrite) {
  auto txn = engine_->createReadWriteTransaction();
  
  // Test set/get cycle
  auto setResult = co_await txn->set("test_key", "test_value");
  ASSERT_TRUE(setResult.hasValue());
  
  auto getResult = co_await txn->get("test_key");
  ASSERT_TRUE(getResult.hasValue());
  EXPECT_EQ(*getResult.value(), "test_value");
  
  auto commitResult = co_await txn->commit();
  ASSERT_TRUE(commitResult.hasValue());
}
```

## Configuration Examples

### Meta Server Configuration
In `meta_main.toml`:

```toml
[kv_engine]
engine_type = "your_kv"

[kv_engine.your_kv]
endpoints = ["127.0.0.1:2379", "127.0.0.1:2380", "127.0.0.1:2381"]
timeout_ms = 10000
retry_max_attempts = 5
connection_pool_size = 10
```

### Testing Configuration
In `testing_configs/meta_main.toml`:

```toml
[kv_engine]
engine_type = "mem"  # Use in-memory for testing

# For production-like testing
# engine_type = "your_kv"
# [kv_engine.your_kv]
# endpoints = ["localhost:2379"]
```

## Reference Implementations

Study these existing implementations for patterns and best practices:

1. **FoundationDB Adapter** (`src/fdb/`)
   - Production-ready implementation
   - Comprehensive error handling
   - Connection pooling and failover
   - Monitoring and metrics

2. **In-Memory Adapter** (`src/common/kv/mem/`)
   - Simple reference implementation
   - Basic transactional semantics
   - Testing-focused design

## Common Pitfalls

1. **Version Semantics**: Ensure proper handling of read/commit versions
2. **Conflict Detection**: Don't forget to track read sets for conflict detection
3. **Error Mapping**: Map KV-specific errors to appropriate TransactionCode values
4. **Resource Cleanup**: Always clean up resources in destructors and error paths
5. **Coroutine Lifecycle**: Be careful with object lifetimes in coroutines

## Support and Maintenance

When adding a new KV adapter:

1. Update documentation in `src/fdb/CLAUDE.md`
2. Add to build system (`CMakeLists.txt`)
3. Update deployment configurations
4. Add monitoring dashboards
5. Document operational procedures

This design provides a solid foundation for integrating any transactional KV service with 3FS while maintaining compatibility with existing components and ensuring production-ready reliability.