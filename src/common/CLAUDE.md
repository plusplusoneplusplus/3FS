# Common Utilities and Libraries

This directory contains shared utilities, libraries, and infrastructure components used throughout the 3FS codebase. These components provide fundamental functionality for networking, serialization, logging, monitoring, and application framework.

## Directory Structure

### Application Framework (`app/`)
- **ApplicationBase.cc/h** - Base application class with common functionality
- **OnePhaseApplication.h** - Single-phase application lifecycle
- **TwoPhaseApplication.h** - Two-phase application lifecycle (init + run)
- **ConfigManager.cc/h** - Configuration loading and management
- **Thread.cc/h** - Thread utilities and helpers
- **Utils.cc/h** - General application utilities
- **AppInfo.cc/h** - Application information and metadata
- **ClientId.h**, **NodeId.h** - Identity management
- **ConfigStatus.h**, **ConfigUpdateRecord.h** - Configuration tracking

### Networking (`net/`)
High-performance networking infrastructure:
- **EventLoop.cc/h** - Main event loop for async operations
- **Server.cc/h** - Generic server implementation
- **Client.h** - Client connection interface
- **Transport.cc/h** - Transport layer abstraction
- **Processor.cc/h** - Request processing framework
- **ServiceGroup.cc/h** - Service grouping and management

#### Network Components
- **IOWorker.cc/h** - I/O worker threads
- **Listener.cc/h** - Network listener implementation
- **Waiter.cc/h** - Async operation waiting
- **MessageHeader.cc/h** - Message framing and headers
- **WriteItem.cc/h** - Write operation management
- **ThreadPoolGroup.cc/h** - Thread pool management
- **TransportPool.cc/h** - Transport connection pooling

#### RDMA Support (`net/ib/`)
- **IBDevice.cc/h** - InfiniBand device management
- **IBSocket.cc/h** - RDMA socket implementation
- **IBConnect.cc/h** - RDMA connection establishment
- **RDMABuf.cc/h** - RDMA buffer management
- **IBConnectService.h** - RDMA connection service

#### Transport Implementations
- **TCP** (`net/tcp/`) - TCP socket implementation
- **Synchronous networking** (`net/sync/`) - Blocking network operations

### Serialization and RPC (`serde/`)
- **Serde.cc/h** - Main serialization framework
- **Service.h** - Service definition framework
- **CallContext.cc/h** - RPC call context management
- **ClientContext.cc/h** - Client-side context
- **MessagePacket.h** - Message packet handling
- **SerdeHelper.h** - Serialization utilities
- **TypeName.h** - Type name utilities
- **Visit.h** - Visitor pattern for serialization

### Utilities (`utils/`)
Comprehensive utility library:

#### Concurrency and Threading
- **Coroutine.h** - Coroutine support
- **CoroutinesPool.h**, **DynamicCoroutinesPool.cc/h** - Coroutine pools
- **CPUExecutorGroup.cc/h** - CPU-bound task execution
- **BackgroundRunner.cc/h** - Background task management
- **Semaphore.h**, **SimpleSemaphore.cc/h** - Synchronization primitives
- **LockManager.h**, **ReentrantLockManager.cc/h** - Lock management
- **CoLockManager.h** - Coroutine-aware locking
- **FairSharedMutex.h** - Fair shared mutex implementation

#### Data Structures
- **BoundedQueue.h** - Thread-safe bounded queue
- **MPSCQueue.h** - Multi-producer single-consumer queue
- **LruCache.h** - LRU cache implementation
- **AtomicValue.h**, **AtomicSharedPtrTable.h** - Atomic operations
- **ObjectPool.h** - Object pooling
- **SimpleRingBuffer.h** - Ring buffer implementation
- **WorkStealingBlockingQueue.h** - Work-stealing queue

#### Configuration and Parsing
- **ConfigBase.cc/h** - Base configuration class
- **Toml.cc/hpp** - TOML configuration parsing
- **ArgParse.h** - Command-line argument parsing
- **RenderConfig.cc/h** - Configuration rendering

#### String and Data Processing
- **StringUtils.cc/h** - String manipulation utilities
- **Conversion.h** - Type conversion utilities
- **SerDeser.h** - Serialization/deserialization helpers
- **MurmurHash3.cc/h** - Hash function implementation
- **coding.cc/h** - Encoding/decoding utilities

