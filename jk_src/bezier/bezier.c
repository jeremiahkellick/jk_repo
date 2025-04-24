#include "bezier.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

// #jk_build compiler_arguments /LD
// #jk_build linker_arguments /OUT:bezier.dll /EXPORT:render
// #jk_build single_translation_unit

// #jk_build dependencies_begin
#include <jk_src/jk_lib/jk_lib.h>
// #jk_build dependencies_end

_Static_assert(sizeof(Color) == 4, "Color must be 4 bytes");

static char debug_print_buffer[4096];

static int debug_printf(void (*debug_print)(char *), char *format, ...)
{
    va_list args;
    va_start(args, format);
    int result = vsnprintf(debug_print_buffer, JK_ARRAY_COUNT(debug_print_buffer), format, args);
    va_end(args);
    debug_print(debug_print_buffer);
    return result;
}

static Color color_background = {CLEAR_COLOR_B, CLEAR_COLOR_G, CLEAR_COLOR_R};

// Color light_squares = {0xde, 0xe2, 0xde};
// Color dark_squares = {0x39, 0x41, 0x3a};

static Color color_light_squares = {0xe9, 0xe2, 0xd7};
static Color color_dark_squares = {0x50, 0x41, 0x2b};

// Blended halfway between the base square colors and #E26D5C

static Color color_selection = {0x5c, 0x6d, 0xe2};
static Color color_move_prev = {0x2b, 0xa6, 0xff};

// Color white = {0x8e, 0x8e, 0x8e};
static Color color_white_pieces = {0x82, 0x92, 0x85};
static Color color_black_pieces = {0xff, 0x73, 0xa2};

static Color blend(Color a, Color b)
{
    return (Color){.r = a.r / 2 + b.r / 2, .g = a.g / 2 + b.g / 2, .b = a.b / 2 + b.b / 2};
}

static Color blend_alpha(Color foreground, Color background, uint8_t alpha)
{
    Color result = {0, 0, 0, 255};
    for (uint8_t i = 0; i < 3; i++) {
        result.v[i] = ((int32_t)foreground.v[i] * (int32_t)alpha
                              + background.v[i] * (255 - (int32_t)alpha))
                / 255;
    }
    return result;
}

void render(Bezier *bezier)
{
    for (int32_t y = 0; y < bezier->draw_square_side_length; y++) {
        for (int32_t x = 0; x < bezier->draw_square_side_length; x++) {
            bezier->draw_buffer[y * DRAW_BUFFER_SIDE_LENGTH + x] =
                    (Color){.r = (uint8_t)x, .b = (uint8_t)y};
        }
    }
}
