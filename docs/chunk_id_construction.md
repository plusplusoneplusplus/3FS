# Chunk ID Construction in 3FS

## Overview

This document explains how chunk identifiers (IDs) are constructed and used in the 3FS distributed file system. Chunk IDs are fundamental to 3FS's storage architecture, providing unique identifiers for data chunks across the distributed storage cluster.

## Chunk ID Structure

### Binary Format

Chunk IDs are **16-byte (128-bit) identifiers** with a fixed binary structure:

```
┌─────────┬─────────┬──────────────┬────────┬─────────────┐
│ Tenant  │Reserved │   Inode ID   │ Track  │Chunk Number │
│ (1 byte)│(1 byte) │   (8 bytes)  │(2 bytes)│  (4 bytes)  │
└─────────┴─────────┴──────────────┴────────┴─────────────┘
```

**Total**: 16 bytes (128 bits)

### Component Details

1. **Tenant ID** (1 byte)
   - Currently set to `0x00`
   - Reserved for future multi-tenancy support
   - Allows isolation between different tenants

2. **Reserved** (1 byte) 
   - Currently set to `0x00`
   - Reserved for future extensions
   - Ensures forward compatibility

3. **Inode ID** (8 bytes)
   - 64-bit unique file identifier
   - Assigned when file is created
   - Remains constant for the lifetime of the file
   - Stored in **big-endian format**

4. **Track ID** (2 bytes)
   - 16-bit track identifier for multi-track files
   - Currently mostly `0x0000` for single-track files
   - Reserved for future multi-track file support
   - Stored in **big-endian format**

5. **Chunk Number** (4 bytes)
   - 32-bit chunk index within the file
   - Calculated as: `file_offset / chunk_size`
   - Starts from 0 for the first chunk
   - Maximum: 4,294,967,295 chunks per file
   - Stored in **big-endian format**

## Construction Process

### Code Implementation

The chunk ID construction is implemented in `src/fbs/meta/Schema.h` and `src/fbs/meta/Schema.cc`:

```cpp
// From File::getChunkId() in Schema.cc
Result<ChunkId> File::getChunkId(InodeId id, uint64_t offset) const {
  auto chunk = offset / layout.chunkSize;
  return ChunkId(id, 0, chunk);  // inode, track, chunk_number
}

// Constructor in Schema.h
ChunkId(InodeId inode, uint16_t track, uint32_t chunk)
    : tenent_({0}),
      reserved_({0}),
      inode_(folly::bit_cast<std::array<uint8_t, 8>>(folly::Endian::big64(inode.u64()))),
      track_(folly::bit_cast<std::array<uint8_t, 2>>(folly::Endian::big16(track))),
      chunk_(folly::bit_cast<std::array<uint8_t, 4>>(folly::Endian::big32(chunk))) {}
```

### Generation Algorithm

1. **File Creation**: Each file gets a unique inode ID from the inode allocator
2. **Chunk Addressing**: For any file offset:
   ```
   chunk_index = file_offset / chunk_size
   chunk_id = ChunkId(inode_id, track_id, chunk_index)
   ```
3. **Endianness**: All multi-byte values stored in big-endian for sort order
4. **Uniqueness**: Combination of inode + track + chunk ensures global uniqueness

## Example Analysis

### Real Chunk ID Breakdown

Consider the chunk ID: `a56fa63500000000000000014801000000000000`

Breaking it down by components:
```
a5 6f a635000000000001 4801 00000000
│  │  │                │    │
│  │  │                │    └─ Chunk: 0 (first chunk)
│  │  │                └────── Track: 0x4801 = 18433
│  │  └─────────────────────── Inode: 0xa635000000000001
│  └────────────────────────── Reserved: 0x6f
└───────────────────────────── Tenant: 0xa5
```

**Interpretation**:
- This is the first chunk (index 0) of a file
- File has inode ID `0xa635000000000001` 
- Uses track `18433` (unusual, typically 0)
- Tenant ID `165` (non-zero, indicating special usage)

## Storage Layer Integration

### RocksDB Key Format

In the storage engine's RocksDB metadata store, chunk IDs are used with additional encoding:

