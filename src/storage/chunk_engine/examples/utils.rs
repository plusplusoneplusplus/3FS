use chunk_engine::*;

/// Parse a size string like "64KB", "8MB", "1GB" into bytes
pub fn parse_size_string(size_str: &str) -> Result<u32> {
    let size_str = size_str.trim().to_uppercase();
    
    if let Ok(bytes) = size_str.parse::<u32>() {
        return Ok(bytes);
    }
    
    let (number_part, unit_part) = if size_str.ends_with("KB") {
        (&size_str[..size_str.len()-2], 1024u32)
    } else if size_str.ends_with("MB") {
        (&size_str[..size_str.len()-2], 1024u32 * 1024)
    } else if size_str.ends_with("GB") {
        (&size_str[..size_str.len()-2], 1024u32 * 1024 * 1024)
    } else if size_str.ends_with("K") {
        (&size_str[..size_str.len()-1], 1024u32)
    } else if size_str.ends_with("M") {
        (&size_str[..size_str.len()-1], 1024u32 * 1024)
    } else if size_str.ends_with("G") {
        (&size_str[..size_str.len()-1], 1024u32 * 1024 * 1024)
    } else {
        return Err(Error::InvalidArg(format!("Invalid size format: {}. Use formats like '64KB', '8MB', '1GB' or raw bytes", size_str)));
    };
    
    let number: f64 = number_part.parse()
        .map_err(|_| Error::InvalidArg(format!("Invalid number in size: {}", size_str)))?;
        
    if number < 0.0 {
        return Err(Error::InvalidArg("Size cannot be negative".to_string()));
    }
    
    let bytes = (number * unit_part as f64) as u32;
    Ok(bytes)
}

/// Format bytes into a friendly size string
pub fn format_size(bytes: u64) -> String {
    const UNITS: &[&str] = &["B", "KB", "MB", "GB", "TB"];
    const THRESHOLD: f64 = 1024.0;
    
    let mut size = bytes as f64;
    let mut unit_index = 0;
    
    while size >= THRESHOLD && unit_index < UNITS.len() - 1 {
        size /= THRESHOLD;
        unit_index += 1;
    }
    
    if unit_index == 0 {
        format!("{} {}", bytes, UNITS[unit_index])
    } else {
        format!("{:.2} {}", size, UNITS[unit_index])
    }
}

/// Parse a hex chunk ID string into bytes
pub fn parse_hex_chunk_id(hex_str: &str) -> Result<Vec<u8>> {
    let hex_str = hex_str.trim().to_lowercase();
    
    if hex_str.len() % 2 != 0 {
        return Err(Error::InvalidArg("Chunk ID hex string must have even length".to_string()));
    }
    
    let mut bytes = Vec::new();
    for i in (0..hex_str.len()).step_by(2) {
        let hex_byte = &hex_str[i..i+2];
        let byte = u8::from_str_radix(hex_byte, 16)
            .map_err(|_| Error::InvalidArg(format!("Invalid hex character in chunk ID: {}", hex_byte)))?;
        bytes.push(byte);
    }
    
    Ok(bytes)
}

/// Format data as hex dump output (like xxd)
pub fn format_hex_output(data: &[u8]) -> String {
    let mut output = String::new();
    
    for (i, chunk) in data.chunks(16).enumerate() {
        // Offset
        output.push_str(&format!("{:08x}  ", i * 16));
        
        // Hex bytes
        for (j, byte) in chunk.iter().enumerate() {
            if j == 8 {
                output.push(' '); // Extra space after 8 bytes
            }
            output.push_str(&format!("{:02x} ", byte));
        }
        
        // Padding for incomplete lines
        if chunk.len() < 16 {
            let padding = (16 - chunk.len()) * 3 + if chunk.len() <= 8 { 1 } else { 0 };
            for _ in 0..padding {
                output.push(' ');
            }
        }
        
        // ASCII representation
        output.push_str(" |");
        for byte in chunk {
            let ch = if byte.is_ascii_graphic() || *byte == b' ' {
                *byte as char
            } else {
                '.'
            };
            output.push(ch);
        }
        output.push_str("|\n");
    }
    
    output
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_parse_size_string() {
        assert_eq!(parse_size_string("1024").unwrap(), 1024);
        assert_eq!(parse_size_string("1KB").unwrap(), 1024);
        assert_eq!(parse_size_string("1MB").unwrap(), 1024 * 1024);
        assert_eq!(parse_size_string("1GB").unwrap(), 1024 * 1024 * 1024);
        assert_eq!(parse_size_string("1.5MB").unwrap(), (1.5 * 1024.0 * 1024.0) as u32);
    }

    #[test]
    fn test_format_size() {
        assert_eq!(format_size(1024), "1.00 KB");
        assert_eq!(format_size(1024 * 1024), "1.00 MB");
        assert_eq!(format_size(1024 * 1024 * 1024), "1.00 GB");
    }

    #[test]
    fn test_parse_hex_chunk_id() {
        let result = parse_hex_chunk_id("a1b2c3d4").unwrap();
        assert_eq!(result, vec![0xa1, 0xb2, 0xc3, 0xd4]);
        
        assert!(parse_hex_chunk_id("a1b2c").is_err()); // Odd length
        assert!(parse_hex_chunk_id("a1b2g3d4").is_err()); // Invalid hex
    }
}