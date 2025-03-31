#!/bin/bash

# Define parameters
MEMORY_TOTAL_SIZE="100G"
MEMORY_OPER="read"
MEMORY_ACCESS_MODE="rnd"
BLOCK_SIZES=(4G 8G 16G)

PERF_MONITOR="./perf_tool"

if [[ ! -x "$PERF_MONITOR" ]]; then
    echo "Error: Performance monitoring binary '$PERF_MONITOR' not found or not executable."
    exit 1
fi

for MEMORY_BLOCK_SIZE in "${BLOCK_SIZES[@]}"; do
    echo "\nRunning sysbench with block size: $MEMORY_BLOCK_SIZE"
    SYSBENCH_CMD=(sysbench memory \
        --memory-block-size="$MEMORY_BLOCK_SIZE" \
        --memory-total-size="$MEMORY_TOTAL_SIZE" \
        --memory-oper="$MEMORY_OPER" \
        --memory-access-mode="$MEMORY_ACCESS_MODE" \
        run)
    
    # Run the benchmark within the performance monitor
    $PERF_MONITOR "${SYSBENCH_CMD[@]}"
    
    echo "---------------------------------------------------"
done