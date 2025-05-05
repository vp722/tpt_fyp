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

# Output CSV header
echo "\"command\",cycles,instructions,dTLB-load-misses,dTLB-loads,dTLB-store-misses,dTLB-stores,dtlb_load_misses.miss_causes_a_walk,dtlb_load_misses.stlb_hit,dtlb_load_misses.walk_completed,dtlb_load_misses.walk_duration,dtlb_store_misses.miss_causes_a_walk,dtlb_store_misses.stlb_hit,dtlb_store_misses.walk_completed,dtlb_store_misses.walk_duration,ept.walk_cycles,page_walker_loads.dtlb_l1,page_walker_loads.dtlb_l2,page_walker_loads.dtlb_l3,page_walker_loads.dtlb_memory,page-faults,major-faults,minor-faults" > output.csv

# Loop through events and group them
for event in "${EVENTS[@]}"; do
    group+=("$event")
    ((count++))

    if [ "$count" -eq "$MAX_GROUP_SIZE" ]; then
        echo -e "\n>>> Running event group $group_id: ${group[*]}"
        # Run perf and format output to CSV
        output=$( $PERF_PATH stat -e "$(IFS=, ; echo "${group[*]}")" -- "$@" 2>&1 )
        
        # Extract the data from the output
        cycles=$(echo "$output" | grep "cycles" | awk '{print $1}')
        instructions=$(echo "$output" | grep "instructions" | awk '{print $1}')
        dtlb_load_misses=$(echo "$output" | grep "dTLB-load-misses" | awk '{print $1}')
        dtlb_loads=$(echo "$output" | grep "dTLB-loads" | awk '{print $1}')
        dtlb_store_misses=$(echo "$output" | grep "dTLB-store-misses" | awk '{print $1}')
        dtlb_stores=$(echo "$output" | grep "dTLB-stores" | awk '{print $1}')
        miss_causes_a_walk_load=$(echo "$output" | grep "dtlb_load_misses.miss_causes_a_walk" | awk '{print $1}')
        stlb_hit_load=$(echo "$output" | grep "dtlb_load_misses.stlb_hit" | awk '{print $1}')
        walk_completed_load=$(echo "$output" | grep "dtlb_load_misses.walk_completed" | awk '{print $1}')
        walk_duration_load=$(echo "$output" | grep "dtlb_load_misses.walk_duration" | awk '{print $1}')
        miss_causes_a_walk_store=$(echo "$output" | grep "dtlb_store_misses.miss_causes_a_walk" | awk '{print $1}')
        stlb_hit_store=$(echo "$output" | grep "dtlb_store_misses.stlb_hit" | awk '{print $1}')
        walk_completed_store=$(echo "$output" | grep "dtlb_store_misses.walk_completed" | awk '{print $1}')
        walk_duration_store=$(echo "$output" | grep "dtlb_store_misses.walk_duration" | awk '{print $1}')
        ept_walk_cycles=$(echo "$output" | grep "ept.walk_cycles" | awk '{print $1}')
        dtlb_l1_load=$(echo "$output" | grep "page_walker_loads.dtlb_l1" | awk '{print $1}')
        dtlb_l2_load=$(echo "$output" | grep "page_walker_loads.dtlb_l2" | awk '{print $1}')
        dtlb_l3_load=$(echo "$output" | grep "page_walker_loads.dtlb_l3" | awk '{print $1}')
        dtlb_memory_load=$(echo "$output" | grep "page_walker_loads.dtlb_memory" | awk '{print $1}')
        page_faults=$(echo "$output" | grep "page-faults" | awk '{print $1}')
        major_faults=$(echo "$output" | grep "major-faults" | awk '{print $1}')
        minor_faults=$(echo "$output" | grep "minor-faults" | awk '{print $1}')

        # Write the result to the CSV
        echo "\"$@\",$cycles,$instructions,$dtlb_load_misses,$dtlb_loads,$dtlb_store_misses,$dtlb_stores,$miss_causes_a_walk_load,$stlb_hit_load,$walk_completed_load,$walk_duration_load,$miss_causes_a_walk_store,$stlb_hit_store,$walk_completed_store,$walk_duration_store,$ept_walk_cycles,$dtlb_l1_load,$dtlb_l2_load,$dtlb_l3_load,$dtlb_memory_load,$page_faults,$major_faults,$minor_faults" >> output.csv
        
        group=()
        count=0
        ((group_id++))
    fi
