#!/bin/bash

# Number of repetitions
REPEATS=3

# Sysbench config
BLOCK_SIZE="8G"
TOTAL_SIZE="32G"
OPERATION="read"
ACCESS_MODE="rnd"

# Events to track
EVENTS1="cycles,instructions,dTLB-load-misses"
EVENTS2="dtlb_load_misses.miss_causes_a_walk,dtlb_load_misses.stlb_hit,dtlb_load_misses.walk_completed,dtlb_load_misses.walk_duration"

# Temp files
TMP1=$(mktemp)
TMP2=$(mktemp)

# Initialize totals
declare -A total1 total2

echo "Running performance test ($REPEATS runs each)..."

# Run the tests
for ((i=1; i<=REPEATS; i++)); do
  echo "Run #$i - General counters..."
  perf stat -e $EVENTS1 -x, --no-big-num sysbench memory --memory-block-size=$BLOCK_SIZE --memory-total-size=$TOTAL_SIZE --memory-oper=$OPERATION --memory-access-mode=$ACCESS_MODE --time=0 run 2> $TMP1
  echo "Run #$i - DTLB walk details..."
  perf stat -e $EVENTS2 -x, --no-big-num sysbench memory --memory-block-size=$BLOCK_SIZE --memory-total-size=$TOTAL_SIZE --memory-oper=$OPERATION --memory-access-mode=$ACCESS_MODE --time=0 run 2> $TMP2

  # Parse and accumulate values
  while IFS=, read -r value event _; do
    [[ "$value" =~ ^[0-9]+$ ]] && total1[$event]=$((total1[$event]+value))
  done < "$TMP1"

  while IFS=, read -r value event _; do
    [[ "$value" =~ ^[0-9]+$ ]] && total2[$event]=$((total2[$event]+value))
  done < "$TMP2"
done

# Show averages
echo ""
echo "=== Average Performance Counters (over $REPEATS runs) ==="
echo "General Metrics:"
for event in "${!total1[@]}"; do
  avg=$((total1[$event]/REPEATS))
  printf "%-40s %'12d\n" "$event" "$avg"
done

echo ""
echo "DTLB Miss Details:"
for event in "${!total2[@]}"; do
  avg=$((total2[$event]/REPEATS))
  printf "%-40s %'12d\n" "$event" "$avg"
done

# Cleanup
rm "$TMP1" "$TMP2"
