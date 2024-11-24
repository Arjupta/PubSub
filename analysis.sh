#!/bin/bash

# Output files
PUB_TIMES_FILE="./output/plot/pub_times.txt"
SUB_TIMES_FILE="./output/plot/sub_times.txt"

# Clear files before appending new results
> "$PUB_TIMES_FILE"
> "$SUB_TIMES_FILE"

# Parameter ranges
threadpools=(5 10 15 20 25)
requests=(100 200 300 400 500)
server_ip="127.0.0.1"
server_port=1000
clients=100  # Change this value for differen amount of clients

# Loop through the parameter combinations
for threadpool in "${threadpools[@]}"; do
    for request in "${requests[@]}"; do
        # Run the simulation
        output=$(./simulator.sh $server_ip $server_port $clients $request $threadpool)

        # Extract values from simulation output
        pub_time=$(echo "$output" | grep "Global Average Response Time (Publisher)" | awk '{print $7}')
        sub_time=$(echo "$output" | grep "Global Average Response Time (Subscriber)" | awk '{print $7}')

        # If pub_time or sub_time is empty, handle it
        if [ -z "$pub_time" ]; then
            pub_time="N/A"
        fi
        if [ -z "$sub_time" ]; then
            sub_time="N/A"
        fi

        # Append to the files with proper formatting
        echo "$threadpool,$request,$pub_time" >> "$PUB_TIMES_FILE"
        echo "$threadpool,$request,$sub_time" >> "$SUB_TIMES_FILE"
    done
done
