#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

#include "base_types.h"

#include "raylib.h"
#include "platform.h"

internal void *
AllocateMemory(usize size)
{
    void *result = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);

    if (result == MAP_FAILED)
        return NULL;

    return result;
}

internal void
DeallocateMemory(void *mem, usize size)
{
    if (mem) {
        munmap(mem, size);
    }
}

int
main(void)
{
    platform_memory memory = { 0 };
    memory.permanent_storage_size = MB(256);

    usize total_size = memory.permanent_storage_size;
    memory.permanent_storage = AllocateMemory(total_size);

    memory.platform.AllocateMemory = AllocateMemory;
    memory.platform.DeallocateMemory = DeallocateMemory;
    Platform = memory.platform;

    InitWindow(1280, 720, "swarm");

    SetTargetFPS(60);
    while (!WindowShouldClose()) {
        game_input input = {
            .dt = GetFrameTime(),
            .action_up = IsKeyDown(KEY_W),
            .action_left = IsKeyDown(KEY_A),
            .action_down = IsKeyDown(KEY_S),
            .action_right = IsKeyDown(KEY_D),
            .action_shoot = IsMouseButtonDown(MOUSE_LEFT_BUTTON),
            .mouse_pos = GetMousePosition(),
        };

        GameUpdateAndRender(&memory, &input);
    }

    CloseWindow();

    return 0;
}