1. **Prefix**: Metadata keys start with `MetaKey::CHUNK_META_KEY_PREFIX` (0x01)
2. **Inversion**: Each byte of the chunk ID is bitwise inverted (`!byte`)
3. **Final Key**: `[0x01] + [inverted_chunk_id_bytes]`

Example transformation:
```
Original Chunk ID: a56fa63500000000000000014801000000000000
RocksDB Key:      01 5a90 5aca ffffffff ffffff eb7e ffffffffffff
                  │  └─── bitwise inverted chunk ID bytes ────┘
                  └─ prefix
```

### Key Parsing

To retrieve the original chunk ID from RocksDB keys:
```rust
// From MetaKey::parse_chunk_meta_key()
pub fn parse_chunk_meta_key(key: &[u8]) -> Bytes {
    let mut out = Bytes::new();
    for num in &key[1..] {  // Skip prefix byte
        out.push(!num);     // Invert each byte back
    }
    out
}
```

## Design Characteristics

### Position-Based Addressing

- **Not Content-Based**: Unlike content-addressable storage, 3FS uses position-based chunk IDs
- **Benefits**:
  - Efficient random access by file offset
  - In-place updates without ID changes
  - Predictable chunk locations
  - No need for content hashing overhead

### Performance Implications

1. **Sequential Access**: Consecutive chunks have consecutive IDs (good cache locality)
2. **Random Access**: Direct calculation of chunk ID from file offset
3. **Storage Efficiency**: Fixed 16-byte overhead per chunk
4. **Sort Order**: Big-endian format maintains lexicographic ordering

### Scalability Limits

- **Max File Size**: With 32-bit chunk numbers and typical 4MB chunks:
  - Maximum: ~17 petabytes per file
- **Max Files**: 64-bit inode space allows ~18 quintillion files
- **Cluster Scale**: Design supports massive distributed deployments

## Related Components

### File System Integration

- **Metadata Service**: Maps file paths to inode IDs
- **Storage Service**: Stores chunks using these IDs
- **Client Libraries**: Calculate chunk IDs for I/O operations
- **FUSE Interface**: Transparent chunk ID management

### Replication and Consistency

- **CRAQ Protocol**: Uses chunk IDs for replication coordination
- **Chain Mapping**: Layout determines which chains store each chunk
- **Versioning**: Chunk metadata includes version information

## Implementation Files

### Core Files
- `src/fbs/meta/Schema.h` - ChunkId class definition
- `src/fbs/meta/Schema.cc` - ChunkId implementation
- `src/fbs/storage/Common.h` - Storage-layer chunk ID utilities
- `src/storage/chunk_engine/src/meta/meta_key.rs` - RocksDB key encoding

### Usage Examples
- `src/client/storage/StorageClientImpl.cc` - Client-side chunk addressing
- `src/meta/components/FileHelper.cc` - File operation chunk management
- `src/storage/chunk_engine/examples/chunk_viewer.rs` - Debugging tools

## Debugging and Tools

### Chunk Viewer Tool

The `chunk_viewer` tool in the storage engine examples can inspect chunk metadata:

```bash
# List chunks by size
cargo run --example chunk_viewer -- /path/to/meta/ --list-size 4MB

# Read specific chunk content
cargo run --example chunk_viewer -- /path/to/meta/ --read-chunk a56fa63500000000000000014801000000000000
```

### Manual Analysis

To decode a chunk ID manually:
1. Convert hex to binary (16 bytes total)
2. Split by component boundaries: [1][1][8][2][4]
3. Convert multi-byte values from big-endian
4. Interpret inode, track, and chunk numbers

## Future Considerations

### Potential Extensions

1. **Multi-tenancy**: Use tenant ID field for isolation
2. **Multi-track Files**: Leverage track ID for advanced file layouts
3. **Compression**: Additional metadata in reserved fields
4. **Encryption**: Key derivation from chunk IDs

### Compatibility

- **Schema Evolution**: Reserved fields allow backward-compatible extensions
- **Migration**: Chunk ID format changes would require cluster-wide migration
- **Interoperability**: Fixed format enables cross-language implementations

---

*Generated from investigation of 3FS codebase - December 2024*