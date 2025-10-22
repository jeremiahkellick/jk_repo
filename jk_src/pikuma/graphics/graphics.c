#include "graphics.h"

// #jk_build library
// #jk_build export render
// #jk_build single_translation_unit

static JkColor fg = {.r = 0x49, .g = 0x6b, .b = 0x83};
static JkColor bg = {.r = CLEAR_COLOR_R, .g = CLEAR_COLOR_G, .b = CLEAR_COLOR_B};

void render(State *state)
{
    for (int32_t y = 0; y < state->dimensions.y; y++) {
        for (int32_t x = 0; x < state->dimensions.x; x++) {
            state->draw_buffer[y * DRAW_BUFFER_SIDE_LENGTH + x] =
                    x % 10 == 0 || y % 10 == 0 ? fg : bg;
        }
    }
}
