#include "graphics.h"

// #jk_build library
// #jk_build export render
// #jk_build single_translation_unit

static JkColor fg = {.r = 0x49, .g = 0x6b, .b = 0x83};
static JkColor bg = {.r = CLEAR_COLOR_R, .g = CLEAR_COLOR_G, .b = CLEAR_COLOR_B};

static void draw_rect(
        State *state, int32_t x, int32_t y, int32_t width, int32_t height, JkColor color)
{
    int32_t x0 = JK_MAX(x, 0);
    int32_t y0 = JK_MAX(y, 0);
    int32_t x1 = JK_MIN(x + width, DRAW_BUFFER_SIDE_LENGTH);
    int32_t y1 = JK_MIN(y + height, DRAW_BUFFER_SIDE_LENGTH);

    for (int32_t cy = y0; cy < y1; cy++) {
        for (int32_t cx = x0; cx < x1; cx++) {
            state->draw_buffer[DRAW_BUFFER_SIDE_LENGTH * cy + cx] = color;
        }
    }
}

void render(State *state)
{
    // Clear buffer
    for (int32_t y = 0; y < state->dimensions.y; y++) {
        for (int32_t x = 0; x < state->dimensions.x; x++) {
            state->draw_buffer[y * DRAW_BUFFER_SIDE_LENGTH + x] = bg;
        }
    }

    draw_rect(state, 100, 100, 640, 480, fg);
}
