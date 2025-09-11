# Distributor and Decentralized Node Assignment in 3FS

**Last Updated:** 2025-09-10  
**Component:** `src/meta/components/Distributor.cc/h`  
**Purpose:** Document the decentralized metadata server assignment and consistent hashing mechanism

## Overview

The **Distributor** component in 3FS implements a **decentralized node assignment system** for metadata servers. Unlike traditional centralized assignment with a dedicated cluster manager, each metadata server self-registers and the cluster reaches consensus through a shared transactional key-value store.

**Note**: This is not truly peer-to-peer (P2P) as it relies on a shared KV store for coordination, but it is decentralized in that no single node manages cluster membership.

## Architecture

### Components

1. **Distributor** (`src/meta/components/Distributor.cc/h`) - Core assignment logic
2. **Weight Class** (`src/fbs/meta/Utils.h`) - Consistent hashing for inode-to-server mapping
3. **KV Store** - Persistent cluster state storage (FoundationDB or Custom KV)
4. **Versionstamps** - Distributed consistency and conflict detection

### Key Concepts

- **Self-Registration**: Servers add themselves to the cluster without central coordination
- **Consistent Hashing**: Deterministic inode-to-server mapping  
- **Shared State Consensus**: Coordination through transactional KV store
- **Automatic Failure Detection**: Timeout-based dead server removal

## Decentralized Assignment Process

### 1. Server Startup and Self-Registration

```cpp
// In Distributor::start() - Each server registers itself
void Distributor::start(CPUExecutorGroup &exec) {
  auto result = folly::coro::blockingWait(update(false));  // Self-register
  // Start background updater...
}
```

**Process:**
1. **Load current cluster state** from KV store
2. **Add self to active server list** if not present
3. **Write presence marker** with versionstamp
4. **Update shared server map** atomically
5. **Start background heartbeat** process

### 2. Distributed State Management

**Key Data Structures:**
```cpp
struct ServerMap {
  std::vector<flat::NodeId> active;  // List of active metadata servers
};

struct LatestServerMap : ServerMap {
  kv::Versionstamp versionstamp{0};  // Consistency tracking
};
```

**Cached vs Persistent State:**
- `latest_` - In-memory cached server map with read locks
- KV Store - Persistent shared state across all metadata servers

### 3. Consistent Hashing for Load Balancing

```cpp
flat::NodeId Distributor::getServer(InodeId inodeId) {
  auto guard = latest_.rlock();
  return Weight::select(guard->active, inodeId);  // Consistent hashing
}
```

**Weight Algorithm:**
- **Input**: List of active servers + inode ID
- **Process**: Calculate hash weight for each (server, inode) pair using MurmurHash3
- **Output**: Server with highest weight for that inode
- **Properties**: Deterministic, load-balanced, fault-tolerant

## KV Store Side View

### Key Layout

The Distributor uses several key patterns in the KV store:

```
Prefix: "META" (kv::KeyPrefix::MetaDistributor)

Keys:
├── "META"                       - Server map (active server list)
├── "META-00000100"             - Server presence (NodeId 100)  
├── "META-00000200"             - Server presence (NodeId 200)
└── "\xff/metadataVersion"      - Global metadata version
```

#### Key Format Details

**Prefix Definition:**
```cpp
// From src/common/kv/KeyPrefix-def.h
DEFINE_PREFIX(MetaDistributor, "META")

// Stored as 4-byte binary value:
makePrefixValue("META") = 'M' + ('E' << 8) + ('T' << 16) + ('A' << 24)
                        = 0x4154454D (little-endian: M-E-T-A)
```

**Key Generation:**
```cpp
// Server map key
kMapKey = kPrefix = "META"

// Per-server presence keys  
PerServerKey::pack(NodeId(100)) = fmt::format("{}-{:08d}", "META", 100)
                                = "META-00000100"

// Global metadata version (FoundationDB system key)
kMetadataVersionKey = "\xff/metadataVersion"
```

