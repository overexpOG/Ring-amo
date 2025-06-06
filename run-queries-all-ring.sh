#!/bin/bash

if [ $# -ne 2 ]
then
	echo "Not enough arguments. Usage: bash build-rings <data_folder> <results_folder> <queries_folder>"
	exit 1
fi

if [ ! -d $1 ]; then echo "Data folder doesn't exist."; exit 1; fi
if [ ! -d $2 ]; then echo "Results folder doesn't exist."; exit 1; fi
if [ ! -d "$3" ]; then echo "Queries folder doesn't exist."; exit 1; fi
if [ ! -d "$4" ]; then echo "Results folder doesn't exist."; exit 1; fi

./build-dynamic-rings.sh $1 $2

for dir in $2/*/
do
    echo "Processing $dir"
    ./run-queries.sh $2/$dir/$dir.ring $2/$dir/$dir.ring.so.mapping $2/$dir/$dir.ring.p.mapping $3 $2/$dir
done