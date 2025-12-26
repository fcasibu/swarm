#ifndef GAME_H
#define GAME_H

#include "raylib.h"
#include "base_types.h"
#include "platform.h"

typedef struct memory_block {
    struct memory_block *prev;
    usize size;
} memory_block;

typedef struct {
    memory_block *current_block;
    u8 *base;
    usize used;
    usize capacity;
    usize minimum_block_size;

    usize temp_count;
} memory_arena;

typedef struct {
    memory_arena *arena;
    usize used;
} temporary_memory;

typedef struct {
    Vector2 value;
} position;

typedef struct {
    Vector2 value;
} velocity;

typedef struct {
    Vector2 value;
} projectile;

typedef struct {
    f32 value;
} health;

typedef struct {
    Color color;
    f32 radius;
    f32 flash_timer;
    Color flash_color;
} renderable;

typedef struct {
    Vector2 pos;
    Vector2 velocity;
    f32 life;
    f32 radius;
    Color color;
} particle;

// clang-format off
typedef Enum(u8, component_mask) {
    Comp_None   = 0,

    Comp_Position   = (1 << 0),
    Comp_Velocity   = (1 << 1),
    Comp_Health     = (1 << 2),
    Comp_Render     = (1 << 3),
    Comp_Collision  = (1 << 4),
};

typedef Enum(u8, tag_type) {
    Tag_Player,
    Tag_Enemy,
    Tag_Projectile,
    Tag_Dead,
};
// clang-format on

typedef struct {
    component_mask components;
    tag_type tag;
} entity;

typedef Enum(u8, game_mode){
    GameMode_Playing,
    GameMode_Gameover,
};

#define MAX_ENTITIES Thousand(10)

// TODO(fcasibu): stats (e.g. damage, speed, etc)
#define COMPONENT_LIST         \
    X(position, positions)     \
    X(velocity, velocities)    \
    X(renderable, renderables) \
    X(health, healths)         \
    X(projectile, projectiles)

typedef struct {
    b32 is_initialized;

    memory_arena world_arena;
    game_mode mode;
    Camera2D camera;
    f32 time;

    entity entities[MAX_ENTITIES];
    usize entity_count;

#define X(type, name) type name[MAX_ENTITIES];
    COMPONENT_LIST
#undef X

    usize player_index;
    f32 enemy_spawn_timer;
    f32 projectile_spawn_timer;

    particle particles[Thousand(2)];
    usize next_particle;
} game_state;

internal inline void
InitializeArena(memory_arena *arena, usize size, void *base)
{
    memory_block *first_block = (memory_block *)base;
    first_block->prev = 0;
    first_block->size = size;

    arena->current_block = first_block;
    arena->base = (u8 *)base + sizeof(*first_block);
    arena->used = 0;
    arena->capacity = size - sizeof(*first_block);
    arena->minimum_block_size = 0;
}

internal inline usize
GetAlignmentOffset(memory_arena *arena, usize alignment)
{
    usize result = (usize)arena->base + arena->used;
    usize mask = alignment - 1;
    return (alignment - (result & mask)) & mask;
}

internal inline usize
GetEffectiveSize(memory_arena *arena, usize size_init, usize alignment)
{
    return GetAlignmentOffset(arena, alignment) + size_init;
}

#define ZeroStruct(p) ZeroSize(sizeof(*(p)), (p))
#define ZeroArray(count, p) ZeroSize((count * sizeof(*(p))), p)

internal inline void
ZeroSize(usize size, void *ptr)
{
    u8 *dest = (u8 *)ptr;
    for (usize i = 0; i < size; ++i) {
        dest[i] = 0;
    }
}

#define PushArray(a, count, type) \
    (type *)PushSize_((a), (count) * sizeof(type), alignof(max_align_t))
#define PushStruct(a, type) (type *)PushSize_((a), sizeof(type), alignof(max_align_t))
#define PushSize(a, size) PushSize_((a), (size), alignof(max_align_t))

internal inline void *
PushSize_(memory_arena *arena, usize size_init, usize alignment)
{
    usize size = GetEffectiveSize(arena, size_init, alignment);

    if (arena->used + size > arena->capacity) {
        if (!arena->minimum_block_size) {
            arena->minimum_block_size = MB(1);
        }

        Assert(Platform.AllocateMemory);
        usize header_size = sizeof(memory_block);
        usize block_size = Max(size + header_size, arena->minimum_block_size);

        memory_block *new_block = (memory_block *)Platform.AllocateMemory(block_size);
        Assert(new_block);

        new_block->prev = arena->current_block;
        new_block->size = block_size;

        arena->current_block = new_block;
        arena->base = (u8 *)new_block + header_size;
        arena->used = 0;
        arena->capacity = block_size - header_size;

        size = GetEffectiveSize(arena, size_init, alignment);
    }

    Assert(arena->used + size <= arena->capacity);
    usize alignment_offset = GetAlignmentOffset(arena, alignment);
    void *result = arena->base + arena->used + alignment_offset;
    arena->used += size;

    Assert(size >= size_init);

    return result;
}

internal inline char *
PushString(memory_arena *arena, const char *source)
{
    usize size = 0;
    while (source[size]) {
        size += 1;
    }

    char *dest = (char *)PushSize(arena, size + 1);

    for (usize i = 0; i < size; ++i) {
        dest[i] = source[i];
    }

    dest[size] = '\0';

    return dest;
}

internal inline temporary_memory
BeginTemporaryMemory(memory_arena *arena)
{
    temporary_memory result = { 0 };

    result.arena = arena;
    result.used = arena->used;
    arena->temp_count += 1;

    return result;
}

internal inline void
EndTemporaryMemory(temporary_memory temp_mem)
{
    memory_arena *arena = temp_mem.arena;
    Assert(arena->used >= temp_mem.used);
    arena->used = temp_mem.used;
    Assert(arena->temp_count > 0);
    arena->temp_count -= 1;
}

internal inline void
FreeArena(memory_arena *arena)
{
    memory_block *block = arena->current_block;
    while (block) {
        memory_block *prev = block->prev;
        Platform.DeallocateMemory(block, block->size);
        block = prev;
    }

    ZeroStruct(arena);
}

#endif // GAME_H
