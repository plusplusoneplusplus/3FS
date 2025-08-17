pub mod args;
pub mod utils;
pub mod content_reader;
pub mod chunk_lister;

use chunk_engine::*;
use clap::Parser;

pub use args::Args;
pub use utils::*;
pub use content_reader::ChunkContentReader;
pub use chunk_lister::ChunkLister;

fn main() -> Result<()> {
    let args = Args::parse();

    let meta_config = MetaStoreConfig {
        rocksdb: RocksDBConfig {
            path: args.path.clone(),
            create: false,
            read_only: true,
        },
        prefix_len: 4,
    };
    let meta_store = MetaStore::open(&meta_config)?;

    // Check if user wants to read a specific chunk
    if let Some(chunk_id_hex) = args.read_chunk {
        let content_reader = ChunkContentReader::new(&meta_config.rocksdb.path)?;
        content_reader.read_chunk_content(
            &chunk_id_hex,
            &args.content_format,
            &args.output_file,
            args.show_preview,
        )?;
        return Ok(());
    }

    let chunk_lister = ChunkLister::new(meta_store);

    // Check if user wants detailed listing for a specific size
    if let Some(size_str) = args.list_size {
        let target_size = parse_size_string(&size_str)?;
        chunk_lister.list_chunks_detailed(target_size, args.page_size, args.page, args.short_ids)?;
        return Ok(());
    }

    // Default: show summary
    chunk_lister.show_summary()?;

    Ok(())
}