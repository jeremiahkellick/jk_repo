#include "graphics.h"

// #jk_build library
// #jk_build export render
// #jk_build single_translation_unit

// #jk_build dependencies_begin
#include <jk_src/jk_lib/jk_lib.h>
// #jk_build dependencies_end

static JkColor normal_bg = {.r = CLEAR_COLOR_R, .g = CLEAR_COLOR_G, .b = CLEAR_COLOR_B, .a = 255};
static JkColor test_bg = {.r = 0x27, .g = 0x27, .b = 0x16, .a = 255};

static JkVec3 camera_position_init = {0, 0, 1.75};
static float camera_rot_angle_init = 5 * JK_PI / 4;

static JkVec3 light_dir = {-1, 4, -1};
static int32_t rotation_seconds = 8;

static float sample_offsets[2][SAMPLE_COUNT] = {
    {0x0.6p0f, 0x0.Ep0f, 0x0.2p0f, 0x0.Ap0f},
    {0x0.2p0f, 0x0.6p0f, 0x0.Ap0f, 0x0.Ep0f},
};

_Alignas(32) static float lane_offsets[2][LANE_COUNT] = {
    {0, 1, 2, 3, 4, 5, 6, 7},
    {0, 0, 0, 0, 0, 0, 0, 0},
};

typedef struct Triangle {
    JkVec3 v[3];
    JkVec2 t[3];
} Triangle;

typedef struct TriangleArray {
    int64_t count;
    Triangle *e;
} TriangleArray;

typedef struct TexturedVertex {
    JkVec3 v;
    JkVec2 t;
} TexturedVertex;

