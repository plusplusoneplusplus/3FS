#!/bin/bash
# Script to configure the chain for 3FS

set -e

ADMIN_CLI=./build/bin/admin_cli
CFG=./testing_configs/minimal_admin_cli.toml
CREATE_TARGET_CMD=deploy/data_placement/output/create_target_cmd.txt
GENERATED_CHAINS=deploy/data_placement/output/generated_chains.csv
GENERATED_CHAIN_TABLE=deploy/data_placement/output/generated_chain_table.csv

# Create storage targets
echo "Creating storage targets..."
$ADMIN_CLI --cfg $CFG < $CREATE_TARGET_CMD

echo "Uploading chains to mgmtd service..."
$ADMIN_CLI --cfg $CFG "upload-chains $GENERATED_CHAINS"

echo "Uploading chain table to mgmtd service..."
$ADMIN_CLI --cfg $CFG "upload-chain-table --desc stage 1 $GENERATED_CHAIN_TABLE"

echo "Chain configuration completed."

echo "Listing chains from mgmtd service..."
$ADMIN_CLI --cfg $CFG "list-chains"

echo "Listing chain tables from mgmtd service..."
$ADMIN_CLI --cfg $CFG "list-chain-tables"
