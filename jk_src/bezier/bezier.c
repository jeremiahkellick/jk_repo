#include "bezier.h"

#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

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

typedef struct Edge {
    float x0;
    float y0;
    float x1;
    float y1;
} Edge;

typedef struct ActiveEdge {
    struct ActiveEdge *next;
    float fx;
    float fdx;
    float fdy;
    float direction;
    float y_start;
    float y_end;
} ActiveEdge;

static float scanline_data[DRAW_BUFFER_WIDTH * 2 + 1];

#define EDGE_COUNT 3

static float sized_trapezoid_area(float height, float top_width, float bottom_width)
{
    JK_ASSERT(top_width >= 0);
    JK_ASSERT(bottom_width >= 0);
    return (top_width + bottom_width) / 2.0f * height;
}

static float position_trapezoid_area(float height, float tx0, float tx1, float bx0, float bx1)
{
    return sized_trapezoid_area(height, tx1 - tx0, bx1 - bx0);
}

void render(Bezier *bezier)
{
    Arena arena = {.memory = {.size = sizeof(bezier->memory), .data = bezier->memory}};

    int32_t width = bezier->draw_square_side_length;
    int32_t height = bezier->draw_square_side_length;

    Edge edges[EDGE_COUNT] = {
        {0.1f, 0.1f, 0.1f, 0.9f},
        {0.1f, 0.1f, 0.9f, 0.9f},
        {0.1f, 0.9f, 0.9f, 0.9f},
    };

    ActiveEdge *active_edges_head = 0;
    float *scanline = scanline_data;
    float *scanline2 = scanline_data + width;

    int32_t y = 0;
    while (y < height) {
        float scan_y_top = y + 0.0f;
        float scan_y_bottom = y + 1.0f;

        memset(scanline_data, 0, sizeof(scanline_data[0]) * (width * 2 + 1));

        // Remove all active edges that terminate before the top of this scanline
        ActiveEdge **step = &active_edges_head;
        while (*step) {
            ActiveEdge *active_edge = *step;
            if (active_edge->y_end <= scan_y_top) {
                *step = active_edge->next;
                // JK_ASSERT(node->direction);
                // node->direction = 0;
                // allocator_free(node);
            } else {
                step = &active_edge->next;
            }
        }

        for (int32_t i = 0; i < EDGE_COUNT && edges[i].y0 <= scan_y_bottom; i++) {
            if (edges[i].y0 != edges[i].y1) {
                // Create new active edge
                ActiveEdge *active_edge = arena_alloc(&arena, sizeof(*active_edge));
                JK_ASSERT(active_edge);
                float dxdy = (edges[i].x1 - edges[i].x0) / (edges[i].y1 - edges[i].y0);
                active_edge->fdx = dxdy;
                active_edge->fdy = dxdy != 0.0f ? (1.0f / dxdy) : 0.0f;
                active_edge->fx = edges[i].x0 + dxdy * (scan_y_top - edges[i].y0);
                // active_edge->direction = edges[i].invert ? 1.0f : -1.0f;
                active_edge->y_start = edges[i].y0;
                active_edge->y_end = edges[i].y1;
                active_edge->next = 0;

                // if (j == 0 && off_y != 0) { ... }

                JK_ASSERT(scan_y_top <= active_edge->y_end);

                // Insert at front
                active_edge->next = active_edges_head;
                active_edges_head = active_edge;
            }
        }

        // Fill active edges
        for (ActiveEdge *active_edge = active_edges_head; active_edge;
                active_edge = active_edge->next) {
            JK_ASSERT(scan_y_top <= active_edge->y_end);

            if (active_edge->fdx == 0.0f) {
            } else {
                float x0 = active_edge->fx;
                float dx = active_edge->fdx;
                float xb = x0 + dx;
                float dy = active_edge->fdy;
                JK_ASSERT(
                        active_edge->y_start <= scan_y_bottom && scan_y_top <= active_edge->y_end);

                float x_top;
                float x_bottom;
                float sy0;
                float sy1;

                // Compute endpoints of line segment clipped to this scanline
                if (scan_y_top < active_edge->y_start) {
                    x_top = x0 + dx * (active_edge->y_start - scan_y_top);
                    sy0 = active_edge->y_start;
                } else {
                    x_top = x0;
                    sy0 = scan_y_top;
                }
                if (active_edge->y_end < scan_y_bottom) {
                    x_bottom = x0 + dx * (active_edge->y_end - scan_y_top);
                    sy1 = active_edge->y_end;
                } else {
                    x_bottom = xb;
                    sy1 = scan_y_bottom;
                }

                if (0 <= x_top && 0 <= x_bottom && x_top < width && x_bottom < width) {
                    if ((int32_t)x_top == (int32_t)x_bottom) {
                        // Only spans one pixel
                        float height;
                        int32_t x = (int32_t)x_top;
                        height = (sy1 - sy0) * active_edge->direction;
                        JK_ASSERT(0 <= x && x < width);
                        scanline[x] += position_trapezoid_area(
                                height, x_top, x + 1.0f, x_bottom, x + 1.0f);
                        scanline2[x] += height; // Everything right of this pixel is filled
                    }
                }
            }
        }

        {
            float sum = 0.0f;
            for (int32_t i = 0; i < width; i++) {
                sum += scanline2[i];
                float k = scanline[i] + sum;
                k = fabsf(k) * 255.0f + 0.5f;
                int32_t m = JK_MIN((int32_t)k, 255);
                uint8_t value = (uint8_t)m;
                bezier->draw_buffer[y * DRAW_BUFFER_WIDTH + i] = (Color){value, value, value};
            }
        }

        // Advance all the active edges
        for (ActiveEdge *active_edge = active_edges_head; active_edge;
                active_edge = active_edge->next) {
            active_edge->fx += active_edge->fdx;
        }
    }
}