done

# Run remaining events
if [ "${#group[@]}" -gt 0 ]; then
    echo -e "\n>>> Running event group $group_id: ${group[*]}"
    output=$( $PERF_PATH stat -e "$(IFS=, ; echo "${group[*]}")" -- "$@" 2>&1 )

    # Extract the data from the output
    cycles=$(echo "$output" | grep "cycles" | awk '{print $1}')
    instructions=$(echo "$output" | grep "instructions" | awk '{print $1}')
    dtlb_load_misses=$(echo "$output" | grep "dTLB-load-misses" | awk '{print $1}')
    dtlb_loads=$(echo "$output" | grep "dTLB-loads" | awk '{print $1}')
    dtlb_store_misses=$(echo "$output" | grep "dTLB-store-misses" | awk '{print $1}')
    dtlb_stores=$(echo "$output" | grep "dTLB-stores" | awk '{print $1}')
    miss_causes_a_walk_load=$(echo "$output" | grep "dtlb_load_misses.miss_causes_a_walk" | awk '{print $1}')
    stlb_hit_load=$(echo "$output" | grep "dtlb_load_misses.stlb_hit" | awk '{print $1}')
    walk_completed_load=$(echo "$output" | grep "dtlb_load_misses.walk_completed" | awk '{print $1}')
    walk_duration_load=$(echo "$output" | grep "dtlb_load_misses.walk_duration" | awk '{print $1}')
    miss_causes_a_walk_store=$(echo "$output" | grep "dtlb_store_misses.miss_causes_a_walk" | awk '{print $1}')
    stlb_hit_store=$(echo "$output" | grep "dtlb_store_misses.stlb_hit" | awk '{print $1}')
    walk_completed_store=$(echo "$output" | grep "dtlb_store_misses.walk_completed" | awk '{print $1}')
    walk_duration_store=$(echo "$output" | grep "dtlb_store_misses.walk_duration" | awk '{print $1}')
    ept_walk_cycles=$(echo "$output" | grep "ept.walk_cycles" | awk '{print $1}')
    dtlb_l1_load=$(echo "$output" | grep "page_walker_loads.dtlb_l1" | awk '{print $1}')
    dtlb_l2_load=$(echo "$output" | grep "page_walker_loads.dtlb_l2" | awk '{print $1}')
    dtlb_l3_load=$(echo "$output" | grep "page_walker_loads.dtlb_l3" | awk '{print $1}')
    dtlb_memory_load=$(echo "$output" | grep "page_walker_loads.dtlb_memory" | awk '{print $1}')
    page_faults=$(echo "$output" | grep "page-faults" | awk '{print $1}')
    major_faults=$(echo "$output" | grep "major-faults" | awk '{print $1}')
    minor_faults=$(echo "$output" | grep "minor-faults" | awk '{print $1}')

    # Write the result to the CSV
    echo "\"$@\",$cycles,$instructions,$dtlb_load_misses,$dtlb_loads,$dtlb_store_misses,$dtlb_stores,$miss_causes_a_walk_load,$stlb_hit_load,$walk_completed_load,$walk_duration_load,$miss_causes_a_walk_store,$stlb_hit_store,$walk_completed_store,$walk_duration_store,$ept_walk_cycles,$dtlb_l1_load,$dtlb_l2_load,$dtlb_l3_load,$dtlb_memory_load,$page_faults,$major_faults,$minor_faults" >> output.csv
fi

echo -e "\n>>> All event groups completed."
echo "Results saved to output.csv"