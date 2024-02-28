#!/bin/bash
#clean the folder
rm -rf client-*/
# Path to the server.h file
server_h_path="tftp_server/server.h"
# Path to the tftp_server directory
server_dir="tftp_server"
# Path to the tftp_client directory
client_dir="tftp_client"

# Extract POOL_SIZE value from server.h
pool_size=$(grep -oP '(?<=#define POOL_SIZE )[0-9]+' "$server_h_path")

# Create directories
for ((i=1; i<=pool_size+1; i++)); do
    mkdir -p "client-$i"
done

echo "Created $(($pool_size+1)) directories."

# Run make in tftp_server directory
echo "Running make in tftp_server directory..."
(cd "$server_dir" && make)

# Run make in tftp_client directory
echo "Running make in tftp_client directory..."
(cd "$client_dir" && make)

echo "make finished in both tftp_server and tftp_client directories."

# Copy client binary files to directories
for ((i=1; i<=pool_size+1; i++)); do
    cp "$client_dir/client" "client-$i/client$i"
done
echo "Have Fun"
