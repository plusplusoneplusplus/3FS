# MetaClient-MetaServer Serialization Framework

## Overview

The 3FS distributed filesystem uses a custom binary serialization framework called **"serde"** for communication between MetaClient and MetaServer. This framework is designed for high-performance, low-latency metadata operations over RDMA and TCP networks.

## Architecture

### Core Components

1. **Custom Binary Serialization**: A proprietary binary format optimized for performance
2. **Compile-time Reflection**: Uses C++ template metaprogramming for automatic code generation
3. **Message Packet Wrapper**: Standardized message envelope with metadata
4. **Type-safe RPC System**: Strongly typed remote procedure calls

### Key Files

```
src/common/serde/
├── Serde.h              # Core serialization framework
├── MessagePacket.h      # Message wrapper structure
├── ClientContext.h      # Client-side RPC context
├── SerdeHelper.h        # Utility functions
└── Service.h            # Service definition macros

src/fbs/meta/
├── Service.h            # Meta service message definitions
├── Schema.h             # Meta data structures
└── Common.h             # Common meta types

src/stubs/MetaService/
├── MetaServiceStub.h    # RPC stub implementation
└── IMetaServiceStub.h   # RPC interface definition
```

## Message Structure

### MessagePacket Envelope

Every message is wrapped in a `MessagePacket<T>` structure:

```cpp
template <class T = Void>
struct MessagePacket {
  SERDE_STRUCT_FIELD(uuid, uint64_t{});              // Request tracking ID
  SERDE_STRUCT_FIELD(serviceId, uint16_t{});         // Service identifier
  SERDE_STRUCT_FIELD(methodId, uint16_t{});          // Method identifier
  SERDE_STRUCT_FIELD(flags, uint16_t{});             // Control flags
  SERDE_STRUCT_FIELD(version, Version{});            // Protocol version
  SERDE_STRUCT_FIELD(payload, Payload<T>{});         // Actual message data
  SERDE_STRUCT_FIELD(timestamp, std::optional<Timestamp>{}); // Latency tracking
};
```

### Essential Flags

```cpp
enum EssentialFlags : uint16_t {
  IsReq = (1 << 0),        // Request vs Response
  UseCompress = (1 << 1),  // Compression enabled
  ControlRDMA = (1 << 2),  // RDMA flow control
};
```

### Timestamp Structure

```cpp
struct Timestamp {
  SERDE_STRUCT_FIELD(clientCalled, int64_t{});      // Client call start
  SERDE_STRUCT_FIELD(clientSerialized, int64_t{});  // Serialization complete
  SERDE_STRUCT_FIELD(serverReceived, int64_t{});    // Server received
  SERDE_STRUCT_FIELD(serverWaked, int64_t{});       // Server processing start
  SERDE_STRUCT_FIELD(serverProcessed, int64_t{});   // Server processing complete
  SERDE_STRUCT_FIELD(serverSerialized, int64_t{});  // Response serialized
  SERDE_STRUCT_FIELD(clientReceived, int64_t{});    // Client received response
  SERDE_STRUCT_FIELD(clientWaked, int64_t{});       // Client processing complete
  
  // Computed latencies
  auto serverLatency() const;
  auto inflightLatency() const;
  auto networkLatency() const;
  auto queueLatency() const;
  auto totalLatency() const;
};
```

## Message Definition Macros

### Field Definition

Messages use macro-based field definitions for automatic serialization:

```cpp
#define SERDE_STRUCT_FIELD(NAME, DEFAULT, ...)
#define SERDE_CLASS_FIELD(NAME, DEFAULT, ...)
#define SERDE_STRUCT_TYPED_FIELD(TYPE, NAME, DEFAULT, ...)
```

### Example Message Definitions

```cpp
// Base request structure
struct ReqBase {
  SERDE_STRUCT_FIELD(user, UserInfo{});           // User information
  SERDE_STRUCT_FIELD(client, ClientId{Uuid::zero()}); // Client ID
  SERDE_STRUCT_FIELD(forward, flat::NodeId(0));   // Forwarding node
  SERDE_STRUCT_FIELD(uuid, Uuid::zero());         // Request UUID
};

// File stat request
struct StatReq : ReqBase {
  SERDE_STRUCT_FIELD(inodeId, InodeId{});         // Inode to stat
  SERDE_STRUCT_FIELD(path, std::optional<Path>{}); // Optional path
  SERDE_STRUCT_FIELD(followLastSymlink, bool{});  // Symlink handling
};

// File stat response
struct StatRsp {
  SERDE_STRUCT_FIELD(inode, Inode{});             // Returned inode data
};
```

## Service Definition

### Service Macro

Services are defined using the `SERDE_SERVICE` macro:

```cpp
SERDE_SERVICE(MetaService, META_SERVICE_VERSION) {
  SERDE_SERVICE_METHOD(stat, 1, StatReq, StatRsp);
  SERDE_SERVICE_METHOD(create, 2, CreateReq, CreateRsp);
  SERDE_SERVICE_METHOD(open, 3, OpenReq, OpenRsp);
  SERDE_SERVICE_METHOD(close, 4, CloseReq, CloseRsp);
  SERDE_SERVICE_METHOD(list, 5, ListReq, ListRsp);
  SERDE_SERVICE_METHOD(remove, 6, RemoveReq, RemoveRsp);
  SERDE_SERVICE_METHOD(rename, 7, RenameReq, RenameRsp);
  SERDE_SERVICE_METHOD(truncate, 8, TruncateReq, TruncateRsp);
  // ... more methods
};
```

