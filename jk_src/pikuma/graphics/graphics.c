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
static JkColor normal_bg = {.r = CLEAR_COLOR_R, .g = CLEAR_COLOR_G, .b = CLEAR_COLOR_B, .a = 0xff};
static JkColor test_bg = {.r = 0x27, .g = 0x27, .b = 0x16, .a = 0xff};

static JkVec3 camera_position_init = {0, 0, 1.75};
static float camera_rot_angle_init = 5 * JK_PI / 4;

static JkVec3 light_dir = {-1, 4, -1};
static int32_t rotation_seconds = 8;

static Pixel pixel_get(State *state, PixelIndex index)
{
    return (Pixel){
        .color = state->draw_buffer + index.i,
        .z = state->z_buffer + index.i,
        .next = state->next_buffer + index.i,
    };
}

static JkColor color_get(State *state, PixelIndex index)
{
    return state->draw_buffer[index.i];
}

static float z_get(State *state, PixelIndex index)
{
    return state->z_buffer[index.i];
}

static PixelIndex *next_get(State *state, PixelIndex index)
{
    return state->next_buffer + index.i;
}

static PixelIndex pixel_alloc(State *state)
{
    if (state->pixel_count < PIXEL_COUNT) {
        return (PixelIndex){state->pixel_count++};
    } else {
        return (PixelIndex){0};
    }
}

static b32 pixel_index_nil(PixelIndex index)
{
    return !index.i;
}

