#include "graphics.h"

// #jk_build library
// #jk_build export render
// #jk_build single_translation_unit

// #jk_build dependencies_begin
#include <jk_src/jk_lib/jk_lib.h>
// #jk_build dependencies_end

static JkColor fgs[] = {
    {.r = 0x2b, .g = 0x41, .b = 0x50, .a = 0xff},
    {.r = 0x9d, .g = 0x6f, .b = 0xfb, .a = 0xff},
};
static JkColor bg = {.r = CLEAR_COLOR_R, .g = CLEAR_COLOR_G, .b = CLEAR_COLOR_B, .a = 0xff};

static JkVec3 camera_pos = {0.0f, -3.0, 0.2};
static JkVec3 light_dir = {-1, 4, -1};
static int32_t rotation_seconds = 8;

static void draw_pixel(State *state, JkIntVec2 pos, JkColor color)
{
    if (0 <= pos.x && pos.x < state->dimensions.x && 0 <= pos.y && pos.y < state->dimensions.y) {
        state->draw_buffer[DRAW_BUFFER_SIDE_LENGTH * pos.y + pos.x] = color;
    }
}

static void draw_rect(State *state, JkIntVec2 pos, JkIntVec2 dimensions, JkColor color)
{
    JkIntVec2 top_left;
    for (int64_t i = 0; i < JK_ARRAY_COUNT(top_left.coords); i++) {
        top_left.coords[i] = JK_MAX(pos.coords[i], 0);
    }
    JkIntVec2 bottom_right;
    for (int64_t i = 0; i < JK_ARRAY_COUNT(bottom_right.coords); i++) {
        bottom_right.coords[i] =
                JK_MIN(top_left.coords[i] + dimensions.coords[i], state->dimensions.coords[i]);
    }

    for (int32_t y = top_left.y; y < bottom_right.y; y++) {
        for (int32_t x = top_left.x; x < bottom_right.x; x++) {
            state->draw_buffer[DRAW_BUFFER_SIDE_LENGTH * y + x] = color;
        }
    }
}