#### System Integration
- **FileUtils.cc/h** - File system utilities
- **Path.cc/h** - Path manipulation
- **SysResource.cc/h** - System resource monitoring
- **SysvShm.cc/h** - System V shared memory
- **FdWrapper.h** - File descriptor wrapper

#### Time and Measurement
- **Duration.cc/h** - Duration utilities
- **UtcTime.cc/h** - UTC time handling
- **Size.cc/h** - Size formatting and parsing

#### Error Handling and Status
- **Status.h** - Status code framework
- **StatusCode.cc/h** - Status code definitions
- **Result.h** - Result type for error handling
- **HandleException.h** - Exception handling utilities

### Logging (`logging/`)
Comprehensive logging infrastructure:
- **LogInit.cc/h** - Logging initialization
- **LogConfig.cc/h** - Logging configuration
- **LogFormatter.cc/h** - Log message formatting
- **FileHelper.cc/h** - File handling utilities
- **AsyncFileWriter.cc/h** - Asynchronous file writing
- **ImmediateFileWriter.cc/h** - Synchronous file writing
- **RotatingFile.cc/h** - Log file rotation
- **SingleFile.cc/h** - Single file logging

### Monitoring (`monitor/`)
Metrics and monitoring infrastructure:
- **Monitor.cc/h** - Main monitoring interface
- **Recorder.cc/h** - Metrics recording
- **Reporter.h** - Metrics reporting interface
- **Sample.h** - Sample data structures
- **DigestBuilder.cc/h** - Metrics aggregation
- **ClickHouseClient.cc/h** - ClickHouse integration
- **TaosClient.cc/h** - TDengine integration
- **MonitorCollectorClient.cc/h** - Monitoring collector client

### Key-Value Store Interface (`kv/`)
- **IKVEngine.h** - Key-value engine interface
- **ITransaction.cc/h** - Transaction interface
- **KeyPrefix.h** - Key prefix utilities
- **TransactionRetry.h** - Retry logic for transactions
- **WithTransaction.h** - Transaction helpers
- **mem/** - In-memory KV implementation

## Key Features

### High-Performance Networking
- **RDMA support**: High-speed InfiniBand/RoCE networking
- **Async I/O**: Non-blocking I/O operations
- **Connection pooling**: Efficient connection reuse
- **Zero-copy**: Minimize data copying where possible

### Robust Serialization
- **Type-safe**: Compile-time type checking
- **Version compatibility**: Forward/backward compatibility
- **Performance**: Optimized for high-throughput scenarios
- **Cross-language**: Support for multiple languages

### Comprehensive Utilities
- **Modern C++**: C++20 features including coroutines
- **Thread-safe**: Safe concurrent access patterns
- **Memory efficient**: Careful memory management
- **Performance optimized**: Optimized data structures and algorithms

### Production-Ready Infrastructure
- **Logging**: Comprehensive logging with rotation and formatting
- **Monitoring**: Rich metrics collection and reporting
- **Configuration**: Flexible TOML-based configuration
- **Error handling**: Robust error handling patterns

## Usage Patterns

### Application Development
1. Inherit from `ApplicationBase` for common functionality
2. Use `ConfigManager` for configuration loading
3. Implement services using the RPC framework
4. Use utilities for common tasks

### Networking
1. Use `Server` class for service implementation
2. Implement `Processor` for request handling
3. Use connection pools for client connections
4. Leverage RDMA for high-performance scenarios

### Configuration
1. Define configuration structures
2. Use TOML files for configuration
3. Implement hot reloading where needed
4. Use `RenderConfig` for configuration validation

## Development Guidelines

### Adding New Utilities
1. Place in appropriate subdirectory
2. Follow existing naming conventions
3. Add comprehensive tests
4. Document usage patterns
5. Consider thread safety requirements

### Performance Considerations
- Use move semantics where appropriate
- Avoid unnecessary memory allocations
- Profile critical paths
- Consider cache-friendly data structures
- Use appropriate synchronization primitives

### Error Handling
- Use `Status` and `Result` types
- Provide meaningful error messages
- Handle all error conditions
- Use exceptions sparingly

### Testing
- Unit tests for all utilities
- Performance benchmarks for critical components
- Thread safety tests for concurrent utilities
- Integration tests with other components