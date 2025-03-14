#!/bin/bash

# Check if sysbench is installed
if ! command -v sysbench &> /dev/null; then
    echo "sysbench is not installed. Please install it first."
    exit 1
fi

# Define loop count and other parameters
LOOPS=${1:-30}        # Number of loops to run (default: 1)
CPU_MAX_PRIME=50000  # Prime number limit for CPU test
RUN_TIME=10          # Time duration for each run (30 seconds)
HOSTNAME=$(hostname) # Get system hostname
DATE=$(date "+%Y-%m-%d %H:%M:%S")  # Get current date and time

# Initialize variables for calculating averages
TOTAL_EVENTS_SUM=0
EVENTS_PER_SECOND_SUM=0

# Run the sysbench test for the defined number of loops
for ((i=1; i<=LOOPS; i++))
do
    echo "Running sysbench CPU test for $RUN_TIME seconds (Loop $i of $LOOPS)..."
    
    # Run the sysbench test
    SYSBENCH_OUTPUT=$(sysbench cpu --cpu-max-prime=$CPU_MAX_PRIME --time=$RUN_TIME run)

    # Extract the relevant statistics
    EVENTS_PER_SECOND=$(echo "$SYSBENCH_OUTPUT" | awk '/events per second:/ {print $4}')
    TOTAL_EVENTS=$(echo "$SYSBENCH_OUTPUT" | awk '/total number of events:/ {print $5}')

    # Update sums for averaging later
    EVENTS_PER_SECOND_SUM=$(echo "$EVENTS_PER_SECOND_SUM + $EVENTS_PER_SECOND" | bc)
    TOTAL_EVENTS_SUM=$(echo "$TOTAL_EVENTS_SUM + $TOTAL_EVENTS" | bc)

    # Save full results to log file
    LOG_FILE="sysbench_cpu_results.log"
    {
        echo "---------------------------------"
        echo "Date: $DATE"
        echo "Host: $HOSTNAME"
        echo "CPU Max Prime: $CPU_MAX_PRIME"
        echo "Run Time: $RUN_TIME seconds"
        echo "Loop: $i of $LOOPS"
        echo "---------------------------------"
        echo "$SYSBENCH_OUTPUT" | awk '
            /total time:/ {print "Total Time:", $3, $4}
            /total number of events:/ {print "Total Events:", $5}
            /min:/ {print "Min Latency:", $2, $3}
            /avg:/ {print "Avg Latency:", $2, $3}
            /max:/ {print "Max Latency:", $2, $3}
            /95th percentile:/ {print "95th Percentile Latency:", $3, $4}
            /events per second:/ {print "Events Per Second:", $4}
        '
        echo "---------------------------------"
    } >> "$LOG_FILE"

done

# Calculate averages
AVG_EVENTS_PER_SECOND=$(echo "scale=3; $EVENTS_PER_SECOND_SUM / $LOOPS" | bc)
AVG_TOTAL_EVENTS=$(echo "scale=3; $TOTAL_EVENTS_SUM / $LOOPS" | bc)

# Display averages on screen
echo "---------------------------------"
echo "Average Events Per Second: $AVG_EVENTS_PER_SECOND"
echo "Average Total Events: $AVG_TOTAL_EVENTS"
echo "---------------------------------"

echo "Results saved to $LOG_FILE"

