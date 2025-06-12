#include <math.h>
#include <string.h>

// #jk_build dependencies_begin
#include <jk_src/jk_lib/jk_lib.h>
// #jk_build dependencies_end

#include "jk_shapes.h"

// ---- Hash table begin -------------------------------------------------------

static b32 jk_shapes_is_load_factor_exceeded(size_t count, size_t capacity)
{
    return count > (capacity * JK_SHAPES_HASH_TABLE_LOAD_FACTOR / 10);
}

JK_PUBLIC JkShapesHashTable *jk_shapes_hash_table_init(JkShapesHashTable *t, JkBuffer memory)
{
    t->capacity = jk_round_down_to_power_of_2(memory.size / sizeof(JkShapesHashTableSlot));
    JK_DEBUG_ASSERT(jk_is_power_of_two(t->capacity));
    t->slots = (JkShapesHashTableSlot *)memory.data;
    t->count = 0;

    jk_buffer_zero(memory);

    return t;
}

JK_PUBLIC JkShapesHashTableSlot *jk_shapes_hash_table_probe(JkShapesHashTable *t, uint64_t key)
{
    // Hash and mask off bits to get a result in the range 0..capacity-1. Assumes capacity is a
    // power if 2.
    size_t slot_i = jk_hash_uint32((uint32_t)(key >> 32) ^ (uint32_t)key) & (t->capacity - 1);

#ifndef NDEBUG
    size_t iterations = 0;
#endif

    while (t->slots[slot_i].filled && t->slots[slot_i].key != key) {
        JK_DEBUG_ASSERT(iterations < t->capacity && "hash table probe failed to find free slot");
        // Linearly probe. Causes more collisions than other methods but seems to make up for it in
        // cache locality.
        slot_i++;
        if (slot_i >= t->capacity) {
            slot_i -= t->capacity;
        }

#ifndef NDEBUG
        iterations++;
#endif
    }

    return &t->slots[slot_i];
}

JK_PUBLIC void jk_shapes_hash_table_set(
        JkShapesHashTable *t, JkShapesHashTableSlot *slot, uint64_t key, JkShapesBitmap value)
{
    if (!slot->filled) {
        // Write to key, mark as filled, and increase the count
        slot->key = key;
        slot->filled = 1;
        t->count++;
        JK_ASSERT(!jk_shapes_is_load_factor_exceeded(t->count, t->capacity));
    }
    slot->value = value;
}

// ---- Hash table end ---------------------------------------------------------

#define JK_SHAPES_CAPACITY 1024

JK_PUBLIC void jk_shapes_renderer_init(
        JkShapesRenderer *renderer, void *base_pointer, JkShapeArray shapes, JkArena *arena)
{
    renderer->base_pointer = base_pointer;
    renderer->shapes = shapes;
    renderer->arena = arena;

    JkBuffer hash_table_memory;
    hash_table_memory.size = JK_SHAPES_CAPACITY * sizeof(JkShapesHashTableSlot);
    hash_table_memory.data = jk_arena_alloc_zero(arena, hash_table_memory.size);
    jk_shapes_hash_table_init(&renderer->hash_table, hash_table_memory);

    renderer->draw_commands_head = 0;
}

static uint64_t jk_shapes_bitmap_key_get(uint32_t shape_index, float scale)
{
    return ((uint64_t)shape_index << 32) | *(uint32_t *)&scale;
}

