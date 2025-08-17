# Chunk Engine Examples

This directory contains example tools and utilities for working with the 3FS chunk engine and analyzing RocksDB metadata stores.

## Available Tools

### `chunk_viewer` - Chunk Metadata Analysis Tool

A comprehensive tool for analyzing and browsing chunk metadata stored in RocksDB. This tool provides both summary statistics and detailed chunk-level information with support for pagination and filtering.

## Building Examples

Build all examples:
```bash
cargo build --examples
```

Build a specific example:
```bash
cargo build --example chunk_viewer
```

## chunk_viewer - Detailed Documentation

### Overview

The `chunk_viewer` tool allows you to:
- Analyze chunk allocation statistics across different size buckets
- View detailed information about individual chunks
- Browse chunks with pagination support
- Filter chunks by size bucket
- View chunk utilization and metadata
- **Read actual chunk content** by chunk ID with multiple output formats
- Export chunk content to files for analysis

### Basic Usage

#### 1. Show Summary Statistics

Display allocation summary across all chunk size buckets:

```bash
cargo run --example chunk_viewer -- /path/to/storage/rocksdb
```

**Example Output:**
```
=== Chunk Allocation Summary ===

Available size buckets:
  64.00 KB   (65536 bytes): 150 used chunks
  4.00 MB    (4194304 bytes): 75 used chunks
  8.00 MB    (8388608 bytes): 30 used chunks

Reserved chunks per size:
  64.00 KB   (65536 bytes): 10 reserved chunks
  4.00 MB    (4194304 bytes): 5 reserved chunks

Group counts (full, active):
  64.00 KB   (65536 bytes): 12 full, 3 active groups
  4.00 MB    (4194304 bytes): 8 full, 1 active groups

Use --list-size <SIZE> to see detailed chunk information (e.g., --list-size 4MB)
Use --read-chunk <CHUNK_ID> to read actual chunk content (e.g., --read-chunk a1b2c3d4...)
```

#### 2. List Detailed Chunk Information

View detailed information for chunks in a specific size bucket:

```bash
# Using friendly size names
cargo run --example chunk_viewer -- /path/to/storage/rocksdb --list-size 4MB

# Using raw bytes
cargo run --example chunk_viewer -- /path/to/storage/rocksdb --list-size 4194304
```

**Example Output:**
```
=== Detailed Chunk Information ===
Size bucket: 4.00 MB (4194304)
Total chunks: 150
Total actual size: 580.50 MB (608554752)
Total allocated size: 600.00 MB (629145600)
Average utilization: 95.34%

Page 1/8 (showing 20 chunks)
Index    Chunk ID (hex)                                                   Alloc Size      Actual Len      Util %   Chain Ver    Chunk Ver    Uncommit
-------------------------------------------------------------------------------------------------------------------------------------------
1        a1b2c3d4e5f67890123456789abcdef0fedcba9876543210abcdef1234567890   4.00 MB         3.85 MB         96.25    1            1            No
2        b2c3d4e5f67891234567890abcdef1fedcba9876543211abcdef1234567891   4.00 MB         3.90 MB         97.50    1            2            No
...

Use --page 2 to see next page
```

#### 3. Read Chunk Content

Read and analyze the actual content stored in a specific chunk:

```bash
# Read chunk content in hex format (default)
cargo run --example chunk_viewer -- /path/to/storage/rocksdb --read-chunk a1b2c3d4e5f67890123456789abcdef0fedcba9876543210abcdef1234567890

# Read chunk content as text
cargo run --example chunk_viewer -- /path/to/storage/rocksdb --read-chunk a1b2c3d4e5f67890... --content-format text

# Read chunk content as binary (outputs raw bytes)
cargo run --example chunk_viewer -- /path/to/storage/rocksdb --read-chunk a1b2c3d4e5f67890... --content-format binary

# Show text preview with hex format
cargo run --example chunk_viewer -- /path/to/storage/rocksdb --read-chunk a1b2c3d4e5f67890... --show-preview
```

**Example Output:**
```
=== Chunk Information ===
Chunk ID: a1b2c3d4e5f67890123456789abcdef0fedcba9876543210abcdef1234567890
Size: 3.85 MB (4038912)
Allocated Size: 4.00 MB (4194304)
Utilization: 96.25%
Chain Version: 1
Chunk Version: 1
Checksum: 0x12345678
Uncommitted: No

=== Chunk Content (Hex) ===
00000000  89 50 4e 47 0d 0a 1a 0a  00 00 00 0d 49 48 44 52  |.PNG........IHDR|
00000010  00 00 04 00 00 00 03 00  08 06 00 00 00 9a 7c 8d  |..............|.|
00000020  62 00 00 00 04 67 41 4d  41 00 00 b1 8e 7c fb 51  |b....gAMA....|.Q|
...

=== Text Preview (first 256 bytes) ===
PNG

IHDR@gAMA|QbKGDIDATx޽}_k^w9w޽{ι;3gΙ3g4ՍXp@QA
TEEEQ@p@AAA"""""")""""")""""""""""""""""""""""""""""""""
... (4038656 more bytes)
```

