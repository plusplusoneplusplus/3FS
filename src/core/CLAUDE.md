# Core Services

This directory contains the core service infrastructure for 3FS, providing shared functionality across all services including user management, configuration, and common service operations.

## Directory Structure

### Service Framework (`service/`)
- **CoreService.cc/h** - Main core service implementation
- **ops/** - Core service operations (configuration, system management)

### Application Infrastructure (`app/`)
- **ServerLauncher.cc/h** - Generic server launcher for all 3FS services
- **ServerAppConfig.cc/h** - Server application configuration management
- **ServerLauncherConfig.cc/h** - Launcher-specific configuration
- **ServerEnv.cc/h** - Server environment setup and management
- **LauncherUtils.cc/h** - Utilities for service launching
- **MgmtdClientFetcher.cc/h** - Management daemon client integration
- **ServerMgmtdClientFetcher.cc/h** - Server-side mgmtd client management

### User Management (`user/`)
- **UserStore.cc/h** - User data storage and management
- **UserStoreEx.cc/h** - Extended user store functionality
- **UserToken.cc/h** - User authentication token management
- **UserCache.h** - User data caching

### Utilities (`utils/`)
- **ServiceOperation.h** - Service operation framework
- **runOp.h** - Operation execution utilities

## Key Components

### Core Service (`service/CoreService.cc/h`)
Central service providing:
- **Configuration management**: Hot-reload of service configurations
- **System information**: Version info, build details, system status
- **Health monitoring**: Service health checks and status reporting
- **Echo operations**: Connectivity testing and debugging
- **Shutdown coordination**: Graceful service shutdown

#### Core Operations (`service/ops/`)
- **EchoOperation.h** - Network connectivity testing
- **GetConfigOperation.h** - Configuration retrieval
- **GetLastConfigUpdateRecordOperation.h** - Configuration change tracking
- **HotUpdateConfigOperation.h** - Dynamic configuration updates
- **RenderConfigOperation.h** - Configuration rendering and validation
- **ShutdownOperation.h** - Graceful service shutdown

### Server Launcher Framework (`app/`)
Standardized service launching infrastructure:
- **ServerLauncher.cc/h** - Main launcher implementation
  - Configuration loading and validation
  - Service initialization and startup
  - Signal handling and shutdown coordination
  - Error handling and recovery

- **Configuration Management**
  - **ServerAppConfig.cc/h** - Application-level configuration
  - **ServerLauncherConfig.cc/h** - Launcher configuration
  - **ServerEnv.cc/h** - Environment variable and system setup

- **Integration Components**
  - **MgmtdClientFetcher.cc/h** - Connect to management daemon
  - **LauncherUtils.cc/h** - Common launcher utilities

### User Management System (`user/`)
Comprehensive user authentication and authorization:
- **UserStore.cc/h** - Core user data management
  - User creation, modification, deletion
  - Password hashing and verification
  - User metadata storage
  - Integration with FoundationDB for persistence

- **UserStoreEx.cc/h** - Extended user functionality
  - Advanced user queries
  - Bulk user operations
  - User group management
  - Permission inheritance

- **UserToken.cc/h** - Authentication token system
  - JWT-style token generation
  - Token validation and expiration
  - Token refresh mechanisms
  - Session management integration

- **UserCache.h** - User data caching
  - In-memory user data caching
  - Cache invalidation strategies
  - Performance optimization for user lookups

## Architecture

### Service Framework
- **Standardized lifecycle**: Common startup, configuration, and shutdown patterns
- **Configuration management**: Hot-reload capabilities for all services
- **Error handling**: Consistent error handling across all services
- **Monitoring integration**: Built-in metrics and health monitoring

### Multi-Service Design
- **Shared components**: Common functionality across all 3FS services
- **Service discovery**: Integration with management daemon
- **Configuration distribution**: Centralized configuration management
- **Operational consistency**: Consistent operational behavior

### User Management
- **Authentication**: Secure user authentication with token-based sessions
- **Authorization**: Role-based access control (RBAC)
- **Scalability**: Efficient user data caching and storage
- **Security**: Secure password storage and token management

## Key Features

### Configuration Management
- **Hot reloading**: Update configuration without service restart
- **Validation**: Comprehensive configuration validation
- **Versioning**: Track configuration changes and rollback support
- **Distribution**: Centralized configuration with local caching

### Service Lifecycle
- **Standardized startup**: Common initialization patterns
- **Graceful shutdown**: Clean resource cleanup on shutdown
- **Signal handling**: Proper handling of system signals
- **Health monitoring**: Continuous health status reporting

### User System
- **Secure authentication**: Modern authentication with JWT-style tokens
- **Permission system**: Fine-grained permission management
- **Session management**: Efficient session tracking
- **Caching**: High-performance user data access

### Operations Support
- **Echo operations**: Network connectivity testing
- **System information**: Runtime system and build information
- **Configuration debugging**: Tools for configuration troubleshooting
- **Health checks**: Comprehensive service health monitoring

## Usage Patterns

### Service Implementation
1. **Inherit from ApplicationBase**: Use common application framework
2. **Implement Core operations**: Add service-specific operations
3. **Configure launcher**: Set up service-specific launch configuration
4. **Integrate user system**: Add authentication and authorization
5. **Add monitoring**: Implement health checks and metrics

### Configuration Management
```cpp
// Hot reload configuration
auto config_op = std::make_shared<HotUpdateConfigOperation>();
config_op->setNewConfig(updated_config);
service->executeOperation(config_op);
```

### User Management
```cpp
// Authenticate user
auto user_store = getUserStore();
auto token = user_store->authenticateUser(username, password);
if (token.isValid()) {
    // User authenticated successfully
}
```

## Development Guidelines

### Adding New Core Operations
1. **Define operation interface** in `service/ops/`
2. **Implement operation logic** with proper error handling
3. **Add serialization support** in `fbs/core/`
4. **Register operation** in CoreService
5. **Add tests** and documentation

### Service Integration
- **Use ServerLauncher**: Leverage common launcher framework
- **Implement health checks**: Add service-specific health monitoring
- **Support hot reload**: Make configuration changes without restart
- **Add user support**: Integrate with user management system

### Configuration Design
- **Use TOML format**: Consistent with rest of 3FS
- **Support validation**: Add configuration validation logic
- **Document options**: Comprehensive configuration documentation
- **Default values**: Provide sensible defaults for all options

### User System Extension
- **Extend UserStore**: Add new user-related functionality
- **Cache appropriately**: Use UserCache for performance
- **Secure by default**: Follow security best practices
- **Audit changes**: Log all user management operations

## Testing

### Core Service Testing
- **Unit tests**: Test individual operations and components
- **Integration tests**: Test with real backend services
- **Configuration tests**: Test configuration loading and validation
- **User system tests**: Test authentication and authorization

### Launcher Testing
- **Startup tests**: Test service initialization
- **Shutdown tests**: Test graceful shutdown behavior
- **Signal tests**: Test signal handling
- **Configuration tests**: Test configuration hot-reload

## Configuration

Core services are configured through TOML files with sections for:
- **Service endpoints**: Network configuration
- **User management**: Authentication and authorization settings
- **Monitoring**: Health check and metrics configuration
- **Logging**: Log levels and output configuration

Example configuration:
```toml
[core]
listen_address = "0.0.0.0:8080"
max_connections = 1000

[user]
token_expiry_hours = 24
password_min_length = 8

[monitoring]
health_check_interval_ms = 30000
metrics_port = 9090
```