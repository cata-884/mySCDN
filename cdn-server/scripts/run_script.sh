#!/bin/bash

if [ "$#" -ne 2 ]; then
    echo "Utilizare: $0 <IP> <NUMAR_NODURI>"
    echo "Exemplu: ./run_script.sh 127.0.0.1 2"
    exit 1
fi

source "$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )/_common.sh"

IP=$1
N_NODES=$2

EXE="$BUILD_DIR/myscdn_node"

MYSCDN_ROOT="$(dirname "$PROJECT_ROOT")"
STORAGE_ROOT="$MYSCDN_ROOT/data/"

DB_DIR="/tmp/myscdn"
DB_PATH="$DB_DIR/CDN.db"

MAX_CONN=20
CACHE_SIZE=104857600
TTL=3600

detect_terminal

mkdir -p "$DB_DIR"
mkdir -p "$STORAGE_ROOT"

cd "$BUILD_DIR" || { mkdir -p "$BUILD_DIR"; cd "$BUILD_DIR"; }
cmake .. && make -j$(nproc)

if [ $? -ne 0 ]; then
    echo "EROARE CRITICA: Build-ul a esuat. Repara erorile C++ inainte de a rula."
    exit 1
fi

if [ ! -f "$EXE" ]; then
    echo "EROARE: Executabilul nu a fost generat."
    exit 1
fi

echo "--- Lansare Cluster ($N_NODES noduri) ---"

for (( i=1; i<=N_NODES; i++ )); do
    CURRENT_PORT=$((8000 + i - 1))
    NODE_DIR="$STORAGE_ROOT"
    mkdir -p "$NODE_DIR"

    EXTRA_ARGS=""
    if [ $i -gt 1 ]; then
        EXTRA_ARGS="--cluster-node node1@${IP}:8000"
    fi

    CMD="$EXE --node-id node$i --listen $IP:$CURRENT_PORT --target-files $NODE_DIR --db-path $DB_PATH --max-connections $MAX_CONN --cache-bytes $CACHE_SIZE --ttl $TTL $EXTRA_ARGS"
    
    echo "Lansare Nod $i ($CURRENT_PORT)..."
    launch_in_terminal "Nod $i ($CURRENT_PORT)" "$CMD"
    
    sleep 0.5
done
