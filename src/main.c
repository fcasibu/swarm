#include "game.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <dlfcn.h>

#include "raylib.h"
#include "base_types.h"
#include "platform.h"

GAME_UPDATE_AND_RENDER(GameUpdateAndRender);

typedef struct {
    void *game_code_handle;
    long last_write_time;

    game_update_and_render *UpdateAndRender;
} game_code;

[[maybe_unused]] internal long
GetLastWriteTime(const char *filename)
{
    struct stat result;
    if (stat(filename, &result) == 0) {
        return result.st_mtime;
    }
    return 0;
}

[[maybe_unused]] internal game_code
LoadGameCode(const char *source_lib_path)
{
    game_code result = { 0 };
    result.game_code_handle = dlopen(source_lib_path, RTLD_NOW);
    result.last_write_time = GetLastWriteTime(source_lib_path);

    if (!result.game_code_handle) {
        fprintf(stderr, "Library could not be loaded: %s\n", dlerror());
        return result;
    }

    result.UpdateAndRender =
        (game_update_and_render *)dlsym(result.game_code_handle, "GameUpdateAndRender");

    return result;
}

[[maybe_unused]] internal void
UnloadGameCode(game_code *code)
{
    if (code->game_code_handle) {
        dlclose(code->game_code_handle);
        code->game_code_handle = NULL;
    }

    code->UpdateAndRender = NULL;
}

[[maybe_unused]] internal void
ReloadGameCode(game_code *game_code, const char *game_lib_path)
{
    if (game_code->UpdateAndRender) {
        UnloadGameCode(game_code);
        *game_code = LoadGameCode(game_lib_path);
    }
}

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
#if DEBUG
    const char *game_lib_path = "./build/game.so";
    game_code game = LoadGameCode(game_lib_path);
#endif

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
#if DEBUG
        long new_write_time = GetLastWriteTime(game_lib_path);

        if (new_write_time > game.last_write_time) {
            ReloadGameCode(&game, game_lib_path);
        }
#endif

        game_input input = {
            .dt = GetFrameTime(),
            .action_up = IsKeyDown(KEY_W),
            .action_left = IsKeyDown(KEY_A),
            .action_down = IsKeyDown(KEY_S),
            .action_right = IsKeyDown(KEY_D),
            .action_shoot = IsMouseButtonDown(MOUSE_LEFT_BUTTON),
            .mouse_pos = GetMousePosition(),
        };

#if DEBUG
        if (game.UpdateAndRender) {
            game.UpdateAndRender(&memory, &input);
        }
#else
        GameUpdateAndRender(&memory, &input);
#endif
    }

    CloseWindow();

    return 0;
}
