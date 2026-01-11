#!/bin/bash

if [ "$#" -ne 1 ]; then
    echo "Utilizare: $0 <NUMAR_NODURI>"
    exit 1
fi

N_NODES=$1
SRC_DIR="./targetFiles"
DEST_ROOT="./targetFilesDistributed"

if [ ! -d "$SRC_DIR" ]; then
    echo "EROARE: nu exista ./targetFiles"
    exit 1
fi

if [ -z "$(ls -A "$SRC_DIR")" ]; then
    echo "EROARE: ./targetFiles e gol"
    exit 1
fi

get_target_node() {
    local str="$1"
    local modulus="$2"
    local sum=0
    local i char ascii

    LC_CTYPE=C
    for (( i=0; i<${#str}; i++ )); do
        char="${str:$i:1}"
        ascii=$(printf "%d" "'$char")
        sum=$((sum + ascii))
    done

    RESULT_INDEX=$(( (sum % modulus) + 1 ))
}

rm -rf "$DEST_ROOT"
mkdir -p "$DEST_ROOT"
for (( i=1; i<=N_NODES; i++ )); do mkdir -p "$DEST_ROOT/node$i"; done


shopt -s nullglob
files=("$SRC_DIR"/*)
count=0
RESULT_INDEX=0

echo ">> Distribuire..."

for file in "${files[@]}"; do
    if [ -f "$file" ]; then
        filename=$(basename "$file") 
        get_target_node "$filename" "$N_NODES"
        
        cp "$file" "$DEST_ROOT/node$RESULT_INDEX/"
        ((count++))
    fi
done
