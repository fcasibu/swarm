mkdir -p build

RAYLIB_FLAGS=$(pkg-config --libs --cflags raylib)

clang -std=c2x -Wall -Wextra -Wpedantic -Wno-unused-function -g -O0 $RAYLIB_FLAGS -I./src ./src/main.c ./src/game.c -o build/main -DDEBUG_MODE
