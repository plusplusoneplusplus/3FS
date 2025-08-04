#!/bin/bash

# 3FS Cluster Cleanup Script
# This script cleans up temporary files and directories created by launch_cluster.sh
#
# Usage: ./cleanup_cluster.sh [--dry-run]
#   --dry-run: Show what would be cleaned but don't actually remove anything

set -e

# Parse command line arguments
DRY_RUN=false
if [[ "$1" == "--dry-run" ]]; then
    DRY_RUN=true
fi

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Get the directory where this script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"

echo -e "${BLUE}3FS Cluster Cleanup Script${NC}"
echo -e "${BLUE}==========================${NC}"
echo "Project root: $PROJECT_ROOT"
echo "Build directory: $BUILD_DIR"
echo ""

if [[ "$DRY_RUN" == "true" ]]; then
    echo -e "${YELLOW}DRY RUN MODE - No files will be deleted${NC}"
    echo ""
fi

# Function to remove directory or show what would be removed
cleanup_directory() {
    local dir_path="$1"
    local description="$2"
    
    if [[ -d "$dir_path" ]]; then
        if [[ "$DRY_RUN" == "true" ]]; then
            echo -e "${YELLOW}Would remove: $dir_path ($description)${NC}"
            echo "  Contents:"
            ls -la "$dir_path" 2>/dev/null | sed 's/^/    /' || echo "    (unable to list contents)"
        else
            echo -e "${BLUE}Removing: $dir_path ($description)${NC}"
            rm -rf "$dir_path"
            echo -e "${GREEN}✓ Removed${NC}"
        fi
    else
        echo -e "${YELLOW}Not found: $dir_path ($description)${NC}"
    fi
}

# Function to remove file or show what would be removed
cleanup_file() {
    local file_path="$1"
    local description="$2"
    
    if [[ -f "$file_path" ]]; then
        if [[ "$DRY_RUN" == "true" ]]; then
            echo -e "${YELLOW}Would remove: $file_path ($description)${NC}"
        else
            echo -e "${BLUE}Removing: $file_path ($description)${NC}"
            rm -f "$file_path"
            echo -e "${GREEN}✓ Removed${NC}"
        fi
    else
        echo -e "${YELLOW}Not found: $file_path ($description)${NC}"
    fi
}

# 1. Clean up storage test directories
echo -e "${BLUE}1. Cleaning up storage test directories...${NC}"

# Clean up numbered storage nodes
for i in {1..10}; do  # Check up to 10 nodes
    storage_path="/tmp/3fs_storage_test_node$i"
    if [[ -d "$storage_path" ]]; then
        cleanup_directory "$storage_path" "Storage node $i data"
    fi
done

# Clean up any other 3fs_storage_test directories
if ls /tmp/3fs_storage_test* >/dev/null 2>&1; then
    echo -e "${BLUE}Found additional 3fs_storage_test directories:${NC}"
    for dir in /tmp/3fs_storage_test*; do
        if [[ -d "$dir" ]]; then
            cleanup_directory "$dir" "Additional storage test directory"
        fi
    done
else
    echo -e "${YELLOW}No additional 3fs_storage_test directories found${NC}"
fi

# 2. Clean up temporary config files
echo -e "\n${BLUE}2. Cleaning up temporary config files...${NC}"
cleanup_directory "$BUILD_DIR/temp_configs" "Temporary configuration files"

# 3. Clean up log files
echo -e "\n${BLUE}3. Cleaning up log files...${NC}"
cleanup_directory "$BUILD_DIR/logs" "Service log files"

# 4. Clean up any leftover process files
echo -e "\n${BLUE}4. Checking for leftover process files...${NC}"

# Check for any running 3FS processes
if pgrep -f "(mgmtd_main|meta_main|storage_main)" >/dev/null 2>&1; then
    echo -e "${RED}Warning: Found running 3FS processes:${NC}"
    pgrep -f "(mgmtd_main|meta_main|storage_main)" -l
    echo -e "${YELLOW}Consider stopping these processes before cleanup${NC}"
    echo -e "${YELLOW}You can kill them with: pkill -f '(mgmtd_main|meta_main|storage_main)'${NC}"
else
    echo -e "${GREEN}✓ No running 3FS processes found${NC}"
fi

# 5. Check for other potential cleanup items
echo -e "\n${BLUE}5. Additional cleanup checks...${NC}"

# Check for any core dumps
if ls /tmp/core.* >/dev/null 2>&1; then
    echo -e "${YELLOW}Found core dump files in /tmp:${NC}"
    for core_file in /tmp/core.*; do
        if [[ -f "$core_file" ]]; then
            cleanup_file "$core_file" "Core dump file"
        fi
    done
else
    echo -e "${GREEN}✓ No core dump files found${NC}"
fi

# Summary
echo ""
if [[ "$DRY_RUN" == "true" ]]; then
    echo -e "${GREEN}✓ Dry run completed!${NC}"
    echo -e "${YELLOW}To actually perform the cleanup, run: $0${NC}"
else
    echo -e "${GREEN}✓ Cleanup completed!${NC}"
fi

echo ""
echo -e "${BLUE}Manual cleanup commands (if needed):${NC}"
echo "  # Kill all 3FS processes:"
echo "  pkill -f '(mgmtd_main|meta_main|storage_main)'"
echo ""
echo "  # Remove all storage test directories:"
echo "  rm -rf /tmp/3fs_storage_test*"
echo ""
echo "  # Remove build artifacts:"
echo "  rm -rf $BUILD_DIR/temp_configs $BUILD_DIR/logs"
echo ""
