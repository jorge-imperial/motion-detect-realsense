#!/bin/bash
MONGO_HOST="192.168.100.50"
echo "Connecting to db host ${MONGO_HOST}"
./build/motion_detect -u mongodb://192.168.100.50 -d motion -c cam1