**Key-Value Examples:**
| Purpose | Key | Value Type | Example Value |
|---------|-----|------------|---------------|
| Server Map | `"META"` | Serialized ServerMap | `{"active": [100, 200, 300]}` |
| Node 100 presence | `"META-00000100"` | 10-byte versionstamp | `[0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A]` |
| Node 200 presence | `"META-00000200"` | 10-byte versionstamp | `[0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0B]` |
| Global version | `"\xff/metadataVersion"` | 10-byte versionstamp | `[0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0C]` |

### Storage Operations

#### 1. Server Registration
```cpp
// Write server presence with versionstamp
auto key = PerServerKey::pack(nodeId_);  // "\x01\x06-{nodeId:08d}"
std::array<char, 10> buf{0};            // 10-byte versionstamp buffer
txn.setVersionstampedValue(key, {buf.data(), buf.size()}, 0);
```

#### 2. Server Map Update
```cpp
// Update global server map
ServerMap map{std::vector<flat::NodeId>(active.begin(), active.end())};
auto serialized = serde::serialize(map);
txn.set(kMapKey, serialized);  // kMapKey = "\x01\x06"
```

#### 3. Metadata Version Tracking
```cpp
// Update global metadata version
std::array<char, 10> buf{0};
txn.setVersionstampedValue(kv::kMetadataVersionKey, {buf.data(), buf.size()}, 0);
// kMetadataVersionKey = "\xff/metadataVersion"
```

### Data Flow

```
Startup:
NodeA ──┐
NodeB ──┼─→ KV Store ──→ Shared Server Map: [A, B, C]
NodeC ──┘

Runtime:
Client Request(InodeId=12345)
    ↓
Distributor::getServer(12345)
    ↓
Weight::select([A,B,C], 12345)
    ↓
MurmurHash3(A+12345), MurmurHash3(B+12345), MurmurHash3(C+12345)
    ↓
Return server with highest weight → NodeB
```

### Consistency Mechanisms

#### 1. Versionstamps for Conflict Detection
```cpp
// Load current version from KV store
auto versionstamp = co_await loadVersion(txn);
auto rlock = latest_.rlock();

// Check for stale cached data
if (*versionstamp > rlock->versionstamp) {
  // Reload server map from KV store
  rlock.unlock();
  CO_RETURN_ON_ERROR(co_await loadServerMap(txn, false));
  rlock = latest_.rlock();
}
```

#### 2. Atomic Server Map Updates
```cpp
// All changes happen in single transaction
auto txn = kvEngine_->createReadWriteTransaction();

// 1. Register self
txn.setVersionstampedValue(perServerKey, versionstampBuffer, 0);

// 2. Update server map
txn.set(kMapKey, serializedServerMap);

// 3. Update metadata version
txn.setVersionstampedValue(kMetadataVersionKey, versionstampBuffer, 0);

// Commit atomically
txn.commit();
```

## Failure Detection and Recovery

### Background Monitoring

```cpp
// Periodic update process
bgRunner_->start("distributor_update", [this]() -> CoTask<void> {
  auto result = co_await update(false);
  // Runs every config_.update_interval() (default: 1 second)
});
```

### Dead Server Detection

```cpp
// Remove servers that haven't updated recently
auto timeout = config_.timeout();  // Default: 30 seconds
for (auto nodeId : current->active) {
  auto it = servers.find(nodeId);
  if (it == servers.end() || SteadyClock::now() - it->second.lastUpdate > timeout) {
    dead.insert(nodeId);  // Mark as dead
  }
}
```

### Recovery Process

1. **Detect dead servers** via timeout
2. **Remove from active list**: `active.erase(deadServer)`
3. **Update server map atomically**
4. **Existing inodes automatically remap** via consistent hashing

## Configuration

### Application Config (`configs/meta_main_app.toml`)
```toml
allow_empty_node_id = true
node_id = 0  # This server's unique identifier
```

### Distributor Config
```cpp
struct Config : ConfigBase<Config> {
  CONFIG_HOT_UPDATED_ITEM(update_interval, 1_s);   // Background update frequency
  CONFIG_HOT_UPDATED_ITEM(timeout, 30_s);          // Server failure timeout
};
```

