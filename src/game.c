#include "game.h"
#include "raylib.h"
#include "raymath.h"

internal void
EmitDisintegrate(game_state *state, Vector2 pos, Color color, f32 radius)
{
    local_const usize particles_count = ArrayCount(state->particles);

    for (usize i = 0; i < (usize)radius; ++i) {
        particle *p = &state->particles[state->next_particle];

        p->pos = pos;

        f32 angle = (f32)GetRandomValue(0, 360) * DEG2RAD;
        f32 speed = (f32)GetRandomValue(50, 150);
        p->velocity = (Vector2){ cosf(angle) * speed, sinf(angle) * speed };

        p->life = 2.0f;
        p->color = color;
        p->radius = radius * 0.1f;

        state->next_particle = (state->next_particle + 1) % particles_count;
    }
}

internal void
UpdateParticles(game_state *state, game_input *input)
{
    usize count = ArrayCount(state->particles);
    for (usize i = 0; i < count; ++i) {
        particle *p = &state->particles[i];
        if (p->life <= 0) {
            continue;
        }

        p->pos = Vector2Add(p->pos, Vector2Scale(p->velocity, input->dt));
        p->velocity = Vector2Scale(p->velocity, 0.95f);
        p->life -= input->dt;
    }
}

internal void
RenderParticles(game_state *state)
{
    usize count = ArrayCount(state->particles);
    for (usize i = 0; i < count; ++i) {
        particle p = state->particles[i];
        if (p.life > 0) {
            DrawCircleV(p.pos, p.life, p.color);
        }
    }
}

internal void
OnEnemyHit(game_state *state, usize idx)
{
    state->healths[idx].value -= 20.0f;
    state->renderables[idx].flash_timer = 0.1f;
    state->renderables[idx].flash_color = WHITE;
}

internal void
OnPlayerHit(game_state *state, usize idx)
{
    state->healths[idx].value -= 10.0f;
    state->renderables[idx].flash_timer = 0.1f;
    state->renderables[idx].flash_color = RED;
}

internal void
SpawnPlayer(game_state *state)
{
    usize idx = state->entity_count++;
    Assert(idx < ArrayCount(state->entities));

    state->player_index = idx;
    state->entities[idx].components = Comp_Position | Comp_Velocity | Comp_Render | Comp_Health;
    state->entities[idx].tag = Tag_Player;

    state->positions[idx].value =
        (Vector2){ (f32)GetScreenWidth() / 2.0f, (f32)GetScreenHeight() / 2.0f };
    state->velocities[idx].value = Vector2Zero();
    state->renderables[idx].color = BLUE;
    state->renderables[idx].radius = 30;
    state->healths[idx].value = 100.0f;
}

internal void
SpawnEnemy(game_state *state, Vector2 pos)
{
    usize idx = state->entity_count++;
    Assert(idx < ArrayCount(state->entities));

    state->entities[idx].components = Comp_Position | Comp_Velocity | Comp_Render | Comp_Health;
    state->entities[idx].tag = Tag_Enemy;

    state->positions[idx].value = pos;
    state->renderables[idx].color = RED;
    state->renderables[idx].radius = 20;
    state->renderables[idx].flash_color = WHITE;
    state->healths[idx].value = 100.0f;

    Vector2 diff =
        Vector2Subtract(state->positions[state->player_index].value, state->positions[idx].value);

    f32 speed = 100.0f;
    state->velocities[idx].value = Vector2Scale(Vector2Normalize(diff), speed);
}

internal void
SpawnProjectile(game_state *state, Vector2 target)
{
    usize idx = state->entity_count++;
    Assert(idx < ArrayCount(state->entities));

    state->entities[idx].components = Comp_Position | Comp_Velocity | Comp_Render;
    state->entities[idx].tag = Tag_Projectile;

    state->positions[idx].value = state->positions[state->player_index].value;
    state->renderables[idx].color = state->renderables[state->player_index].color;
    state->renderables[idx].radius = 5;

    Vector2 diff = Vector2Subtract(target, state->positions[state->player_index].value);

    f32 speed = 500.0f;
    state->velocities[idx].value = Vector2Scale(Vector2Normalize(diff), speed);
}

