#include "chess.h"

#include <math.h>

// #jk_build compiler_arguments /LD
// #jk_build linker_arguments /OUT:chess.dll /EXPORT:update /EXPORT:render
// #jk_build single_translation_unit

// #jk_build dependencies_begin
#include <jk_src/jk_lib/jk_lib.h>
// #jk_build dependencies_end

_Static_assert(sizeof(Color) == 4, "Color must be 4 bytes");

static void audio_write(Audio *audio, int32_t pitch_multiplier)
{
    for (uint32_t sample_index = 0; sample_index < audio->sample_count; sample_index++) {
        for (int channel_index = 0; channel_index < AUDIO_CHANNEL_COUNT; channel_index++) {
            audio->sample_buffer[sample_index].channels[channel_index] = 0;
        }
        audio->audio_time++;
    }
}

UPDATE_FUNCTION(update)
{
    if (chess->input.flags & INPUT_FLAG_UP) {
        chess->y -= 4;
    }
    if (chess->input.flags & INPUT_FLAG_DOWN) {
        chess->y += 4;
    }
    if (chess->input.flags & INPUT_FLAG_LEFT) {
        chess->x -= 4;
    }
    if (chess->input.flags & INPUT_FLAG_RIGHT) {
        chess->x += 4;
    }

    audio_write(&chess->audio, (chess->input.flags & INPUT_FLAG_UP) ? 2 : 1);

    chess->time++;
}

Color background = {0x25, 0x29, 0x24};

Color light_squares = {0xc6, 0xcd, 0xc5};
Color dark_squares = {0x39, 0x41, 0x38};

RENDER_FUNCTION(render)
{
    int32_t square_size = 112;
    int32_t board_size = square_size * 8;
    int32_t x_offset = (chess->bitmap.width - board_size) / 2;
    if (x_offset < 0) {
        x_offset = 0;
    }
    int32_t y_offset = (chess->bitmap.height - board_size) / 2;
    if (y_offset < 0) {
        y_offset = 0;
    }

    for (int32_t y = 0; y < chess->bitmap.height; y++) {
        for (int32_t x = 0; x < chess->bitmap.width; x++) {
            int32_t board_x = x - x_offset;
            int32_t board_y = y - y_offset;
            Color color;
            if (board_x >= 0 && board_x < board_size && board_y >= 0 && board_y < board_size) {
                if ((board_x / square_size) % 2 == (board_y / square_size) % 2) {
                    color = light_squares;
                } else {
                    color = dark_squares;
                }
            } else {
                color = background;
            }
            chess->bitmap.memory[y * chess->bitmap.width + x] = color;
        }
    }
}
