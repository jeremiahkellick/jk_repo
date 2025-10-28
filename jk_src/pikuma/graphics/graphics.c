#include "graphics.h"

// #jk_build library
// #jk_build export render
// #jk_build single_translation_unit

// #jk_build dependencies_begin
#include <jk_src/jk_lib/jk_lib.h>
// #jk_build dependencies_end

static JkColor fg = {.r = 0xff, .g = 0x69, .b = 0xb4};
static JkColor bg = {.r = CLEAR_COLOR_R, .g = CLEAR_COLOR_G, .b = CLEAR_COLOR_B};

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

float camera_side_length = 640.0f;

void render(State *state)
{
    if (!JK_FLAG_GET(state->flags, FLAG_INITIALIZED)) {
        JK_FLAG_SET(state->flags, FLAG_INITIALIZED, 1);

        for (int32_t i = 0; i < (int32_t)JK_ARRAY_COUNT(state->points); i++) {
            state->points[i].x = -1.0f + (2.0f / 8.0f) * (i % 9);
            state->points[i].y = -1.0f + (2.0f / 8.0f) * ((i / 9) % 9);
            state->points[i].z = -1.0f + (2.0f / 8.0f) * (int32_t)(i / (9 * 9));
        }
    }

    // Clear buffer
    for (int32_t y = 0; y < state->dimensions.y; y++) {
        for (int32_t x = 0; x < state->dimensions.x; x++) {
            state->draw_buffer[y * DRAW_BUFFER_SIDE_LENGTH + x] = bg;
        }
    }

    float scale = JK_MIN(state->dimensions.x * 0.5f, state->dimensions.y * 0.5f) / 2.0f;
    JkVector2 offset = {state->dimensions.x / 2.0f, state->dimensions.y / 2.0f};

    for (int32_t i = 0; i < (int32_t)JK_ARRAY_COUNT(state->points); i++) {
        JkVector2 pos = jk_vector_2_add(
                jk_vector_2_mul(scale, (JkVector2){state->points[i].x, state->points[i].z}),
                offset);
        draw_pixel(state, jk_vector_2_round(pos), fg);
    }
}
