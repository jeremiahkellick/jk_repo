#include "graphics.h"

// #jk_build library
// #jk_build export render
// #jk_build single_translation_unit

static JkColor fg = {.r = 0x49, .g = 0x6b, .b = 0x83};
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

    draw_pixel(state, (JkIntVector2){10, 10}, (JkColor){.r = 0xff, .g = 0x88, .b = 0x88});

    draw_rect(state, (JkIntVector2){100, 100}, (JkIntVector2){640, 480}, fg);
}
