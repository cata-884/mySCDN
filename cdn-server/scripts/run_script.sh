#!/bin/bash
if [ "$#" -ne 2 ]; then
    echo "Utilizare: $0 <IP> <NUMAR_NODURI>"
    exit 1
fi

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

IP=$1
N_NODES=$2

BUILD_DIR="$PROJECT_ROOT/build"
EXE="$BUILD_DIR/myscdn_node"
STORAGE_ROOT="$PROJECT_ROOT/data/storage"
DB_PATH="$PROJECT_ROOT/data/db/CDN.db"

mkdir -p "$PROJECT_ROOT/data/db"

chmod +x "$SCRIPT_DIR/distributeTargetFiles.sh"
"$SCRIPT_DIR/distributeTargetFiles.sh" $N_NODES

cd "$BUILD_DIR" || { mkdir -p "$BUILD_DIR"; cd "$BUILD_DIR"; }
cmake .. && make -j$(nproc)

for (( i=1; i<=N_NODES; i++ )); do
    CURRENT_PORT=$((8000 + i - 1))
    NODE_DIR="$STORAGE_ROOT/node$i"
    EXTRA=""
    [ $i -gt 1 ] && EXTRA="--cluster-node node1@${IP}:8000"

    CMD="$EXE --node-id node$i --listen $IP:$CURRENT_PORT --target-files $NODE_DIR --db-path $DB_PATH $EXTRA"
    
    kitty --title "Node $i" bash -c "$CMD" &
done