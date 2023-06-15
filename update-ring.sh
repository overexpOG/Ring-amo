#!/bin/bash
if [ $# -ne 3 ]
then
	echo "Not enough arguments. Usage: bash run-benchmark <data_folder> <queries_folder> <results_folder>"
	exit 1
fi

if [ ! -d $1 ]
then
	echo "Data folder doesn't exist."
	exit 1
fi

if [ ! -d $2 ]
then
	echo "Queries folder doesn't exist."
	exit 1
fi

if [ ! -d $3 ]
then
	echo "Results folder doesn't exist."
	exit 1
fi

cd build

echo Processing deleteEdge
./delete-edge ../$1/wikidata-wcg-filtered.nt.ring-dyn ../$2/updates/deleteEdge.txt ../$1/wikidata-wcg-filtered.nt.so.mapping ../$1/wikidata-wcg-filtered.nt.p.mapping > ../$3/updates/output/deleteEdge/ring-dyn-map
echo "[Done]"

echo Processing insert
./insert-edge ../$1/wikidata-wcg-filtered.nt.updated.ring-dyn ../$2/updates/insert.txt ../$1/wikidata-wcg-filtered.nt.so.updated.mapping ../$1/wikidata-wcg-filtered.nt.p.updated.mapping > ../$3/updates/output/insert/ring-dyn-map
echo "[Done]"

echo Processing deleteNode
./delete-node ../$1/wikidata-wcg-filtered.nt.ring-dyn ../$2/updates/deleteNode.txt ../$1/wikidata-wcg-filtered.nt.so.mapping ../$1/wikidata-wcg-filtered.nt.p.mapping > ../$3/updates/output/deleteNode/ring-dyn-map
echo "[Done]"

cd ..