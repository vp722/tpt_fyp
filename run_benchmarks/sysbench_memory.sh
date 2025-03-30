#!/bin/bash

# Check if sysbench is installed
if ! command -v sysbench &> /dev/null; then
    echo "ERROR: sysbench is not installed. Install it using:"
    echo "  sudo apt install sysbench  # (Debian/Ubuntu)"
    exit 1
fi

# Define parameters with defaults
LOOPS=${1:-30}                 # Number of loops (default: 30)
MEMORY_TOTAL_SIZE=${2:-100G}    # Total memory to transfer (default: 100G)
MEMORY_BLOCK_SIZE=${3:-4G}      # Block size (default: 4G)
MEMORY_OPER=${4:-read}          # Operation type: read/write (default: read)
MEMORY_ACCESS_MODE=${5:-rnd}    # Access mode: seq/rnd (default: rnd)

HOSTNAME=$(hostname)
DATE=$(date "+%Y-%m-%d %H:%M:%S")

# Validate memory operation type
if [[ "$MEMORY_OPER" != "read" && "$MEMORY_OPER" != "write" ]]; then
    echo "ERROR: Invalid memory operation. Use 'read' or 'write'."
    exit 1
fi

# Validate memory access mode
if [[ "$MEMORY_ACCESS_MODE" != "seq" && "$MEMORY_ACCESS_MODE" != "rnd" ]]; then
    echo "ERROR: Invalid access mode. Use 'seq' or 'rnd'."
    exit 1
fi

# Ensure numfmt is available
if ! command -v numfmt &> /dev/null; then
    echo "ERROR: 'numfmt' command is missing. Install 'coreutils' if needed."
    exit 1
fi

# Initialize counters
THROUGHPUT_SUM=0
TOTAL_TIME_SUM=0

# Log file
LOG_FILE="sysbench_memory_results.log"
echo "Starting sysbench memory benchmark..." > "$LOG_FILE"

# Run the sysbench test for the defined number of loops
for ((i=1; i<=LOOPS; i++)); do
    echo "Running sysbench Memory test (Loop $i of $LOOPS) with total size $MEMORY_TOTAL_SIZE..."

    # Execute sysbench and capture output
    SYSBENCH_OUTPUT=$(sysbench memory \
        --memory-block-size="$MEMORY_BLOCK_SIZE" \
        --memory-total-size="$MEMORY_TOTAL_SIZE" \
        --memory-oper="$MEMORY_OPER" \
        --memory-access-mode="$MEMORY_ACCESS_MODE" \
        run)

    # Extract performance metrics
    THROUGHPUT=$(echo "$SYSBENCH_OUTPUT" | awk -F'[()]' '/MiB transferred/ {print $2}' | awk '{print $1}')
    TOTAL_TIME=$(echo "$SYSBENCH_OUTPUT" | awk '/total time:/ {print $3}')

    # Validate extracted values before using bc
    if [[ "$THROUGHPUT" =~ ^[0-9]+([.][0-9]+)?$ ]]; then
        THROUGHPUT_SUM=$(echo "$THROUGHPUT_SUM + $THROUGHPUT" | bc)
    fi

    if [[ "$TOTAL_TIME" =~ ^[0-9]+([.][0-9]+)?$ ]]; then
        TOTAL_TIME_SUM=$(echo "$TOTAL_TIME_SUM + $TOTAL_TIME" | bc)
    fi

    # Save results to log file
    {
        echo "---------------------------------"
        echo "Date: $DATE"
        echo "Host: $HOSTNAME"
        echo "Total Size: $MEMORY_TOTAL_SIZE"
        echo "Block Size: $MEMORY_BLOCK_SIZE"
        echo "Operation: $MEMORY_OPER"
        echo "Access Mode: $MEMORY_ACCESS_MODE"
        echo "Loop: $i of $LOOPS"
        echo "---------------------------------"
        echo "$SYSBENCH_OUTPUT"
        echo "---------------------------------"
    } >> "$LOG_FILE"
done

# Calculate averages safely
if (( LOOPS > 0 )); then
    AVG_THROUGHPUT=$(echo "scale=3; $THROUGHPUT_SUM / $LOOPS" | bc)
    AVG_TOTAL_TIME=$(echo "scale=3; $TOTAL_TIME_SUM / $LOOPS" | bc)
else
    AVG_THROUGHPUT=0
    AVG_TOTAL_TIME=0
fi

# Calculate total data transferred safely
TOTAL_GB=$(echo "$LOOPS * $(numfmt --from=iec $MEMORY_TOTAL_SIZE)" | bc)
TOTAL_GB_HUMAN=$(numfmt --to=iec-i <<< "$TOTAL_GB")

# Display summary results
echo "================================="
echo "Benchmark Summary ($LOOPS runs)"
echo "================================="
echo "Average Throughput: $AVG_THROUGHPUT MiB/sec"
echo "---------------------------------"
echo "Results saved to: $LOG_FILE"
