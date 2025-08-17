use chunk_engine::*;
use derse::Deserialize;
use std::{
    collections::{BTreeMap, HashMap},
    sync::Arc,
};

use super::utils::format_size;

pub struct ChunkLister {
    meta_store: MetaStore,
}

impl ChunkLister {
    pub fn new(meta_store: MetaStore) -> Self {
        Self { meta_store }
    }

    pub fn show_summary(&self) -> Result<()> {
        let mut chunk_allocators = HashMap::new();
        let mut used_map = BTreeMap::new();
        let mut reversed_map = BTreeMap::new();
        let mut group_count = BTreeMap::new();
        let mut chunk_size = CHUNK_SIZE_SMALL;
        let mut real_map = BTreeMap::new();

        // Load allocation data for all chunk sizes
        loop {
            let counter = Arc::new(AllocatorCounter::new(chunk_size));
            let it = self.meta_store.iterator();
            let chunk_allocator = ChunkAllocator::load(it, counter.clone(), chunk_size)?;
            let allocated_chunks = counter.allocated_chunks();
            let reserved_chunks = counter.reserved_chunks();
            used_map.insert(chunk_size, allocated_chunks - reserved_chunks);
            reversed_map.insert(chunk_size, reserved_chunks);
            group_count.insert(
                chunk_size,
                (
                    chunk_allocator.full_groups.len(),
                    chunk_allocator.active_groups.len(),
                ),
            );
            real_map.insert(chunk_size, 0u64);
            chunk_allocators.insert(chunk_size, chunk_allocator);

            if chunk_size >= CHUNK_SIZE_ULTRA {
                break;
            }
            chunk_size *= 2;
        }

        // Count actual chunks in metadata
        let mut it = self.meta_store.iterator();
        let end_key = MetaKey::chunk_meta_key_prefix();
        it.seek(&end_key)?;

        if it.key() == Some(end_key.as_ref()) {
            it.next(); // [begin, end)
        }

        loop {
            if !it.valid() {
                break;
            }

            if it.key().unwrap()[0] != MetaKey::CHUNK_META_KEY_PREFIX {
                break;
            }

            let chunk_meta =
                ChunkMeta::deserialize(it.value().unwrap()).map_err(Error::SerializationError)?;

            let chunk_size = chunk_meta.pos.chunk_size();
            let allocator = chunk_allocators.get_mut(&chunk_size).unwrap();
            allocator.reference(chunk_meta.pos, true);
            real_map.entry(chunk_size).and_modify(|v| *v += 1);

            it.next();
        }
        
        // Display summary
        self.display_summary(&used_map, &reversed_map, &group_count);
        assert_eq!(used_map, real_map);

        Ok(())
    }

    pub fn list_chunks_detailed(
        &self,
        target_size: u32,
        page_size: usize,
        page: usize,
        short_ids: bool,
    ) -> Result<()> {
        let mut it = self.meta_store.iterator();
        let end_key = MetaKey::chunk_meta_key_prefix();
        it.seek(&end_key)?;

        if it.key() == Some(end_key.as_ref()) {
            it.next(); // [begin, end)
        }

        let mut chunks_info: Vec<(Bytes, ChunkMeta)> = Vec::new();
        let mut total_chunks = 0u64;
        let mut total_actual_size = 0u64;
        let mut total_allocated_size = 0u64;

        // Collect all chunks for the target size
        loop {
            if !it.valid() {
                break;
            }

            if it.key().unwrap()[0] != MetaKey::CHUNK_META_KEY_PREFIX {
                break;
            }

            let chunk_meta =
                ChunkMeta::deserialize(it.value().unwrap()).map_err(Error::SerializationError)?;

            let chunk_size = chunk_meta.pos.chunk_size();
            
            if chunk_size == target_size {
                let raw_key = it.key().unwrap();
                let chunk_id = MetaKey::parse_chunk_meta_key(raw_key);
                chunks_info.push((chunk_id, chunk_meta.clone()));
                total_chunks += 1;
                total_actual_size += chunk_meta.len as u64;
                total_allocated_size += u64::from(chunk_size);
            }

            it.next();
        }

        if chunks_info.is_empty() {
            println!("No chunks found for size bucket: {} ({} bytes)", format_size(target_size as u64), target_size);
            println!("Run without --list-size to see available size buckets");
            return Ok(());
        }

        // Sort chunks by chunk ID for consistent ordering
        chunks_info.sort_by(|a, b| a.0.cmp(&b.0));

        // Calculate pagination
        let total_pages = (total_chunks as usize + page_size - 1) / page_size;
        let start_idx = (page - 1) * page_size;
        let end_idx = std::cmp::min(start_idx + page_size, chunks_info.len());

        self.display_detailed_header(target_size, total_chunks, total_actual_size, total_allocated_size, page, total_pages, end_idx - start_idx);
        self.display_chunks_table(&chunks_info, start_idx, end_idx, target_size, short_ids);
        self.display_pagination_info(page, total_pages);

        Ok(())
    }

