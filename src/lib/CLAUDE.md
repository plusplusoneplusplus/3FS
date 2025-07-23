# Public APIs and Libraries

This directory contains the public APIs and libraries for 3FS, providing interfaces for applications to integrate with the distributed file system through various programming languages and access patterns.

## Directory Structure

### Public APIs (`api/`)
- **hf3fs.h** - Main C API for 3FS filesystem operations
- **hf3fs_usrbio.h** - User Block I/O (USRBIO) API for high-performance applications
- **UsrbIo.cc** - USRBIO implementation
- **UsrbIo.md** - USRBIO documentation and usage guide
- **fuse.h** - FUSE-specific API definitions
- **hf3fs_expected.h** - Expected/Result type utilities for error handling

### Language Bindings
- `py/` - Python bindings for native API access
- `rs/` - Rust bindings and utilities

### Common Libraries (`common/`)
- **Shm.cc/h** - Shared memory utilities
- **PerProcTable.h** - Per-process data structures
- **paths.h** - Standard path definitions

## Key Components

### Core C API (`api/hf3fs.h`)
Standard filesystem API providing:
- **File operations**: open, read, write, close, sync
- **Directory operations**: mkdir, rmdir, opendir, readdir
- **Metadata operations**: stat, chmod, chown, utimes
- **Path operations**: rename, link, unlink, symlink
- **Extended attributes**: getxattr, setxattr, listxattr, removexattr

### User Block I/O API (`api/hf3fs_usrbio.h`)
High-performance block I/O interface:
- **Direct I/O**: Bypass filesystem cache for maximum performance
- **Async operations**: Non-blocking I/O with completion callbacks
- **Batch operations**: Submit multiple I/O requests efficiently
- **Zero-copy**: Minimize data copying for high throughput
- **Memory management**: Efficient buffer allocation and reuse

#### USRBIO Features
- **High bandwidth**: Optimized for AI/ML workloads requiring high I/O throughput
- **Low latency**: Minimal overhead for time-critical applications
- **Parallel I/O**: Concurrent access to multiple storage targets
- **Custom allocators**: Support for application-specific memory management

### Python Bindings (`py/`)
- **binding.cc** - Main Python C extension module
- **usrbio_binding.cc** - Python bindings for USRBIO API
- **Native performance**: Direct access to C++ implementation
- **Pythonic interface**: Easy-to-use Python API
- **Error handling**: Proper Python exception handling

### Rust Integration (`rs/`)
- **hf3fs-usrbio-sys/** - Low-level Rust bindings to C API
  - **Cargo.toml** - Rust package configuration
  - **build.rs** - Build script for FFI integration
  - **src/lib.rs** - Rust FFI bindings
  - **lib/** - Native shared libraries

### Shared Memory Support (`common/`)
- **Shm.cc/h** - Cross-process shared memory utilities
- **PerProcTable.h** - Per-process lookup tables for shared resources
- **paths.h** - Standard system paths and file locations

## Architecture

### Multi-Language Support
- **C API**: Core interface for maximum compatibility
- **Python bindings**: High-level interface for scripting and applications
- **Rust bindings**: Safe, zero-cost abstractions for systems programming
- **FFI integration**: Foreign Function Interface for easy language binding

### Performance Tiers
1. **Standard API**: POSIX-compatible interface via FUSE
2. **Native API**: Direct C API for better performance
3. **USRBIO API**: Highest performance block I/O interface

### Memory Management
- **Shared memory**: Efficient inter-process communication
- **Zero-copy operations**: Minimize memory copying overhead
- **Custom allocators**: Support for application-specific allocation strategies
- **Buffer pooling**: Reuse buffers to reduce allocation overhead

## Key Features

### High-Performance I/O
- **USRBIO interface**: Direct access to storage layer
- **Async operations**: Non-blocking I/O with event-driven completion
- **Batch processing**: Submit multiple operations efficiently
- **RDMA integration**: High-speed network I/O for remote storage

### Multi-Language Support
- **C/C++**: Native performance with direct API access
- **Python**: Easy integration for data science and ML workflows
- **Rust**: Memory-safe systems programming interface
- **Extensible**: Framework for adding additional language bindings

### POSIX Compatibility
- **Standard interface**: Works with existing applications
- **Extended attributes**: Support for custom metadata
- **File locking**: Advisory and mandatory locking support
- **Symbolic links**: Full symbolic link support

## Usage Patterns

### Standard Applications
```c
// Using standard C API
#include "hf3fs.h"

int fd = hf3fs_open("/path/to/file", O_RDWR);
ssize_t bytes_read = hf3fs_read(fd, buffer, size);
hf3fs_close(fd);
```

### High-Performance Applications
```c
// Using USRBIO API
#include "hf3fs_usrbio.h"

hf3fs_usrbio_context_t* ctx = hf3fs_usrbio_init();
hf3fs_usrbio_read_async(ctx, buffer, size, offset, callback);
hf3fs_usrbio_wait(ctx);
```

### Python Applications
```python
import hf3fs

# Standard file operations
with hf3fs.open('/path/to/file', 'r') as f:
    data = f.read()

# High-performance I/O
usrbio = hf3fs.usrbio.Context()
usrbio.read_async(buffer, size, offset, callback)
```

## Development Guidelines

### API Design Principles
- **Consistency**: Consistent naming and behavior across all APIs
- **Performance**: Optimize for high-throughput and low-latency scenarios
- **Safety**: Provide safe interfaces with proper error handling
- **Compatibility**: Maintain backward compatibility across versions

### Adding New Language Bindings
1. **Define FFI interface**: Create C-compatible interface
2. **Implement bindings**: Use language-specific FFI mechanisms
3. **Add error handling**: Convert C errors to language-specific exceptions
4. **Write tests**: Comprehensive test suite for new bindings
5. **Document usage**: Provide examples and documentation

### Performance Optimization
- **Profile hot paths**: Identify and optimize critical code paths
- **Minimize copies**: Use zero-copy techniques where possible
- **Batch operations**: Group operations to reduce overhead
- **Cache data**: Cache frequently accessed data appropriately

### Error Handling
- **Consistent codes**: Use consistent error codes across all APIs
- **Rich information**: Provide detailed error information
- **Language-specific**: Convert to appropriate error types for each language
- **Recovery guidance**: Provide guidance for error recovery

## Testing

### API Testing
- **Unit tests**: Test individual API functions
- **Integration tests**: Test with real 3FS backend
- **Performance tests**: Benchmark API performance
- **Compatibility tests**: Test across different versions

### Language Binding Testing
- **FFI tests**: Test foreign function interface correctness
- **Memory safety**: Test for memory leaks and corruption
- **Error propagation**: Verify proper error handling
- **Performance parity**: Ensure bindings don't add significant overhead

## Build and Packaging

### Build Integration
- **CMake integration**: Integrated with main build system
- **Cross-platform**: Support for Linux and other platforms
- **Dependency management**: Handle library dependencies properly
- **Installation**: Standard installation procedures

### Packaging
- **Shared libraries**: Package as shared libraries for runtime linking
- **Development packages**: Include headers and documentation
- **Language packages**: Separate packages for each language binding
- **Versioning**: Proper semantic versioning for API compatibility