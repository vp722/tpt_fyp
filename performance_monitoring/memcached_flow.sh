#!/usr/bin/env bash
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

PROFILER="$SCRIPT_DIR/../phase2_predictor/p2" 


# if running then stop memcached
if systemctl is-active --quiet memcached; then
    echo "Stopping memcached service..."
    sudo systemctl stop memcached
else
    echo "Memcached service is not running."
fi


# run memcached with profiler 

echo "Starting memcached with profiler..."
$PROFILER /usr/bin/memcached -m 1024 -p 11211 -c 1024 -t 1 > memcached.log 2>&1 &
MEMCACHED_PID=$!
echo "Memcached is running (PID $MEMCACHED_PID)"


echo "sleeping for 2 minuts - idle memcached."
sleep 180 # wait for 2 minutes 

# load the data into memcached

echo "Loading data into memcached..."
start=$(date +%s)
./memcached_data_load.sh localhost:11211 
end=$(date +%s)
elapsed=$((end - start))
echo "Data loaded successfully in $elapsed seconds."


echo "Sleeping for 5 minutes before starting the workload..."
sleep 300 # wait for 2 minutes before starting the workload

# run the workload
echo "Running workload on memcached..."
./memcached_load_gen.sh localhost:11211 

echo "Workload completed."


