#! /bin/bash

plat="linux"
arc="x64"
arch=$(uname -m)
if [[ $OSTYPE == darwin* ]]; then
    plat="macos"
fi
if [[ "$arch" == arm* ]] || [[ "$arch" == aarch64 ]]; then
    arc="arm64"
fi

mkdir -p bin
mkdir -p bin/screens
mkdir -p bin/themes

echo $plat-$arc

echo -n "Building setup..."
cd setup
gcc setup.c ../common/common.c -o setup
if [ $? -eq 0 ]; then
    echo -e "\033[32;1mOK\033[0m"
else
    echo -n "Result: "
    echo $?
    exit $?
fi
cp setup ../bin

cd ..

echo -n "Building umrc-bridge..."
cd umrc-bridge
gcc bridge.c ../common/common.c -o umrc-bridge -lssl -lcrypto
if [ $? -eq 0 ]; then
    echo -e "\033[32;1mOK\033[0m"
else
    echo -n "Result: "
    echo $?
    exit $?
fi
cp umrc-bridge ../bin

cd ..

echo -n "Building umrc-client..."
cd umrc-client
gcc main.c func.c ../common/common.c -o umrc-client -L ../lib/odoors/$plat-$arc -lODoors
if [ $? -eq 0 ]; then
    echo -e "\033[32;1mOK\033[0m"
else
    echo -n "Result: "
    echo $?
    exit $?
fi
cp umrc-client ../bin

cd ..

echo "Copying assets..."
cp -r ./assets/$plat/* ./bin/
cp -r ./assets/screens/* ./bin/screens/
cp -r ./assets/themes/* ./bin/themes/

echo -e "All jobs done. Check the \033[32;1mbin\033[0m subdirectory for the output files."

