#!/bin/bash

if [ "$#" -ne 5 ]; then
    echo "Usage: $0 <server_ip> <server_port> <number_of_clients> <number_of_reqs> <threadpool>"
    exit 1
fi

SERVER_IP=$1
SERVER_PORT=$2
NUM_CLIENTS=$3
NUM_REQUESTS_PER_CLIENT=$4
THREADPOOL_SIZE=$5

# Compile the server client
g++ ./scripts/broker_server.cpp ./misc/helper.cpp -lpthread -o ./output/bin/broker
if [ $? -ne 0 ]; then
    echo "Compilation of broker_server.cpp failed"
    exit 1
fi

# Compile the publisher client
g++ ./scripts/publisher_client.cpp ./misc/helper.cpp -lpthread -o ./output/bin/publisher
if [ $? -ne 0 ]; then
    echo "Compilation of publisher_client.cpp failed"
    exit 1
fi

# Compile the subscriber client
g++ ./scripts/subscriber_client.cpp ./misc/helper.cpp -lpthread -o ./output/bin/subscriber
if [ $? -ne 0 ]; then
    echo "Compilation of subscriber_client.cpp failed"
    exit 1
fi

echo "Starting server at $SERVER_IP:$SERVER_PORT"
# Start the server
./output/bin/broker $SERVER_IP $SERVER_PORT $THREADPOOL_SIZE &
SERVER_PID=$!
#SERVER_PID=$2

echo "Starting $NUM_CLIENTS clients to connect to $SERVER_IP:$SERVER_PORT"

# Temporary file to store the response times of all clients
pub_temp_file="./output/log/pub_log.txt"
sub_temp_file="./output/log/sub_log.txt"

# Clear the files if they already exist
> "$pub_temp_file"
> "$sub_temp_file"

# Array to hold client PIDs
CLIENT_PIDS=()

# Loop to start each client and capture its average response time
for ((i = 1; i <= NUM_CLIENTS; i++)); do
    echo "Starting client #$i"
    
    # Run the client in the background, store its average response time in the temp file
    (./output/bin/publisher $SERVER_IP $SERVER_PORT $i $NUM_REQUESTS_PER_CLIENT | tee >(grep "Average response time" | awk '{print $6}' >> "$pub_temp_file")) &
    CLIENT_PIDS+=($!)
    echo "Launched publisher $i with PID: ${CLIENT_PIDS[-1]}"
    (./output/bin/subscriber $SERVER_IP $SERVER_PORT $i $NUM_REQUESTS_PER_CLIENT | tee >(grep "Average response time" | awk '{print $6}' >> "$sub_temp_file")) &
    CLIENT_PIDS+=($!)
    echo "Launched subscriber $i with PID: ${CLIENT_PIDS[-1]}"
done

# Wait for all client processes to finish
for pid in "${CLIENT_PIDS[@]}"; do
    wait $pid
    echo "Client with PID $pid has finished."
done

echo "All clients have finished."

# End the server as it runs indefinitely
kill $SERVER_PID

total_response_time=0

# Sum up all the response times for publishers
while IFS= read -r response_time; do
    total_response_time=$(echo "$total_response_time + $response_time" | bc)
done < "$pub_temp_file"

# Calculate the global average response time for publishers
#global_avg_response_time_pub=$(echo "$total_response_time / $NUM_CLIENTS" | bc -l)
global_avg_response_time_pub=$(printf "%.3f" "$(echo "($total_response_time / $NUM_CLIENTS) * 1000000" | bc -l)")

total_response_time=0

# Sum up all the response times for subscribers
while IFS= read -r response_time; do
    total_response_time=$(echo "$total_response_time + $response_time" | bc)
done < "$sub_temp_file"

# Calculate the global average response time for subscribers
#global_avg_response_time_sub=$(echo "$total_response_time / $NUM_CLIENTS" | bc -l)
global_avg_response_time_sub=$(printf "%.3f" "$(echo "($total_response_time / $NUM_CLIENTS) * 1000000" | bc -l)")


# Clean up the temporary file and binaries
#rm "$pub_temp_file"
#rm "$sub_temp_file"
#rm output/bin/*

# Print out the results
echo "=========================================="
echo "Global Average Response Time (Publisher) = $global_avg_response_time_pub ms"
echo "Global Average Response Time (Subscriber) = $global_avg_response_time_sub ms"
echo "=========================================="