### RPC Stub Interface

```cpp
class IMetaServiceStub {
public:
  #define META_STUB_METHOD(NAME, REQ, RESP) \
    CoTryTask<RESP> NAME(const REQ &req, \
                         const net::UserRequestOptions &options, \
                         serde::Timestamp *timestamp = nullptr) = 0

  META_STUB_METHOD(stat, StatReq, StatRsp);
  META_STUB_METHOD(create, CreateReq, CreateRsp);
  META_STUB_METHOD(open, OpenReq, OpenRsp);
  // ... more methods
};
```

## Serialization Process

### 1. Request Creation

```cpp
// MetaClient creates a request
StatReq req;
req.user = userInfo;
req.inodeId = targetInode;
req.path = filePath;
req.followLastSymlink = true;
```

### 2. Message Packaging

```cpp
// Wrap in MessagePacket
MessagePacket<StatReq> packet(req);
packet.uuid = generateUUID();
packet.serviceId = MetaService::SERVICE_ID;
packet.methodId = MetaService::STAT_METHOD_ID;
packet.flags = EssentialFlags::IsReq;
```

### 3. Binary Serialization

The framework automatically generates serialization code using template metaprogramming:

```cpp
// Automatic serialization based on SERDE_STRUCT_FIELD definitions
auto serializedData = serde::serialize(packet);
```

### 4. Network Transport

```cpp
// Send over RDMA/TCP using the network client
auto result = co_await stub.stat(ctx, req, options);
```

### 5. Response Handling

```cpp
// Automatic deserialization on response
if (result.hasError()) {
  // Handle error
} else {
  StatRsp response = *result;
  // Use response.inode
}
```

## Network Integration

### Transport Protocols

- **Primary**: RDMA (Remote Direct Memory Access) for low latency
- **Fallback**: TCP for compatibility
- **Addressing**: Custom address format supporting multiple protocols

```cpp
enum Type : uint16_t { TCP, RDMA, IPoIB, LOCAL, UNIX };
```

### Connection Management

```cpp
// Create network client context
auto ctxCreator = [this](net::Address addr) { 
  return client->serdeCtx(addr); 
};

// Create MetaClient with stub factory
metaClient = std::make_shared<meta::client::MetaClient>(
  clientId,
  config.meta(),
  std::make_unique<MetaClient::StubFactory>(ctxCreator),
  mgmtdClient,
  storageClient,
  true /* dynStripe */
);
```

## Performance Features

### Latency Tracking

Built-in latency measurement at multiple points:
- Client call initiation
- Serialization time
- Network transit time
- Server processing time
- Deserialization time

### RDMA Optimization

```cpp
// RDMA-specific optimizations
struct RDMATransmission {
  CoTask<void> applyTransmission(Duration timeout);
};

// Buffer management for RDMA
std::shared_ptr<net::RDMABufPool> bufPool;
```

### Compression Support

Optional compression for large messages:
```cpp
bool useCompress() const { 
  return flags & EssentialFlags::UseCompress; 
}
```

## Error Handling

### Status Codes

The framework includes comprehensive error handling:

```cpp
// Check for errors in responses
if (!result.hasError()) {
  // Success case
  auto data = *result;
} else {
  // Error case
  Status error = result.error();
  XLOGF(ERR, "Operation failed: {} - {}", 
        error.code(), error.message());
}
```

### Retry Logic

Built-in retry mechanisms with exponential backoff:
```cpp
struct RetryConfig {
  Duration rpc_timeout = 5_s;
  uint32_t retry_send = 1;
  Duration retry_fast = 1_s;
  Duration retry_init_wait = 500_ms;
  Duration retry_max_wait = 5_s;
  Duration retry_total_time = 1_min;
  uint32_t max_failures_before_failover = 1;
};
```

## Advantages

1. **High Performance**: Custom binary format optimized for distributed filesystem workloads
2. **Type Safety**: Compile-time type checking prevents serialization errors
3. **Network Efficiency**: Supports both RDMA and TCP with protocol-specific optimizations
4. **Monitoring**: Built-in latency tracking and performance metrics
5. **Versioning**: Protocol version handling for backward compatibility
6. **Flexibility**: Easy to extend with new message types and fields

## Disadvantages

1. **Custom Format**: Not interoperable with standard serialization libraries
2. **Complexity**: Requires understanding of the custom macro system
3. **Debugging**: Binary format makes wire-level debugging more challenging
4. **Language Binding**: Limited to C++ implementation

## Future Considerations

- **Schema Evolution**: How to handle breaking changes in message formats
- **Cross-Language Support**: Potential for other language bindings
- **Debugging Tools**: Wire format analyzers and debugging utilities
- **Performance Profiling**: Enhanced monitoring and profiling capabilities
