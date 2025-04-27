#include "bezier.h"

#include <math.h>
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

// -------- Arena begin ---------------------------------------------------------

typedef struct Arena {
    JkBuffer memory;
    uint64_t pos;
} Arena;

void *arena_alloc(Arena *arena, uint64_t byte_count)
{
    uint64_t new_pos = arena->pos + byte_count;
    if (new_pos <= arena->memory.size) {
        void *result = arena->memory.data + arena->pos;
        arena->pos = new_pos;
        return result;
    } else {
        return 0;
    }
}

void *arena_pointer_get(Arena *arena)
{
    return arena->memory.data + arena->pos;
}

void arena_pointer_set(Arena *arena, void *pointer)
{
    arena->pos = (uint8_t *)pointer - arena->memory.data;
}

// -------- Arena end -----------------------------------------------------------

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

typedef union Segment {
    JkVector2 endpoints[2];
    struct {
        JkVector2 p1;
        JkVector2 p2;
    };
} Segment;

#define EDGE_COUNT 3

// No bounds checking so returns the scanline intersection as if the segment was an infinite line
static float segment_scanline_intersection(Segment segment, float scanline_y)
{
    float delta_y = segment.p2.y - segment.p1.y;
    JK_ASSERT(delta_y != 0);
    return ((segment.p2.x - segment.p1.x) / delta_y) * (scanline_y - segment.p1.y) + segment.p1.x;
}

void render(Bezier *bezier)
{
    Arena arena = {.memory = {.size = sizeof(bezier->memory), .data = bezier->memory}};

    Segment edges[EDGE_COUNT] = {
        {.p1 = {0.1f, 0.01f}, .p2 = {0.01f, 0.99f}},
        {.p1 = {0.1f, 0.01f}, .p2 = {0.99f, 0.99f}},
        {.p1 = {0.01f, 0.99f}, .p2 = {0.99f, 0.99f}},
    };
    for (int32_t i = 0; i < EDGE_COUNT; i++) {
        for (int32_t j = 0; j < 2; j++) {
            for (int32_t k = 0; k < 2; k++) {
                edges[i].endpoints[j].coords[k] *= (float)bezier->draw_square_side_length;
            }
        }
    }

    for (int32_t y = 0; y < bezier->draw_square_side_length; y++) {
        float yf = y + 0.5f;
        float *intersections = arena_pointer_get(&arena);
        for (int32_t i = 0; i < EDGE_COUNT; i++) {
            if (edges[i].p1.y <= yf && yf < edges[i].p2.y) {
                float delta_y = edges[i].p2.y - edges[i].p1.y;
                JK_ASSERT(delta_y != 0);
                *(float *)arena_alloc(&arena, sizeof(float)) =
                        segment_scanline_intersection(edges[i], yf);
            }
        }
        uint64_t intersections_count = (float *)arena_pointer_get(&arena) - intersections;
        jk_quicksort_floats(intersections, (int)intersections_count);
        uint64_t intersection_index = 0;
        for (int32_t x = 0; x < bezier->draw_square_side_length; x++) {
            float xf = x + 0.5f;
            while (intersection_index < intersections_count
                    && intersections[intersection_index] < xf) {
                intersection_index++;
            }
            bezier->draw_buffer[y * DRAW_BUFFER_SIDE_LENGTH + x] =
                    intersection_index % 2 == 0 ? color_background : color_light_squares;
        }
        arena_pointer_set(&arena, intersections);
    }
}
