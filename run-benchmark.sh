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
mkdir $3/bgps
mkdir $3/updates
mkdir $3/bgps/output
mkdir $3/updates/output


for file in $2/bgps/*.txt
do
	queryName=$(basename $file)
	mkdir $3/bgps/output/${queryName%%.txt}
done

for file in $2/updates/*.txt
do
	queryName=$(basename $file)
	mkdir $3/updates/output/${queryName%%.txt}
done

mkdir $3/updates/output/insertJoin

bash build-rings.sh $1 $3
bash query-rings.sh $1 $2 $3
bash update-ring.sh $1 $2 $3