static JkShapesEdge jk_shapes_points_to_edge(JkVector2 a, JkVector2 b)
{
    JkShapesEdge edge;
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

static JkVector2 jk_shapes_evaluate_bezier_quadratic(
        float t, JkVector2 p0, JkVector2 p1, JkVector2 p2)
{
    float t_squared = t * t;
    float one_minus_t = 1.0f - t;
    float one_minus_t_squared = one_minus_t * one_minus_t;

    JkVector2 result = jk_vector_2_mul(one_minus_t_squared, p0);
    result = jk_vector_2_add(result, jk_vector_2_mul(2.0f * one_minus_t * t, p1));
    result = jk_vector_2_add(result, jk_vector_2_mul(t_squared, p2));
    return result;
}

static JkVector2 jk_shapes_evaluate_bezier_cubic(
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

typedef struct JkShapesArcByCenter {
    b32 treat_as_line;
    JkVector2 center;
    JkVector2 dimensions;
    JkVector2 point_end;
    float rotation_matrix[2][2];
    float angle_start;
    float angle_delta;
} JkShapesArcByCenter;

// https://www.w3.org/TR/SVG2/implnote.html#ArcImplementationNotes
static JkShapesArcByCenter jk_shapes_arc_endpoint_to_center(
        JkTransform2 transform, JkVector2 point_start, JkShapesArcByEndpoint a)
{
    JkShapesArcByCenter r = {0};

    // Transform arc values
    a.dimensions = jk_transform_2_apply(transform, a.dimensions);
    r.point_end = jk_transform_2_apply(transform, a.point_end);

    if (jk_vector_2_approx_equal(point_start, r.point_end, 0.00001f)) {
        r.treat_as_line = 1;
        return r;
    }

    r.rotation_matrix[0][0] = cosf(a.rotation);
    r.rotation_matrix[0][1] = -sinf(a.rotation);
    r.rotation_matrix[1][0] = sinf(a.rotation);
    r.rotation_matrix[1][1] = cosf(a.rotation);

    float inverse_rotation_matrix[2][2] = {
        {cosf(a.rotation), sinf(a.rotation)},
        {-sinf(a.rotation), cosf(a.rotation)},
    };

    // Transform point_start into ellipse space
    JkVector2 point_prime = jk_matrix_2x2_multiply_vector(inverse_rotation_matrix,
            jk_vector_2_mul(
                    0.5f, jk_vector_2_add(point_start, jk_vector_2_mul(-1.0f, r.point_end))));

    // Correct out-of-range radii
    float lambda = 0.0f;
    for (int32_t i = 0; i < 2; i++) {
        if (!a.dimensions.coords[i]) {
            r.treat_as_line = 1;
            return r;
        }
        r.dimensions.coords[i] = fabsf(a.dimensions.coords[i]);
        lambda += (point_prime.coords[i] * point_prime.coords[i])
                / (r.dimensions.coords[i] * r.dimensions.coords[i]);
    }
    if (1.0f < lambda) {
        r.dimensions = jk_vector_2_mul(sqrtf(lambda), r.dimensions);
    }

    // Compute center in ellipse space
    JkVector2 center_prime;
    {
        float rx_sqr = r.dimensions.x * r.dimensions.x;
        float ry_sqr = r.dimensions.y * r.dimensions.y;
        float x_sqr = point_prime.x * point_prime.x;
        float y_sqr = point_prime.y * point_prime.y;
        float expr = (rx_sqr * ry_sqr - rx_sqr * y_sqr - ry_sqr * x_sqr)
                / (rx_sqr * y_sqr + ry_sqr * x_sqr);
        float scalar = sqrtf(JK_MAX(0.0f, expr));
        JkVector2 vector = {(r.dimensions.x * point_prime.y) / r.dimensions.y,
            -(r.dimensions.y * point_prime.x) / r.dimensions.x};
        b32 flag_large = (a.flags >> JK_SHAPES_ARC_FLAG_INDEX_LARGE) & 1;
        b32 flag_sweep = (a.flags >> JK_SHAPES_ARC_FLAG_INDEX_SWEEP) & 1;
        float sign = flag_large == flag_sweep ? -1.0f : 1.0f;
        center_prime = jk_vector_2_mul(sign * scalar, vector);
    }
    // Transform center point back into screen space
    r.center = jk_vector_2_add(jk_matrix_2x2_multiply_vector(r.rotation_matrix, center_prime),
            jk_vector_2_lerp(point_start, r.point_end, 0.5f));

    // Compute angles
    {
        JkVector2 delta = jk_vector_2_add(point_prime, jk_vector_2_mul(-1.0f, center_prime));
        JkVector2 nsum = jk_vector_2_mul(-1.0f, jk_vector_2_add(point_prime, center_prime));

        JkVector2 v1 = (JkVector2){delta.x / r.dimensions.x, delta.y / r.dimensions.y};
        JkVector2 v2 = (JkVector2){nsum.x / r.dimensions.x, nsum.y / r.dimensions.y};

        r.angle_start = jk_vector_2_angle_between((JkVector2){1.0f, 0.0f}, v1);
        r.angle_delta = jk_vector_2_angle_between(v1, v2);

        if (a.flags & JK_SHAPES_ARC_FLAG_SWEEP) {
            if (r.angle_delta < 0.0f) {
                r.angle_delta += 2.0f * (float)JK_PI;
            }
        } else {
            if (0.0f < r.angle_delta) {
                r.angle_delta -= 2.0f * (float)JK_PI;
            }
        }
    }

    return r;
}

static JkVector2 jk_shapes_evaluate_arc(float t, JkShapesArcByCenter arc)
{
    float angle = arc.angle_start + t * arc.angle_delta;
    return jk_vector_2_add(
            jk_matrix_2x2_multiply_vector(arc.rotation_matrix,
                    (JkVector2){arc.dimensions.x * cosf(angle), arc.dimensions.y * sinf(angle)}),
            arc.center);
}

typedef struct JkShapesLinearizer {
    JkArena *arena;
    float tolerance_squared;
    JkShapesPointListNode **current_node;
    JkShapesPointListNode *start_node;
    b32 has_new_nodes;

    float t;
} JkShapesLinearizer;

static void jk_shapes_linearizer_init(JkShapesLinearizer *l,
        JkArena *arena,
        JkShapesPointListNode **current_node,
        JkVector2 target,
        float tolerance)
{
    l->arena = arena;
    l->current_node = current_node;
    l->tolerance_squared = tolerance * tolerance;

    JkShapesPointListNode *end_node = jk_arena_alloc(l->arena, sizeof(*end_node));
    end_node->next = 0;
    end_node->point = target;
    end_node->t = 1.0f;
    end_node->is_cursor_movement = 0;

    l->start_node = *l->current_node;
    l->start_node->next = end_node;
    l->start_node->t = 0.0f;

    l->has_new_nodes = 0;
}

static b32 jk_shapes_linearizer_running(JkShapesLinearizer *l)
{
    if (!(*l->current_node)->next) {
        if (l->has_new_nodes) {
            *l->current_node = l->start_node;
            l->has_new_nodes = 0;
        } else {
            return 0;
        }
    }

    l->t = ((*l->current_node)->t + (*l->current_node)->next->t) / 2.0f;

    return 1;
}

static void jk_shapes_linearizer_evaluate(JkShapesLinearizer *l, JkVector2 point)
{
    JkShapesPointListNode *next = (*l->current_node)->next;
    JkVector2 approx_point = jk_vector_2_lerp((*l->current_node)->point, next->point, 0.5f);

    if (l->tolerance_squared < jk_vector_2_distance_squared(approx_point, point)) {
        l->has_new_nodes = 1;

        JkShapesPointListNode *new_node = jk_arena_alloc(l->arena, sizeof(*new_node));
        new_node->next = next;
        new_node->point = point;
        new_node->t = l->t;
        new_node->is_cursor_movement = 0;

        (*l->current_node)->next = new_node;
    }

    *l->current_node = next;
}

static JkShapesEdgeArray jk_shapes_edges_get(
        JkArena *arena, JkShapesPenCommandArray commands, JkTransform2 transform, float tolerance)
{
    JkShapesPointListNode *start_node = jk_arena_alloc(arena, sizeof(*start_node));
    start_node->next = 0;
    start_node->point = (JkVector2){0};
    start_node->is_cursor_movement = 0;

    JkShapesPointListNode *current_node = start_node;

    for (int32_t i = 0; i < commands.count; i++) {
        JkShapesPenCommand *command = commands.items + i;

        switch (command->type) {
        case JK_SHAPES_PEN_COMMAND_MOVE: {
            JkShapesPointListNode *previous_node = current_node;
            current_node = jk_arena_alloc(arena, sizeof(*current_node));
            previous_node->next = current_node;
            current_node->next = 0;
            current_node->point = jk_transform_2_apply(transform, command->coords[0]);
            current_node->is_cursor_movement = 1;
        } break;

        case JK_SHAPES_PEN_COMMAND_LINE: {
            JkShapesPointListNode *previous_node = current_node;
            current_node = jk_arena_alloc(arena, sizeof(*current_node));
            previous_node->next = current_node;
            current_node->next = 0;
            current_node->point = jk_transform_2_apply(transform, command->coords[0]);
            current_node->is_cursor_movement = 0;
        } break;

        case JK_SHAPES_PEN_COMMAND_CURVE_QUADRATIC: {
            JkVector2 p0 = current_node->point;
            JkVector2 p1 = jk_transform_2_apply(transform, command->coords[0]);
            JkVector2 p2 = jk_transform_2_apply(transform, command->coords[1]);

            JkShapesLinearizer l;
            jk_shapes_linearizer_init(&l, arena, &current_node, p2, tolerance);

            while (jk_shapes_linearizer_running(&l)) {
                jk_shapes_linearizer_evaluate(
                        &l, jk_shapes_evaluate_bezier_quadratic(l.t, p0, p1, p2));
            }
        } break;

        case JK_SHAPES_PEN_COMMAND_CURVE_CUBIC: {
            JkVector2 p0 = current_node->point;
            JkVector2 p1 = jk_transform_2_apply(transform, command->coords[0]);
            JkVector2 p2 = jk_transform_2_apply(transform, command->coords[1]);
            JkVector2 p3 = jk_transform_2_apply(transform, command->coords[2]);

            JkShapesLinearizer l;
            jk_shapes_linearizer_init(&l, arena, &current_node, p3, tolerance);

            while (jk_shapes_linearizer_running(&l)) {
                jk_shapes_linearizer_evaluate(
                        &l, jk_shapes_evaluate_bezier_cubic(l.t, p0, p1, p2, p3));
            }
        } break;

        case JK_SHAPES_PEN_COMMAND_ARC: {
            JkShapesArcByCenter arc =
                    jk_shapes_arc_endpoint_to_center(transform, current_node->point, command->arc);

            JkShapesLinearizer l;
            jk_shapes_linearizer_init(&l, arena, &current_node, arc.point_end, tolerance);

            while (jk_shapes_linearizer_running(&l)) {
                jk_shapes_linearizer_evaluate(&l, jk_shapes_evaluate_arc(l.t, arc));
            }
        } break;
        }
    }

    // The linearization is finished. Create an array of edges from the point list.
    JkShapesEdgeArray edges = {.items = jk_arena_pointer_get(arena)};
    for (JkShapesPointListNode *node = start_node; node && node->next; node = node->next) {
        if (!node->next->is_cursor_movement && node->point.y != node->next->point.y) {
            JkShapesEdge *new_edge = jk_arena_alloc(arena, sizeof(*new_edge));
            *new_edge = jk_shapes_points_to_edge(node->point, node->next->point);
        }
    }
    edges.count = (JkShapesEdge *)jk_arena_pointer_get(arena) - edges.items;

    return edges;
}

// No bounds checking so they return the intersection as if the segment was an infinite line

static float jk_shapes_segment_y_intersection(JkShapesSegment segment, float y)
{
    float delta_y = segment.p2.y - segment.p1.y;
    JK_ASSERT(delta_y != 0);
    return ((segment.p2.x - segment.p1.x) / delta_y) * (y - segment.p1.y) + segment.p1.x;
}

static float jk_shapes_segment_x_intersection(JkShapesSegment segment, float x)
{
    float delta_x = segment.p2.x - segment.p1.x;
    JK_ASSERT(delta_x != 0);
    return ((segment.p2.y - segment.p1.y) / delta_x) * (x - segment.p1.x) + segment.p1.y;
}

JK_PUBLIC JkShapesBitmap *jk_shapes_bitmap_get(
        JkShapesRenderer *renderer, uint32_t shape_index, float scale)
{
    JkShapesBitmap *result = 0;

    JkShape shape = renderer->shapes.items[shape_index];
    if (shape.dimensions.x && shape.dimensions.y) {
        uint64_t bitmap_key = jk_shapes_bitmap_key_get(shape_index, scale);
        JkShapesHashTableSlot *bitmap_slot =
                jk_shapes_hash_table_probe(&renderer->hash_table, bitmap_key);
        result = &bitmap_slot->value;
        if (!bitmap_slot->filled) {
            JkShapesBitmap bitmap;
            bitmap.dimensions.x = (int32_t)ceilf(scale * shape.dimensions.x);
            bitmap.dimensions.y = (int32_t)ceilf(scale * shape.dimensions.y);
            // TODO: do we really need to zero it?
            bitmap.data = jk_arena_alloc_zero(renderer->arena,
                    bitmap.dimensions.x * bitmap.dimensions.y * sizeof(bitmap.data[0]));
            jk_shapes_hash_table_set(&renderer->hash_table, bitmap_slot, bitmap_key, bitmap);

            JkTransform2 transform;
            transform.scale = scale;
            transform.position = jk_vector_2_mul(-1.0f, jk_vector_2_mul(scale, shape.offset));

            void *arena_saved_pointer = jk_arena_pointer_get(renderer->arena);

            uint64_t coverage_size = sizeof(float) * (bitmap.dimensions.x + 1);
            float *coverage = jk_arena_alloc(renderer->arena, coverage_size);
            float *fill = jk_arena_alloc(renderer->arena, coverage_size);

            JkShapesPenCommandArray commands;
            commands.count = shape.commands.size / sizeof(commands.items[0]);
            commands.items = (JkShapesPenCommand *)(renderer->base_pointer + shape.commands.offset);
            JkShapesEdgeArray edges =
                    jk_shapes_edges_get(renderer->arena, commands, transform, 0.25f);

            for (int32_t y = 0; y < bitmap.dimensions.y; y++) {
                memset(coverage, 0, coverage_size * 2);

                float scan_y_top = (float)y;
                float scan_y_bottom = scan_y_top + 1.0f;
                for (int32_t i = 0; i < edges.count; i++) {
                    float y_top = JK_MAX(edges.items[i].segment.p1.y, scan_y_top);
                    float y_bottom = JK_MIN(edges.items[i].segment.p2.y, scan_y_bottom);
                    if (y_top < y_bottom) {
                        float height = y_bottom - y_top;
                        float x_top =
                                jk_shapes_segment_y_intersection(edges.items[i].segment, y_top);
                        float x_bottom =
                                jk_shapes_segment_y_intersection(edges.items[i].segment, y_bottom);

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
                            float first_x_intersection = jk_shapes_segment_x_intersection(
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
                for (int32_t x = 0; x < bitmap.dimensions.x; x++) {
                    acc += fill[x];
                    int32_t value = (int32_t)(fabsf((coverage[x] + acc) * 255.0f));
                    if (255 < value) {
                        value = 255;
                    }
                    bitmap.data[y * bitmap.dimensions.x + x] = (uint8_t)value;
                }
            }

            jk_arena_pointer_set(renderer->arena, arena_saved_pointer);
        }
    }

    return result;
}

// Returns the shape's scaled advance_width
JK_PUBLIC float jk_shapes_draw(JkShapesRenderer *renderer,
        uint32_t shape_index,
        JkVector2 position,
        float scale,
        JkColor color)
{
    JkShape shape = renderer->shapes.items[shape_index];

    if (shape.dimensions.x && shape.dimensions.y) {
        JkShapesDrawCommandListNode *node = jk_arena_alloc(renderer->arena, sizeof(*node));
        node->command.position =
                jk_vector_2_round(jk_vector_2_add(position, jk_vector_2_mul(scale, shape.offset)));
        node->command.color = color;
        node->command.bitmap = jk_shapes_bitmap_get(renderer, shape_index, scale);
        node->next = renderer->draw_commands_head;
        renderer->draw_commands_head = node;
    }

    return scale * shape.advance_width;
}

static int jk_shapes_draw_command_compare(void *a, void *b)
{
    JkShapesDrawCommand *x = a;
    JkShapesDrawCommand *y = b;
    return x->position.y - y->position.y;
}

static void jk_shapes_draw_commands_quicksort(JkShapesDrawCommandArray commands)
{
    JkShapesDrawCommand tmp;
    jk_quicksort(commands.items,
            commands.count,
            sizeof(commands.items[0]),
            &tmp,
            jk_shapes_draw_command_compare);
}

JK_PUBLIC JkShapesDrawCommandArray jk_shapes_draw_commands_get(JkShapesRenderer *renderer)
{
    JkShapesDrawCommandArray result;
    result.items = jk_arena_pointer_get(renderer->arena);
    for (JkShapesDrawCommandListNode *node = renderer->draw_commands_head; node;
            node = node->next) {
        JkShapesDrawCommand *new_command = jk_arena_alloc(renderer->arena, sizeof(*new_command));
        *new_command = node->command;
    }
    result.count = (JkShapesDrawCommand *)jk_arena_pointer_get(renderer->arena) - result.items;

    jk_shapes_draw_commands_quicksort(result);

    return result;
}
