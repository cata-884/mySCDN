#!/bin/bash

if [ "$#" -ne 2 ]; then
    echo "Utilizare: $0 <IP> <NUMAR_NODURI>"
    exit 1
fi

IP=$1
N_NODES=$2
BUILD_DIR="build"
EXE="./build/myscdn_node"
STORAGE_ROOT="./targetFilesDistributed" 
DB_PATH="./CDN.db"
AUTH_FILE="./auth.txt"  

START_PORT=8000
MAX_CONN=10
CACHE_BYTES=104857600
TTL=3600

#alternativ - gnome-terminal/xterm
TERMINAL_CMD="kitty"

chmod +x distributeTargetFiles.sh
./distributeTargetFiles.sh $N_NODES
if [ $? -ne 0 ]; then
    echo "Eroare la distribuirea fisierelor!"
    exit 1
fi

echo "Build project..."
mkdir -p $BUILD_DIR
cd $BUILD_DIR
cmake -DCMAKE_POLICY_VERSION_MINIMUM=3.5 .. > /dev/null
make -j4
if [ $? -ne 0 ]; then
    echo "Eroare la compilare!"
    exit 1
fi
cd .. 

echo "Lansare cluster ($N_NODES noduri) pe IP $IP"

for (( i=1; i<=N_NODES; i++ ))
do
    CURRENT_ID="node$i"
    CURRENT_PORT=$(($START_PORT + $i - 1))
    NODE_SPECIFIC_DIR="$STORAGE_ROOT/$CURRENT_ID"
    EXTRA_ARGS=""
    
    if [ $i -eq 1 ]; then
        echo "Lansare SEED ($CURRENT_ID) -> $NODE_SPECIFIC_DIR"
    else
        PREV_ID="node1"
        PREV_PORT=$START_PORT
        
        EXTRA_ARGS="--cluster-node ${PREV_ID}@${IP}:${PREV_PORT}"
        echo "Lansare $CURRENT_ID (se ataseaza la $PREV_ID) -> $NODE_SPECIFIC_DIR"
    fi

    CMD="$EXE \
    --node-id $CURRENT_ID \
    --listen $IP:$CURRENT_PORT \
    --target-files $NODE_SPECIFIC_DIR \
    --max-connections $MAX_CONN \
    --cache-bytes $CACHE_BYTES \
    --ttl $TTL \
    --db-path $DB_PATH \
    --auth-file $AUTH_FILE \
    $EXTRA_ARGS"

    TITLE="NODE $i ($CURRENT_PORT)"
    
    $TERMINAL_CMD --title "$TITLE" --hold bash -c "$CMD" &

    sleep 1.5
done