# 3FS KV Store Table Structures

This document describes the key-value table structures used by the 3FS meta server. The meta server uses FoundationDB as its backend storage with a well-organized key-value structure using 4-byte prefixes for different data types.

## Overview

The 3FS meta server stores all filesystem metadata in FoundationDB using a prefix-based key organization. Each table type has a unique 4-character prefix that allows for efficient range scans and data organization.

## Table Definitions

### 1. INOD - Inode Table

**Purpose**: Stores file/directory metadata (similar to filesystem inodes)

**Key Structure**: `"INOD" + InodeId(8 bytes)`

**Value Structure**: Serialized `InodeData` containing:

```cpp
struct InodeData {
    variant<File, Directory, Symlink> type;
    Acl acl;                    // permissions (uid, gid, mode)
    uint16_t nlink;             // hard link count
    UtcTime atime, ctime, mtime; // timestamps
}

// File-specific data:
struct File {
    uint64_t length;            // file size
    uint64_t truncateVer;       // truncation version
    Layout layout;              // storage layout (chunk size, stripe size)
    Flags flags;                // file flags (has holes, etc.)
    uint32_t dynStripe;         // dynamic stripe configuration
}

// Directory-specific data:
struct Directory {
    InodeId parent;             // parent directory inode
    Layout layout;              // storage layout
    string name;                // directory name
    uint32_t chainAllocCounter; // chain allocation counter
    optional<Lock> lock;        // directory lock info
}

// Symlink-specific data:
struct Symlink {
    Path target;                // symbolic link target path
}
```

**Usage Examples**:
```
Key: "INOD" + 0x0000000000000001 (root directory)
Value: {type: Directory, parent: 0, name: "/", acl: {uid:0, gid:0, mode:755}, ...}

Key: "INOD" + 0x0000000000000123 (regular file)
Value: {type: File, length: 1048576, layout: {chunkSize: 1MB, stripeSize: 4}, ...}

Key: "INOD" + 0x0000000000000124 (symbolic link)
Value: {type: Symlink, target: "/usr/bin/python3", acl: {uid:1000, gid:1000, mode:777}}
```

### 2. DENT - Directory Entry Table

**Purpose**: Maps filenames to inodes (directory listings)

**Key Structure**: `"DENT" + ParentInodeId(8 bytes) + filename`

**Value Structure**: Child inode information

```cpp
struct DirEntry {
    InodeId parent;             // parent directory inode
    string name;                // entry name
    InodeId id;                 // child inode ID
    InodeType type;             // File/Directory/Symlink
    optional<Acl> dirAcl;       // ACL for directories
}
```

**Usage Examples**:
```
Key: "DENT" + 0x0000000000000001 + "home"
Value: {id: 0x0000000000000045, type: Directory, acl: {uid:0, gid:0, mode:755}}

Key: "DENT" + 0x0000000000000045 + "user.txt" 
Value: {id: 0x0000000000000123, type: File}

Key: "DENT" + 0x0000000000000045 + "symlink1"
Value: {id: 0x0000000000000124, type: Symlink}

Key: "DENT" + 0x0000000000000045 + ".hidden_file"
Value: {id: 0x0000000000000125, type: File}
```

### 3. INOS - Inode Session Table

**Purpose**: Tracks active file sessions (open files)

**Key Structure**: `"INOS" + InodeId(8 bytes) + SessionId(16 bytes UUID)`

**Value Structure**: Session metadata

```cpp
struct FileSession {
    InodeId inodeId;            // file inode
    ClientId client;            // client UUID + hostname
    Uuid sessionId;             // session UUID
    UtcTime timestamp;          // when session was created
    string payload;             // placeholder for additional data
}
```

**Usage Examples**:
```
Key: "INOS" + 0x0000000000000123 + session_uuid_1
Value: {client: {uuid: client_id, hostname: "worker01"}, timestamp: "2024-01-15T10:30:00Z"}

Key: "INOS" + 0x0000000000000123 + session_uuid_2  
Value: {client: {uuid: client_id2, hostname: "worker02"}, timestamp: "2024-01-15T10:31:00Z"}

Key: "INOS" + 0x0000000000000456 + session_uuid_3
Value: {client: {uuid: client_id3, hostname: "compute-node-05"}, timestamp: "2024-01-15T10:32:15Z"}
```

### 4. CHIT - Chain Table

**Purpose**: Storage replication chain configurations

**Key Structure**: `"CHIT" + ChainTableId + ChainTableVersion`

**Value Structure**: Chain configuration with target mappings

```cpp
struct ChainTable {
    vector<ChainId> chains;           // available chains
    string desc;                      // description
    map<range, ChainId> assignments;  // data range to chain mapping
}
```

**Usage Examples**:
```
Key: "CHIT" + table_id_1 + version_1
Value: {chains: [chain_1, chain_2, chain_3], desc: "Main storage chains"}

Key: "CHIT" + table_id_1 + version_2
Value: {chains: [chain_1, chain_2, chain_3, chain_4], desc: "Expanded storage chains"}
```

