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
#include <jk_src/stb/stb_truetype.h>
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

static void *arena_alloc(Arena *arena, uint64_t byte_count)
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

static void *arena_pointer_get(Arena *arena)
{
    return arena->memory.data + arena->pos;
}

static void arena_pointer_set(Arena *arena, void *pointer)
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
    JkVector2 scale;
} Transform;

static JkVector2 transform_apply(Transform transform, JkVector2 point)
{
    return jk_vector_2_add(transform.position,
            (JkVector2){transform.scale.x * point.x, transform.scale.y * point.y});
}

static Edge points_to_edge(JkVector2 a, JkVector2 b)
{
    Edge edge;
    if (a.y < b.y) {
        edge.segment.p1 = a;
        edge.segment.p2 = b;
        edge.direction = -1.0f;
    } else {
        edge.segment.p1 = b;
        edge.segment.p2 = a;
        edge.direction = 1.0f;
    }
    return edge;
}

static JkVector2 evaluate_bezier_quadratic(float t, JkVector2 p0, JkVector2 p1, JkVector2 p2)
{
    float t_squared = t * t;
    float one_minus_t = 1.0f - t;
    float one_minus_t_squared = one_minus_t * one_minus_t;

    JkVector2 result = jk_vector_2_mul(one_minus_t_squared, p0);
    result = jk_vector_2_add(result, jk_vector_2_mul(2.0f * one_minus_t * t, p1));
    result = jk_vector_2_add(result, jk_vector_2_mul(t_squared, p2));
    return result;
}

static JkVector2 evaluate_bezier_cubic(
        float t, JkVector2 p0, JkVector2 p1, JkVector2 p2, JkVector2 p3)
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

typedef struct Linearizer {
    Arena *edge_arena;
    Arena *scratch_arena;

    void *scratch_arena_saved_pointer;

    float tolerance_squared;
    PointListNode *start_node;
    PointListNode *current_node;
    b32 has_new_nodes;

    float t;
} Linearizer;

static void linearizer_init(Linearizer *l,
        Arena *edge_arena,
        Arena *scratch_arena,
        float tolerance,
        JkVector2 start,
        JkVector2 end)
{
    l->edge_arena = edge_arena;
    l->scratch_arena = scratch_arena;

    l->scratch_arena_saved_pointer = arena_pointer_get(l->scratch_arena);

    l->tolerance_squared = tolerance * tolerance;
    PointListNode *end_node = arena_alloc(l->scratch_arena, sizeof(*end_node));
    end_node->next = 0;
    end_node->point = end;
    end_node->t = 1.0f;

    l->start_node = arena_alloc(l->scratch_arena, sizeof(*l->start_node));
    l->start_node->next = end_node;
    l->start_node->point = start;
    l->start_node->t = 0.0f;

    l->current_node = l->start_node;
    l->has_new_nodes = 0;
}

static b32 linearizer_running(Linearizer *l)
{
    if (!l->current_node->next) {
        l->current_node = l->start_node;
        if (!l->has_new_nodes) {
            // The Linearizer is finished. Add edges to edge_arena.
            for (PointListNode *node = l->start_node; node && node->next; node = node->next) {
                if (node->point.y != node->next->point.y) {
                    Edge *new_edge = arena_alloc(l->edge_arena, sizeof(*new_edge));
                    *new_edge = points_to_edge(node->point, node->next->point);
                }
            }

            arena_pointer_set(l->scratch_arena, l->scratch_arena_saved_pointer);

            return 0;
        }
        l->has_new_nodes = 0;
    }

    l->t = (l->current_node->t + l->current_node->next->t) / 2.0f;

    return 1;
}

static void linearizer_evaluate(Linearizer *l, JkVector2 point)
{
    PointListNode *next = l->current_node->next;
    JkVector2 approx_point = vector_average(l->current_node->point, next->point);

    if (l->tolerance_squared < vector_distance_squared(approx_point, point)) {
        l->has_new_nodes = 1;

        PointListNode *new_node = arena_alloc(l->scratch_arena, sizeof(*new_node));
        new_node->next = next;
        new_node->point = point;
        new_node->t = l->t;

        l->current_node->next = new_node;
    }

    l->current_node = next;
}