static uint8_t color_multiply(uint8_t a, uint8_t b)
{
    return ((uint32_t)a * (uint32_t)b) / 255;
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

// ---- Xiaolin Wu's line algorithm begin --------------------------------------------

static uint8_t region_code(JkIntVec2 dimensions, JkVec2 v)
{
    return ((v.x < 0.0f) << 0) | ((dimensions.x - 1.0f < v.x) << 1) | ((v.y < 0.0f) << 2)
            | ((dimensions.y - 1.0f < v.y) << 3);
}

typedef struct Endpoint {
    uint8_t code;
    JkVec2 *point;
} Endpoint;

static b32 clip_to_draw_region(JkIntVec2 dimensions, JkVec2 *a, JkVec2 *b)
{
    Endpoint endpoint_a = {.code = region_code(dimensions, *a), .point = a};
    Endpoint endpoint_b = {.code = region_code(dimensions, *b), .point = b};

    for (;;) {
        if (!(endpoint_a.code | endpoint_b.code)) {
            return 1;
        } else if (endpoint_a.code & endpoint_b.code) {
            return 0;
        } else {
            JkVec2 u = *a;
            JkVec2 v = *b;
            Endpoint *endpoint = endpoint_a.code < endpoint_b.code ? &endpoint_b : &endpoint_a;
            if ((endpoint->code >> 0) & 1) {
                endpoint->point->x = 0.0f;
                endpoint->point->y = u.y + (v.y - u.y) * (0.0f - u.x) / (v.x - u.x);
            } else if ((endpoint->code >> 1) & 1) {
                endpoint->point->x = dimensions.x - 1.0f;
                endpoint->point->y = u.y + (v.y - u.y) * (dimensions.x - 1.0f - u.x) / (v.x - u.x);
            } else if ((endpoint->code >> 2) & 1) {
                endpoint->point->x = u.x + (v.x - u.x) * (0.0f - u.y) / (v.y - u.y);
                endpoint->point->y = 0.0f;
            } else if ((endpoint->code >> 3) & 1) {
                endpoint->point->x = u.x + (v.x - u.x) * (dimensions.y - 1.0f - u.y) / (v.y - u.y);
                endpoint->point->y = dimensions.y - 1.0f;
            }
            endpoint->code = region_code(dimensions, *endpoint->point);
        }
    }
}

static float fpart(float x)
{
    return x - jk_floor_f32(x);
}

static float fpart_complement(float x)
{
    return 1.0f - fpart(x);
}

static void plot(JkColor *draw_buffer, JkColor color, int32_t x, int32_t y, float brightness)
{
    int32_t brightness_i = (int32_t)(brightness * 255.0f);
    if (brightness_i > 255) {
        brightness_i = 255;
    }
    draw_buffer[y * DRAW_BUFFER_SIDE_LENGTH + x] = blend_alpha(color,
            draw_buffer[y * DRAW_BUFFER_SIDE_LENGTH + x],
            color_multiply(color.a, (uint8_t)brightness_i));
}

static void draw_line(State *state, JkColor color, JkVec2 a, JkVec2 b)
{
    if (!clip_to_draw_region(state->dimensions, &a, &b)) {
        return;
    }

    JkColor *draw_buffer = state->draw_buffer;

    b32 steep = JK_ABS(b.y - a.y) > JK_ABS(b.x - a.x);

    if (steep) {
        JK_SWAP(a.x, a.y, float);
        JK_SWAP(b.x, b.y, float);
    }
    if (a.x > b.x) {
        JK_SWAP(a, b, JkVec2);
    }

    JkVec2 delta = jk_vec2_sub(b, a);

    float gradient;
    if (delta.x) {
        gradient = delta.y / delta.x;
    } else {
        gradient = 1.0f;
    }

    // handle first endpoint
    int32_t x_pixel_1;
    float intery;
    {
        x_pixel_1 = jk_round(a.x);
        float yend = a.y + gradient * (x_pixel_1 - a.x);
        float xcoverage = fpart_complement(a.x + 0.5f);
        int32_t y_pixel_1 = (int32_t)yend;
        if (steep) {
            plot(draw_buffer, color, y_pixel_1, x_pixel_1, fpart_complement(yend) * xcoverage);
            plot(draw_buffer, color, y_pixel_1 + 1, x_pixel_1, fpart(yend) * xcoverage);
        } else {
            plot(draw_buffer, color, x_pixel_1, y_pixel_1, fpart_complement(yend) * xcoverage);
            plot(draw_buffer, color, x_pixel_1, y_pixel_1 + 1, fpart(yend) * xcoverage);
        }
        intery = yend + gradient;
    }

    // handle second endpoint
    int32_t x_pixel_2;
    {
        x_pixel_2 = jk_round(b.x);
        float yend = b.y + gradient * (x_pixel_2 - b.x);
        float xcoverage = fpart(b.x + 0.5f);
        int32_t y_pixel_2 = (int32_t)yend;
        if (steep) {
            plot(draw_buffer, color, y_pixel_2, x_pixel_2, fpart_complement(yend) * xcoverage);
            plot(draw_buffer, color, y_pixel_2 + 1, x_pixel_2, fpart(yend) * xcoverage);
        } else {
            plot(draw_buffer, color, x_pixel_2, y_pixel_2, fpart_complement(yend) * xcoverage);
            plot(draw_buffer, color, x_pixel_2, y_pixel_2 + 1, fpart(yend) * xcoverage);
        }
    }

    if (steep) {
        for (int32_t x = x_pixel_1 + 1; x < x_pixel_2; x++) {
            plot(draw_buffer, color, (int32_t)intery, x, fpart_complement(intery));
            plot(draw_buffer, color, (int32_t)intery + 1, x, fpart(intery));
            intery += gradient;
        }
    } else {
        for (int32_t x = x_pixel_1 + 1; x < x_pixel_2; x++) {
            plot(draw_buffer, color, x, (int32_t)intery, fpart_complement(intery));
            plot(draw_buffer, color, x, (int32_t)intery + 1, fpart(intery));
            intery += gradient;
        }
    }
}

// ---- Xiaolin Wu's line algorithm end ----------------------------------------------

typedef struct Triangle {
    JkVec3 v[3];
    JkVec2 t[3];
    Bitmap texture;
} Triangle;

static JkIntRect triangle_bounding_box(Triangle t)
{
    return (JkIntRect){
        .min.x = JK_MIN3(t.v[0].x, t.v[1].x, t.v[2].x),
        .min.y = JK_MIN3(t.v[0].y, t.v[1].y, t.v[2].y),
        .max.x = jk_ceil_f32(JK_MAX3(t.v[0].x, t.v[1].x, t.v[2].x)),
        .max.y = jk_ceil_f32(JK_MAX3(t.v[0].y, t.v[1].y, t.v[2].y)),
    };
}

static void add_cover(float *coverage, JkIntRect bounds, int32_t x, float value)
{
    if (bounds.min.x <= x && x < bounds.max.x) {
        coverage[x - bounds.min.x] += value;
    }
}

static void add_fill(float *fill, JkIntRect bounds, int32_t x, float value)
{
    if (x < bounds.max.x) {
        fill[JK_MAX(0, x - bounds.min.x)] += value;
    }
}

static JkColor texture_lookup(Bitmap texture, JkVec2 texcoord)
{
    int32_t x = JK_CLAMP(texcoord.x * texture.dimensions.x, 0, texture.dimensions.x - 1);
    int32_t y = JK_CLAMP(texcoord.y * texture.dimensions.y, 0, texture.dimensions.y - 1);
    return jk_color3_to_4(texture.memory[texture.dimensions.x * y + x], 0xff);
}

static void triangle_fill(JkArena *arena, State *state, Triangle tri)
{
    JkArena tmp_arena = jk_arena_child_get(arena);

    JkIntRect screen_rect = {.min = (JkIntVec2){0}, .max = state->dimensions};
    JkIntRect bounds = jk_int_rect_intersect(screen_rect, triangle_bounding_box(tri));
    JkIntVec2 dimensions = jk_int_rect_dimensions(bounds);

    JkVec2 verts_2d[3];
    for (int64_t i = 0; i < 3; i++) {
        verts_2d[i] = jk_vec3_to_2(tri.v[i]);
    }

    if (dimensions.x < 1) {
        return;
    }

    JkEdgeArray edges = {.items = jk_arena_pointer_current(&tmp_arena)};
    for (int64_t i = 0; i < 3; i++) {
        int64_t next = (i + 1) % 3;
        if (verts_2d[i].y != verts_2d[next].y) {
            JkEdge *edge = jk_arena_push(&tmp_arena, JK_SIZEOF(*edge));
            *edge = jk_points_to_edge(verts_2d[i], verts_2d[next]);
        }
    }
    edges.count = (JkEdge *)jk_arena_pointer_current(&tmp_arena) - edges.items;

    int64_t coverage_size = JK_SIZEOF(float) * (dimensions.x + 1);
    JkBuffer coverage_buf = jk_arena_push_buffer(&tmp_arena, 2 * coverage_size);
    float *coverage = (float *)coverage_buf.data;
    float *fill = (float *)(coverage_buf.data + coverage_size);
    for (int32_t y = bounds.min.y; y < bounds.max.y; y++) {
        jk_buffer_zero(coverage_buf);

        float scan_y_top = (float)y;
        float scan_y_bottom = scan_y_top + 1.0f;
        for (int64_t i = 0; i < edges.count; i++) {
            float y_top = JK_MAX(edges.items[i].segment.p1.y, scan_y_top);
            float y_bottom = JK_MIN(edges.items[i].segment.p2.y, scan_y_bottom);
            if (y_top < y_bottom) {
                float height = y_bottom - y_top;
                float x_top = jk_segment_y_intersection(edges.items[i].segment, y_top);
                float x_bottom = jk_segment_y_intersection(edges.items[i].segment, y_bottom);

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
                    add_cover(coverage, bounds, first_pixel_index, edges.items[i].direction * area);

                    // Fill everything to the right with height
                    add_fill(
                            fill, bounds, first_pixel_index + 1, edges.items[i].direction * height);
                } else {
                    // Edge covers multiple pixels
                    float delta_y = (edges.items[i].segment.p2.y - edges.items[i].segment.p1.y)
                            / (edges.items[i].segment.p2.x - edges.items[i].segment.p1.x);

                    // Handle first pixel
                    float first_x_intersection =
                            jk_segment_x_intersection(edges.items[i].segment, first_pixel_right);
                    float first_pixel_y_offset = first_x_intersection - y_start;
                    float first_pixel_area =
                            (first_pixel_right - x_start) * JK_ABS(first_pixel_y_offset) / 2.0f;
                    add_cover(coverage,
                            bounds,
                            first_pixel_index,
                            edges.items[i].direction * first_pixel_area);

                    // Handle middle pixels (if there are any)
                    float y_offset = first_pixel_y_offset;
                    int32_t pixel_index = first_pixel_index + 1;
                    for (; (float)(pixel_index + 1) < x_end; pixel_index++) {
                        add_cover(coverage,
                                bounds,
                                pixel_index,
                                edges.items[i].direction * JK_ABS(y_offset + delta_y / 2.0f));
                        y_offset += delta_y;
                    }

                    // Handle last pixel
                    float last_x_intersection = y_start + y_offset;
                    float uncovered_triangle = JK_ABS(y_end - last_x_intersection)
                            * (x_end - (float)pixel_index) / 2.0f;
                    add_cover(coverage,
                            bounds,
                            pixel_index,
                            edges.items[i].direction * (height - uncovered_triangle));

                    // Fill everything to the right with height
                    add_fill(fill, bounds, pixel_index + 1, edges.items[i].direction * height);
                }
            }
        }

        // Fill the scanline according to coverage
        float acc = 0.0f;
        for (int32_t x = bounds.min.x; x < bounds.max.x; x++) {
            acc += fill[x - bounds.min.x];

            JkVec2 point = {x + 0.5, y + 0.5};
            float total_area = JK_ABS(jk_vec2_cross(
                    jk_vec2_sub(verts_2d[1], verts_2d[0]), jk_vec2_sub(verts_2d[2], verts_2d[0])));
            float weight[3];
            for (int64_t i = 0; i < 3; i++) {
                int64_t a = (i + 1) % 3;
                int64_t b = (i + 2) % 3;
                float area = JK_ABS(jk_vec2_cross(
                        jk_vec2_sub(verts_2d[a], point), jk_vec2_sub(verts_2d[b], point)));
                weight[i] = area / total_area;
            }

            JkVec2 texcoord = {0};
            float z = 0;
            for (int64_t i = 0; i < 3; i++) {
                texcoord = jk_vec2_add(texcoord, jk_vec2_mul(weight[i], tri.t[i]));
                z += weight[i] * tri.v[i].z;
            }
            texcoord.x /= z;
            texcoord.y /= z;

            int32_t alpha = (int32_t)(JK_ABS((coverage[x - bounds.min.x] + acc) * 255.0f));
            if (255 < alpha) {
                alpha = 255;
            }
            JkColor pixel_color = texture_lookup(tri.texture, texcoord);
            pixel_color.a = (uint8_t)alpha;

            int64_t i = y * DRAW_BUFFER_SIDE_LENGTH + x;
            state->draw_buffer[i] = jk_color_disjoint_over(state->draw_buffer[i], pixel_color);
        }
    }
}

