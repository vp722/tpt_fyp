#!/bin/bash

# Configuration
TOTAL_SIZE="32G"
BLOCK_SIZE="1G"
ITERATIONS=5

echo "Starting mixed sysbench memory test..."
echo "Each phase will use $TOTAL_SIZE total, $BLOCK_SIZE block size"

for i in $(seq 1 $ITERATIONS); do
    echo -e "\n================= Iteration $i: SEQUENTIAL ACCESS ================="
    sysbench memory \
        --memory-block-size=$BLOCK_SIZE \
        --memory-total-size=$TOTAL_SIZE \
        --memory-oper=read \
        --memory-access-mode=seq \
        run

    echo -e "\n================= Iteration $i: RANDOM ACCESS ======================"
    sysbench memory \
        --memory-block-size=$BLOCK_SIZE \
        --memory-total-size=$TOTAL_SIZE \
        --memory-oper=read \
        --memory-access-mode=rnd \
        run
done

echo -e "\nMixed sysbench memory test complete."
