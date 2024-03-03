#!/bin/bash
#clean the folder
rm -rf client-*/
# Path to the server.h file
server_h_path="tftp_server/server.h"
# Path to the tftp_server directory
server_dir="tftp_server"
# Path to the tftp_client directory
client_dir="tftp_client"

# Extract max_client value from server.h
max_client=$(grep -oP '(?<=#define MAX_CLIENT )[0-9]+' "$server_h_path")

# Create directories
for ((i=1; i<=max_client+1; i++)); do
    mkdir -p "client-$i"
done

echo "Created $(($max_client+1)) directories."

# Run make in tftp_server directory
echo "Running make in tftp_server directory..."
(cd "$server_dir" && make)

# Run make in tftp_client directory
echo "Running make in tftp_client directory..."
(cd "$client_dir" && make)

echo "make finished in both tftp_server and tftp_client directories."

# Copy client binary files to directories
for ((i=1; i<=max_client+1; i++)); do
    cp "$client_dir/client" "client-$i/client$i"
done
echo "Have Fun"
