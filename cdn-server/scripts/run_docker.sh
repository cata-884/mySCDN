#!/bin/bash

IP=${1:-"0.0.0.0"}
N_NODES=${2:-2}

PROJECT_ROOT="/app"
EXE="$PROJECT_ROOT/build/myscdn_node"
STORAGE_ROOT="$PROJECT_ROOT/assets/targetFilesDistributed"
DB_PATH="$PROJECT_ROOT/db/CDN.db"

chmod +x "$PROJECT_ROOT/scripts/distributeTargetFiles.sh"
"$PROJECT_ROOT/scripts/distributeTargetFiles.sh" $N_NODES

pids=""
for (( i=1; i<=N_NODES; i++ )); do
    CURRENT_ID="node$i"
    CURRENT_PORT=$((8000 + i - 1))
    NODE_DIR="$STORAGE_ROOT/$CURRENT_ID"
    
    EXTRA=""
    [ $i -gt 1 ] && EXTRA="--cluster-node node1@${IP}:8000"

    $EXE --node-id $CURRENT_ID --listen $IP:$CURRENT_PORT \
         --target-files $NODE_DIR --db-path $DB_PATH $EXTRA &
    pids="$pids $!"
done

wait $pids