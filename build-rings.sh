#!/bin/bash
if [ $# -ne 2 ]
then
	echo "Not enough arguments. Usage: bash build-rings <data_folder> <results_folder>"
	exit 1
fi

if [ ! -d $1 ]
then
	echo "Data folder doesn't exist."
	exit 1
fi

if [ ! -d $3 ]
then
	echo "Results folder doesn't exist."
	exit 1
fi

mkdir $2/buildOutput

cd build
echo "Building ring"
./build-index ../$1/wikidata-wcg-filtered-num.nt ring > ../$2/buildOutput/ring
echo "[Done]"
echo "Building ring-c"
./build-index ../$1/wikidata-wcg-filtered-num.nt c-ring > ../$2/buildOutput/ring-c
echo "[Done]"
echo "Building ring dyn with mapping"
./build-index ../$1/wikidata-wcg-filtered.nt ring-dyn-map > ../$2/buildOutput/ring-dyn-map
echo "[Done]"

cd ..