static PixelIndex pixel_index_from_pos(State *state, JkIntVec2 pos)
{
    return (PixelIndex){DRAW_BUFFER_SIDE_LENGTH * pos.y + pos.x + 1};
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

typedef struct Triangle {
    JkVec3 v[3];
    JkVec2 t[3];
} Triangle;

typedef struct TriangleArray {
    int64_t count;
    Triangle *items;
} TriangleArray;

typedef struct TexturedVertex {
    JkVec3 v;
    JkVec2 t;
} TexturedVertex;

typedef struct TexturedVertexArray {
    int64_t count;
    TexturedVertex *items;
} TexturedVertexArray;

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

static void triangle_fill(JkArena *arena, State *state, Triangle tri, Bitmap texture)
{
    JkArena tmp_arena = jk_arena_child_get(arena);

    JkIntRect screen_rect = {.min = (JkIntVec2){0}, .max = state->dimensions};
    JkIntRect bounds = jk_int_rect_intersect(screen_rect, triangle_bounding_box(tri));
    JkIntVec2 dimensions = jk_int_rect_dimensions(bounds);

    JkVec2 tex_float_dimensions = jk_vec2_from_int(texture.dimensions);
    JkIntVec2 tex_half_dimensions = jk_int_vec2_div(2, texture.dimensions);

    if (dimensions.x < 1) {
        return;
    }

    JkVec2 verts_2d[3];
    for (int64_t i = 0; i < 3; i++) {
        verts_2d[i] = jk_vec3_to_2(tri.v[i]);
    }

    float barycentric_divisor = 0;
    JkVec2 barycentric_delta[3];
    float barycentric_init[3];
    JkVec2 init_pos = {bounds.min.x + 0.5, bounds.min.y + 0.5};
    for (int64_t i = 0; i < 3; i++) {
        int64_t a = (i + 1) % 3;
        int64_t b = (i + 2) % 3;
        float cross = jk_vec2_cross(verts_2d[a], verts_2d[b]);
        barycentric_divisor += cross;
        barycentric_delta[i].x = verts_2d[a].y - verts_2d[b].y;
        barycentric_delta[i].y = verts_2d[b].x - verts_2d[a].x;
        barycentric_init[i] =
                barycentric_delta[i].x * init_pos.x + barycentric_delta[i].y * init_pos.y + cross;
    }
    for (int64_t i = 0; i < 3; i++) {
        barycentric_delta[i].x /= barycentric_divisor;
        barycentric_delta[i].y /= barycentric_divisor;
        barycentric_init[i] /= barycentric_divisor;
    }

    JkVec3 vertex_texcoords[3];
    for (int64_t i = 0; i < 3; i++) {
        vertex_texcoords[i] = jk_vec2_to_3(tri.t[i], tri.v[i].z);
    }
    JkVec3 texcoord_row = {0};
    for (int64_t i = 0; i < 3; i++) {
        texcoord_row =
                jk_vec3_add(texcoord_row, jk_vec3_mul(barycentric_init[i], vertex_texcoords[i]));
    }
    JkVec3 texcoord_deltas[2] = {0};
    for (int64_t axis_index = 0; axis_index < 2; axis_index++) {
        for (int64_t vertex_index = 0; vertex_index < 3; vertex_index++) {
            texcoord_deltas[axis_index] = jk_vec3_add(texcoord_deltas[axis_index],
                    jk_vec3_mul(barycentric_delta[vertex_index].v[axis_index],
                            vertex_texcoords[vertex_index]));
        }
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

    PixelIndex pixel_index_row = pixel_index_from_pos(state, bounds.min);
    int64_t coverage_size = JK_SIZEOF(float) * (dimensions.x + 1);
    JkBuffer coverage_buf = jk_arena_push_buffer(&tmp_arena, 2 * coverage_size);
    float *coverage = (float *)coverage_buf.data;
    float *fill = (float *)(coverage_buf.data + coverage_size);
    for (int32_t y = bounds.min.y; y < bounds.max.y; y++) {
        jk_buffer_zero(coverage_buf);

        float scan_y_top = (float)y;
        float scan_y_bottom = scan_y_top + 1.0f;
        for (int64_t i = 0; i < edges.count; i++) {
            float y_top = JK_MAX(edges.items[i].segment.p0.y, scan_y_top);
            float y_bottom = JK_MIN(edges.items[i].segment.p1.y, scan_y_bottom);
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
                    float delta_y = (edges.items[i].segment.p1.y - edges.items[i].segment.p0.y)
                            / (edges.items[i].segment.p1.x - edges.items[i].segment.p0.x);

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
        PixelIndex pixel_index = pixel_index_row;
        JkVec3 texcoord_3d = texcoord_row;
        for (int32_t x = bounds.min.x; x < bounds.max.x; x++) {
            acc += fill[x - bounds.min.x];

            int32_t alpha = (int32_t)(JK_ABS((coverage[x - bounds.min.x] + acc) * 255.0f));
            if (255 < alpha) {
                alpha = 255;
            }

            if (0 < alpha) {
                JkVec2 texcoord_2d = {
                    texcoord_3d.x / texcoord_3d.z,
                    texcoord_3d.y / texcoord_3d.z,
                };

                // Texture lookup
                int32_t tex_x = jk_remainder_f32(texcoord_2d.x, tex_float_dimensions.x);
                int32_t tex_y = jk_remainder_f32(texcoord_2d.y, tex_float_dimensions.y);
                tex_x += tex_half_dimensions.x;
                tex_y += tex_half_dimensions.y;
                JkColor pixel_color = jk_color3_to_4(
                        texture.memory[texture.dimensions.x * tex_y + tex_x], (uint8_t)alpha);

                PixelIndex *head_next = next_get(state, pixel_index);
                PixelIndex new_pixel_index = pixel_alloc(state);
                if (!pixel_index_nil(new_pixel_index)) {
                    Pixel new_pixel = pixel_get(state, new_pixel_index);

                    *new_pixel.color = pixel_color;
                    *new_pixel.z = texcoord_3d.z;
                    *new_pixel.next = *head_next;

                    *head_next = new_pixel_index;
                }
            }

            pixel_index.i++;
            texcoord_3d = jk_vec3_add(texcoord_3d, texcoord_deltas[0]);
        }

        pixel_index_row.i += DRAW_BUFFER_SIDE_LENGTH;
        texcoord_row = jk_vec3_add(texcoord_row, texcoord_deltas[1]);
    }
}

static b32 clockwise(Triangle t)
{
    return (t.v[1].x - t.v[0].x) * (t.v[2].y - t.v[0].y)
            - (t.v[1].y - t.v[0].y) * (t.v[2].x - t.v[0].x)
            < 0;
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

static void add_textured_vertex(JkArena *arena, JkMat4 pixel_matrix, JkVec4 v, JkVec2 t)
{
    TexturedVertex *new = jk_arena_push(arena, sizeof(*new));
    new->v = jk_mat4_mul_point(pixel_matrix, jk_vec4_perspective_divide(v));
    new->t = jk_vec2_mul(new->v.z, t);
}

typedef struct Plane {
    JkVec3 normal;
    JkVec3 point;
} Plane;

typedef struct Move {
    JkSegment3d s;
    Plane prev_projection_plane;
} Move;

typedef struct EdgeFunction {
    JkVec2 deltas;
    float init;
} EdgeFunction;

static void move_against_box(Move *move, JkMat4 world_matrix, ObjectId id, JkArena *arena)
{
    static float const padding = 0.3;

    static JkVec3 const local_extents[3] = {
        {0.5, 0, 0},
        {0, 0.5, 0},
        {0, 0, 0.5},
    };

    JkVec3 origin = jk_mat4_mul_point(world_matrix, (JkVec3){0, 0, 0});

    // Compute all 6 planes
    Plane planes[6];
    for (int64_t extent = 0; extent < 3; extent++) {
        JkVec3 transformed = jk_mat4_mul_normal(world_matrix, local_extents[extent]);
        JkVec3 padded =
                jk_vec3_add(transformed, jk_vec3_mul(padding, jk_vec3_normalized(transformed)));
        for (int64_t negative = 0; negative < 2; negative++) {
            JkVec3 signed_extent = jk_vec3_mul(negative ? -1 : 1, padded);
            int64_t plane = 2 * extent + negative;
            planes[plane].normal = jk_vec3_normalized(signed_extent);
            planes[plane].point = jk_vec3_add(origin, signed_extent);
            // The origin is centered along X and Y, but it's aligned with the bottom face along the
            // Z axis, so if we're dealing with the Z axis, we need to add an offset
            if (extent == 2) {
                planes[plane].point = jk_vec3_add(planes[plane].point, transformed);
            }
        }
    }

    JkVec3 move_delta = jk_vec3_sub(move->s.p1, move->s.p0);

    b32 inside = 1;
    int64_t entry_plane = -1;
    float smallest_depenetration = jk_infinity_f32.f32;
    int64_t depenetration_plane = -1;
    float t_entry = -jk_infinity_f32.f32;
    float t_exit = jk_infinity_f32.f32;
    for (int64_t plane = 0; plane < 6; plane++) {
        float delta_dot = jk_vec3_dot(planes[plane].normal, move_delta);
        float p0_dot =
                jk_vec3_dot(planes[plane].normal, jk_vec3_sub(move->s.p0, planes[plane].point));
        float p1_dot =
                jk_vec3_dot(planes[plane].normal, jk_vec3_sub(move->s.p1, planes[plane].point));

        if (0 < p0_dot) {
            inside = 0;
        }

        if (inside && -p0_dot < smallest_depenetration) {
            smallest_depenetration = -p0_dot;
            depenetration_plane = plane;
        }

        b32 is_entry_plane = delta_dot < 0;
        if (is_entry_plane) {
            delta_dot *= -1;
            p0_dot *= -1;
            p1_dot *= -1;
        }

        float t;
        if (p0_dot < 0 && p1_dot < 0) { // both points are before the plane
            t = jk_infinity_f32.f32;
        } else if (0 <= p0_dot && 0 <= p1_dot) { // boths points are after the plane
            t = -jk_infinity_f32.f32;
        } else if (p0_dot < 0 && 0 <= p1_dot) { // there's an intersection
            // Set t to the percentage between p0 and p1 where we encounter the plane
            t = -p0_dot / delta_dot;
        } else {
            // We're probably parallel. Set t value such that we're guaranteed to no-op
            t = is_entry_plane ? -jk_infinity_f32.f32 : jk_infinity_f32.f32;
        }

        if (is_entry_plane) {
            if (t_entry < t) {
                t_entry = t;
                entry_plane = plane;
            }
        } else {
            t_exit = JK_MIN(t_exit, t);
        }
    }

    int64_t projection_plane = -1;
    if (inside) {
        // We're inside the box, depenetrate p0 and project p1 to the same plane
        projection_plane = depenetration_plane;
        move->s.p0 = jk_vec3_add(move->s.p0,
                jk_vec3_mul(smallest_depenetration, planes[depenetration_plane].normal));
    } else if (t_entry < t_exit) {
        // Our path intersects the box. Set p0 to the intersection point and project p1.
        projection_plane = entry_plane;
        move->s.p0 = jk_vec3_lerp(move->s.p0, move->s.p1, t_entry);
    }

    if (0 <= projection_plane) {
        move->s.p1 = jk_vec3_add(move->s.p1,
                jk_vec3_mul(-jk_vec3_dot(planes[projection_plane].normal,
                                    jk_vec3_sub(move->s.p1, planes[projection_plane].point)),
                        planes[projection_plane].normal));

        // If p1 is inside the previous projection plane, we've hit a corner and should project p1
        // to the line of intersection between the two planes
        if (jk_vec3_dot(move->prev_projection_plane.normal,
                    jk_vec3_sub(move->s.p1, move->prev_projection_plane.point))
                < 0) {
            Plane a = move->prev_projection_plane;
            Plane b = planes[projection_plane];
            JkVec3 intersect_normal = jk_vec3_cross(a.normal, b.normal);
            float sqr_mag = jk_vec3_magnitude_sqr(intersect_normal);
            if (0.0001f < JK_ABS(sqr_mag)) {
                intersect_normal = jk_vec3_mul(1 / jk_sqrt_f32(sqr_mag), intersect_normal);
                float dot = jk_vec3_dot(a.normal, b.normal);
                float denom = 1 - dot * dot;
                if (denom) {
                    float d_a = jk_vec3_dot(a.normal, a.point);
                    float d_b = jk_vec3_dot(b.normal, b.point);
                    JkVec3 a_comp = jk_vec3_mul(d_a - dot * d_b, a.normal);
                    JkVec3 b_comp = jk_vec3_mul(d_b - dot * d_a, b.normal);
                    JkVec3 intersect_point = jk_vec3_mul(1 / denom, jk_vec3_add(a_comp, b_comp));

                    for (int64_t i = 0; i < 2; i++) {
                        JkVec3 delta = jk_vec3_mul(
                                jk_vec3_dot(intersect_normal,
                                        jk_vec3_sub(move->s.endpoints[i], intersect_point)),
                                intersect_normal);
                        move->s.endpoints[i] = jk_vec3_add(intersect_point, delta);
                    }
                }
            }
        }

        move->prev_projection_plane = planes[projection_plane];
    }
}

void render(Assets *assets, State *state)
{
    jk_print = state->print;

    if (jk_key_pressed(&state->keyboard, JK_KEY_R)) {
        JK_FLAG_SET(state->flags, FLAG_INITIALIZED, 0);
    }

    if (jk_key_pressed(&state->keyboard, JK_KEY_T)) {
        JK_FLAG_SET(state->flags, FLAG_INITIALIZED, 0);
        state->test_frames_remaining = 240;
    }

    if (!JK_FLAG_GET(state->flags, FLAG_INITIALIZED)) {
        JK_FLAG_SET(state->flags, FLAG_INITIALIZED, 1);

        state->camera_position = jk_vec3_to_2(camera_position_init);
        state->camera_yaw = camera_rot_angle_init;
        state->camera_pitch = 0;

        jk_profile_reset();
    }

    jk_profile_frame_begin();

    JkArenaRoot arena_root;
    JkArena arena = jk_arena_fixed_init(&arena_root, state->memory);

    JkVec3Array vertices;
    JK_ARRAY_FROM_SPAN(vertices, assets, assets->vertices);
    JkVec2Array texcoords;
    JK_ARRAY_FROM_SPAN(texcoords, assets, assets->texcoords);
    ObjectArray objects;
    JK_ARRAY_FROM_SPAN(objects, assets, assets->objects);

    if (state->test_frames_remaining <= 0) {
        float mouse_sensitivity = 0.4 * DELTA_TIME;
        state->camera_yaw += jk_remainder_f32(mouse_sensitivity * -state->mouse.delta.x, 2 * JK_PI);
        state->camera_pitch =
                JK_CLAMP(state->camera_pitch + mouse_sensitivity * -state->mouse.delta.y,
                        -JK_PI / 2,
                        JK_PI / 2);
    }

    JkVec4 yaw_quat = jk_quat_angle_axis(state->camera_yaw, (JkVec3){0, 0, 1});

    if (state->test_frames_remaining <= 0) {
        JkVec3 camera_move = {0};
        if (jk_key_down(&state->keyboard, JK_KEY_W)) {
            camera_move.y += 1;
        }
        if (jk_key_down(&state->keyboard, JK_KEY_S)) {
            camera_move.y -= 1;
        }
        if (jk_key_down(&state->keyboard, JK_KEY_A)) {
            camera_move.x -= 1;
        }
        if (jk_key_down(&state->keyboard, JK_KEY_D)) {
            camera_move.x += 1;
        }
        camera_move = jk_vec3_mul(
                5 * DELTA_TIME, jk_vec3_normalized(jk_quat_rotate(yaw_quat, camera_move)));

        JkVec3 move_start = jk_vec2_to_3(state->camera_position, camera_position_init.z);
        Move move = {
            .s = {move_start, jk_vec3_add(move_start, camera_move)},
            .prev_projection_plane = {.normal = {0, 0, 1}, .point = {0, 0, -1000}},
        };
        JkVec3 prev_p1;
        do {
            prev_p1 = move.s.p1;
            for (ObjectId object_id = {1}; object_id.i < objects.count; object_id.i++) {
                JkMat4 world_matrix = jk_mat4_i;
                for (ObjectId parent_id = object_id; parent_id.i;
                        parent_id = objects.items[parent_id.i].parent) {
                    Object *parent = objects.items + parent_id.i;
                    world_matrix =
                            jk_mat4_mul(jk_transform_to_mat4(parent->transform), world_matrix);
                }

                move_against_box(&move, world_matrix, object_id, &arena);
            }
        } while (!jk_vec3_equal(prev_p1, move.s.p1, 0.0001));
        state->camera_position = jk_vec3_to_2(move.s.p1);
    }

    JkTransform camera_transform = {
        .translation = jk_vec2_to_3(state->camera_position, camera_position_init.z),
        .rotation =
                jk_quat_mul(yaw_quat, jk_quat_angle_axis(state->camera_pitch, (JkVec3){1, 0, 0})),
        .scale = {1, 1, 1},
    };

    state->pixel_count = PIXEL_COUNT / 2;

    // Clear next links
    for (int32_t y = 0; y < state->dimensions.y; y++) {
        jk_memset(state->next_buffer + (y * DRAW_BUFFER_SIDE_LENGTH),
                0,
                JK_SIZEOF(PixelIndex) * state->dimensions.x + 1);
    }

    float near_clip = 0.2f;
    JkMat4 clip_space_matrix = jk_transform_to_mat4_inv(camera_transform);
    clip_space_matrix =
            jk_mat4_mul(jk_mat4_conversion_to((JkCoordinateSystem){JK_RIGHT, JK_UP, JK_BACKWARD}),
                    clip_space_matrix);
    clip_space_matrix = jk_mat4_mul(
            jk_mat4_perspective(state->dimensions, JK_PI / 3, near_clip), clip_space_matrix);

    JkMat4 pixel_matrix = jk_mat4_translate((JkVec3){1, -1, 0});
    pixel_matrix = jk_mat4_mul(
            jk_mat4_scale((JkVec3){state->dimensions.x / 2.0f, -state->dimensions.y / 2.0f, 1}),
            pixel_matrix);

    JK_PROFILE_ZONE_TIME_BEGIN(triangles);
    for (ObjectId object_id = {1}; object_id.i < objects.count; object_id.i++) {
        Object *object = objects.items + object_id.i;
        JkArena object_arena = jk_arena_child_get(&arena);

        JkMat4 world_matrix = jk_mat4_i;
        for (ObjectId parent_id = object_id; parent_id.i;
                parent_id = objects.items[parent_id.i].parent) {
            Object *parent = objects.items + parent_id.i;
            world_matrix = jk_mat4_mul(jk_transform_to_mat4(parent->transform), world_matrix);
        }

        JkVec3 *world_vertices =
                jk_arena_push(&object_arena, vertices.count * JK_SIZEOF(*world_vertices));
        for (int64_t i = 0; i < vertices.count; i++) {
            world_vertices[i] = jk_mat4_mul_point(world_matrix, vertices.items[i]);
        }

        JkVec4 *clip_space_vertices =
                jk_arena_push(&object_arena, vertices.count * JK_SIZEOF(*clip_space_vertices));
        for (int64_t i = 0; i < vertices.count; i++) {
            clip_space_vertices[i] =
                    jk_mat4_mul_vec4(clip_space_matrix, jk_vec3_to_4(world_vertices[i], 1));
        }

        FaceArray faces;
        JK_ARRAY_FROM_SPAN(faces, assets, object->faces);
        Bitmap texture = bitmap_from_span(assets, object->texture);

        for (int64_t face_index = 0; face_index < faces.count; face_index++) {
            Face face = faces.items[face_index];
            JkArena face_arena = jk_arena_child_get(&object_arena);

            JkVec2 uv[3];
            if (object->repeat_size) {
                JkVec3 local_points[3];
                for (int64_t i = 0; i < 3; i++) {
                    local_points[i] = jk_vec3_mul(1 / object->repeat_size,
                            jk_vec3_hadamard_prod(
                                    vertices.items[face.v[i]], object->transform.scale));
                }
                JkVec3 normal = jk_vec3_cross(jk_vec3_sub(local_points[1], local_points[0]),
                        jk_vec3_sub(local_points[2], local_points[0]));

                // Find which basis plane this face is most in line with
                int64_t plane_index = 0;
                float max_coord = JK_ABS(normal.v[0]);
                for (int64_t i = 1; i < 3; i++) {
                    float coord = JK_ABS(normal.v[i]);
                    if (max_coord < coord) {
                        max_coord = coord;
                        plane_index = i;
                    }
                }

                for (int64_t i = 0; i < 3; i++) {
                    switch (plane_index) {
                    case 0: {
                        uv[i] = (JkVec2){local_points[i].y, local_points[i].z};
                    } break;

                    case 1: {
                        uv[i] = (JkVec2){local_points[i].x, local_points[i].z};
                    } break;

                    case 2: {
                        uv[i] = (JkVec2){local_points[i].x, local_points[i].y};
                    } break;

                    default: {
                        JK_ASSERT(0);
                    } break;
                    }
                }
            } else {
                for (int64_t i = 0; i < 3; i++) {
                    uv[i] = texcoords.items[face.t[i]];
                }
            }

            for (int64_t i = 0; i < 3; i++) {
                // Make some adjustments to the uv coordinates now to save some per-pixel texture
                // lookup math
                uv[i] = jk_vec2_sub(uv[i], (JkVec2){0.5, 0.5});
                uv[i].x *= texture.dimensions.x;
                uv[i].y *= texture.dimensions.y;
            }

            // Apply near clipping and projection
            TexturedVertexArray vs = {.items = jk_arena_pointer_current(&face_arena)};
            for (int64_t i = 0; i < 3; i++) {
                int64_t b_i = (i + 1) % 3;
                JkVec4 a = clip_space_vertices[face.v[i]];
                JkVec4 b = clip_space_vertices[face.v[b_i]];
                b32 a_inside = !!(a.z < a.w);
                b32 b_inside = !!(b.z < b.w);
                if (a_inside != b_inside) { // Crosses clip plane, add interpolated vertex
                    float t = (near_clip - a.w) / (b.w - a.w);
                    add_textured_vertex(&face_arena,
                            pixel_matrix,
                            jk_vec4_lerp(a, b, t),
                            jk_vec2_lerp(uv[i], uv[b_i], t));
                }
                if (b_inside) {
                    add_textured_vertex(&face_arena, pixel_matrix, b, uv[b_i]);
                }
            }
            vs.count = (TexturedVertex *)jk_arena_pointer_current(&face_arena) - vs.items;

            // Triangulate the resulting polygon
            TriangleArray tris = {.items = jk_arena_pointer_current(&face_arena)};
            for (int64_t i = 2; i < vs.count; i++) {
                Triangle *tri = jk_arena_push(&face_arena, sizeof(*tri));
                int64_t indexes[3] = {0, i - 1, i};
                for (int64_t j = 0; j < 3; j++) {
                    tri->v[j] = vs.items[indexes[j]].v;
                    tri->t[j] = vs.items[indexes[j]].t;
                }
            }
            tris.count = (Triangle *)jk_arena_pointer_current(&face_arena) - tris.items;

            for (int64_t i = 0; i < tris.count; i++) {
                if (clockwise(tris.items[i])) {
                    triangle_fill(&face_arena, state, tris.items[i], texture);
                }
            }
        }
    }
    JK_PROFILE_ZONE_END(triangles);

    JkColor bg = 0 < state->test_frames_remaining ? test_bg : normal_bg;
    JK_PROFILE_ZONE_TIME_BEGIN(pixels);
    PixelIndex list[16];
    PixelIndex pixel_index_row = {1};
    for (int32_t y = 0; y < state->dimensions.y; y++) {
        PixelIndex pixel_index = pixel_index_row;
        for (int32_t x = 0; x < state->dimensions.x; x++) {
            Pixel pixel = pixel_get(state, pixel_index);
            JkColor color = {0};

            int64_t list_count = 0;
            for (PixelIndex new = *pixel.next;
                    !pixel_index_nil(new) && list_count < JK_ARRAY_COUNT(list);
                    new = *next_get(state, new)) {
                int64_t j = list_count++;
                list[j] = new;
                for (; 0 < j && z_get(state, list[j - 1]) < z_get(state, list[j]); j--) {
                    JK_SWAP(list[j - 1], list[j], PixelIndex);
                }
            }

            for (int64_t i = 0; i < list_count; i++) {
                color = jk_color_disjoint_over(color, color_get(state, list[i]));
            }
            color = jk_color_disjoint_over(color, bg);

            *pixel.color = color;

            pixel_index.i++;
        }
        pixel_index_row.i += DRAW_BUFFER_SIDE_LENGTH;
    }
    JK_PROFILE_ZONE_END(pixels);

    jk_profile_frame_end();

    if (jk_key_pressed(&state->keyboard, JK_KEY_P)) {
        jk_print(jk_profile_report(&arena, state->estimate_cpu_frequency(100)));
    }

    if (0 < state->test_frames_remaining) {
        if (--state->test_frames_remaining == 0) {
            jk_print(jk_profile_report(&arena, state->estimate_cpu_frequency(100)));
        }
    }
}
