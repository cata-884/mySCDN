#!/bin/bash

source "$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )/_common.sh"

if [ "$#" -ne 3 ]; then
    echo "Utilizare: $0 <IP_TINTA> <PORT_TINTA> <NUMAR_CLIENTI>"
    exit 1
fi

IP=$1
PORT=$2
COUNT=$3
CLIENT_EXE="$BUILD_DIR/client"

if [ ! -f "$CLIENT_EXE" ]; then
    echo "Eroare: Nu am gasit executabilul la: $CLIENT_EXE"
    echo "Asigura-te ca ai compilat proiectul (cmake .. && make)."
    exit 1
fi

detect_terminal
echo "Lansare $COUNT clienti catre $IP:$PORT folosind $TERM_APP..."

for (( i=1; i<=COUNT; i++ ))
do
    launch_in_terminal "Client #$i" "$CLIENT_EXE $IP $PORT"
    sleep 0.2
done