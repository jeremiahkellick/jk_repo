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

typedef union Segment {
    JkVector2 endpoints[2];
    struct {
        JkVector2 p1;
        JkVector2 p2;
    };
} Segment;

typedef struct Edge {
    Segment segment;
    float direction;
} Edge;

typedef struct ActiveEdge {
    struct ActiveEdge *next;
    Edge edge;
} ActiveEdge;

static int edge_compare(void *a, void *b)
{
    return jk_float_compare(((Edge *)a)->segment.p1.y, ((Edge *)b)->segment.p1.y);
}

static void edge_sort(Edge *array, int length)
{
    Edge tmp;
    jk_quicksort(array, length, sizeof(Edge), &tmp, edge_compare);
}

// No bounds checking so they return the intersection as if the segment was an infinite line

static float segment_y_intersection(Segment segment, float y)
{
    float delta_y = segment.p2.y - segment.p1.y;
    JK_ASSERT(delta_y != 0);
    return ((segment.p2.x - segment.p1.x) / delta_y) * (y - segment.p1.y) + segment.p1.x;
}

static float segment_x_intersection(Segment segment, float x)
{
    float delta_x = segment.p2.x - segment.p1.x;
    JK_ASSERT(delta_x != 0);
    return ((segment.p2.y - segment.p1.y) / delta_x) * (x - segment.p1.x) + segment.p1.y;
}

static JkVector2 point_on_circle(float t, float radius)
{
    float angle = t * 2.0f * (float)JK_PI;
    return (JkVector2){radius * cosf(angle), radius * sinf(angle)};
}

#define POINT_COUNT 128

