#!/bin/bash

if [ $# -lt 5 ] || [ $# -gt 6 ]
then
    echo "Not enough arguments. Usage: bash run-queries <index> <SO mapping> <P mapping> <queries_folder> <results_folder>"
    exit 1
fi

if [ ! -f "$1" ]; then echo "Index file doesn't exist."; exit 1; fi
if [ ! -f "$2" ]; then echo "SO mapping file doesn't exist."; exit 1; fi
if [ ! -f "$3" ]; then echo "P mapping file doesn't exist."; exit 1; fi
if [ ! -d "$4" ]; then echo "Queries folder doesn't exist."; exit 1; fi
if [ ! -d "$5" ]; then echo "Results folder doesn't exist."; exit 1; fi
if [ $# -eq 6 ]; then
    theta="$6"
else
    theta="0.01"
fi

cd build

for file in "$4"/*.txt
do
    queryName=$(basename "$file")
    echo "testing $queryName"
    /usr/bin/time -v ./test-queries "$1" "$file" "$2" "$3" "$theta" > "$5/$queryName" 2>&1
done

cd ..