    fn display_summary(
        &self,
        used_map: &BTreeMap<Size, u64>,
        reversed_map: &BTreeMap<Size, u64>,
        group_count: &BTreeMap<Size, (usize, usize)>,
    ) {
        println!("=== Chunk Allocation Summary ===");
        
        // Show available size buckets with friendly names
        println!("\nAvailable size buckets:");
        for (size, count) in used_map {
            println!("  {:<10} ({} bytes): {} used chunks", format_size(u64::from(*size)), size, count);
        }
        
        println!("\nReserved chunks per size:");
        for (size, count) in reversed_map {
            println!("  {:<10} ({} bytes): {} reserved chunks", format_size(u64::from(*size)), size, count);
        }
        
        println!("\nGroup counts (full, active):");
        for (size, (full, active)) in group_count {
            println!("  {:<10} ({} bytes): {} full, {} active groups", format_size(u64::from(*size)), size, full, active);
        }
        
        println!("\nUse --list-size <SIZE> to see detailed chunk information (e.g., --list-size 4MB)");
        println!("Use --read-chunk <CHUNK_ID> to read actual chunk content (e.g., --read-chunk a1b2c3d4...)");
    }

    fn display_detailed_header(
        &self,
        target_size: u32,
        total_chunks: u64,
        total_actual_size: u64,
        total_allocated_size: u64,
        page: usize,
        total_pages: usize,
        chunks_on_page: usize,
    ) {
        println!("=== Detailed Chunk Information ===");
        println!("Size bucket: {} ({})", format_size(target_size as u64), target_size);
        println!("Total chunks: {}", total_chunks);
        println!("Total actual size: {} ({})", format_size(total_actual_size), total_actual_size);
        println!("Total allocated size: {} ({})", format_size(total_allocated_size), total_allocated_size);
        println!("Average utilization: {:.2}%", (total_actual_size as f64 / total_allocated_size as f64) * 100.0);
        println!();
        println!("Page {}/{} (showing {} chunks)", page, total_pages, chunks_on_page);
    }

    fn display_chunks_table(
        &self,
        chunks_info: &[(Bytes, ChunkMeta)],
        start_idx: usize,
        end_idx: usize,
        target_size: u32,
        short_ids: bool,
    ) {
        let (id_width, total_width) = if short_ids { (20, 130) } else { (68, 175) };
        println!("{:<8} {:<width$} {:<15} {:<15} {:<8} {:<12} {:<12} {:<8}", 
                 "Index", "Chunk ID (hex)", "Alloc Size", "Actual Len", "Util %", "Chain Ver", "Chunk Ver", "Uncommit", width = id_width);
        println!("{}", "-".repeat(total_width));

        for (i, (chunk_id, chunk_meta)) in chunks_info.iter().enumerate().skip(start_idx).take(end_idx - start_idx) {
            let chunk_id_hex = chunk_id.iter()
                .map(|b| format!("{:02x}", b))
                .collect::<Vec<_>>()
                .join("");
            
            let chunk_id_display = if short_ids {
                if chunk_id_hex.len() > 16 {
                    format!("{}...", &chunk_id_hex[..16])
                } else {
                    chunk_id_hex
                }
            } else {
                chunk_id_hex
            };
            
            let utilization = (chunk_meta.len as f64 / target_size as f64) * 100.0;
            
            println!("{:<8} {:<width$} {:<15} {:<15} {:<8.2} {:<12} {:<12} {:<8}", 
                     i + 1,
                     chunk_id_display,
                     format_size(target_size as u64),
                     format_size(chunk_meta.len as u64),
                     utilization,
                     chunk_meta.chain_ver,
                     chunk_meta.chunk_ver,
                     if chunk_meta.uncommitted { "Yes" } else { "No" },
                     width = id_width
            );
        }
    }

    fn display_pagination_info(&self, page: usize, total_pages: usize) {
        println!();
        if page < total_pages {
            println!("Use --page {} to see next page", page + 1);
        }
        if page > 1 {
            println!("Use --page {} to see previous page", page - 1);
        }
    }
}