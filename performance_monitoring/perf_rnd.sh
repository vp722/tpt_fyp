#!/bin/bash
# Find the actual perf binary for the current kernel
PERF_PATH=$(find /usr/lib/linux-tools/*/perf | head -1)
echo "Using perf at: $PERF_PATH"

if [ -z "$PERF_PATH" ] || [ ! -x "$PERF_PATH" ]; then
    echo "ERROR: Could not find a usable perf executable"
    echo "Looking for installed perf binaries:"
    find /usr/lib/linux-tools -name "perf" 2>/dev/null || echo "No perf binaries found"
    exit 1
fi

# Test perf with explicit path
echo "Testing perf functionality..."
$PERF_PATH --version

# Number of repetitions
REPEATS=3
# Sysbench config
BLOCK_SIZE="8G"        # More realistic size for TLB tests
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

# Check if sysbench is installed
if ! command -v sysbench &> /dev/null; then
    echo "ERROR: sysbench command not found. Please install sysbench."
    echo "On Ubuntu/Debian, try: sudo apt install sysbench"
    exit 1
fi

echo "Running performance test ($REPEATS runs each)..."


# Run the tests with perf using full path
for ((i=1; i<=REPEATS; i++)); do
  echo "Run #$i - General counters..."
  $PERF_PATH stat -e $EVENTS1 sysbench memory --memory-block-size=$BLOCK_SIZE --memory-total-size=$TOTAL_SIZE --memory-oper=$OPERATION --memory-access-mode=$ACCESS_MODE --time=0 run 2> $TMP1
  
  echo "Run #$i - DTLB walk details..."
  $PERF_PATH stat -e $EVENTS2 sysbench memory --memory-block-size=$BLOCK_SIZE --memory-total-size=$TOTAL_SIZE --memory-oper=$OPERATION --memory-access-mode=$ACCESS_MODE --time=0 run 2> $TMP2
  
  # Parse and accumulate values for standard perf format
  while read -r line; do
    # Look for lines with performance counter data (more flexible pattern)
    if [[ "$line" =~ ([0-9,.]+)[[:space:]]+(cycles|instructions|[dD][tT][lL][bB][-_][a-zA-Z0-9_.:-]+) ]]; then
      value=${BASH_REMATCH[1]}
      event=${BASH_REMATCH[2]}
      # Remove commas and convert to integer
      value_clean=$(echo "$value" | tr -d ',')
      value_int=$(printf "%.0f" "$value_clean" 2>/dev/null || echo "$value_clean" | cut -d. -f1)
      total1[$event]=$((${total1[$event]:-0} + value_int))
    fi
  done < "$TMP1"
  
  while read -r line; do
    # Look for lines with performance counter data (more flexible pattern)
    if [[ "$line" =~ ([0-9,.]+)[[:space:]]+(cycles|instructions|[dD][tT][lL][bB][-_][a-zA-Z0-9_.:-]+) ]]; then
      value=${BASH_REMATCH[1]}
      event=${BASH_REMATCH[2]}
      # Remove commas and convert to integer
      value_clean=$(echo "$value" | tr -d ',')
      value_int=$(printf "%.0f" "$value_clean" 2>/dev/null || echo "$value_clean" | cut -d. -f1)
      total2[$event]=$((${total2[$event]:-0} + value_int))
    fi
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

echo ""
echo "Benchmark complete!"