void render(Bezier *bezier)
{
    int32_t bitmap_width = bezier->draw_square_side_length;
    int32_t bitmap_height = bezier->draw_square_side_length;
    float scale = (float)bezier->draw_square_side_length;

    Arena arena = {.memory = {.size = sizeof(bezier->memory), .data = bezier->memory}};

    JkVector2 points[POINT_COUNT];
    for (int32_t i = 0; i < POINT_COUNT; i++) {
        points[i] = jk_vector_2_add(
                (JkVector2){0.5f, 0.5f}, point_on_circle((1.0f / POINT_COUNT) * i, 0.4f));
    }

    Edge *edges = arena_pointer_get(&arena);
    {
        int32_t j = JK_ARRAY_COUNT(points) - 1;
        for (int32_t k = 0; k < JK_ARRAY_COUNT(points); j = k++) {
            if (points[j].y != points[k].y) {
                Edge *edge = arena_alloc(&arena, sizeof(*edge));
                if (points[j].y < points[k].y) {
                    edge->segment.p1 = jk_vector_2_mul(scale, points[j]);
                    edge->segment.p2 = jk_vector_2_mul(scale, points[k]);
                    edge->direction = -1.0f;
                } else {
                    edge->segment.p1 = jk_vector_2_mul(scale, points[k]);
                    edge->segment.p2 = jk_vector_2_mul(scale, points[j]);
                    edge->direction = 1.0f;
                }
            }
        }
    }
    uint64_t edge_count = (Edge *)arena_pointer_get(&arena) - edges;

    // Sort edges by p1.y
    edge_sort(edges, (int)edge_count);

    uint64_t edge_index = 0;
    ActiveEdge *active_edges = 0;

    uint64_t coverage_size = sizeof(float) * (bitmap_width + 1) * 2;
    float *coverage = arena_alloc(&arena, coverage_size);

    float *fill = coverage + bitmap_width + 1;

    for (int32_t y = 0; y < bitmap_height; y++) {
        memset(coverage, 0, coverage_size);

        float scan_y_top = (float)y;
        float scan_y_bottom = scan_y_top + 1.0f;

        // Activate relevant edges
        while (edge_index < edge_count && edges[edge_index].segment.p1.y < scan_y_bottom) {
            ActiveEdge *new_active_edge = arena_alloc(&arena, sizeof(*new_active_edge));
            new_active_edge->next = active_edges;
            new_active_edge->edge = edges[edge_index];
            active_edges = new_active_edge;
            edge_index++;
        }

        for (ActiveEdge *active_edge = active_edges; active_edge; active_edge = active_edge->next) {
            Edge edge = active_edge->edge;
            float y_top = JK_MAX(edge.segment.p1.y, scan_y_top);
            float y_bottom = JK_MIN(edge.segment.p2.y, scan_y_bottom);
            if (y_top < y_bottom) {
                float height = y_bottom - y_top;
                float x_top = segment_y_intersection(edge.segment, y_top);
                float x_bottom = segment_y_intersection(edge.segment, y_bottom);

                float y_start;
                float y_end;
                float x_start;
                float x_end;
                if (x_top < x_bottom) {
                    y_start = y_top;
                    y_end = y_bottom;
                    x_start = x_top;
                    x_end = x_bottom;
                } else {
                    y_start = y_bottom;
                    y_end = y_top;
                    x_start = x_bottom;
                    x_end = x_top;
                }

                int32_t first_pixel_index = (int32_t)x_start;
                float first_pixel_right = (float)(first_pixel_index + 1);

                if (first_pixel_index == (int32_t)x_end) {
                    // Edge only covers one pixel

                    // Compute trapezoid area
                    float top_width = first_pixel_right - x_top;
                    float bottom_width = first_pixel_right - x_bottom;
                    float area = (top_width + bottom_width) / 2.0f * height;
                    coverage[first_pixel_index] += edge.direction * area;

                    // Fill everything to the right with height
                    fill[first_pixel_index + 1] += edge.direction * height;
                } else {
                    // Edge covers multiple pixels
                    float delta_y = (edge.segment.p2.y - edge.segment.p1.y)
                            / (edge.segment.p2.x - edge.segment.p1.x);

                    // Handle first pixel
                    float first_x_intersection =
                            segment_x_intersection(edge.segment, first_pixel_right);
                    float first_pixel_y_offset = first_x_intersection - y_start;
                    float first_pixel_area =
                            (first_pixel_right - x_start) * fabsf(first_pixel_y_offset) / 2.0f;
                    coverage[first_pixel_index] += edge.direction * first_pixel_area;

                    // Handle middle pixels (if there are any)
                    float y_offset = first_pixel_y_offset;
                    int32_t pixel_index = first_pixel_index + 1;
                    for (; (float)(pixel_index + 1) < x_end; pixel_index++) {
                        coverage[pixel_index] += edge.direction * fabsf(y_offset + delta_y / 2.0f);
                        y_offset += delta_y;
                    }

                    // Handle last pixel
                    float last_x_intersection = y_start + y_offset;
                    float uncovered_triangle = fabsf(y_end - last_x_intersection)
                            * (x_end - (float)pixel_index) / 2.0f;
                    coverage[pixel_index] += edge.direction * (height - uncovered_triangle);

                    // Fill everything to the right with height
                    fill[pixel_index + 1] += edge.direction * height;
                }
            }
        }

        // Fill the scanline according to coverage
        float acc = 0.0f;
        for (int32_t x = 0; x < bitmap_width; x++) {
            acc += fill[x];
            int32_t value = (int32_t)((coverage[x] + acc) * 255.0f);
            if (255 < value) {
                value = 255;
            }
            bezier->draw_buffer[y * DRAW_BUFFER_SIDE_LENGTH + x] =
                    blend_alpha(color_light_squares, color_background, (uint8_t)value);
        }

        // Deactivate edges that are no longer relevant
        ActiveEdge **stride = &active_edges;
        while (*stride) {
            ActiveEdge *active_edge = *stride;
            if (active_edge->edge.segment.p2.y < scan_y_top) {
                *stride = active_edge->next;
            } else {
                stride = &active_edge->next;
            }
        }
    }
}
