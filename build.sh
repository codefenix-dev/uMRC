#!/bin/sh

CC="${CC:-cc}"

plat="linux"
case "$(uname -s)" in
    FreeBSD)
        plat="freebsd"
        ;;
    Darwin)
        plat="macos"
        ;;
esac

arc="x64"
case "$(uname -m)" in
    arm*|aarch64)
        arc="arm64"
        ;;
esac

assetpath="${plat}-${arc}"
if [ "$plat" = "macos" ]; then
    assetpath="$plat"
fi

mkdir -p bin
mkdir -p bin/screens
mkdir -p bin/themes

printf 'Platform: %s\n' "${plat}-${arc}"

printf 'Building \033[1msetup\033[0m...'
cd setup || exit 1
if "$CC" setup.c ../common/common.c -o setup; then
    printf '\033[32;1mOK\033[0m\n'
else
    status=$?
    printf 'Result: %d\n' "$status"
    exit "$status"
fi
cp setup ../bin

cd .. || exit 1

printf 'Building \033[1mumrc-bridge\033[0m...'
cd umrc-bridge || exit 1
if "$CC" bridge.c ../common/common.c -o umrc-bridge -lssl -lcrypto -pthread; then
    printf '\033[32;1mOK\033[0m\n'
else
    status=$?
    printf 'Result: %d\n' "$status"
    exit "$status"
fi
cp umrc-bridge ../bin

cd .. || exit 1

printf 'Building \033[1mumrc-client\033[0m...'
cd umrc-client || exit 1
if "$CC" main.c func.c ../common/common.c -o umrc-client -pthread \
    -L "../lib/odoors/${plat}-${arc}" -lODoors; then
    printf '\033[32;1mOK\033[0m\n'
else
    status=$?
    printf 'Result: %d\n' "$status"
    exit "$status"
fi
cp umrc-client ../bin

cd .. || exit 1

printf 'Copying assets...\n'
cp changelog.txt ./bin/
cp LICENSE ./bin/
cp README.md ./bin/
cp -R "./assets/${assetpath}/"* ./bin/
cp -R ./assets/screens/* ./bin/screens/
cp -R ./assets/themes/* ./bin/themes/

printf 'All jobs done. Check the \033[32;1mbin\033[0m subdirectory for the output files.\n'
