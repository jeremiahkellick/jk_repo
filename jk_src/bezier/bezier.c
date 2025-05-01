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

static JkVector2 vector_average(JkVector2 a, JkVector2 b)
{
    return jk_vector_2_mul(0.5f, jk_vector_2_add(a, b));
}

static float vector_distance_squared(JkVector2 a, JkVector2 b)
{
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    return dx * dx + dy * dy;
}

typedef struct PointListNode {
    struct PointListNode *next;
    JkVector2 point;
    float t;
} PointListNode;

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

typedef struct EdgeArray {
    uint64_t count;
    Edge *items;
} EdgeArray;

typedef struct Transform {
    JkVector2 position;
    float scale;
} Transform;

static JkVector2 transform_apply(Transform transform, JkVector2 point)
{
    return jk_vector_2_add(transform.position, jk_vector_2_mul(transform.scale, point));
}

static Edge points_to_edge(JkVector2 a, JkVector2 b)
{
    Edge edge;
    if (a.y < b.y) {
        edge.segment.p1 = a;
        edge.segment.p2 = b;
        edge.direction = 1.0f;
    } else {
        edge.segment.p1 = b;
        edge.segment.p2 = a;
        edge.direction = -1.0f;
    }
    return edge;
}

JkVector2 evaluate_cubic_bezier(float t, JkVector2 p0, JkVector2 p1, JkVector2 p2, JkVector2 p3)
{
    float t_squared = t * t;
    float t_cubed = t_squared * t;
    float one_minus_t = 1.0f - t;
    float one_minus_t_squared = one_minus_t * one_minus_t;
    float one_minus_t_cubed = one_minus_t_squared * one_minus_t;

    JkVector2 result = jk_vector_2_mul(one_minus_t_cubed, p0);
    result = jk_vector_2_add(result, jk_vector_2_mul(3.0f * one_minus_t_squared * t, p1));
    result = jk_vector_2_add(result, jk_vector_2_mul(3.0f * one_minus_t * t_squared, p2));
    result = jk_vector_2_add(result, jk_vector_2_mul(t_cubed, p3));
    return result;
}

