# Custom Transactional KV Server Integration Guide

This guide explains how to integrate your Rust-based transactional KV server with the 3FS metadata service.

## Overview

The skeleton implementation provided adds support for a custom transactional KV server to replace FoundationDB in 3FS. The implementation follows the existing patterns and provides the same transaction interface that 3FS expects.

## Files Added

### Core Implementation
- `src/fdb/CustomKvEngine.h` - Main KV engine interface implementation
- `src/fdb/CustomKvEngine.cc` - KV engine implementation with connection management
- `src/fdb/CustomTransaction.h` - Transaction interface declarations
- `src/fdb/CustomTransaction.cc` - Transaction implementation with all required methods

### Configuration Support
- `src/fdb/HybridKvEngineConfig.h` - Updated to include custom KV configuration
- `src/fdb/HybridKvEngine.h/.cc` - Updated to support custom KV engine selection
- `configs/custom_kv_example.toml` - Example configuration file

## Integration Steps

### 1. Complete the Rust Client Integration

The current implementation provides method stubs that need to be connected to your Rust KV server. You'll need to:

#### A. Add Thrift/gRPC Client Dependencies
Add the appropriate client libraries to link against your Rust server:

```cpp
// In CustomKvEngine.cc - Impl class
#include "your_rust_kv_client.h"  // Generated Thrift or gRPC client

class CustomKvEngine::Impl {
  std::unique_ptr<YourRustKvClient> rust_client_;
  std::unique_ptr<ConnectionPool> connection_pool_;
};
```

#### B. Implement Connection Management
Replace the placeholder initialization in `CustomKvEngine::Impl::initialize()`:

```cpp
void initialize() {
  // Create connection pool to Rust KV servers
  connection_pool_ = std::make_unique<ConnectionPool>(config_.cluster_endpoints());
  
  // Create client instance
  rust_client_ = std::make_unique<YourRustKvClient>(connection_pool_.get());
  
  // Perform health check
  if (rust_client_->ping()) {
    healthy_ = true;
  }
}
```

#### C. Implement Transaction Methods
Replace the `TODO` comments in `CustomTransaction.cc` with actual calls to your Rust server:

```cpp
CoTryTask<std::optional<String>> CustomTransaction::get(std::string_view key) {
  // Call your Rust server's get method
  auto request = CreateGetRequest(transaction_id_, key);
  auto response = co_await rust_client_->get(request);
  
  if (response.has_error()) {
    co_return Status::fromRustError(response.error());
  }
  
  if (response.found()) {
    co_return String(response.value());
  } else {
    co_return std::nullopt;
  }
}
```

### 2. Error Handling Integration

Map your Rust server's error codes to 3FS status codes:

```cpp
// Add to CustomTransaction.cc
Status mapRustErrorToStatus(const RustError& error) {
  switch (error.type()) {
    case RustErrorType::CONFLICT:
      return Status::TransactionConflict(error.message());
    case RustErrorType::TIMEOUT:
      return Status::Timeout(error.message());
    case RustErrorType::NOT_FOUND:
      return Status::NotFound(error.message());
    default:
      return Status::Internal(error.message());
  }
}
```

### 3. Configuration Integration

Update your service configuration files to use the custom KV engine:

```toml
# In your meta service config (e.g., meta_main.toml)
[kv_engine]
kv_engine_type = "custom"

[kv_engine.custom_kv]
cluster_endpoints = ["rust-kv-1:9090", "rust-kv-2:9090", "rust-kv-3:9090"]
transaction_timeout_ms = 10000
max_retry_count = 10
connection_pool_size = 10
```

### 4. Build System Integration

Add the new files to your CMakeLists.txt:

```cmake
# In src/fdb/CMakeLists.txt
target_sources(fdb PRIVATE
  CustomKvEngine.cc
  CustomTransaction.cc
  # ... existing sources
)

# Link against your Rust client library
target_link_libraries(fdb PRIVATE
  your_rust_kv_client
  # ... existing dependencies
)
```

