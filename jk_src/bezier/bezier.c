#include "bezier.h"

#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

// #jk_build compiler_arguments /LD
// #jk_build linker_arguments /OUT:bezier.dll /EXPORT:bezier_render
// #jk_build single_translation_unit

// #jk_build dependencies_begin
#include <jk_src/jk_lib/jk_lib.h>
#include <jk_src/jk_shapes/jk_shapes.h>
#include <jk_src/stb/stb_truetype.h>
// #jk_build dependencies_end

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

static JkColor color_background = {CLEAR_COLOR_B, CLEAR_COLOR_G, CLEAR_COLOR_R};

// JkColor light_squares = {0xde, 0xe2, 0xde};
// JkColor dark_squares = {0x39, 0x41, 0x3a};

static JkColor color_light_squares = {0xe9, 0xe2, 0xd7};
static JkColor color_dark_squares = {0x50, 0x41, 0x2b};

// Blended halfway between the base square colors and #E26D5C

static JkColor color_selection = {0x5c, 0x6d, 0xe2};
static JkColor color_move_prev = {0x2b, 0xa6, 0xff};

// JkColor white = {0x8e, 0x8e, 0x8e};
static JkColor color_white_pieces = {0x82, 0x92, 0x85};
static JkColor color_black_pieces = {0xff, 0x73, 0xa2};

static JkColor blend(JkColor a, JkColor b)
{
    return (JkColor){.r = a.r / 2 + b.r / 2, .g = a.g / 2 + b.g / 2, .b = a.b / 2 + b.b / 2};
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

void bezier_render(ChessAssets *assets, Bezier *bezier)
{
    float const canvas_size = 640.0f;

    JkBuffer display_string = JKS("Hello, world!");
    float first_point = -assets->shapes[display_string.data[0] + CHARACTER_SHAPE_OFFSET].offset.x;
    float last_point = first_point;
    for (int32_t i = 0; i < display_string.size - 1; i++) {
        last_point += assets->shapes[display_string.data[i] + CHARACTER_SHAPE_OFFSET].advance_width;
    }
    JkShape *last_character = assets->shapes
            + (display_string.data[display_string.size - 1] + CHARACTER_SHAPE_OFFSET);
    float display_string_width =
            last_point + last_character->offset.x + last_character->dimensions.x;
    int32_t display_string_height = assets->font_y1_max - assets->font_y0_min;
    int32_t max_dimension = JK_MAX(jk_round(display_string_width), display_string_height);

    float font_scale = (canvas_size - 1.0f) / (float)max_dimension;

    JkArena arena = {.memory = {.size = sizeof(bezier->memory), .data = bezier->memory}};

    JkShapesRenderer renderer;
    jk_shapes_renderer_init(&renderer,
            (float)bezier->draw_square_side_length / canvas_size,
            assets,
            (JkShapeArray){.count = JK_ARRAY_COUNT(assets->shapes), .items = assets->shapes},
            &arena);

    {
        jk_shapes_draw(&renderer, 2, (JkVector2){256.0f, 256.0f}, 1.0f, color_black_pieces);
        jk_shapes_draw(&renderer, 1, (JkVector2){320.0f, 256.0f}, 1.0f, color_black_pieces);
        jk_shapes_draw(&renderer, 3, (JkVector2){256.0f, 320.0f}, 1.0f, color_black_pieces);
        jk_shapes_draw(&renderer, 4, (JkVector2){320.0f, 320.0f}, 1.0f, color_black_pieces);

        JkVector2 current_point = {font_scale * first_point, -font_scale * assets->font_y0_min};
        for (int32_t i = 0; i < display_string.size; i++) {
            current_point.x += jk_shapes_draw(&renderer,
                    display_string.data[i] + CHARACTER_SHAPE_OFFSET,
                    current_point,
                    font_scale,
                    color_light_squares);
        }

        JkShapesDrawCommandArray draw_commands = jk_shapes_draw_commands_get(&renderer);

        JkIntVector2 pos;
        int32_t cs = 0;
        int32_t ce = 0;
        for (pos.y = 0; pos.y < bezier->draw_square_side_length; pos.y++) {
            while (ce < draw_commands.count && draw_commands.items[ce].position.y <= pos.y) {
                ce++;
            }
            while (cs < draw_commands.count
                    && !(pos.y < draw_commands.items[cs].position.y
                                    + draw_commands.items[cs].bitmap->dimensions.y)) {
                cs++;
            }

            for (pos.x = 0; pos.x < bezier->draw_square_side_length; pos.x++) {
                JkColor color = color_dark_squares;
                for (int32_t i = cs; i < ce; i++) {
                    JkShapesBitmap *bitmap = draw_commands.items[i].bitmap;
                    JkIntVector2 bitmap_pos =
                            jk_int_vector_2_sub(pos, draw_commands.items[i].position);
                    if (0 <= bitmap_pos.x && bitmap_pos.x < bitmap->dimensions.x
                            && bitmap_pos.y < bitmap->dimensions.y) {
                        int32_t index = bitmap_pos.y * bitmap->dimensions.x + bitmap_pos.x;
                        color = blend_alpha(
                                color_light_squares, color_dark_squares, bitmap->data[index]);
                        break;
                    }
                }
                bezier->draw_buffer[pos.y * DRAW_BUFFER_SIDE_LENGTH + pos.x] = color;
            }
        }
    }
}
