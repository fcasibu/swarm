#!/bin/sh
set -xe

mkdir -p build

CC="clang"
CFLAGS="-std=c2x -Wall -Wextra -Wpedantic -g -O0 -I./src -DDEBUG_MODE -fPIC"
RAYLIB_FLAGS=$(pkg-config --libs --cflags raylib)

$CC $CFLAGS -shared src/game.c -o build/game.so.tmp $RAYLIB_FLAGS
mv build/game.so.tmp build/game.so

$CC $CFLAGS src/main.c -o build/main $RAYLIB_FLAGS -ldl