typedef struct FaceIdsSortContext {
    JkVec3 *vertices;
    Face *faces;
} FaceIdsSortContext;

static int face_ids_compare(void *data, void *a_id_ptr, void *b_id_ptr)
{
    FaceIdsSortContext *c = data;
    JkVec3 *verts = c->vertices;
    Face *a = c->faces + *(int64_t *)a_id_ptr;
    Face *b = c->faces + *(int64_t *)b_id_ptr;
    float a_avg_z = (verts[a->v[0]].z + verts[a->v[1]].z + verts[a->v[2]].z) / 3.0f;
    float b_avg_z = (verts[b->v[0]].z + verts[b->v[1]].z + verts[b->v[2]].z) / 3.0f;
    if (a_avg_z < b_avg_z) {
        return 1;
    } else if (b_avg_z < a_avg_z) {
        return -1;
    } else {
        return 0;
    }
}

static void face_ids_sort(JkVec3 *vertices, Face *faces, JkInt64Array face_ids)
{
    int64_t tmp;
    FaceIdsSortContext c = {.vertices = vertices, .faces = faces};
    jk_quicksort(
            face_ids.items, face_ids.count, JK_SIZEOF(*face_ids.items), &tmp, &c, face_ids_compare);
}

static b32 clockwise(JkVec3 *vertices, Face face)
{
    JkVec3 p[3];
    for (int64_t i = 0; i < 3; i++) {
        p[i] = vertices[face.v[i]];
    }
    return (p[1].x - p[0].x) * (p[2].y - p[0].y) - (p[1].y - p[0].y) * (p[2].x - p[0].x) < 0;
}

