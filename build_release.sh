#!/bin/bash
mkdir -p build

CC="clang"
CFLAGS="-std=c2x -Wall -Wextra -Wpedantic -O2 -I./src"
RAYLIB_FLAGS=$(pkg-config --libs --cflags raylib)

$CC $CFLAGS src/main.c src/game.c -o build/game $RAYLIB_FLAGS
