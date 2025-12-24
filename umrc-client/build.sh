#! /bin/bash

mkdir ../bin

rm umrc-client
rm ../bin/umrc-client
gcc main.c ../common/common.c -o umrc-client -L ../lib -lODoors
cp umrc-client ../bin