## Current Issues and Limitations

### 1. Missing Versionstamp Implementation

**Problem**: Custom KV store doesn't implement `setVersionstampedValue()`
```cpp
// In CustomTransaction.cc - Currently just a stub!
CoTryTask<void> CustomTransaction::setVersionstampedValue(...) {
  // TODO: Implement actual call to Rust client for versionstamped value
  co_return folly::unit;  // Does nothing!
}
```

**Impact**:
- Consistency checks fail
- Dead server cleanup doesn't work
- Servers crash with zero versionstamps

### 2. Bootstrap Chicken-and-Egg Problem

**Scenario**: First server starting with empty KV store
- `loadVersion()` returns zero versionstamp (no data)
- Server tries to register with zero versionstamp
- Consistency assertions fail

### 3. NodeId Configuration Mismatch

**Problem**: Config has `node_id = 0` but crash shows `NodeId(100)`
- Suggests stale data in KV store
- Previous server registered with different ID

**Specific Keys in Crash Scenario:**
```cpp
// Your crash shows these keys are involved:
"META"           → Contains ServerMap with active: [NodeId(100)]
"META-00000100"  → Should contain versionstamp for Node 100 (likely empty/stale)
"\xff/metadataVersion" → Returns zero versionstamp (unimplemented)

// But your config expects:
"META-00000000"  → Key for NodeId(0) from config
```

**Diagnosis**: The KV store contains data from a previous run where a server with `NodeId(100)` was active, but your current configuration is set to `node_id = 0`. The consistency check fails because the cached and loaded server maps don't match when versionstamps are zero.

## Debugging and Troubleshooting

### Key Debugging Points

1. **Check current server map**:
   ```bash
   # In KV store, key "\x01\x06" contains serialized server list
   ```

2. **Check server presence markers**:
   ```bash
   # Keys like "\x01\x06-00000100" show which servers are registered
   ```

3. **Check metadata version**:
   ```bash
   # Key "\xff/metadataVersion" should contain 10-byte versionstamp
   ```

### Common Issues

| Issue | Symptom | Cause | Solution |
|-------|---------|-------|----------|
| Zero versionstamp crash | `versionstamp 0,0,0,0,0,0,0,0,0,0` | Missing versionstamp implementation | Implement `setVersionstampedValue()` |
| NodeId mismatch | Config nodeId ≠ crash nodeId | Stale KV data | Clear KV or update config |
| Server not in map | `not in server map, create a new map` | First startup or timeout | Normal - server self-registers |
| DFATAL active mismatch | `active != rlock->active` | Inconsistent cached vs persistent state | Check versionstamp implementation |

## Development Guidelines

### Adding New Metadata Servers

1. **Assign unique NodeId** in config
2. **Start server** - self-registration is automatic
3. **Verify registration** in server map
4. **Test inode distribution** with consistent hashing

### Modifying Server Assignment Logic

1. **Understand consistent hashing** implications
2. **Test with multiple servers** to verify load balancing
3. **Handle server failures** gracefully
4. **Maintain backward compatibility** in Weight algorithm

### Testing Decentralized Assignment

```cpp
// Key test scenarios:
// 1. Single server startup
// 2. Multiple servers joining
// 3. Server failure and recovery
// 4. Consistent inode mapping
// 5. Concurrent server registration
```

## References

- **Implementation**: `src/meta/components/Distributor.cc/h`
- **Consistent Hashing**: `src/fbs/meta/Utils.h` (Weight class)
- **KV Interface**: `src/common/kv/ITransaction.h`
- **Configuration**: `src/core/app/ServerAppConfig.h`
- **Issue Tracking**: `ISSUE_versionstamp_support.md`

---

This decentralized assignment system provides **automatic load balancing**, **fault tolerance**, and **horizontal scalability** for 3FS metadata services without requiring a dedicated cluster manager, though it does rely on shared KV store infrastructure for coordination.