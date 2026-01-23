#! /bin/bash

mkdir -p bin
mkdir -p bin/screens
mkdir -p bin/themes

echo -n "Building setup..."
cd setup
gcc setup.c ../common/common.c -o setup -fcompare-debug-second -Wno-format-truncation
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
gcc bridge.c ../common/common.c -o umrc-bridge -lssl -lcrypto -fcompare-debug-second -Wno-format-truncation
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
gcc main.c ../common/common.c -o umrc-client -L ../lib -lODoors -fcompare-debug-second -Wno-format-truncation
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
cp -r ./assets/nix/* ./bin/
cp -r ./assets/screens/* ./bin/screens/
cp -r ./assets/themes/* ./bin/themes/

echo -e "All jobs done. Check the \033[32;1mbin\033[0m subdirectory for the output files."