internal void
SpawnSystem(game_state *state, game_input *input)
{
    state->enemy_spawn_timer += input->dt;
    state->projectile_spawn_timer += input->dt;

    f32 min_interval = 1.0f;
    f32 max_interval = 2.0f;

    f32 current_interval =
        Lerp(max_interval, min_interval, Clamp(state->time / 300.0f, 0.0f, 1.0f));

    if (state->enemy_spawn_timer >= current_interval) {
        state->enemy_spawn_timer = 0.0f;

        usize spawn_count = 3 + ((usize)state->time / 60.0f);

        for (usize i = 0; i < spawn_count; ++i) {
            Vector2 center = state->positions[state->player_index].value;

            f32 angle = (f32)GetRandomValue(0, 360) * DEG2RAD;
            f32 radius = 700.0f;
            Vector2 spawn_pos = { center.x + cosf(angle) * radius,
                                  center.y + sinf(angle) * radius };

            SpawnEnemy(state, spawn_pos);
        }
    }

    if (input->action_shoot && state->projectile_spawn_timer >= 0.1f) {
        state->projectile_spawn_timer = 0.0f;

        Vector2 world_mouse = GetScreenToWorld2D(input->mouse_pos, state->camera);
        SpawnProjectile(state, world_mouse);
    }
}

internal void
DespawnSystem(game_state *state)
{
    usize write_idx = 0;
    for (usize read_idx = 0; read_idx < state->entity_count; ++read_idx) {
        if (state->entities[read_idx].tag == Tag_Dead) {
            continue;
        }

        if (write_idx != read_idx) {
            state->entities[write_idx] = state->entities[read_idx];
#define X(type, name) state->name[write_idx] = state->name[read_idx];
            COMPONENT_LIST
#undef X

            if (read_idx == state->player_index) {
                state->player_index = write_idx;
            }
        }
        write_idx += 1;
    }

    state->entity_count = write_idx;
}

internal void
MovementSystem(game_state *state, game_input *input)
{
    for (usize i = 0; i < state->entity_count; ++i) {
        if (!HasFlags(state->entities[i].components, Comp_Velocity | Comp_Position)) {
            continue;
        }

        position *p = &state->positions[i];
        velocity *v = &state->velocities[i];

        p->value = Vector2Add(p->value, Vector2Scale(v->value, input->dt));
    }
}

internal void
EnemyAISystem(game_state *state)
{
    if (state->entities[state->player_index].tag == Tag_Dead)
        return;

    Vector2 player_pos = state->positions[state->player_index].value;

    for (usize i = 0; i < state->entity_count; ++i) {
        if (!HasFlags(state->entities[i].components, Comp_Position | Comp_Velocity) ||
            state->entities[i].tag != Tag_Enemy) {
            continue;
        }

        position pos = state->positions[i];
        velocity *vel = &state->velocities[i];

        Vector2 dir = Vector2Normalize(Vector2Subtract(player_pos, pos.value));
        f32 min_speed = 100.0f;
        f32 max_speed = 500.0f;

        f32 speed = Lerp(min_speed, max_speed, Clamp(state->time / 300.0f, 0.0f, 1.0f));
        vel->value = Vector2Scale(dir, speed);
    }
}

internal void
PlayerEnemyCollisionSystem(game_state *state)
{
    for (usize enemy_idx = 0; enemy_idx < state->entity_count; ++enemy_idx) {
        if (!HasFlags(state->entities[enemy_idx].components, Comp_Position | Comp_Render) ||
            state->entities[enemy_idx].tag != Tag_Enemy) {
            continue;
        }

        position player_pos = state->positions[state->player_index];
        position entity_pos = state->positions[enemy_idx];

        renderable player_r = state->renderables[state->player_index];
        renderable entity_r = state->renderables[enemy_idx];

        if (CheckCollisionCircles(
                player_pos.value, player_r.radius, entity_pos.value, entity_r.radius)) {
            state->entities[enemy_idx].tag = Tag_Dead;
            AddFlag(state->entities[state->player_index].components, Comp_Collision);

            OnPlayerHit(state, state->player_index);
        }
    }
}

internal void
ProjectileCollisionSystem(game_state *state)
{
    for (usize projectile_idx = 0; projectile_idx < state->entity_count; ++projectile_idx) {
        if (state->entities[projectile_idx].tag != Tag_Projectile ||
            !HasFlag(state->entities[projectile_idx].components, Comp_Render)) {
            continue;
        }

        renderable projectile_r = state->renderables[projectile_idx];
        position projectile_pos = state->positions[projectile_idx];

        for (usize enemy_idx = 0; enemy_idx < state->entity_count; ++enemy_idx) {
            if (state->entities[enemy_idx].tag != Tag_Enemy) {
                continue;
            }

            renderable enemy_r = state->renderables[enemy_idx];
            position enemy_pos = state->positions[enemy_idx];

            if (CheckCollisionCircles(
                    projectile_pos.value, projectile_r.radius, enemy_pos.value, enemy_r.radius)) {
                AddFlag(state->entities[enemy_idx].components, Comp_Collision);
                state->entities[projectile_idx].tag = Tag_Dead;

                OnEnemyHit(state, enemy_idx);

                break;
            }
        }
    }
}

