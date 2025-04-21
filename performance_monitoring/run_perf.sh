#!/bin/bash

# Check if at least one argument is passed (the command to run)
if [ "$#" -lt 1 ]; then
    echo "Usage: $0 <command> [args...]"
    exit 1
fi

# Define the list of perf events
EVENTS=(
  cycles
  instructions
  dTLB-load-misses
  dTLB-loads
  dTLB-store-misses
  dTLB-stores
  dtlb_load_misses.miss_causes_a_walk
  dtlb_load_misses.stlb_hit
  dtlb_load_misses.walk_completed
  dtlb_load_misses.walk_duration
  dtlb_store_misses.miss_causes_a_walk
  dtlb_store_misses.stlb_hit
  dtlb_store_misses.walk_completed
  dtlb_store_misses.walk_duration
  ept.walk_cycles
  page_walker_loads.dtlb_l1
  page_walker_loads.dtlb_l2
  page_walker_loads.dtlb_l3
  page_walker_loads.dtlb_memory
  page-faults
  major-faults
  minor-faults
)

# Join events into a comma-separated string
EVENT_LIST=$(IFS=, ; echo "${EVENTS[*]}")

# Run perf stat with the events and no multiplexing
perf stat --no-multiplex -e "$EVENT_LIST" -- "$@"
