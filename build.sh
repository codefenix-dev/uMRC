#! /bin/bash

mkdir bin

cd setup
rm setup
rm ../bin/setup
gcc setup.c ../common/common.c -o setup
cp setup ../bin

cd ..

cd umrc-bridge
rm umrc-bridge
rm ../bin/umrc-bridge
gcc bridge.c ../common/common.c -o umrc-bridge -lssl -lcrypto
cp umrc-bridge ../bin

cd ..

cd umrc-client
rm umrc-client
rm ../bin/umrc-client
gcc main.c ../common/common.c -o umrc-client -L ../lib -lODoors
cp umrc-client ../bin

cd ..