static JkColor color_scalar_mul(float s, JkColor c)
{
    for (int64_t i = 0; i < 3; i++) {
        c.v[i] = JK_MIN(255.0f, c.v[i] * s);
    }
    return c;
}

static Bitmap bitmap_from_span(Assets *assets, BitmapSpan span)
{
    return (Bitmap){
        .dimensions = span.dimensions,
        .memory = (JkColor3 *)((uint8_t *)assets + span.offset),
    };
}

void render(Assets *assets, State *state)
{
    jk_print = state->print;

    JkArenaRoot arena_root;
    JkArena arena = jk_arena_fixed_init(&arena_root, state->memory);

    JkVec3Array vertices;
    JK_ARRAY_FROM_SPAN(vertices, assets, assets->vertices);
    JkVec2Array texcoords;
    JK_ARRAY_FROM_SPAN(texcoords, assets, assets->texcoords);
    ObjectArray objects;
    JK_ARRAY_FROM_SPAN(objects, assets, assets->objects);

    if (!JK_FLAG_GET(state->flags, FLAG_INITIALIZED)) {
        JK_FLAG_SET(state->flags, FLAG_INITIALIZED, 1);
    }

    // Clear buffer
    for (int32_t y = 0; y < state->dimensions.y; y++) {
        jk_memset(state->draw_buffer + (y * DRAW_BUFFER_SIDE_LENGTH),
                0,
                JK_SIZEOF(JkColor) * state->dimensions.x);
    }

    int32_t rotation_ticks = rotation_seconds * state->os_timer_frequency;
    float angle = 2 * JK_PI * ((float)(state->os_time % rotation_ticks) / (float)rotation_ticks);
    JkVec3 light_dir_n = jk_vec3_normalized(light_dir);

    JkMat4 world_matrix =
            jk_mat4_conversion_from((JkCoordinateSystem){JK_LEFT, JK_BACKWARD, JK_UP});
    world_matrix = jk_mat4_mul(jk_mat4_rotate_x(JK_PI / 7), world_matrix);
    world_matrix = jk_mat4_mul(jk_mat4_rotate_z(angle), world_matrix);
    world_matrix = jk_mat4_mul(jk_mat4_translate((JkVec3){0, 0, -0.05}), world_matrix);

    JkMat4 ndc_matrix = jk_mat4_translate(jk_vec3_mul(-1, camera_pos));
    ndc_matrix = jk_mat4_mul(
            jk_mat4_conversion_to((JkCoordinateSystem){JK_RIGHT, JK_UP, JK_BACKWARD}), ndc_matrix);
    ndc_matrix = jk_mat4_mul(jk_mat4_perspective(state->dimensions, JK_PI / 2, 0.05f), ndc_matrix);

    JkMat4 pixel_matrix = jk_mat4_translate((JkVec3){1, -1, 0});
    pixel_matrix = jk_mat4_mul(
            jk_mat4_scale((JkVec3){state->dimensions.x / 2.0f, -state->dimensions.y / 2.0f, 1}),
            pixel_matrix);

    JkVec3 *world_vertices = jk_arena_push(&arena, vertices.count * JK_SIZEOF(*world_vertices));
    for (int64_t i = 0; i < vertices.count; i++) {
        world_vertices[i] = jk_mat4_mul_point(world_matrix, vertices.items[i]);
    }

    JkVec3 *screen_vertices = jk_arena_push(&arena, vertices.count * JK_SIZEOF(*screen_vertices));
    for (int64_t i = 0; i < vertices.count; i++) {
        JkVec4 vec4 = jk_mat4_mul_vec4(ndc_matrix, jk_vec3_to_4(world_vertices[i], 1));
        screen_vertices[i] = jk_mat4_mul_point(pixel_matrix, jk_vec4_perspective_divide(vec4));
    }

    for (ObjectId object_id = {1}; object_id.i < objects.count; object_id.i++) {
        Object *object = objects.items + object_id.i;

        FaceArray faces;
        JK_ARRAY_FROM_SPAN(faces, assets, object->faces);
        Bitmap texture = bitmap_from_span(assets, object->texture);

        JkInt64Array face_ids;
        JK_ARENA_PUSH_ARRAY(&arena, face_ids, faces.count);
        for (int64_t i = 0; i < face_ids.count; i++) {
            face_ids.items[i] = i;
        }
        face_ids_sort(screen_vertices, faces.items, face_ids);
        for (int32_t face_id_index = 0; face_id_index < face_ids.count; face_id_index++) {
            int64_t face_id = face_ids.items[face_id_index];
            Face face = faces.items[face_id];

            if (clockwise(screen_vertices, face)) {
                /*
                JkVec3 normal = jk_vec3_normalized(
                        jk_vec3_cross(jk_vec3_sub(world_vertices[face.v[1]],
                world_vertices[face.v[0]]), jk_vec3_sub(world_vertices[face.v[2]],
                world_vertices[face.v[0]]))); float dot = -jk_vec3_dot(light_dir_n, normal); float
                multiplier = 1.2; if (0 < dot) { multiplier += dot * 5;
                }
                */

                Triangle tri = {.texture = texture};
                for (int64_t i = 0; i < 3; i++) {
                    tri.v[i] = screen_vertices[face.v[i]];
                    tri.t[i] = jk_vec2_mul(tri.v[i].z, texcoords.items[face.t[i]]);
                }
                triangle_fill(&arena, state, tri);
            }
        }

        for (int32_t y = 0; y < state->dimensions.y; y++) {
            for (int32_t x = 0; x < state->dimensions.x; x++) {
                int32_t i = y * DRAW_BUFFER_SIDE_LENGTH + x;
                state->draw_buffer[i] = jk_color_disjoint_over(state->draw_buffer[i], bg);
            }
        }
    }
}
