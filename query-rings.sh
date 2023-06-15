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

for file in ../$2/bgps/*.txt
do
	queryName=$(basename $file)
  echo Processing ring $queryName
	./query-index ../$1/wikidata-wcg-filtered-num.nt.ring ../$2/bgps/$queryName ../$1/wikidata-wcg-filtered.nt.so.mapping ../$1/wikidata-wcg-filtered.nt.p.mapping > ../$3/bgps/output/${queryName%%.txt}/ring
done

for file in ../$2/bgps/*.txt
do
	queryName=$(basename $file)
  echo Processing ring-c $queryName
	./query-index ../$1/wikidata-wcg-filtered-num.nt.c-ring ../$2/bgps/$queryName ../$1/wikidata-wcg-filtered.nt.so.mapping ../$1/wikidata-wcg-filtered.nt.p.mapping > ../$3/bgps/output/${queryName%%.txt}/ring-c
done

for file in ../$2/bgps/*.txt
do
	queryName=$(basename $file)
  echo Processing ring-dyn $queryName
	./query-index ../$1/wikidata-wcg-filtered.nt.ring-dyn ../$2/bgps/$queryName ../$1/wikidata-wcg-filtered.nt.so.mapping ../$1/wikidata-wcg-filtered.nt.p.mapping > ../$3/bgps/output/${queryName%%.txt}/ring-dyn-map
done

cd ..