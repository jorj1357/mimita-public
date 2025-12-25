#!/bin/bash
# Build and run script for mimita-public (macOS)

set -e  # Exit on error

echo "=== Building mimita ==="
make clean
make

echo ""
echo "=== Running mimita ==="
./mimita 2>&1 | tee run.log

echo ""
echo "=== Program exited ==="
echo "Full output saved to run.log"

