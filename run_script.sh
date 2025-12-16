#!/bin/bash

if [ -z "$1" ]; then
    echo "Usage: sh run_script.sh [IP]"
    exit 1
fi

IP=$1

l1="n1.log"
l2="n2.log"
l3="n3.log"

cleanup() {
    echo ""
    echo "killing nodes..."
    kill $pid1 $pid2 $pid3 2>/dev/null
    exit
}
trap cleanup EXIT INT TERM

echo "Recompilare..."
rm -rf build
mkdir build
cd build

cmake -DCMAKE_POLICY_VERSION_MINIMUM=3.5 .. > /dev/null
make > /dev/null

if [ $? -ne 0 ]; then
    echo "Eroare la build"
    exit 1
fi
cd ..

echo "Initierea nodurilor..."

./build/myscdn_node --node-id node1 --listen $IP:8000 \
    --target-files ./targetFiles --cache-bytes 50000000 > $l1 2>&1 &
pid1=$! #pid

./build/myscdn_node --node-id node2 --listen $IP:8001 \
    --cluster-node node1@$IP:8000 \
    --target-files ./targetFiles --cache-bytes 50000000 > $l2 2>&1 &
pid2=$!

./build/myscdn_node --node-id node3 --listen $IP:8002 \
    --cluster-node node1@$IP:8000 \
    --target-files ./targetFiles --cache-bytes 50000000 > $l3 2>&1 &
pid3=$!

sleep 2
echo "Pregatit"

echo "Testarea request-urilor..."

cnt1=0
cnt2=0
cnt3=0

for f in ./targetFiles/*.mp3; do
    #golirea log-urilor 
    name=$(basename "$f")
    > $l1
    > $l2
    > $l3

    echo -n "$name: "
    echo "GET $name" | timeout 1 nc $IP 8000 > /dev/null
    
    sleep 0.1

    found="-"
    if grep -q "Fetching from Origin" $l1; then
        found="Node 1"
        ((cnt1++))
    elif grep -q "Fetching from Origin" $l2; then
        found="Node 2"
        ((cnt2++))
    elif grep -q "Fetching from Origin" $l3; then
        found="Node 3"
        ((cnt3++))
    fi

    echo "$found"
done

echo ""
echo "Statistici:"
echo "N1: $cnt1"
echo "N2: $cnt2"
echo "N3: $cnt3"
total=$(($cnt1 + $cnt2 + $cnt3))
echo "Total: $total"

echo ""
echo "Rulare... Ctrl+C ca sa-l opresti."
wait