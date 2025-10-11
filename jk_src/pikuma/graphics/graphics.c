#include "graphics.h"

void render(State *state)
{
    for (int32_t y = 0; y < state->dimensions.y; y++) {
        for (int32_t x = 0; x < state->dimensions.x; x++) {
            state->draw_buffer[y * DRAW_BUFFER_SIDE_LENGTH + x] = (JkColor){
                .r = 255 * y / state->dimensions.y, .b = 255 * x / state->dimensions.x};
        }
    }
}