### 5. CHIF - Chain Info Table  

**Purpose**: Individual chain metadata and target lists

**Key Structure**: `"CHIF" + ChainId`

**Value Structure**: Chain configuration

```cpp
struct ChainInfo {
    ChainId chainId;                        // chain identifier
    ChainVersion version;                   // chain version
    vector<ChainTargetInfo> targets;        // storage targets in chain
    vector<TargetId> preferredTargetOrder;  // preferred target ordering
}
```

**Usage Examples**:
```
Key: "CHIF" + chain_1
Value: {targets: [target_1, target_2, target_3], preferredOrder: [target_1, target_2, target_3]}

Key: "CHIF" + chain_2
Value: {targets: [target_4, target_5, target_6], preferredOrder: [target_4, target_5, target_6]}
```

### 6. USER - User Table

**Purpose**: User authentication and authorization data

**Key Structure**: `"USER" + user_key`

**Value Structure**: User credentials and metadata (implementation details in UserStore)

### 7. CONF - Configuration Table

**Purpose**: Dynamic system configuration storage

**Key Structure**: `"CONF" + config_key`

**Value Structure**: Configuration content

```cpp
struct ConfigInfo {
    ConfigVersion version;      // configuration version
    string content;             // TOML configuration content
    string desc;                // description of changes
}
```

### 8. IDEM - Idempotent Operations Table

**Purpose**: Ensures operations aren't repeated (request deduplication)

**Key Structure**: `"IDEM" + RequestId + ClientId`

**Value Structure**: Cached operation results to prevent duplicate execution

### 9. META - Meta Distributor Table

**Purpose**: Metadata service distribution information

### 10. NODE - Node Table

**Purpose**: Cluster node information and status

### 11. SING - Single Keys

**Purpose**: Singleton values that don't belong in tables

### 12. UTGS - Universal Tags

**Purpose**: Global tagging system for cluster management

### 13. TGIF - Target Info

**Purpose**: Storage target information and status

## File Operations and Session Management

### Read Operation Flow

When a client opens a file for reading:

1. **Path Resolution**: 
   - Multiple DENT lookups to traverse path components
   - Example: `/home/user/file.txt` requires lookups for "home", "user", "file.txt"

2. **Inode Loading**:
   - INOD lookup to get file metadata and layout information
   - Load file size, permissions, and storage layout

3. **Session Creation**:
   - INOS entry created to track the open file session
   - Session includes client information and timestamp

4. **Data Access**:
   - File layout determines which storage chains to contact
   - CHIT/CHIF lookups to get current chain configurations

**Example Transaction**:
```
# Opening /home/user/document.txt for reading
1. DENT lookup: "DENT" + root_inode + "home" → home_inode
2. DENT lookup: "DENT" + home_inode + "user" → user_inode  
3. DENT lookup: "DENT" + user_inode + "document.txt" → file_inode
4. INOD lookup: "INOD" + file_inode → file metadata
5. INOS create: "INOS" + file_inode + new_session_id → session info
```

### Write Operation Flow

When a client opens a file for writing:

1. **Path Resolution and Inode Loading** (same as read)

2. **Session Creation**:
   - INOS entry created with write permissions
   - Multiple concurrent write sessions can exist

3. **Metadata Updates**:
   - INOD updates for file size changes
   - INOD updates for timestamp modifications

4. **Chain Selection**:
   - CHIT/CHIF lookups for storage chain assignment
   - New chunks may require chain allocation

**Example Transaction**:
```
# Writing to /tmp/output.dat
1. Path resolution (DENT lookups for /tmp/output.dat)
2. INOD load: get current file metadata
3. INOS create: track write session
4. CHIT lookup: get available storage chains
5. CHIF lookup: get target lists for selected chain
6. [Data write to storage targets]
7. INOD update: new file size and mtime
8. INOS update: session activity timestamp
```

### Session Cleanup

Sessions are cleaned up through:

1. **Explicit Close**: Client calls close, removes INOS entry
2. **Session Timeout**: Background cleanup of stale INOS entries
3. **Client Disconnect**: Cleanup based on client heartbeat failure

## Key Design Characteristics

- **Hierarchical Keys**: Directory structure maps naturally to key hierarchy
- **Atomic Operations**: All operations use FoundationDB ACID transactions
- **Efficient Scans**: Key prefixes enable efficient range scans for directory listings
- **Versioning**: Supports concurrent access with version tracking
- **Strong Consistency**: FoundationDB provides strong consistency guarantees
- **Scalability**: Key design supports horizontal scaling across multiple meta servers

## Performance Considerations

- **Hot Spotting**: Root directory operations can create hot spots
- **Range Scans**: Directory listings use efficient prefix-based range scans
- **Caching**: Frequently accessed inodes can be cached at the meta server level
- **Batch Operations**: Multiple related operations can be batched in single transactions