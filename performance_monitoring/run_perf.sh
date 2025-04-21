#!/bin/bash

PERF_PATH=$(find /usr/lib/linux-tools/*/perf | head -1)
echo "Using perf at: $PERF_PATH"

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

# Max number of events per group (adjust depending on CPU PMU capacity)
MAX_GROUP_SIZE=4
group=()
count=0
group_id=1

# Loop through events and group them
for event in "${EVENTS[@]}"; do
    group+=("$event")
    ((count++))

    if [ "$count" -eq "$MAX_GROUP_SIZE" ]; then
        echo -e "\n>>> Running event group $group_id: ${group[*]}"
        $PERF_PATH stat -e "$(IFS=, ; echo "${group[*]}")" -- "$@"
        group=()
        count=0
        ((group_id++))
    fi
done

# Run remaining events
if [ "${#group[@]}" -gt 0 ]; then
    echo -e "\n>>> Running event group $group_id: ${group[*]}"
    $PERF_PATH stat -e "$(IFS=, ; echo "${group[*]}")" -- "$@"
fi
echo -e "\n>>> All event groups completed."