#!/bin/bash

if [ $# -ne 2 ]
then
	echo "Not enough arguments. Usage: bash build-rings <data_folder> <results_folder>"
	exit 1
fi

if [ ! -d $1 ]; then echo "Data folder doesn't exist."; exit 1; fi
if [ ! -d $2 ]; then echo "Results folder doesn't exist."; exit 1; fi

mkdir $2/buildOutput
mkdir $2/buildOutput/ring-dyn-map
mkdir $2/buildOutput/ring-dyn-map-avl
mkdir $2/buildOutput/ring-dyn-amo-map
mkdir $2/buildOutput/ring-dyn-amo-map-avl

cd build
echo "Building ring dyn with basic mapping"
./build-index ../$1/wikidata-wcg-filtered.nt ring-dyn-map ../$2/buildOutput/ring-dyn-map > ../$2/buildOutput/ring-dyn-map/ring-dyn-map.txt
echo "[Done]"
echo "Building ring dyn with avl mapping"
./build-index ../$1/wikidata-wcg-filtered.nt ring-dyn-map-avl ../$2/buildOutput/ring-dyn-map-avl > ../$2/buildOutput/ring-dyn-map-avl/ring-dyn-map-avl.txt
echo "[Done]"
echo "Building ring dyn amo with basic mapping"
./build-index ../$1/wikidata-wcg-filtered.nt ring-dyn-amo-map ../$2/buildOutput/ring-dyn-amo-map > ../$2/buildOutput/ring-dyn-amo-map/ring-dyn-amo-map.txt
echo "[Done]"
echo "Building ring dyn amo with avl mapping"
./build-index ../$1/wikidata-wcg-filtered.nt ring-dyn-amo-map-avl ../$2/buildOutput/ring-dyn-amo-map-avl > ../$2/buildOutput/ring-dyn-amo-map-avl/ring-dyn-amo-map-avl.txt
echo "[Done]"

cd ..