static EdgeArray shape_edges_get(Arena *edge_arena,
        Arena *scratch_arena,
        PenCommandArray commands,
        Transform transform,
        float tolerance)
{
    EdgeArray edges = {.items = arena_pointer_get(edge_arena)};

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
                Edge *new_edge = arena_alloc(edge_arena, sizeof(*new_edge));
                *new_edge = points_to_edge(prev_cursor, cursor);
            }
        } break;

        case PEN_COMMAND_CURVE_QUADRATIC: {
            JkVector2 p0 = cursor;
            JkVector2 p1 = transform_apply(transform, commands.items[i].coords[0]);
            JkVector2 p2 = transform_apply(transform, commands.items[i].coords[1]);

            cursor = p2;

            Linearizer l;
            linearizer_init(&l, edge_arena, scratch_arena, tolerance, p0, p2);

            while (linearizer_running(&l)) {
                linearizer_evaluate(&l, evaluate_bezier_quadratic(l.t, p0, p1, p2));
            }
        } break;

        case PEN_COMMAND_CURVE_CUBIC: {
            JkVector2 p0 = cursor;
            JkVector2 p1 = transform_apply(transform, commands.items[i].coords[0]);
            JkVector2 p2 = transform_apply(transform, commands.items[i].coords[1]);
            JkVector2 p3 = transform_apply(transform, commands.items[i].coords[2]);

            cursor = p3;

            Linearizer l;
            linearizer_init(&l, edge_arena, scratch_arena, tolerance, p0, p3);

            while (linearizer_running(&l)) {
                linearizer_evaluate(&l, evaluate_bezier_cubic(l.t, p0, p1, p2, p3));
            }
        } break;
        }
    }

    edges.count = (Edge *)arena_pointer_get(edge_arena) - edges.items;
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

typedef struct CharacterDrawCommand {
    uint8_t index;
    JkIntVector2 pos;
} CharacterDrawCommand;

typedef struct CharacterDrawCommandArray {
    uint64_t count;
    CharacterDrawCommand *items;
} CharacterDrawCommandArray;