internal void
ProjectileBoundarySystem(game_state *state)
{
    Vector2 top_lefft = GetScreenToWorld2D(Vector2Zero(), state->camera);
    Vector2 bottom_right = GetScreenToWorld2D(
        (Vector2){ (f32)GetScreenWidth(), (f32)GetScreenHeight() }, state->camera);

    f32 buffer = 100.0f;
    f32 min_x = top_lefft.x - buffer;
    f32 max_x = bottom_right.x + buffer;
    f32 min_y = top_lefft.y - buffer;
    f32 max_y = bottom_right.y + buffer;

    for (usize i = 0; i < state->entity_count; ++i) {
        if (state->entities[i].tag != Tag_Projectile) {
            continue;
        }

        Vector2 p = state->positions[i].value;

        if (p.x < min_x || p.x > max_x || p.y < min_y || p.y > max_y) {
            state->entities[i].tag = Tag_Dead;
        }
    }
}

internal void
CameraSystem(game_state *state)
{
    if (!HasFlags(state->entities[state->player_index].components, Comp_Position))
        return;

    Vector2 player_pos = state->positions[state->player_index].value;

    f32 lerp_factor = 0.1f;
    state->camera.target = Vector2Lerp(state->camera.target, player_pos, lerp_factor);
    state->camera.zoom = 1.0f;

    state->camera.offset = (Vector2){ (f32)GetScreenWidth() / 2.0f, (f32)GetScreenHeight() / 2.0f };
}

internal void
EffectSystem(game_state *state, game_input *input)
{
    for (usize i = 0; i < state->entity_count; ++i) {
        if (!HasFlag(state->entities[i].components, Comp_Render)) {
            continue;
        }

        if (state->renderables[i].flash_timer > 0) {
            state->renderables[i].flash_timer -= input->dt;
        }
    }
}

internal void
RenderSystem(game_state *state)
{
    for (usize i = 0; i < state->entity_count; ++i) {
        if (state->entities[i].tag == Tag_Dead) {
            continue;
        }

        if (!HasFlags(state->entities[i].components, Comp_Render | Comp_Position)) {
            continue;
        }

        position p = state->positions[i];
        renderable r = state->renderables[i];

        Color draw_color = r.flash_timer > 0 ? r.flash_color : r.color;
        DrawCircleV(p.value, r.radius, draw_color);
    }
}

internal void
PlayerInputSystem(game_state *state, game_input *input)
{
    velocity *player_velocity = &state->velocities[state->player_index];
    player_velocity->value = Vector2Zero();

    f32 speed = 200.0f;
    if (input->action_up)
        player_velocity->value.y = -speed;
    if (input->action_down)
        player_velocity->value.y = speed;
    if (input->action_left)
        player_velocity->value.x = -speed;
    if (input->action_right)
        player_velocity->value.x = speed;
}

internal void
HealthSystem(game_state *state)
{
    for (usize i = 0; i < state->entity_count; ++i) {
        entity *e = &state->entities[i];

        if (!HasFlags(e->components, Comp_Health | Comp_Render | Comp_Position)) {
            continue;
        }

        health *health = &state->healths[i];

        if (health->value <= 0.0f) {
            health->value = 0.0f;

            EmitDisintegrate(state,
                             state->positions[i].value,
                             state->renderables[i].color,
                             state->renderables[i].radius);
            state->entities[i].tag = Tag_Dead;
        }
    }
}

void
GameUpdateAndRender(platform_memory *memory, game_input *input)
{
    game_state *state = (game_state *)memory->permanent_storage;
    state->time += input->dt;

    if (!state->is_initialized) {
        InitializeArena(&state->world_arena,
                        memory->permanent_storage_size - sizeof(game_state),
                        (u8 *)memory->permanent_storage + sizeof(game_state));

        SpawnPlayer(state);
        state->mode = GameMode_Playing;

        state->is_initialized = true;
    }

    if (state->mode == GameMode_Gameover) {
        ZeroSize(state->world_arena.used, state->world_arena.base);
        ZeroSize(sizeof(*state), state);

        return;
    }

    if (state->mode == GameMode_Playing) {
        PlayerInputSystem(state, input);
        EnemyAISystem(state);
        SpawnSystem(state, input);

        MovementSystem(state, input);

        PlayerEnemyCollisionSystem(state);
        ProjectileCollisionSystem(state);
        ProjectileBoundarySystem(state);

        HealthSystem(state);
        EffectSystem(state, input);

        if (state->entities[state->player_index].tag == Tag_Dead) {
            state->mode = GameMode_Gameover;
        }
    }

    UpdateParticles(state, input);
    CameraSystem(state);

    BeginDrawing();
    ClearBackground(BLACK);

    BeginMode2D(state->camera);
    RenderSystem(state);
    RenderParticles(state);
    EndMode2D();
    DrawFPS(10, 10);
    EndDrawing();

    DespawnSystem(state);
}
