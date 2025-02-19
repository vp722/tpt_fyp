#!/bin/bash

# Check if sysbench is installed
if ! command -v sysbench &> /dev/null; then
    echo "sysbench is not installed. Please install it first."
    exit 1
fi

# Define loop count and other parameters
LOOPS=${1:-2}        # Number of loops to run (default: 1)
MEMORY_BLOCK_SIZE=4G # Memory block size for test
MEMORY_OPER=read      # Memory operation type (read/write)
MEMORY_ACCESS_MODE=rnd # Memory access mode (seq/rnd)
HOSTNAME=$(hostname) # Get system hostname
DATE=$(date "+%Y-%m-%d %H:%M:%S")  # Get current date and time

# Initialize variables for calculating averages
THROUGHPUT_SUM=0

# Run the sysbench test for the defined number of loops
for ((i=1; i<=LOOPS; i++))
do
    echo "Running sysbench Memory test for $RUN_TIME seconds (Loop $i of $LOOPS)..."

    # Run the sysbench memory test
    SYSBENCH_OUTPUT=$(sysbench memory \
        --memory-block-size=$MEMORY_BLOCK_SIZE \
        --memory-oper=$MEMORY_OPER \
        --memory-access-mode=$MEMORY_ACCESS_MODE \
        run)

    # Extract the average throughput
    AVG_THROUGHPUT=$(echo "$SYSBENCH_OUTPUT" | awk -F'[()]' '/MiB transferred/ {print $2}' | awk '{print $1}')
    THROUGHPUT_SUM=$(echo "$THROUGHPUT_SUM + $AVG_THROUGHPUT" | bc)

    # Save full results to log file
    LOG_FILE="sysbench_memory_results.log"
    {
        echo "---------------------------------"
        echo "Date: $DATE"
        echo "Host: $HOSTNAME"
        echo "Memory Block Size: $MEMORY_BLOCK_SIZE"
        echo "Memory Operation: $MEMORY_OPER"
        echo "Memory Access Mode: $MEMORY_ACCESS_MODE"
        echo "Loop: $i of $LOOPS"
        echo "---------------------------------"
        echo "$SYSBENCH_OUTPUT"
        echo "---------------------------------"
    } >> "$LOG_FILE"

done

# Calculate averages
AVG_THROUGHPUT_TOTAL=$(echo "scale=3; $THROUGHPUT_SUM / $LOOPS" | bc)

# Display averages on screen
echo "---------------------------------"
echo "Average Throughput: $AVG_THROUGHPUT_TOTAL MiB/sec"
echo "---------------------------------"

echo "Results saved to $LOG_FILE"