void render(Bezier *bezier)
{
    if (!bezier->initialized) {
        bezier->initialized = 1;
        stbtt_InitFont(&bezier->font,
                bezier->ttf_file.data,
                stbtt_GetFontOffsetForIndex(bezier->ttf_file.data, 0));

        stbtt_GetCodepointHMetrics(
                &bezier->font, ' ', &bezier->printable_characters[0].advance_width, 0);

        bezier->font_y0_min = INT32_MAX;
        bezier->font_y1_max = INT32_MIN;
        for (int32_t codepoint_offset = 1;
                codepoint_offset < JK_ARRAY_COUNT(bezier->printable_characters);
                codepoint_offset++) {
            int32_t codepoint = codepoint_offset + 32;
            Character *character = bezier->printable_characters + codepoint_offset;

            stbtt_GetCodepointBox(&bezier->font,
                    codepoint,
                    &character->x0,
                    &character->y0,
                    &character->x1,
                    &character->y1);
            stbtt_GetCodepointHMetrics(&bezier->font,
                    codepoint,
                    &character->advance_width,
                    &character->left_side_bearing);

            if (character->y0 < bezier->font_y0_min) {
                bezier->font_y0_min = character->y0;
            }
            if (bezier->font_y1_max < character->y1) {
                bezier->font_y1_max = character->y1;
            }

            stbtt_GetCodepointBox(&bezier->font,
                    codepoint,
                    &character->x0,
                    &character->y0,
                    &character->x1,
                    &character->y1);

            stbtt_vertex *verticies;
            character->shape.count = stbtt_GetCodepointShape(&bezier->font, codepoint, &verticies);
            character->shape.items = arena_alloc(
                    &bezier->arena, character->shape.count * sizeof(character->shape.items[0]));
            for (int32_t i = 0; i < character->shape.count; i++) {
                switch (verticies[i].type) {
                case STBTT_vmove:
                case STBTT_vline: {
                    character->shape.items[i].type =
                            verticies[i].type == STBTT_vmove ? PEN_COMMAND_MOVE : PEN_COMMAND_LINE;
                    character->shape.items[i].coords[0].x = verticies[i].x;
                    character->shape.items[i].coords[0].y = verticies[i].y;
                } break;

                case STBTT_vcurve: {
                    character->shape.items[i].type = PEN_COMMAND_CURVE_QUADRATIC;
                    character->shape.items[i].coords[0].x = verticies[i].cx;
                    character->shape.items[i].coords[0].y = verticies[i].cy;
                    character->shape.items[i].coords[1].x = verticies[i].x;
                    character->shape.items[i].coords[1].y = verticies[i].y;
                    character->shape.items[i].coords[2].x = verticies[i].x;
                    character->shape.items[i].coords[2].y = verticies[i].y;
                } break;

                case STBTT_vcubic: {
                    character->shape.items[i].type = PEN_COMMAND_CURVE_CUBIC;
                    character->shape.items[i].coords[0].x = verticies[i].cx;
                    character->shape.items[i].coords[0].y = verticies[i].cy;
                    character->shape.items[i].coords[1].x = verticies[i].cx1;
                    character->shape.items[i].coords[1].y = verticies[i].cy1;
                    character->shape.items[i].coords[2].x = verticies[i].x;
                    character->shape.items[i].coords[2].y = verticies[i].y;
                } break;

                default: {
                    JK_ASSERT(0 && "Unsupported vertex type");
                } break;
                }
            }
        }

        bezier->arena_saved_pointer = arena_pointer_get(&bezier->arena);
    }

    Arena arena = {.memory = {.size = sizeof(bezier->memory), .data = bezier->memory}};
    Arena scratch_arena = {
        .memory = {.size = sizeof(bezier->scratch_memory), .data = bezier->scratch_memory}};

    uint64_t coverage_size = sizeof(float) * (bezier->draw_square_side_length + 1) * 2;
    float *coverage = arena_alloc(&arena, coverage_size);
    float *fill = coverage + bezier->draw_square_side_length + 1;

    JkBuffer display_string = JKS("Hello, world!");
    int32_t first_point = -bezier->printable_characters[display_string.data[0] - 32].x0;
    int32_t last_point = first_point;
    for (int32_t i = 0; i < display_string.size - 1; i++) {
        last_point += bezier->printable_characters[display_string.data[i] - 32].advance_width;
    }
    int32_t display_string_width = last_point
            + bezier->printable_characters[display_string.data[display_string.size - 1] - 32].x1;
    int32_t display_string_height = bezier->font_y1_max - bezier->font_y0_min;
    int32_t max_dimension = JK_MAX(display_string_width, display_string_height);

    float scale = (float)bezier->draw_square_side_length / (float)max_dimension;

    if (!(fabsf(scale - bezier->prev_scale) < 0.0001f)) {
        for (uint32_t i = 0; i < JK_ARRAY_COUNT(bezier->printable_characters); i++) {
            bezier->printable_characters[i].bitmap.up_to_date = 0;
        }
        arena_pointer_set(&bezier->arena, bezier->arena_saved_pointer);
    }

    for (int32_t character_index = 0; character_index < display_string.size; character_index++) {
        Character *character =
                bezier->printable_characters + (display_string.data[character_index] - 32);

        if (!character->bitmap.up_to_date) {
            character->bitmap.up_to_date = 1;
            character->bitmap.advance_width = (int32_t)(scale * (float)character->advance_width);
            character->bitmap.offset.x = (int32_t)(scale * (float)character->x0);
            character->bitmap.offset.y = (int32_t)(-scale * (float)character->y1);
            character->bitmap.dimensions.x =
                    (uint32_t)(scale * (float)(character->x1 - character->x0)) + 1;
            character->bitmap.dimensions.y =
                    (uint32_t)(scale * (float)(character->y1 - character->y0)) + 1;
            character->bitmap.data = arena_alloc(&bezier->arena,
                    character->bitmap.dimensions.x * character->bitmap.dimensions.y
                            * sizeof(character->bitmap.data[0]));

            Transform transform;
            transform.scale.x = scale;
            transform.scale.y = -scale;
            transform.position = (JkVector2){scale * -character->x0, scale * character->y1};

            void *arena_saved_pointer = arena_pointer_get(&arena);

            EdgeArray edges =
                    shape_edges_get(&arena, &scratch_arena, character->shape, transform, 0.25f);

            for (int32_t y = 0; y < character->bitmap.dimensions.y; y++) {
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
                            float delta_y =
                                    (edges.items[i].segment.p2.y - edges.items[i].segment.p1.y)
                                    / (edges.items[i].segment.p2.x - edges.items[i].segment.p1.x);

                            // Handle first pixel
                            float first_x_intersection = segment_x_intersection(
                                    edges.items[i].segment, first_pixel_right);
                            float first_pixel_y_offset = first_x_intersection - y_start;
                            float first_pixel_area = (first_pixel_right - x_start)
                                    * fabsf(first_pixel_y_offset) / 2.0f;
                            coverage[first_pixel_index] +=
                                    edges.items[i].direction * first_pixel_area;

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
                for (int32_t x = 0; x < character->bitmap.dimensions.x; x++) {
                    acc += fill[x];
                    int32_t value = (int32_t)(fabsf((coverage[x] + acc) * 255.0f));
                    if (255 < value) {
                        value = 255;
                    }
                    character->bitmap.data[y * character->bitmap.dimensions.x + x] = (uint8_t)value;
                }
            }

            arena_pointer_set(&arena, arena_saved_pointer);
        }
    }

    {
        JkIntVector2 current_point = {
            (int32_t)(scale * (float)first_point),
            (int32_t)(scale * (float)bezier->font_y1_max),
        };
        CharacterDrawCommandArray characters = {.count = display_string.size};
        characters.items = arena_alloc(&arena, characters.count * sizeof(characters.items[0]));
        for (int32_t i = 0; i < display_string.size; i++) {
            characters.items[i].index = display_string.data[i] - 32;
            characters.items[i].pos = jk_int_vector_2_add(current_point,
                    bezier->printable_characters[characters.items[i].index].bitmap.offset);
            current_point.x +=
                    bezier->printable_characters[characters.items[i].index].bitmap.advance_width;
        }

        JkIntVector2 pos;
        for (pos.y = 0; pos.y < bezier->draw_square_side_length; pos.y++) {
            for (pos.x = 0; pos.x < bezier->draw_square_side_length; pos.x++) {
                Color color = color_dark_squares;
                for (int32_t i = 0; i < characters.count; i++) {
                    CharacterBitmap *bitmap =
                            &bezier->printable_characters[characters.items[i].index].bitmap;
                    JkIntVector2 bitmap_pos = jk_int_vector_2_sub(pos, characters.items[i].pos);
                    if (0 <= bitmap_pos.x && bitmap_pos.x < bitmap->dimensions.x
                            && 0 <= bitmap_pos.y && bitmap_pos.y < bitmap->dimensions.y) {
                        int32_t index = bitmap_pos.y * bitmap->dimensions.x + bitmap_pos.x;
                        color = blend_alpha(
                                color_light_squares, color_dark_squares, bitmap->data[index]);
                    } else {
                    }
                }
                bezier->draw_buffer[pos.y * DRAW_BUFFER_SIDE_LENGTH + pos.x] = color;
            }
        }
    }

    bezier->prev_scale = scale;
}