## Key Implementation Points

### Transaction Lifecycle

The implementation follows the standard 3FS transaction pattern:

1. **Creation**: `CustomKvEngine::createReadWriteTransaction()` creates a new transaction
2. **Operations**: `get()`, `set()`, `clear()`, `getRange()` methods execute operations
3. **Commit**: `commit()` attempts to commit all operations atomically
4. **Cleanup**: Transactions auto-abort on destruction if not committed

### Conflict Detection

Your Rust server must implement:
- **Read-write conflicts**: Detect when a read key was modified by another transaction
- **Write-write conflicts**: Detect concurrent writes to the same key
- **Range conflicts**: Support conflict detection for range operations

### Versioning Support

Implement these versioning features in your Rust server:
- **Read versions**: Snapshot isolation for reads
- **Commit versions**: Monotonically increasing commit timestamps
- **Versionstamps**: 10-byte unique identifiers for each transaction

### Performance Considerations

- **Connection pooling**: Reuse connections across transactions
- **Batching**: Support batching multiple operations in single requests
- **Async operations**: All methods return coroutines for non-blocking execution

## Testing Integration

### Unit Tests
Create tests for your custom KV engine:

```cpp
// tests/CustomKvEngineTest.cc
TEST(CustomKvEngineTest, BasicOperations) {
  CustomKvEngineConfig config;
  config.cluster_endpoints() = {"localhost:9090"};
  
  auto engine = std::make_unique<CustomKvEngine>(config);
  auto txn = engine->createReadWriteTransaction();
  
  // Test basic operations
  EXPECT_TRUE(txn->set("key1", "value1").await().isOK());
  auto result = txn->get("key1").await();
  EXPECT_TRUE(result.isOK());
  EXPECT_EQ(*result, "value1");
}
```

### Integration Tests
Test with actual meta server operations:

```cpp
TEST(MetaServerTest, WithCustomKv) {
  // Configure meta server to use custom KV
  MetaServerConfig config;
  config.kv_engine().kv_engine_type() = "custom";
  config.kv_engine().custom_kv().cluster_endpoints() = {"localhost:9090"};
  
  // Run meta server operations
  auto meta_server = MetaServer::create(config);
  // ... test metadata operations
}
```

## Migration Strategy

### Development Phase
1. Complete the Rust client integration
2. Test with synthetic workloads
3. Validate ACID properties and performance

### Testing Phase  
1. Deploy in test environment
2. Run existing 3FS test suite against custom KV
3. Performance benchmarking vs FoundationDB

### Production Phase
1. Canary deployment with small workloads
2. Gradual rollout with monitoring
3. Fallback capability to FoundationDB if needed

## Monitoring and Observability

Add metrics to track custom KV performance:

```cpp
// In CustomTransaction.cc
void CustomTransaction::recordMetrics(const std::string& operation, 
                                     std::chrono::microseconds latency,
                                     bool success) {
  monitor::Monitor::get().record(fmt::format("custom_kv.{}.latency", operation), latency.count());
  monitor::Monitor::get().record(fmt::format("custom_kv.{}.success", operation), success ? 1 : 0);
}
```

Monitor these key metrics:
- Transaction latency (P50, P95, P99)
- Transaction throughput 
- Conflict rates
- Connection pool health
- Error rates by type

## Next Steps

1. **Complete Rust Client Integration**: Implement the actual client calls to your Rust server
2. **Error Handling**: Map all Rust error types to appropriate 3FS status codes  
3. **Performance Optimization**: Add connection pooling, batching, and caching
4. **Testing**: Create comprehensive test suite for custom KV integration
5. **Documentation**: Add operational documentation for deployment and monitoring

This skeleton provides the foundation for integrating your Rust transactional KV server with 3FS. The key remaining work is connecting the stub methods to your actual Rust client implementation.