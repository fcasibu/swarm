#ifndef PLATFORM_H
#define PLATFORM_H

typedef void *
platform_allocate_memory(usize size);
typedef void
platform_deallocate_memory(void *mem, usize size);

typedef struct {
    platform_allocate_memory *AllocateMemory;
    platform_deallocate_memory *DeallocateMemory;
} platform_api;

global platform_api Platform;

typedef struct {
    f32 dt;

    b32 action_up;
    b32 action_down;
    b32 action_left;
    b32 action_right;
    b32 action_shoot;

    Vector2 mouse_pos;
} game_input;

typedef struct {
    usize permanent_storage_size;
    void *permanent_storage;

    usize temporary_storage_size;
    void *temporary_storage;

    platform_api platform;
} platform_memory;


#define GAME_UPDATE_AND_RENDER(name) void name(platform_memory *memory, game_input *input)
typedef GAME_UPDATE_AND_RENDER(game_update_and_render);

#endif // PLATFORM_H
