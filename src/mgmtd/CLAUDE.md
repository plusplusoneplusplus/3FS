# Management Daemon (mgmtd)

This directory contains the management daemon implementation for 3FS, responsible for cluster membership, configuration distribution, and coordination of distributed storage operations.

## Directory Structure

### Core Service
- `service/` - Main management service implementation and state management
- `ops/` - Individual management operations and their implementations
- `background/` - Background workers for maintenance tasks
- `store/` - Persistent storage for management data

## Key Components

### Service Layer (`service/`)
Core management service infrastructure:
- **MgmtdService.cc/h** - Main management service interface
- **MgmtdOperator.cc/h** - Operation processing and dispatch
- **MgmtdData.cc/h** - Cluster data management
- **MgmtdState.cc/h** - Cluster state tracking and updates
- **MgmtdConfig.h** - Configuration structures
- **RoutingInfo.cc/h** - Cluster routing information management
- **MockMgmtd.h** - Mock implementation for testing

#### Supporting Components
- **ClientSession.h** - Client session management
- **LeaseInfo.h** - Lease management for distributed coordination
- **TargetInfo.h** - Storage target information
- **NodeInfoWrapper.h** - Node information utilities
- **WithTimestamp.h** - Timestamped data structures
- **helpers.cc/h** - Utility functions
- **updateChain.cc/h** - Chain update coordination

### Operations (`ops/`)
Individual management operations:

#### Node Management
- **RegisterNodeOperation.cc/h** - Node registration
- **UnregisterNodeOperation.cc/h** - Node deregistration
- **EnableDisableNodeOperation.cc/h** - Node state changes
- **HeartbeatOperation.cc/h** - Node heartbeat processing
- **SetNodeTagsOperation.cc/h** - Node tagging and metadata

#### Configuration Management
- **GetConfigOperation.cc/h** - Configuration retrieval
- **SetConfigOperation.cc/h** - Configuration updates
- **GetConfigVersionsOperation.cc/h** - Configuration version tracking

#### Cluster Coordination
- **GetRoutingInfoOperation.cc/h** - Routing information distribution
- **GetPrimaryMgmtdOperation.cc/h** - Primary management node selection
- **UpdateChainOperation.cc/h** - Storage chain updates

#### Storage Chain Management
- **SetChainsOperation.cc/h** - Chain configuration
- **SetChainTableOperation.cc/h** - Chain table management
- **SetPreferredTargetOrderOperation.cc/h** - Target ordering
- **RotateAsPreferredOrderOperation.cc/h** - Target rotation
- **RotateLastSrvOperation.cc/h** - Last server rotation

#### Client Session Management
- **GetClientSessionOperation.cc/h** - Session retrieval
- **ExtendClientSessionOperation.cc/h** - Session extension
- **ListClientSessionsOperation.cc/h** - Session enumeration

#### Metadata and Monitoring
- **SetUniversalTagsOperation.cc/h** - Universal tag management
- **GetUniversalTagsOperation.cc/h** - Universal tag retrieval
- **ListOrphanTargetsOperation.cc/h** - Orphaned target detection

### Background Workers (`background/`)
Automated maintenance tasks:
- **MgmtdBackgroundRunner.cc/h** - Background task coordinator
- **MgmtdHeartbeatChecker.cc/h** - Node health monitoring
- **MgmtdHeartbeater.cc/h** - Heartbeat generation
- **MgmtdChainsUpdater.cc/h** - Chain state updates
- **MgmtdClientSessionsChecker.cc/h** - Session cleanup
- **MgmtdLeaseExtender.cc/h** - Lease renewal
- **MgmtdMetricsUpdater.cc/h** - Metrics collection
- **MgmtdNewBornChainsChecker.cc/h** - New chain validation
- **MgmtdRoutingInfoVersionUpdater.cc/h** - Routing version updates
- **MgmtdTargetInfoLoader.cc/h** - Target information loading
- **MgmtdTargetInfoPersister.cc/h** - Target information persistence

### Storage Layer (`store/`)
- **MgmtdStore.cc/h** - Persistent storage interface for management data

### Server Infrastructure
- **MgmtdServer.cc/h** - Server implementation
- **MgmtdConfigFetcher.cc/h** - Configuration loading
- **MgmtdLauncherConfig.cc/h** - Launcher configuration

## Architecture

### High Availability
- **Multi-node deployment**: Multiple management daemons for redundancy
- **Leader election**: Automatic primary node selection
- **Failover**: Transparent failover to backup nodes
- **State replication**: Shared state via FoundationDB

### Cluster Coordination
- **Membership management**: Track active nodes and their capabilities
- **Configuration distribution**: Push configuration updates to all nodes
- **Service discovery**: Maintain routing information for all services
- **Health monitoring**: Monitor node health and trigger failover

### Storage Management
- **Chain management**: Create and maintain replication chains
- **Target allocation**: Assign storage targets to chains
- **Load balancing**: Distribute storage load across targets
- **Failure handling**: Rebuild chains on node failures

## Key Features

### Node Lifecycle Management
- **Registration**: Nodes register with management daemon on startup
- **Heartbeats**: Regular health checks to monitor node status
- **Graceful shutdown**: Coordinate clean node shutdowns
- **Failure detection**: Detect and handle node failures

### Configuration Management
- **Centralized config**: Single source of truth for cluster configuration
- **Hot updates**: Push configuration changes without service restart
- **Version tracking**: Track configuration versions and changes
- **Rollback support**: Ability to revert configuration changes

### Chain Replication Management
- **Chain creation**: Create new replication chains as needed
- **Chain reconfiguration**: Add/remove nodes from chains
- **Chain monitoring**: Monitor chain health and performance
- **Chain repair**: Automatically repair broken chains

### Client Session Management
- **Session tracking**: Track active client sessions
- **Lease management**: Manage client leases and renewals
- **Session cleanup**: Clean up expired or abandoned sessions

## Configuration

Management daemon configuration in `mgmtd_main.toml`:
- Service endpoints and networking
- FoundationDB connection settings
- Heartbeat and lease parameters
- Background task scheduling

## Development Guidelines

### Adding New Operations
1. Define operation interface in `ops/Include.h`
2. Implement operation class in `ops/`
3. Add serialization support in `fbs/mgmtd/`
4. Register operation in `MgmtdOperator.cc`
5. Add tests in `tests/mgmtd/`

### Background Task Development
1. Inherit from appropriate base class
2. Implement task logic with proper error handling
3. Register task in `MgmtdBackgroundRunner`
4. Add monitoring and logging
5. Test task behavior under various conditions

### State Management
- Use atomic operations for shared state
- Implement proper locking for critical sections
- Consider performance impact of state synchronization
- Handle FoundationDB transaction conflicts appropriately

### Testing
- Unit tests for individual operations
- Integration tests with mock clusters
- Fault injection tests for failure scenarios
- Performance tests for scalability