static EdgeArray shape_edges_get(Arena *arena,
        Arena *scratch_arena,
        PenCommandArray commands,
        Transform transform,
        float tolerance)
{
    EdgeArray edges = {.items = arena_pointer_get(arena)};

    JkVector2 cursor = {0};
    for (int32_t i = 0; i < commands.count; i++) {
        switch (commands.items[i].type) {
        case PEN_COMMAND_MOVE: {
            cursor = transform_apply(transform, commands.items[i].coords[0]);
        } break;

        case PEN_COMMAND_LINE: {
            JkVector2 prev_cursor = cursor;
            cursor = transform_apply(transform, commands.items[i].coords[0]);

            if (prev_cursor.y != cursor.y) {
                Edge *new_edge = arena_alloc(arena, sizeof(*new_edge));
                *new_edge = points_to_edge(prev_cursor, cursor);
            }
        } break;

        case PEN_COMMAND_CURVE: {
            JkVector2 p0 = cursor;
            JkVector2 p1 = transform_apply(transform, commands.items[i].coords[0]);
            JkVector2 p2 = transform_apply(transform, commands.items[i].coords[1]);
            JkVector2 p3 = transform_apply(transform, commands.items[i].coords[2]);

            cursor = p3;

            void *prev_scratch_arena_pointer = arena_pointer_get(scratch_arena);

            PointListNode *end_node = arena_alloc(scratch_arena, sizeof(*end_node));
            end_node->next = 0;
            end_node->point = p3;
            end_node->t = 1.0f;
            PointListNode *start_node = arena_alloc(scratch_arena, sizeof(*start_node));
            start_node->next = end_node;
            start_node->point = p0;
            start_node->t = 0.0f;

            b32 changed = 1;
            while (changed) {
                changed = 0;
                PointListNode *node = start_node;
                while (node && node->next) {
                    PointListNode *next = node->next;

                    JkVector2 approx_point = vector_average(node->point, next->point);
                    float t = (node->t + next->t) / 2.0f;
                    JkVector2 true_point = evaluate_cubic_bezier(t, p0, p1, p2, p3);

                    if (tolerance * tolerance < vector_distance_squared(approx_point, true_point)) {
                        changed = 1;

                        PointListNode *new_node = arena_alloc(scratch_arena, sizeof(*new_node));
                        new_node->next = next;
                        new_node->point = true_point;
                        new_node->t = t;

                        node->next = new_node;
                    }

                    node = next;
                }
            }

            for (PointListNode *node = start_node; node && node->next; node = node->next) {
                if (node->point.y != node->next->point.y) {
                    Edge *new_edge = arena_alloc(arena, sizeof(*new_edge));
                    *new_edge = points_to_edge(node->point, node->next->point);
                }
            }

            arena_pointer_set(scratch_arena, prev_scratch_arena_pointer);
        } break;
        }
    }

    edges.count = (Edge *)arena_pointer_get(arena) - edges.items;
    return edges;
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

#define POINT_COUNT 129

void render(Bezier *bezier)
{
    int32_t bitmap_width = bezier->draw_square_side_length;
    int32_t bitmap_height = bezier->draw_square_side_length;
    Transform transform = {
        .scale = bitmap_width / 64.0f,
    };

    Arena arena = {.memory = {.size = sizeof(bezier->memory), .data = bezier->memory}};
    Arena scratch_arena = {
        .memory = {.size = sizeof(bezier->scratch_memory), .data = bezier->scratch_memory}};

    EdgeArray edges = shape_edges_get(&arena, &scratch_arena, bezier->shape, transform, 0.25f);

    uint64_t coverage_size = sizeof(float) * (bitmap_width + 1) * 2;
    float *coverage = arena_alloc(&arena, coverage_size);

    float *fill = coverage + bitmap_width + 1;

    for (int32_t y = 0; y < bitmap_height; y++) {
        memset(coverage, 0, coverage_size);

        float scan_y_top = (float)y;
        float scan_y_bottom = scan_y_top + 1.0f;
        for (int32_t i = 0; i < edges.count; i++) {
            float y_top = JK_MAX(edges.items[i].segment.p1.y, scan_y_top);
            float y_bottom = JK_MIN(edges.items[i].segment.p2.y, scan_y_bottom);
            if (y_top < y_bottom) {
                float height = y_bottom - y_top;
                float x_top = segment_y_intersection(edges.items[i].segment, y_top);
                float x_bottom = segment_y_intersection(edges.items[i].segment, y_bottom);

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
                    coverage[first_pixel_index] += edges.items[i].direction * area;

                    // Fill everything to the right with height
                    fill[first_pixel_index + 1] += edges.items[i].direction * height;
                } else {
                    // Edge covers multiple pixels
                    float delta_y = (edges.items[i].segment.p2.y - edges.items[i].segment.p1.y)
                            / (edges.items[i].segment.p2.x - edges.items[i].segment.p1.x);

                    // Handle first pixel
                    float first_x_intersection =
                            segment_x_intersection(edges.items[i].segment, first_pixel_right);
                    float first_pixel_y_offset = first_x_intersection - y_start;
                    float first_pixel_area =
                            (first_pixel_right - x_start) * fabsf(first_pixel_y_offset) / 2.0f;
                    coverage[first_pixel_index] += edges.items[i].direction * first_pixel_area;

                    // Handle middle pixels (if there are any)
                    float y_offset = first_pixel_y_offset;
                    int32_t pixel_index = first_pixel_index + 1;
                    for (; (float)(pixel_index + 1) < x_end; pixel_index++) {
                        coverage[pixel_index] +=
                                edges.items[i].direction * fabsf(y_offset + delta_y / 2.0f);
                        y_offset += delta_y;
                    }

                    // Handle last pixel
                    float last_x_intersection = y_start + y_offset;
                    float uncovered_triangle = fabsf(y_end - last_x_intersection)
                            * (x_end - (float)pixel_index) / 2.0f;
                    coverage[pixel_index] +=
                            edges.items[i].direction * (height - uncovered_triangle);

                    // Fill everything to the right with height
                    fill[pixel_index + 1] += edges.items[i].direction * height;
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
    }
}
