# Chunk Size and Stripe Size Calculation in 3FS

This document explains how chunkSize and stripeSize are determined when files are created through the FUSE interface (e.g., during `cp` commands).

## Overview

3FS uses a **layout inheritance model** where new files automatically inherit chunk and stripe configuration from their parent directory. There is no dynamic calculation - values are inherited through the directory hierarchy.

## Default Values

### Root Directory Layout
When the metadata server starts, the root directory gets a hardcoded default layout:

```cpp
// src/meta/service/MetaServer.cc:70
rootLayout = Layout::newEmpty(ChainTableId(1), 512 << 10, 1);
//                                            ^^^^^^^^  ^
//                                         512KB chunk, 1 stripe
```

- **chunkSize**: `512 << 10` = **512KB**
- **stripeSize**: `1` (no striping across multiple chains)

## Layout Inheritance Process

### 1. FUSE File Creation
When a file is created via FUSE (e.g., `cp source dest`):

```cpp
// src/fuse/FuseOps.cc:1885
d.metaClient->create(userInfo, parent, name, session, 
                    meta::Permission(mode & ALLPERMS), fi->flags)
```

**Key Point**: FUSE client does NOT specify any layout parameters.

### 2. Meta Service Layout Resolution
The metadata service handles layout inheritance:

```cpp
// src/meta/store/ops/BatchOperation.cc:463-466
auto layout = req.layout;
if (!layout.has_value()) {
  // user doesn't specify layout, inherit parent directory's layout.
  layout = parent.asDirectory().layout;
}
```

### 3. Inheritance Chain Example
```
Root Directory
├── Layout: ChunkSize=512KB, StripeSize=1
├── /home Directory  
│   ├── Layout: ChunkSize=512KB, StripeSize=1 (inherited)
│   └── /home/user Directory
│       ├── Layout: ChunkSize=512KB, StripeSize=1 (inherited)
│       └── new_file.txt
│           └── Layout: ChunkSize=512KB, StripeSize=1 (inherited)
```

## Configuration and Overrides

### Admin CLI Tools
Layout can be customized using command-line tools:

```bash
# src/tools/commands/Layout.cc
--chunk_size="4MB"    # Custom chunk size
--stripe_size=8       # Custom stripe size for parallelism
```

### FUSE Configuration
FUSE client can set limits but doesn't change inheritance:

```toml
# configs/hf3fs_fuse_main.toml
chunk_size_limit = '0'  # 0 = no limit, use inherited chunk size
```

### Directory-Specific Layouts
Administrators can set custom layouts on specific directories, which will then be inherited by all files created within those directories:

```cpp
// Example: High-performance directory with larger chunks and striping
Layout::newEmpty(ChainTableId(1), 4 << 20, 8);  // 4MB chunks, 8-way striping
```

## Implementation Details

### Layout Storage
- **Directory inodes** store layout configuration in their `Layout` field
- **File inodes** inherit and store the layout they were created with
- **No dynamic recalculation** - layout is determined once at creation time

### Chunk ID Generation
Chunks are identified using the inherited layout:

```cpp
// src/fbs/meta/Schema.cc:66
auto chunk = offset / layout.chunkSize;

// ChunkId format: [inode_id] + [track_id] + [chunk_number]
ChunkId(InodeId inode, uint16_t track, uint32_t chunk)
```

### Storage Chain Assignment
Files use their inherited layout to determine storage placement:

- **stripeSize = 1**: All chunks go to the same storage chain
- **stripeSize > 1**: Chunks are distributed across multiple chains for parallelism

## Benefits of Inheritance Model

1. **Consistency**: All files in a directory tree have consistent chunk parameters
2. **Performance**: No calculation overhead during file creation
3. **Flexibility**: Different directory trees can have optimized layouts
4. **Simplicity**: Clear inheritance rules, easy to reason about
5. **Scalability**: Works efficiently regardless of file system size

## Configuration Recommendations

### General Purpose
- **chunkSize**: 512KB - 4MB (balance between overhead and parallelism)
- **stripeSize**: 1-4 (depending on workload parallelism needs)

### High-Performance Workloads
- **chunkSize**: 4MB - 16MB (reduce metadata overhead)
- **stripeSize**: 8-16 (maximize parallel I/O)

### Small File Workloads
- **chunkSize**: 64KB - 512KB (reduce internal fragmentation)
- **stripeSize**: 1 (minimize coordination overhead)

## Updating Directory Chunk Size

### Using Admin CLI Tool
You can update the chunk size and stripe size for a directory using the admin CLI tool. This will affect all new files created within that directory and its subdirectories.

```bash
# Update chunk size for a specific directory
./build/src/tools/admin \
  --set_dir_layout \
  --dir_path="/path/to/directory" \
  --chunk_size="4MB" \
  --stripe_size=4 \
  --chain_table_id=1

# Examples:
# Set 1MB chunks with no striping
./build/src/tools/admin \
  --set_dir_layout \
  --dir_path="/high_performance" \
  --chunk_size="1MB" \
  --stripe_size=1 \
  --chain_table_id=1

# Set 8MB chunks with 8-way striping for maximum throughput
./build/src/tools/admin \
  --set_dir_layout \
  --dir_path="/ml_datasets" \
  --chunk_size="8MB" \
  --stripe_size=8 \
  --chain_table_id=1
```

### Command Parameters
- `--dir_path`: Path to the directory whose layout should be updated
- `--chunk_size`: New chunk size (supports units: KB, MB, GB)
- `--stripe_size`: Number of storage chains to stripe across (1 = no striping)
- `--chain_table_id`: Chain table identifier (typically 1)
- `--chain_table_ver`: Chain table version (defaults to 0)

### Important Notes
- **Existing files are NOT affected** - only new files created after the layout change
- **Inheritance applies** - subdirectories will inherit the new layout unless explicitly overridden
- **Permissions required** - admin privileges needed to modify directory layouts
- **Directory must exist** - cannot set layout on non-existent directories

### Programmatic API
The layout update functionality is also available via the MetaClient API:

```cpp
// src/client/meta/MetaClient.h:199
CoTryTask<Inode> setLayout(const UserInfo &userInfo, InodeId inodeId, 
                          const std::optional<Path> &path, Layout layout);
```

## Code References

- Layout inheritance: `src/meta/store/ops/BatchOperation.cc:463-466`
- Root layout initialization: `src/meta/service/MetaServer.cc:70`
- FUSE file creation: `src/fuse/FuseOps.cc:1885`
- Directory operations: `src/meta/store/ops/Mkdirs.cc:62-63`
- Layout definitions: `src/fbs/meta/Schema.h:71-169`
- Set directory layout: `src/tools/commands/SetDirLayout.cc:9-13`
- Layout flag parsing: `src/tools/commands/Layout.cc:10-37`
- Admin CLI entry point: `src/tools/admin.cc:94-95`