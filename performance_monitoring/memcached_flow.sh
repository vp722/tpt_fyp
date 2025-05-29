#!/usr/bin/env bash
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

PROFILER="$SCRIPT_DIR/../phase2_profiler/p2" 


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

echo "sleeping for 2 minuts - idle memcached."
sleep 120 # wait for 2 minutes 

# load the data into memcached

echo "Loading data into memcached..."

./memcached_data_load.sh localhost:11211 

echo "Data loaded successfully."

echo "Sleeping for 2 minutes before starting the workload..."
sleep 120 # wait for 2 minutes before starting the workload

# run the workload
echo "Running workload on memcached..."
./memcached_load_gen.sh localhost:11211 

echo "Workload completed."


