#! /bin/bash

mkdir ../bin

rm umrc-bridge
rm ../bin/umrc-bridge
gcc bridge.c ../common/common.c -o umrc-bridge -lssl -lcrypto
cp umrc-bridge ../bin
