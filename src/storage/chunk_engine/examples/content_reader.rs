use chunk_engine::*;
use std::{
    fs::File,
    io::Write,
    path::PathBuf,
};

use super::utils::{parse_hex_chunk_id, format_size, format_hex_output};

pub struct ChunkContentReader {
    meta_store: MetaStore,
    engine: Engine,
}

impl ChunkContentReader {
    pub fn new(rocksdb_path: &PathBuf) -> Result<Self> {
        let meta_config = MetaStoreConfig {
            rocksdb: RocksDBConfig {
                path: rocksdb_path.clone(),
                create: false,
                read_only: true,
            },
            prefix_len: 4,
        };
        let meta_store = MetaStore::open(&meta_config)?;

        // Create engine to read chunk data  
        let parent_path = rocksdb_path.parent()
            .ok_or_else(|| Error::InvalidArg("Invalid RocksDB path".to_string()))?;
        
        let engine_config = EngineConfig {
            path: parent_path.to_path_buf(),
            create: false,
            prefix_len: 4, // Default prefix length used in examples
        };
        
        let engine = Engine::open(&engine_config)?;

        Ok(Self {
            meta_store,
            engine,
        })
    }

    pub fn read_chunk_content(
        &self,
        chunk_id_hex: &str, 
        content_format: &str, 
        output_file: &Option<String>,
        show_preview: bool
    ) -> Result<()> {
        // Parse chunk ID from hex
        let chunk_id = parse_hex_chunk_id(chunk_id_hex)?;
        
        // Get chunk metadata
        let chunk_meta = self.meta_store.get_chunk_meta(&chunk_id)?;
        let chunk_meta = match chunk_meta {
            Some(meta) => meta,
            None => {
                println!("Chunk not found: {}", chunk_id_hex);
                return Ok(());
            }
        };
        
        // Get chunk reference
        let chunk_arc = self.engine.get(&chunk_id)?;
        let chunk = match chunk_arc {
            Some(chunk) => chunk,
            None => {
                println!("Chunk data not found: {}", chunk_id_hex);
                return Ok(());
            }
        };
        
        // Read chunk data
        let mut buffer = vec![0u8; chunk_meta.len as usize];
        chunk.pread(&mut buffer, 0)?;
        
        // Display metadata
        self.display_chunk_info(chunk_id_hex, &chunk_meta, &chunk);
        
        // Process and output content
        self.output_content(&buffer, content_format, output_file, show_preview)?;
        
        Ok(())
    }

    fn display_chunk_info(&self, chunk_id_hex: &str, chunk_meta: &ChunkMeta, chunk: &Chunk) {
        println!("=== Chunk Information ===");
        println!("Chunk ID: {}", chunk_id_hex);
        println!("Size: {} ({})", format_size(chunk_meta.len as u64), chunk_meta.len);
        println!("Allocated Size: {} ({})", format_size(chunk.capacity() as u64), chunk.capacity());
        println!("Utilization: {:.2}%", (chunk_meta.len as f64 / chunk.capacity() as f64) * 100.0);
        println!("Chain Version: {}", chunk_meta.chain_ver);
        println!("Chunk Version: {}", chunk_meta.chunk_ver);
        println!("Checksum: 0x{:08x}", chunk_meta.checksum);
        println!("Uncommitted: {}", if chunk_meta.uncommitted { "Yes" } else { "No" });
        println!();
    }

    fn output_content(
        &self,
        buffer: &[u8],
        content_format: &str,
        output_file: &Option<String>,
        show_preview: bool,
    ) -> Result<()> {
        // Prepare output based on format
        let is_hex_format = content_format == "hex";
        let hex_output = if is_hex_format {
            Some(format_hex_output(buffer))
        } else {
            None
        };
        
        // Write output
        match output_file {
            Some(file_path) => {
                self.write_to_file(buffer, content_format, file_path, &hex_output)?;
            }
            None => {
                self.write_to_stdout(buffer, content_format, &hex_output)?;
            }
        }
        
        // Show preview if requested
        if show_preview && content_format != "text" {
            self.show_text_preview(buffer);
        }
        
        Ok(())
    }

    fn write_to_file(
        &self,
        buffer: &[u8],
        content_format: &str,
        file_path: &str,
        hex_output: &Option<String>,
    ) -> Result<()> {
        let mut file = File::create(file_path)
            .map_err(|e| Error::IoError(format!("Failed to create output file: {}", e)))?;
        
        match content_format {
            "hex" => {
                if let Some(hex_data) = hex_output {
                    file.write_all(hex_data.as_bytes())
                        .map_err(|e| Error::IoError(format!("Failed to write to file: {}", e)))?;
                }
            }
            "binary" | "text" => {
                file.write_all(buffer)
                    .map_err(|e| Error::IoError(format!("Failed to write to file: {}", e)))?;
            }
            _ => return Err(Error::InvalidArg(format!("Invalid content format: {}. Use 'hex', 'binary', or 'text'", content_format))),
        }
        
        println!("Content written to: {}", file_path);
        Ok(())
    }

    fn write_to_stdout(
        &self,
        buffer: &[u8],
        content_format: &str,
        hex_output: &Option<String>,
    ) -> Result<()> {
        match content_format {
            "hex" => {
                println!("=== Chunk Content (Hex) ===");
                if let Some(hex_data) = hex_output {
                    print!("{}", hex_data);
                }
            }
            "binary" => {
                // For binary, just write to stdout
                std::io::stdout().write_all(buffer)
                    .map_err(|e| Error::IoError(format!("Failed to write to stdout: {}", e)))?;
            }
            "text" => {
                println!("=== Chunk Content (Text) ===");
                // Try to display as UTF-8, with fallback for invalid characters
                match String::from_utf8(buffer.to_vec()) {
                    Ok(text) => println!("{}", text),
                    Err(_) => {
                        // Display as best-effort UTF-8 with replacement characters
                        let text = String::from_utf8_lossy(buffer);
                        println!("{}", text);
                    }
                }
            }
            _ => return Err(Error::InvalidArg(format!("Invalid content format: {}. Use 'hex', 'binary', or 'text'", content_format))),
        }
        
        Ok(())
    }

    fn show_text_preview(&self, buffer: &[u8]) {
        println!("\n=== Text Preview (first 256 bytes) ===");
        let preview_len = std::cmp::min(256, buffer.len());
        let preview_text = String::from_utf8_lossy(&buffer[..preview_len]);
        println!("{}", preview_text);
        if buffer.len() > 256 {
            println!("... ({} more bytes)", buffer.len() - 256);
        }
    }
}