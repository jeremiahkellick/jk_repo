#include "graphics.h"

// #jk_build library
// #jk_build export render
// #jk_build single_translation_unit

// #jk_build dependencies_begin
#include <jk_src/jk_lib/jk_lib.h>
// #jk_build dependencies_end

static JkColor fg = {.r = 0xff, .g = 0x69, .b = 0xb4, .a = 0xff};
static JkColor bg = {.r = CLEAR_COLOR_R, .g = CLEAR_COLOR_G, .b = CLEAR_COLOR_B, .a = 0xff};

static void draw_pixel(State *state, JkIntVector2 pos, JkColor color)
{
    if (0 <= pos.x && pos.x < state->dimensions.x && 0 <= pos.y && pos.y < state->dimensions.y) {
        state->draw_buffer[DRAW_BUFFER_SIDE_LENGTH * pos.y + pos.x] = color;
    }
}

static void draw_rect(State *state, JkIntVector2 pos, JkIntVector2 dimensions, JkColor color)
{
    JkIntVector2 top_left;
    for (uint64_t i = 0; i < JK_ARRAY_COUNT(top_left.coords); i++) {
        top_left.coords[i] = JK_MAX(pos.coords[i], 0);
    }
    JkIntVector2 bottom_right;
    for (uint64_t i = 0; i < JK_ARRAY_COUNT(bottom_right.coords); i++) {
        bottom_right.coords[i] =
                JK_MIN(top_left.coords[i] + dimensions.coords[i], state->dimensions.coords[i]);
    }

    for (int32_t y = top_left.y; y < bottom_right.y; y++) {
        for (int32_t x = top_left.x; x < bottom_right.x; x++) {
            state->draw_buffer[DRAW_BUFFER_SIDE_LENGTH * y + x] = color;
        }
    }
}

static uint8_t color_multiply(uint8_t a, uint8_t b)
{
    return ((uint32_t)a * (uint32_t)b) / 255;
}

static JkColor blend_alpha(JkColor foreground, JkColor background, uint8_t alpha)
{
    JkColor result = {0, 0, 0, 255};
    for (uint8_t i = 0; i < 3; i++) {
        result.v[i] = ((int32_t)foreground.v[i] * (int32_t)alpha
                              + background.v[i] * (255 - (int32_t)alpha))
                / 255;
    }
    return result;
}

// ---- Xiaolin Wu's line algorithm begin --------------------------------------------

static uint8_t region_code(JkIntVector2 dimensions, JkVector2 v)
{
    return ((v.x < 0.0f) << 0) | ((dimensions.x - 1.0f < v.x) << 1) | ((v.y < 0.0f) << 2)
            | ((dimensions.y - 1.0f < v.y) << 3);
}

typedef struct Endpoint {
    uint8_t code;
    JkVector2 *point;
} Endpoint;

static b32 clip_to_draw_region(JkIntVector2 dimensions, JkVector2 *a, JkVector2 *b)
{
    Endpoint endpoint_a = {.code = region_code(dimensions, *a), .point = a};
    Endpoint endpoint_b = {.code = region_code(dimensions, *b), .point = b};

    for (;;) {
        if (!(endpoint_a.code | endpoint_b.code)) {
            return 1;
        } else if (endpoint_a.code & endpoint_b.code) {
            return 0;
        } else {
            JkVector2 u = *a;
            JkVector2 v = *b;
            Endpoint *endpoint = endpoint_a.code < endpoint_b.code ? &endpoint_b : &endpoint_a;
            if ((endpoint->code >> 0) & 1) {
                endpoint->point->x = 0.0f;
                endpoint->point->y = u.y + (v.y - u.y) * (0.0f - u.x) / (v.x - u.x);
            } else if ((endpoint->code >> 1) & 1) {
                endpoint->point->x = dimensions.x - 1.0f;
                endpoint->point->y = u.y + (v.y - u.y) * (dimensions.x - 1.0f - u.x) / (v.x - u.x);
            } else if ((endpoint->code >> 2) & 1) {
                endpoint->point->x = u.x + (v.x - u.x) * (0.0f - u.y) / (v.y - u.y);
                endpoint->point->y = 0.0f;
            } else if ((endpoint->code >> 3) & 1) {
                endpoint->point->x = u.x + (v.x - u.x) * (dimensions.y - 1.0f - u.y) / (v.y - u.y);
                endpoint->point->y = dimensions.y - 1.0f;
            }
            endpoint->code = region_code(dimensions, *endpoint->point);
        }
    }
}