### Command Line Options

```bash
cargo run --example chunk_viewer -- [OPTIONS] <PATH>
```

#### Arguments
- `<PATH>` - Path to the RocksDB directory (required)

#### Options
- `--list-size <SIZE>` - List detailed information for chunks of specific size bucket
  - Supports friendly names: `64KB`, `4MB`, `1GB`
  - Supports decimal values: `1.5MB`, `0.5GB`
  - Supports raw bytes: `4194304`
- `--page-size <SIZE>` - Number of chunks to display per page (default: 20)
- `--page <PAGE>` - Page number to display (default: 1)
- `--short-ids` - Show short chunk IDs (first 16 hex chars) for compact display
- `--summary-only` - Show only summary statistics (default behavior when no --list-size)
- `--read-chunk <CHUNK_ID>` - Read and display content of a specific chunk by ID (hex format)
- `--content-format <FORMAT>` - Output format for chunk content: `hex`, `binary`, `text` (default: `hex`)
- `--output-file <FILE>` - Output chunk content to file instead of stdout
- `--show-preview` - Show text preview (first 256 bytes as text) along with hex/binary

### Advanced Usage Examples

#### Pagination

Browse large datasets with pagination:

```bash
# View page 2 with 10 chunks per page
cargo run --example chunk_viewer -- /path/to/rocksdb --list-size 8MB --page 2 --page-size 10

# View page 5 with 50 chunks per page
cargo run --example chunk_viewer -- /path/to/rocksdb --list-size 4MB --page 5 --page-size 50
```

#### Compact Display

Use short chunk IDs for narrow terminals:

```bash
cargo run --example chunk_viewer -- /path/to/rocksdb --list-size 4MB --short-ids
```

**Output with Short IDs:**
```
Index    Chunk ID (hex)       Alloc Size      Actual Len      Util %   Chain Ver    Chunk Ver    Uncommit
------------------------------------------------------------------------------------------------------
1        a1b2c3d4e5f67890...  4.00 MB         3.85 MB         96.25    1            1            No
2        b2c3d4e5f67891234...  4.00 MB         3.90 MB         97.50    1            2            No
```

#### Different Size Buckets

Analyze different chunk size categories:

```bash
# Small chunks
cargo run --example chunk_viewer -- /path/to/rocksdb --list-size 64KB

# Medium chunks  
cargo run --example chunk_viewer -- /path/to/rocksdb --list-size 4MB

# Large chunks
cargo run --example chunk_viewer -- /path/to/rocksdb --list-size 16MB

# Using decimal sizes
cargo run --example chunk_viewer -- /path/to/rocksdb --list-size 1.5MB
```

#### Chunk Content Analysis

Read and analyze chunk content in different formats:

```bash
# Read chunk content in hex format with text preview
cargo run --example chunk_viewer -- /path/to/rocksdb \
  --read-chunk a1b2c3d4e5f67890123456789abcdef0fedcba9876543210abcdef1234567890 \
  --show-preview

# Export chunk content to binary file for external analysis
cargo run --example chunk_viewer -- /path/to/rocksdb \
  --read-chunk a1b2c3d4e5f67890... \
  --content-format binary \
  --output-file chunk_content.bin

# View chunk content as text (useful for text files, logs, JSON, etc.)
cargo run --example chunk_viewer -- /path/to/rocksdb \
  --read-chunk a1b2c3d4e5f67890... \
  --content-format text

# Export hex dump to file for documentation
cargo run --example chunk_viewer -- /path/to/rocksdb \
  --read-chunk a1b2c3d4e5f67890... \
  --content-format hex \
  --output-file chunk_hexdump.txt
```

### Understanding the Output

#### Summary Statistics
- **Used chunks**: Chunks actively storing data
- **Reserved chunks**: Chunks allocated but not yet storing data
- **Group counts**: Storage group statistics (full vs active groups)

