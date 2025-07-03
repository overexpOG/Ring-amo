#!/bin/bash

if [ $# -ne 5 ]
then
    echo "Not enough arguments. Usage: bash run-queries <index> <SO mapping> <P mapping> <queries_folder> <results_folder>"
    exit 1
fi

if [ ! -f "$1" ]; then echo "Index file doesn't exist."; exit 1; fi
if [ ! -f "$2" ]; then echo "SO mapping file doesn't exist."; exit 1; fi
if [ ! -f "$3" ]; then echo "P mapping file doesn't exist."; exit 1; fi
if [ ! -d "$4" ]; then echo "Queries folder doesn't exist."; exit 1; fi
if [ ! -d "$5" ]; then echo "Results folder doesn't exist."; exit 1; fi

thetas=(
    "100000"
    "10000"
    "1000"
    "100"
    "10"
    "1"
    "0.1"
    "0.01"
    "0.001"
    "0.0001"
    "0.00001"
)

for theta in "${thetas[@]}"; do
    echo "testing theta = $theta"
    theta_dir=${theta//./_}
    results_folder="$5/theta_$theta_dir"
    mkdir -p "$results_folder"

    ./run-queries.sh "$1" "$2" "$3" "$4" "$results_folder" "$theta"
done