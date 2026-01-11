#!/bin/bash

if [ "$#" -ne 3 ]; then
    echo "Utilizare: $0 <IP_TINTA> <PORT_TINTA> <NUMAR_CLIENTI>"
    exit 1
fi

IP=$1
PORT=$2
COUNT=$3

CLIENT_EXE="./build/myscdn_client"

if [ ! -f "$CLIENT_EXE" ]; then
    echo "Eroare: Nu am gasit executabilul '$CLIENT_EXE'."
    echo "Te rog sa rulezi intai './run_script.sh' pentru a compila proiectul."
    exit 1
fi

TERMINAL_CMD="kitty"

echo "Lansare $COUNT clienti conectati la $IP:$PORT..."

for (( i=1; i<=COUNT; i++ ))
do
    TITLE="Client #$i -> $PORT"
    CMD="$CLIENT_EXE $IP $PORT"

    $TERMINAL_CMD --title "$TITLE" --detach --hold bash -c "$CMD"
    sleep 0.2
done
