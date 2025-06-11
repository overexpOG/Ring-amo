#!/bin/bash

if [ $# -ne 3 ]
then
	echo "Not enough arguments. Usage: bash run-queries-all-rings <data_file> <results_folder> <queries_folder>"
	exit 1
fi

data_file=$(realpath "$1")
results_dir=$(realpath "$2")
queries_dir=$(realpath "$3")

if [ ! -f "$data_file" ]; then echo "Data file doesn't exist."; exit 1; fi
if [ ! -d "$results_dir" ]; then echo "Results folder doesn't exist."; exit 1; fi
if [ ! -d "$queries_dir" ]; then echo "Queries folder doesn't exist."; exit 1; fi

./build-dynamic-rings.sh "$data_file" "$results_dir"

for subdir in "$results_dir"/buildOutput/*/
do
    subdir=$(realpath "$subdir")
    index_name=$(basename "$subdir")

    echo "Processing $index_name"

    ./run-queries.sh "$subdir/$index_name.ring" "$subdir/$index_name.ring.so.mapping" "$subdir/$index_name.ring.p.mapping" "$queries_dir" "$subdir"
done