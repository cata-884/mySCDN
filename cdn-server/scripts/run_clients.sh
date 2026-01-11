#!/bin/bash

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

if [ "$#" -ne 3 ]; then
    echo "Utilizare: $0 <IP_TINTA> <PORT_TINTA> <NUMAR_CLIENTI>"
    exit 1
fi

IP=$1
PORT=$2
COUNT=$3

# Cale absoluta catre client
CLIENT_EXE="$PROJECT_ROOT/build/myscdn_client"

if [ ! -f "$CLIENT_EXE" ]; then
    echo "Eroare: Nu am gasit executabilul la: $CLIENT_EXE"
    echo "Ruleaza intai run_script.sh pentru a compila."
    exit 1
fi

TERMINAL_CMD="kitty"
# TERMINAL_CMD="gnome-terminal"

echo "Lansare $COUNT clienti conectati la $IP:$PORT..."

for (( i=1; i<=COUNT; i++ ))
do
    TITLE="Client #$i -> $PORT"
    CMD="$CLIENT_EXE $IP $PORT"

    if [ "$TERMINAL_CMD" == "gnome-terminal" ]; then
        $TERMINAL_CMD --title="$TITLE" -- bash -c "$CMD; exec bash" &
    else
        $TERMINAL_CMD --title "$TITLE" --detach --hold bash -c "$CMD"
    fi
    sleep 0.2
done