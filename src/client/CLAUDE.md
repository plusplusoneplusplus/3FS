# Client Components

This directory contains all client-side components for 3FS, providing various interfaces and utilities for applications to interact with the distributed file system.

## Directory Structure

### Core Client Libraries
- `core/` - Core client interface and base functionality
- `meta/` - Metadata service client with server selection strategies
- `mgmtd/` - Management daemon client for cluster operations
- `storage/` - Storage service client with target selection and messaging

### Command Line Interface
- `cli/` - Administrative CLI tools and commands
  - `admin/` - Admin-specific commands (cluster management, debugging, maintenance)
  - `common/` - Shared CLI utilities (parsing, printing, dispatching)

### Specialized Components
- `bin/` - Binary executables
- `trash_cleaner/` - Rust-based trash cleaning service

## Key Components

### MetaClient (`meta/`)
Handles communication with metadata services:
- **MetaClient.cc/h** - Main client interface for file system metadata operations
- **ServerSelectionStrategy.cc/h** - Strategies for selecting metadata servers (round-robin, failover)

### MgmtdClient (`mgmtd/`)
Manages cluster-level operations:
- **MgmtdClient.cc/h** - Primary management client
- **RoutingInfo.cc/h** - Cluster routing and topology information
- **ServiceInfo.h** - Service discovery and registration

### StorageClient (`storage/`)
Handles data storage operations:
- **StorageClient.cc/h** - Main storage interface
- **StorageClientImpl.cc/h** - Implementation with CRAQ protocol support
- **StorageMessenger.cc/h** - Network communication layer
- **TargetSelection.cc/h** - Storage target selection algorithms
- **UpdateChannelAllocator.cc/h** - Channel allocation for updates

### Administrative CLI (`cli/admin/`)
Comprehensive command-line tools for:
- **Cluster Management**: Node registration, configuration, chain management
- **Data Operations**: File/directory operations, benchmarking, verification
- **Debugging**: Dump operations, metadata inspection, chunk analysis
- **Maintenance**: GC operations, session management, target lifecycle

## Build Configuration

Each component has its own CMakeLists.txt with dependencies on:
- Core 3FS libraries (`src/common/`, `src/fbs/`)
- Third-party libraries (FoundationDB, networking, serialization)
- System libraries (RDMA, FUSE, etc.)

## Usage Patterns

### Client Application Integration
1. **Native API**: Use `CoreClient` for high-performance applications
2. **POSIX Interface**: Mount via FUSE client for standard applications
3. **Administrative Tasks**: Use CLI tools for cluster management

### Development Guidelines
- Client components are stateless where possible
- Error handling follows 3FS status code conventions
- Network operations use async/coroutine patterns
- Configuration follows TOML-based patterns

## Testing

Client components include comprehensive tests in the `tests/client/` directory:
- Unit tests for individual client classes
- Integration tests with mock services
- Stress tests for client-server interactions