static float fpart(float x)
{
    return x - jk_floor_f32(x);
}

static float fpart_complement(float x)
{
    return 1.0f - fpart(x);
}

static void plot(JkColor *draw_buffer, JkColor color, int32_t x, int32_t y, float brightness)
{
    int32_t brightness_i = (int32_t)(brightness * 255.0f);
    if (brightness_i > 255) {
        brightness_i = 255;
    }
    draw_buffer[y * DRAW_BUFFER_SIDE_LENGTH + x] = blend_alpha(color,
            draw_buffer[y * DRAW_BUFFER_SIDE_LENGTH + x],
            color_multiply(color.a, (uint8_t)brightness_i));
}

static void draw_line(State *state, JkColor color, JkVector2 a, JkVector2 b)
{
    if (!clip_to_draw_region(state->dimensions, &a, &b)) {
        return;
    }

    JkColor *draw_buffer = state->draw_buffer;

    b32 steep = JK_ABS(b.y - a.y) > JK_ABS(b.x - a.x);

    if (steep) {
        JK_SWAP(a.x, a.y, float);
        JK_SWAP(b.x, b.y, float);
    }
    if (a.x > b.x) {
        JK_SWAP(a, b, JkVector2);
    }

    JkVector2 delta = jk_vector_2_sub(b, a);

    float gradient;
    if (delta.x) {
        gradient = delta.y / delta.x;
    } else {
        gradient = 1.0f;
    }

    // handle first endpoint
    int32_t x_pixel_1;
    float intery;
    {
        x_pixel_1 = jk_round(a.x);
        float yend = a.y + gradient * (x_pixel_1 - a.x);
        float xcoverage = fpart_complement(a.x + 0.5f);
        int32_t y_pixel_1 = (int32_t)yend;
        if (steep) {
            plot(draw_buffer, color, y_pixel_1, x_pixel_1, fpart_complement(yend) * xcoverage);
            plot(draw_buffer, color, y_pixel_1 + 1, x_pixel_1, fpart(yend) * xcoverage);
        } else {
            plot(draw_buffer, color, x_pixel_1, y_pixel_1, fpart_complement(yend) * xcoverage);
            plot(draw_buffer, color, x_pixel_1, y_pixel_1 + 1, fpart(yend) * xcoverage);
        }
        intery = yend + gradient;
    }

    // handle second endpoint
    int32_t x_pixel_2;
    {
        x_pixel_2 = jk_round(b.x);
        float yend = b.y + gradient * (x_pixel_2 - b.x);
        float xcoverage = fpart(b.x + 0.5f);
        int32_t y_pixel_2 = (int32_t)yend;
        if (steep) {
            plot(draw_buffer, color, y_pixel_2, x_pixel_2, fpart_complement(yend) * xcoverage);
            plot(draw_buffer, color, y_pixel_2 + 1, x_pixel_2, fpart(yend) * xcoverage);
        } else {
            plot(draw_buffer, color, x_pixel_2, y_pixel_2, fpart_complement(yend) * xcoverage);
            plot(draw_buffer, color, x_pixel_2, y_pixel_2 + 1, fpart(yend) * xcoverage);
        }
    }

    if (steep) {
        for (int32_t x = x_pixel_1 + 1; x < x_pixel_2; x++) {
            plot(draw_buffer, color, (int32_t)intery, x, fpart_complement(intery));
            plot(draw_buffer, color, (int32_t)intery + 1, x, fpart(intery));
            intery += gradient;
        }
    } else {
        for (int32_t x = x_pixel_1 + 1; x < x_pixel_2; x++) {
            plot(draw_buffer, color, x, (int32_t)intery, fpart_complement(intery));
            plot(draw_buffer, color, x, (int32_t)intery + 1, fpart(intery));
            intery += gradient;
        }
    }
}