#### Detailed Chunk Information
- **Index**: Sequential number for the current page
- **Chunk ID**: Unique identifier for the chunk (full 64-char hex or short 16-char)
- **Alloc Size**: Size bucket the chunk belongs to
- **Actual Len**: Real data size stored in the chunk
- **Util %**: Utilization percentage (Actual Len / Alloc Size * 100)
- **Chain Ver**: CRAQ chain version
- **Chunk Ver**: Individual chunk version
- **Uncommit**: Whether the chunk has uncommitted changes

#### Size Format
All sizes are displayed in human-readable format:
- Bytes: `1023 B`
- Kilobytes: `64.00 KB`
- Megabytes: `4.00 MB`
- Gigabytes: `1.50 GB`

### Use Cases

#### 1. Storage Utilization Analysis
```bash
# Check overall allocation patterns
cargo run --example chunk_viewer -- /path/to/rocksdb

# Analyze specific size bucket efficiency
cargo run --example chunk_viewer -- /path/to/rocksdb --list-size 4MB
```

#### 2. Debugging Storage Issues
```bash
# Find chunks with low utilization
cargo run --example chunk_viewer -- /path/to/rocksdb --list-size 8MB --page-size 100

# Look for uncommitted chunks
cargo run --example chunk_viewer -- /path/to/rocksdb --list-size 4MB | grep "Yes"
```

#### 3. Performance Analysis
```bash
# Check chunk distribution across size buckets
cargo run --example chunk_viewer -- /path/to/rocksdb

# Analyze chunk fragmentation
cargo run --example chunk_viewer -- /path/to/rocksdb --list-size 64KB
```

#### 4. Data Migration Planning
```bash
# Understand current storage patterns before migration
cargo run --example chunk_viewer -- /path/to/rocksdb

# Analyze specific size categories for migration planning
cargo run --example chunk_viewer -- /path/to/rocksdb --list-size 16MB
```

#### 5. Content Verification and Debugging
```bash
# Verify chunk data integrity by reading content
cargo run --example chunk_viewer -- /path/to/rocksdb --read-chunk <chunk_id> --content-format hex

# Export corrupted chunks for analysis
cargo run --example chunk_viewer -- /path/to/rocksdb --read-chunk <chunk_id> --content-format binary --output-file corrupted_chunk.bin

# Analyze log files or text data stored in chunks
cargo run --example chunk_viewer -- /path/to/rocksdb --read-chunk <chunk_id> --content-format text

# Quick preview of chunk content to identify data types
cargo run --example chunk_viewer -- /path/to/rocksdb --read-chunk <chunk_id> --show-preview
```

### Troubleshooting

#### Common Issues

**Error: RocksDB fail: Not supported operation in read only mode**
- This usually means the RocksDB path doesn't exist or is invalid
- Verify the path points to a valid RocksDB directory
- Check that the storage service has created the database

**Error: Invalid size format**
- Use supported formats: `64KB`, `4MB`, `1GB`, or raw bytes
- Examples: `--list-size 4MB`, `--list-size 4194304`

**No chunks found for size bucket**
- The specified size bucket has no chunks
- Run without `--list-size` to see available size buckets
- Check if the storage service is using different chunk sizes

**Output too wide for terminal**
- Use `--short-ids` flag for compact display
- Reduce `--page-size` for fewer rows
- Use a wider terminal or redirect output to a file

#### Tips

1. **Start with summary**: Always run without `--list-size` first to understand available size buckets
2. **Use pagination**: For large datasets, use reasonable page sizes (20-50 chunks)
3. **Save output**: Redirect to file for large reports: `cargo run --example chunk_viewer -- /path/to/rocksdb > report.txt`
4. **Monitor utilization**: Low utilization percentages may indicate storage inefficiency

### Integration with Other Tools

The chunk_viewer output can be used with standard Unix tools:

```bash
# Count chunks by utilization
cargo run --example chunk_viewer -- /path/to/rocksdb --list-size 4MB --page-size 1000 | grep -E "^\d+" | wc -l

# Find low utilization chunks
cargo run --example chunk_viewer -- /path/to/rocksdb --list-size 4MB --page-size 1000 | awk '$5 < 50.0'

# Save detailed report
cargo run --example chunk_viewer -- /path/to/rocksdb --list-size 4MB > chunk_report_4mb.txt
```

## Contributing

When adding new examples:

1. Add the example to `examples/` directory
2. Update this README with documentation
3. Follow the existing naming conventions
4. Include comprehensive help text in the tool
5. Add usage examples and common use cases

## See Also

- [Chunk Engine Architecture](../docs/architecture.drawio.svg)
- [Chunk Engine Documentation](../README.md)
- [3FS Storage Documentation](../../CLAUDE.md)