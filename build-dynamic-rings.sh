#!/bin/bash

# Lista de rings a probar
configs=(
    "ring-dyn-amo-map"
    "ring-dyn-amo-map-avl"
)

# Función para verificar archivos
check_files() {
    local dir="$1"
    local base_name=$(basename "$dir")

    if [ -f "$dir/$base_name.ring" ] && \
       [ -f "$dir/$base_name.ring.so.mapping" ] && \
       [ -f "$dir/$base_name.ring.p.mapping" ]; then
        return 0
    else
        return 1
    fi
}

if [ $# -ne 2 ]
then
	echo "Not enough arguments. Usage: bash build-dynamic-rings <data_file> <results_folder>"
	exit 1
fi

data_file=$(realpath "$1")
results_dir=$(realpath "$2")	

if [ ! -f "$data_file" ]; then echo "Data file doesn't exist."; exit 1; fi
if [ ! -d "$results_dir" ]; then echo "Results folder doesn't exist."; exit 1; fi

mkdir "$results_dir/buildOutput"

cd build || exit 1

for name in "${configs[@]}"; do
	out_dir="$results_dir/buildOutput/$name"
	mkdir -p "$out_dir"

	echo "Building $name"
	if check_files "$out_dir"; then
        echo "Files already exist in $out_dir, skipping..."
    else
        if ./build-index "$data_file" "$name" "$out_dir" > "$out_dir/$name.txt"; then
            echo "[Done]"
        else
            echo "Error while building $name" >&2
            echo "$name" >> "$results_dir/build_errors.log"
        fi
    fi
done

cd ..