// ---- Xiaolin Wu's line algorithm end ----------------------------------------------

JkVector2 perspective_project(JkVector3 v)
{
    return (JkVector2){v.x / v.z, v.y / v.z};
}

JkVector3 camera_pos = {0.0f, 0.0f, -4.0f};
int32_t rotation_seconds = 8;

void render(Assets *assets, State *state)
{
    JkArenaRoot arena_root;
    JkArena arena = jk_arena_fixed_init(&arena_root, state->memory);

    JkVector3Array vertices = {
        .count = assets->vertices.size / sizeof(*vertices.items),
        .items = (JkVector3 *)((uint8_t *)assets + assets->vertices.offset),
    };
    FaceArray faces = {
        .count = assets->faces.size / sizeof(*faces.items),
        .items = (Face *)((uint8_t *)assets + assets->faces.offset),
    };

    if (!JK_FLAG_GET(state->flags, FLAG_INITIALIZED)) {
        JK_FLAG_SET(state->flags, FLAG_INITIALIZED, 1);
    }

    // Clear buffer
    for (int32_t y = 0; y < state->dimensions.y; y++) {
        for (int32_t x = 0; x < state->dimensions.x; x++) {
            state->draw_buffer[y * DRAW_BUFFER_SIDE_LENGTH + x] = bg;
        }
    }

    float scale = JK_MIN(state->dimensions.x * 1.5f, state->dimensions.y * 1.5f) / 2.0f;
    JkVector2 offset = {state->dimensions.x / 2.0f, state->dimensions.y / 2.0f};
    int32_t rotation_ticks = rotation_seconds * state->os_timer_frequency;
    float angle = 2 * JK_PI * ((float)(state->os_time % rotation_ticks) / (float)rotation_ticks);

    JkVector2 *screen_verticies = jk_arena_push(&arena, vertices.count * sizeof(*screen_verticies));
    for (int32_t i = 0; i < (int32_t)vertices.count; i++) {
        JkVector3 pos = vertices.items[i];
        pos = (JkVector3){
            .x = pos.x * jk_cos_f32(angle) + pos.z * jk_sin_f32(angle),
            .y = pos.y,
            .z = -pos.x * jk_sin_f32(angle) + pos.z * jk_cos_f32(angle),
        };
        pos = jk_vector_3_sub(pos, camera_pos);

        JkVector2 screen_pos = perspective_project(pos);
        screen_pos = jk_vector_2_mul(scale, screen_pos);
        screen_pos = jk_vector_2_add(screen_pos, offset);
        screen_verticies[i] = screen_pos;
    }

    uint8_t *edge_drawn = jk_arena_push_zero(&arena, vertices.count * vertices.count);
    for (int32_t face_index = 0; face_index < (int32_t)faces.count; face_index++) {
        Face *face = faces.items + face_index;
        for (int32_t i = 0; i < 3; i++) {
            int32_t next = (i + 1) % 3;
            int32_t indexes[2];
            if (face->v[i] < face->v[next]) {
                indexes[0] = face->v[i];
                indexes[1] = face->v[next];
            } else {
                indexes[0] = face->v[next];
                indexes[1] = face->v[i];
            }
            if (!edge_drawn[vertices.count * indexes[0] + indexes[1]]) {
                edge_drawn[vertices.count * indexes[0] + indexes[1]] = 1;
                draw_line(state, fg, screen_verticies[indexes[0]], screen_verticies[indexes[1]]);
            }
        }
    }
}
