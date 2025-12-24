#! /bin/bash

mkdir ../bin

rm setup
rm ../bin/setup
gcc setup.c ../common/common.c -o setup
cp setup ../bin
