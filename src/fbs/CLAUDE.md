# Serialization Framework (FBS)

This directory contains the FlatBuffers-based serialization framework for 3FS, providing efficient, type-safe serialization for all inter-service communication and data persistence.

## Directory Structure

### Service-Specific Schemas
- `meta/` - Metadata service serialization schemas and utilities
- `storage/` - Storage service message definitions
- `mgmtd/` - Management daemon serialization and RPC definitions
- `core/` - Core service interfaces and user management
- `monitor_collector/` - Monitoring service definitions

### Framework Components
- `lib/` - Core serialization library and utilities
- `macros/` - C++ macros for code generation and service stubs
- `migration/`, `simple_example/` - Example services and migration utilities

## Key Components

### Meta Service Serialization (`meta/`)
- **Schema.cc/h** - Core metadata schema definitions
- **FileOperation.cc/h** - File system operation serialization
- **Common.h** - Common metadata types and structures
- **Service.h** - Meta service RPC interface definitions
- **Utils.h** - Metadata serialization utilities
- **MockService.h** - Mock service for testing

### Storage Service (`storage/`)
- **Common.cc/h** - Storage-specific message types
- **Service.h** - Storage service RPC interface

### Management Daemon (`mgmtd/`)
Comprehensive cluster management serialization:
- **MgmtdServiceBase.h**, **MgmtdServiceClient.h**, **MgmtdServiceDef.h** - Service interfaces
- **ChainInfo.cc/h** - Replication chain information
- **NodeInfo.cc/h** - Cluster node information
- **RoutingInfo.cc/h** - Cluster routing data
- **ClientSession.cc/h** - Client session management
- **ConfigInfo.h** - Configuration data structures
- **HeartbeatInfo.h** - Node heartbeat information
- **TargetInfo.h** - Storage target information
- **Rpc.h** - RPC method definitions

### Core Services (`core/`)
- **CoreServiceBase.h**, **CoreServiceClient.h**, **CoreServiceDef.h** - Core service interfaces
- **User.cc/h** - User management and authentication
- **Rpc.h** - Core service RPC definitions

### Framework Library (`lib/`)
- **Schema.h** - Base schema definitions and utilities
- **Service.h** - Service framework and RPC infrastructure

### Code Generation Macros (`macros/`)
C++ macros for generating service code:
- **SerdeDef.h** - Serialization definition macros
- **Stub.h**, **SyncStub.h** - Service stub generation
- **IStub.h**, **ISyncStub.h** - Interface definitions
- **Undef.h** - Macro cleanup

## Architecture

### FlatBuffers Integration
- **Zero-copy serialization**: Direct memory access without parsing
- **Cross-language support**: Compatible across different programming languages
- **Schema evolution**: Forward and backward compatibility
- **Type safety**: Compile-time type checking

### Service Framework
- **RPC abstraction**: High-level RPC interface over network transport
- **Async operations**: Non-blocking service calls with coroutines
- **Error handling**: Standardized error propagation
- **Service discovery**: Integration with cluster management

### Code Generation
- **Automatic stub generation**: Reduce boilerplate code
- **Type-safe interfaces**: Compile-time guarantees for service contracts
- **Mock support**: Automatic mock generation for testing
- **Versioning support**: Handle schema evolution gracefully

## Key Features

### High Performance
- **Zero-copy**: Access serialized data without copying
- **Minimal overhead**: Efficient binary representation
- **Batch operations**: Support for batching multiple operations
- **Memory efficiency**: Compact binary format

### Type Safety
- **Schema definitions**: Formal schema for all messages
- **Compile-time checking**: Catch errors at compile time
- **Interface contracts**: Clear contracts between services
- **Version compatibility**: Safe schema evolution

### Developer Productivity
- **Code generation**: Automatic generation of client/server stubs
- **Easy integration**: Simple integration with existing code
- **Rich tooling**: Schema validation and debugging tools
- **Mock support**: Easy testing with generated mocks

## Usage Patterns

### Defining New Services
1. Create schema file in appropriate subdirectory
2. Define message structures using FlatBuffers syntax
3. Define service interface with RPC methods
4. Generate C++ code using build system
5. Implement service logic using generated stubs

### Adding New Operations
1. Extend existing schema with new message types
2. Add RPC method to service interface
3. Implement operation in service class
4. Update client code to use new operation
5. Add tests for new functionality

### Schema Evolution
1. Add new fields as optional with default values
2. Never remove or change existing fields
3. Use field deprecation for unused fields
4. Test compatibility with older versions
5. Document breaking changes clearly

## Development Guidelines

### Schema Design
- **Use optional fields**: Allow for schema evolution
- **Provide defaults**: Set sensible default values
- **Group related data**: Use tables for related fields
- **Minimize nesting**: Keep schemas flat where possible
- **Document schemas**: Add comments explaining field usage

### Service Design
- **Keep operations atomic**: Each RPC should be self-contained
- **Use appropriate types**: Choose efficient types for data
- **Handle all errors**: Define error cases in schema
- **Support batching**: Allow multiple operations in single call
- **Consider versioning**: Plan for future schema changes

### Performance Optimization
- **Profile serialization**: Measure serialization overhead
- **Optimize hot paths**: Focus on frequently used operations
- **Use vectors efficiently**: Prefer vectors over individual fields
- **Minimize string usage**: Use enums instead of strings where possible
- **Batch operations**: Reduce network round trips

### Testing
- **Unit tests**: Test serialization and deserialization
- **Integration tests**: Test service communication
- **Schema compatibility**: Test with different schema versions
- **Performance tests**: Benchmark serialization performance
- **Mock services**: Use generated mocks for testing

## Build Integration

The serialization framework is integrated with the CMake build system:
- **Automatic generation**: FlatBuffers schemas are compiled automatically
- **Dependency tracking**: Changes to schemas trigger recompilation
- **Cross-service dependencies**: Handle dependencies between services
- **Testing support**: Generate mocks and test utilities

## Monitoring and Debugging

### Schema Validation
- **Build-time validation**: Catch schema errors during compilation
- **Runtime validation**: Optional runtime schema validation
- **Compatibility checking**: Verify schema compatibility between versions

### Performance Monitoring
- **Serialization metrics**: Track serialization/deserialization time
- **Memory usage**: Monitor memory usage of serialized data
- **Network efficiency**: Measure network bandwidth usage
- **Error rates**: Track serialization errors and failures