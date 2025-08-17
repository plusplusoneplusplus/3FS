use clap::Parser;
use std::path::PathBuf;

/// A chunk viewer tool for analyzing RocksDB chunk metadata.
#[derive(Parser, Debug, Clone)]
#[command(version, about, long_about = None)]
pub struct Args {
    /// Path to rocksdb.
    pub path: PathBuf,
    
    /// List detailed information for chunks of specific size bucket (e.g., "64KB", "8MB", "1GB" or raw bytes)
    #[arg(long, value_name = "SIZE")]
    pub list_size: Option<String>,
    
    /// Number of chunks to display per page (default: 20)
    #[arg(long, default_value = "20")]
    pub page_size: usize,
    
    /// Page number to display (default: 1)
    #[arg(long, default_value = "1")]
    pub page: usize,
    
    /// Show only summary statistics (default behavior)
    #[arg(long)]
    pub summary_only: bool,
    
    /// Show short chunk IDs (first 16 hex chars) for compact display
    #[arg(long)]
    pub short_ids: bool,
    
    /// Read and display content of a specific chunk by ID (hex format)
    #[arg(long, value_name = "CHUNK_ID")]
    pub read_chunk: Option<String>,
    
    /// Output format for chunk content: hex, binary, text (default: hex)
    #[arg(long, default_value = "hex")]
    pub content_format: String,
    
    /// Output chunk content to file instead of stdout
    #[arg(long, value_name = "FILE")]
    pub output_file: Option<String>,
    
    /// Show text preview (first 256 bytes as text) along with hex/binary
    #[arg(long)]
    pub show_preview: bool,
}