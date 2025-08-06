#!/bin/bash

# Script to generate configuration files for 3FS data placement
# This script runs the data placement model and chain table generation

set -e  # Exit on any error

# Get the script directory and project root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

echo "Project root: ${PROJECT_ROOT}"
echo "Generating data placement configuration..."

# Step 1: Generate data placement model
echo "Running data placement model..."

# Change to the data placement directory to ensure output goes to the right place
cd "${PROJECT_ROOT}/deploy/data_placement"

python3 "${PROJECT_ROOT}/deploy/data_placement/src/model/data_placement.py" \
   -ql -relax -type CR --num_nodes 5 --replication_factor 3 --min_targets_per_disk 6

# Find the actual output directory that was created
OUTPUT_BASE_DIR="${PROJECT_ROOT}/deploy/data_placement/output"
if [ ! -d "${OUTPUT_BASE_DIR}" ]; then
    echo "Error: Output base directory not found: ${OUTPUT_BASE_DIR}"
    exit 1
fi

# Find the most recently created DataPlacementModel directory
OUTPUT_DIR=$(find "${OUTPUT_BASE_DIR}" -name "DataPlacementModel-*" -type d | head -1)
if [ -z "${OUTPUT_DIR}" ] || [ ! -d "${OUTPUT_DIR}" ]; then
    echo "Error: No DataPlacementModel output directory found"
    echo "Available directories in ${OUTPUT_BASE_DIR}:"
    ls -la "${OUTPUT_BASE_DIR}/" 2>/dev/null || echo "No output directory found"
    exit 1
fi

echo "Data placement model generated successfully."
echo "Using output directory: ${OUTPUT_DIR}"

# Step 2: Generate chain table
echo "Running chain table generation..."
python3 "${PROJECT_ROOT}/deploy/data_placement/src/setup/gen_chain_table.py" \
   --chain_table_type CR --node_id_begin 10001 --node_id_end 10005 \
   --num_disks_per_node 1 --num_targets_per_disk 6 \
   --target_id_prefix 1 --chain_id_prefix 9 \
   --incidence_matrix_path "${OUTPUT_DIR}/incidence_matrix.pickle"

echo "Chain table generated successfully."
echo "Configuration generation completed!"