typedef struct TexturedVertexArray {
    int64_t count;
    TexturedVertex *e;
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

typedef enum Interpolant {
    I_BARYCENTRIC_0,
    I_BARYCENTRIC_1,
    I_BARYCENTRIC_2,
    I_U,
    I_V,
    I_Z,
    INTERPOLANT_COUNT,
} Interpolant;

typedef struct Interpolants {
    JkF32x8 e[INTERPOLANT_COUNT];
} Interpolants;

static void triangle_fill(State *state, Triangle tri, Bitmap texture)
{
    JkIntRect screen_rect = {.min = (JkIntVec2){0}, .max = state->dimensions};
    JkIntRect bounds = jk_int_rect_intersect(screen_rect, triangle_bounding_box(tri));
    if (!(bounds.min.x < bounds.max.x && bounds.min.y < bounds.max.y)) {
        return;
    }
    bounds.min.x &= ~(8 - 1);
    JkVec2 tex_float_dimensions = jk_vec2_from_int(texture.dimensions);

    JkVec2 verts_2d[3];
    for (int64_t i = 0; i < 3; i++) {
        verts_2d[i] = jk_vec3_to_2(tri.v[i]);
    }

    Interpolants interpolants_row[SAMPLE_COUNT] = {0};
    float deltas[2][INTERPOLANT_COUNT] = {0};

    float barycentric_divisor = 0;
    JkF32x8 init_pos[2][SAMPLE_COUNT];
    for (int64_t axis_index = 0; axis_index < 2; axis_index++) {
        JkF32x8 pixel_coord = jk_f32x8_add(jk_f32x8_broadcast(bounds.min.v[axis_index]),
                jk_f32x8_load(lane_offsets[axis_index]));
        for (int64_t sample_index = 0; sample_index < SAMPLE_COUNT; sample_index++) {
            init_pos[axis_index][sample_index] = jk_f32x8_add(
                    pixel_coord, jk_f32x8_broadcast(sample_offsets[axis_index][sample_index]));
        }
    }

    for (int64_t i = 0; i < 3; i++) {
        int64_t a = (i + 1) % 3;
        int64_t b = (i + 2) % 3;
        float cross = jk_vec2_cross(verts_2d[a], verts_2d[b]);
        barycentric_divisor += cross;
        float x_delta = verts_2d[a].y - verts_2d[b].y;
        float y_delta = verts_2d[b].x - verts_2d[a].x;
        deltas[0][I_BARYCENTRIC_0 + i] = 8 * x_delta;
        deltas[1][I_BARYCENTRIC_0 + i] = y_delta;
        JkF32x8 cross_wide = jk_f32x8_broadcast(cross);
        JkF32x8 x_delta_wide = jk_f32x8_broadcast(x_delta);
        JkF32x8 y_delta_wide = jk_f32x8_broadcast(y_delta);
        for (int64_t sample_index = 0; sample_index < SAMPLE_COUNT; sample_index++) {
            JkF32x8 coord =
                    jk_f32x8_add(cross_wide, jk_f32x8_mul(x_delta_wide, init_pos[0][sample_index]));
            coord = jk_f32x8_add(coord, jk_f32x8_mul(y_delta_wide, init_pos[1][sample_index]));
            interpolants_row[sample_index].e[I_BARYCENTRIC_0 + i] = coord;
        }
    }

    JkF32x8 barycentric_divisor_wide = jk_f32x8_broadcast(barycentric_divisor);
    for (int64_t i = 0; i < 3; i++) {
        for (int64_t axis_index = 0; axis_index < 2; axis_index++) {
            deltas[axis_index][I_BARYCENTRIC_0 + i] /= barycentric_divisor;
            deltas[axis_index][I_U] += deltas[axis_index][I_BARYCENTRIC_0 + i] * tri.t[i].x;
            deltas[axis_index][I_V] += deltas[axis_index][I_BARYCENTRIC_0 + i] * tri.t[i].y;
            deltas[axis_index][I_Z] += deltas[axis_index][I_BARYCENTRIC_0 + i] * tri.v[i].z;
        }
        for (int64_t sample_index = 0; sample_index < SAMPLE_COUNT; sample_index++) {
            interpolants_row[sample_index].e[I_BARYCENTRIC_0 + i] =
                    jk_f32x8_div(interpolants_row[sample_index].e[I_BARYCENTRIC_0 + i],
                            barycentric_divisor_wide);
            interpolants_row[sample_index].e[I_U] =
                    jk_f32x8_add(interpolants_row[sample_index].e[I_U],
                            jk_f32x8_mul(interpolants_row[sample_index].e[I_BARYCENTRIC_0 + i],
                                    jk_f32x8_broadcast(tri.t[i].x)));
            interpolants_row[sample_index].e[I_V] =
                    jk_f32x8_add(interpolants_row[sample_index].e[I_V],
                            jk_f32x8_mul(interpolants_row[sample_index].e[I_BARYCENTRIC_0 + i],
                                    jk_f32x8_broadcast(tri.t[i].y)));
            interpolants_row[sample_index].e[I_Z] =
                    jk_f32x8_add(interpolants_row[sample_index].e[I_Z],
                            jk_f32x8_mul(interpolants_row[sample_index].e[I_BARYCENTRIC_0 + i],
                                    jk_f32x8_broadcast(tri.v[i].z)));
        }
    }

    for (int64_t sample_index = 0; sample_index < SAMPLE_COUNT; sample_index++) {
        for (int32_t y = bounds.min.y; y < bounds.max.y; y++) {
            Interpolants interpolants = interpolants_row[sample_index];
            for (int32_t x = bounds.min.x; x < bounds.max.x; x += 8) {
                JkF32x8 outside_triangle = jk_f32x8_or(interpolants.e[I_BARYCENTRIC_0],
                        jk_f32x8_or(
                                interpolants.e[I_BARYCENTRIC_1], interpolants.e[I_BARYCENTRIC_2]));
                if (!jk_f32x8_all(outside_triangle)) {
                    int32_t index = PIXEL_COUNT * sample_index + DRAW_BUFFER_SIDE_LENGTH * y + x;
                    JkF32x8 z_buffer = jk_f32x8_load(state->z_buffer + index);
                    JkF32x8 in_front = jk_f32x8_less_than(z_buffer, interpolants.e[I_Z]);
                    JkF32x8 visible = jk_f32x8_andnot(outside_triangle, in_front);
                    if (jk_f32x8_any(visible)) {
                        jk_f32x8_store(state->z_buffer + index,
                                jk_f32x8_blend(z_buffer, interpolants.e[I_Z], visible));
                        JkF32x8 u = jk_f32x8_div(interpolants.e[I_U], interpolants.e[I_Z]);
                        JkF32x8 v = jk_f32x8_div(interpolants.e[I_V], interpolants.e[I_Z]);
                        JkF32x8 tex_x = jk_f32x8_mul(jk_f32x8_broadcast(tex_float_dimensions.x),
                                jk_f32x8_sub(u, jk_f32x8_floor(u)));
                        JkF32x8 tex_y = jk_f32x8_mul(jk_f32x8_broadcast(tex_float_dimensions.y),
                                jk_f32x8_sub(v, jk_f32x8_floor(v)));

                        JkF32x8 tex_index = jk_f32x8_mul(
                                jk_f32x8_broadcast(tex_float_dimensions.x), jk_f32x8_floor(tex_y));
                        tex_index = jk_f32x8_floor(jk_f32x8_add(tex_index, tex_x));
                        JkF32x8 tex_offset = jk_f32x8_mul(jk_f32x8_broadcast(3), tex_index);

                        JkF32x8 color_buffer = jk_f32x8_load((float *)(state->draw_buffer + index));
                        JkF32x8 color = jk_f32x8_gather(texture.memory, tex_offset);
                        jk_f32x8_store((float *)(state->draw_buffer + index),
                                jk_f32x8_blend(color_buffer, color, visible));
                    }
                }

                for (int64_t i = 0; i < INTERPOLANT_COUNT; i++) {
                    interpolants.e[i] =
                            jk_f32x8_add(interpolants.e[i], jk_f32x8_broadcast(deltas[0][i]));
                }
            }

            for (int64_t i = 0; i < INTERPOLANT_COUNT; i++) {
                interpolants_row[sample_index].e[i] = jk_f32x8_add(
                        interpolants_row[sample_index].e[i], jk_f32x8_broadcast(deltas[1][i]));
            }
        }
    }
}

static b32 clockwise(Triangle t)
{
    return (t.v[1].x - t.v[0].x) * (t.v[2].y - t.v[0].y)
            - (t.v[1].y - t.v[0].y) * (t.v[2].x - t.v[0].x)
            < 0;
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

void render(JkContext *context, Assets *assets, State *state)
{
    jk_context = context;

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

    if (0 < state->test_frames_remaining) {
        state->dimensions.x = 1902;
        state->dimensions.y = 970;
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
                        parent_id = objects.e[parent_id.i].parent) {
                    Object *parent = objects.e + parent_id.i;
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

    // Clear
    JkColor bg = 0 < state->test_frames_remaining ? test_bg : normal_bg;
    for (int32_t sample_index = 0; sample_index < SAMPLE_COUNT; sample_index++) {
        for (int32_t y = 0; y < state->dimensions.y; y++) {
            int32_t row_index = PIXEL_COUNT * sample_index + DRAW_BUFFER_SIDE_LENGTH * y;
            for (int32_t x = 0; x < state->dimensions.x; x++) {
                state->draw_buffer[row_index + x] = bg;
            }
            jk_memset(state->z_buffer + row_index,
                    0,
                    state->dimensions.x * JK_SIZEOF(*state->z_buffer));
        }
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
        Object *object = objects.e + object_id.i;
        JkArena object_arena = jk_arena_child_get(&arena);

        JkMat4 world_matrix = jk_mat4_i;
        for (ObjectId parent_id = object_id; parent_id.i;
                parent_id = objects.e[parent_id.i].parent) {
            Object *parent = objects.e + parent_id.i;
            world_matrix = jk_mat4_mul(jk_transform_to_mat4(parent->transform), world_matrix);
        }

        JkVec3 *world_vertices =
                jk_arena_push(&object_arena, vertices.count * JK_SIZEOF(*world_vertices));
        for (int64_t i = 0; i < vertices.count; i++) {
            world_vertices[i] = jk_mat4_mul_point(world_matrix, vertices.e[i]);
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
            Face face = faces.e[face_index];
            JkArena face_arena = jk_arena_child_get(&object_arena);

            JkVec2 uv[3];
            if (object->repeat_size) {
                JkVec3 local_points[3];
                for (int64_t i = 0; i < 3; i++) {
                    local_points[i] = jk_vec3_mul(1 / object->repeat_size,
                            jk_vec3_hadamard_prod(vertices.e[face.v[i]], object->transform.scale));
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
                    uv[i] = texcoords.e[face.t[i]];
                }
            }

            // Apply near clipping and projection
            TexturedVertexArray vs = {.e = jk_arena_pointer_current(&face_arena)};
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
            vs.count = (TexturedVertex *)jk_arena_pointer_current(&face_arena) - vs.e;

            // Triangulate the resulting polygon
            TriangleArray tris = {.e = jk_arena_pointer_current(&face_arena)};
            for (int64_t i = 2; i < vs.count; i++) {
                Triangle *tri = jk_arena_push(&face_arena, sizeof(*tri));
                int64_t indexes[3] = {0, i - 1, i};
                for (int64_t j = 0; j < 3; j++) {
                    tri->v[j] = vs.e[indexes[j]].v;
                    tri->t[j] = vs.e[indexes[j]].t;
                }
            }
            tris.count = (Triangle *)jk_arena_pointer_current(&face_arena) - tris.e;

            for (int64_t i = 0; i < tris.count; i++) {
                if (clockwise(tris.e[i])) {
                    triangle_fill(state, tris.e[i], texture);
                }
            }
        }
    }
    JK_PROFILE_ZONE_END(triangles);

    JK_PROFILE_ZONE_TIME_BEGIN(pixels);
    for (int32_t y = 0; y < state->dimensions.y; y++) {
        for (int32_t x = 0; x < state->dimensions.x; x += 8) {
            JkI256 channels[3] = {jk_i256_zero(), jk_i256_zero(), jk_i256_zero()};
            for (int64_t sample_index = 0; sample_index < SAMPLE_COUNT; sample_index++) {
                JkI256 sample = jk_i256_load(state->draw_buffer
                        + (PIXEL_COUNT * sample_index + DRAW_BUFFER_SIDE_LENGTH * y + x));
                JkI256 byte_mask = jk_i256_broadcast_i32(0xff);
                channels[0] = jk_i256_add_i32(channels[0], jk_i256_and(sample, byte_mask));
                channels[1] = jk_i256_add_i32(channels[1],
                        jk_i256_and(JK_I256_SHIFT_RIGHT_SIGN_FILL_I32(sample, 8), byte_mask));
                channels[2] = jk_i256_add_i32(channels[2],
                        jk_i256_and(JK_I256_SHIFT_RIGHT_SIGN_FILL_I32(sample, 16), byte_mask));
            }
            for (int32_t channel_index = 0; channel_index < 3; channel_index++) {
                channels[channel_index] =
                        JK_I256_SHIFT_RIGHT_SIGN_FILL_I32(channels[channel_index], 2);
            }

            JkI256 color = channels[0];
            color = jk_i256_or(color, JK_I256_SHIFT_LEFT_I32(channels[1], 8));
            color = jk_i256_or(color, JK_I256_SHIFT_LEFT_I32(channels[2], 16));

            jk_i256_store(state->draw_buffer + (DRAW_BUFFER_SIDE_LENGTH * y + x), color);
        }
    }
    JK_PROFILE_ZONE_END(pixels);

    jk_profile_frame_end();

    if (jk_key_pressed(&state->keyboard, JK_KEY_P)) {
        jk_log(JK_LOG_INFO, jk_profile_report(&arena, state->estimate_cpu_frequency(100)));
    }

    if (0 < state->test_frames_remaining) {
        if (--state->test_frames_remaining == 0) {
            jk_log(JK_LOG_INFO, jk_profile_report(&arena, state->estimate_cpu_frequency(100)));
        }
    }
}
