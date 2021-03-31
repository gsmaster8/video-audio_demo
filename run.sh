#!/usr/bin/bash

echo "Run Client_Server Demo ..."

make clean

make all

./server&

sleep 1

./client

rm ./avfile/demuxer*
rm ./avfile/muxer*

make clean
#####################################
#   chmod +x ./run.sh && ./run.sh   #
#####################################