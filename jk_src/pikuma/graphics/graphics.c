#include "graphics.h"

// #jk_build library
// #jk_build export render
// #jk_build single_translation_unit

// #jk_build dependencies_begin
#include <jk_src/jk_lib/jk_lib.h>
#include <jk_src/jk_shapes/jk_shapes.h>
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

typedef struct TriangleNode {
    struct TriangleNode *next;
    Bitmap texture;
    Triangle tri;
} TriangleNode;

typedef struct Tile {
    TriangleNode *head;
} Tile;

typedef struct TileArray {
    int64_t count;
    Tile *e;
} TileArray;

typedef struct TexturedVertex {
    JkVec3 v;
    JkVec2 t;
} TexturedVertex;

typedef struct TexturedVertexArray {
    int64_t count;
    TexturedVertex *e;
} TexturedVertexArray;

static JkIntRect triangle_bounding_box(JkVec3 v0, JkVec3 v1, JkVec3 v2)
{
    return (JkIntRect){
        .min.x = JK_MIN3(v0.x, v1.x, v2.x),
        .min.y = JK_MIN3(v0.y, v1.y, v2.y),
        .max.x = jk_ceil_f32(JK_MAX3(v0.x, v1.x, v2.x)),
        .max.y = jk_ceil_f32(JK_MAX3(v0.y, v1.y, v2.y)),
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

static void triangle_fill(Environment *env, TriangleNode *node, JkIntRect bounding_box)
{
    Triangle tri = node->tri;
    JkIntRect bounds = jk_int_rect_intersect(
            bounding_box, triangle_bounding_box(tri.v[0], tri.v[1], tri.v[2]));
    if (!(bounds.min.x < bounds.max.x && bounds.min.y < bounds.max.y)) {
        return;
    }
    bounds.min.x &= ~(8 - 1);
    JkVec2 tex_float_dimensions = jk_vec2_from_int(node->texture.dimensions);

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
                JkF32x8 outside_triangle =
                        jk_f32x8_to_mask(jk_f32x8_or(interpolants.e[I_BARYCENTRIC_0],
                                jk_f32x8_or(interpolants.e[I_BARYCENTRIC_1],
                                        interpolants.e[I_BARYCENTRIC_2])));
                if (!jk_f32x8_all(outside_triangle)) {
                    int32_t index = PIXEL_COUNT * sample_index + DRAW_BUFFER_SIDE_LENGTH * y + x;
                    JkF32x8 z_buffer = jk_f32x8_load(env->z_buffer + index);
                    JkF32x8 in_front = jk_f32x8_less_than(z_buffer, interpolants.e[I_Z]);
                    JkF32x8 visible = jk_f32x8_andnot(outside_triangle, in_front);
                    if (jk_f32x8_any(visible)) {
                        jk_f32x8_store(env->z_buffer + index,
                                jk_f32x8_blend(z_buffer, interpolants.e[I_Z], visible));
                        JkF32x8 u = jk_f32x8_div(interpolants.e[I_U], interpolants.e[I_Z]);
                        JkF32x8 v = jk_f32x8_div(interpolants.e[I_V], interpolants.e[I_Z]);
                        JkF32x8 tex_x = jk_f32x8_mul(jk_f32x8_broadcast(tex_float_dimensions.x),
                                jk_f32x8_sub(u, jk_f32x8_floor(u)));
                        JkF32x8 tex_y = jk_f32x8_mul(jk_f32x8_broadcast(tex_float_dimensions.y),
                                jk_f32x8_sub(v, jk_f32x8_floor(v)));

                        JkF32x8 tex_index = jk_f32x8_mul(
                                jk_f32x8_broadcast(tex_float_dimensions.x), jk_f32x8_floor(tex_y));
                        tex_index = jk_f32x8_add(tex_index, tex_x);

                        JkF32x8 color_buffer = jk_f32x8_load((float *)(env->draw_buffer + index));
                        JkF32x8 color = jk_f32x8_gather(node->texture.memory,
                                jk_truncate_f32x8_to_i32x8(tex_index),
                                visible);
                        jk_f32x8_store((float *)(env->draw_buffer + index),
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

static Bitmap bitmap_from_span(Environment *env, BitmapSpan span)
{
    return (Bitmap){
        .dimensions = span.dimensions,
        .memory = (JkColor *)((uint8_t *)env->assets + span.offset),
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

static void move_against_box(Move *move, JkMat4 world_matrix, ObjectId id)
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
        float p1_dot = jk_vec3_dot(planes[projection_plane].normal,
                jk_vec3_sub(move->s.p1, planes[projection_plane].point));
        if (p1_dot < 0) {
            move->s.p1 =
                    jk_vec3_add(move->s.p1, jk_vec3_mul(-p1_dot, planes[projection_plane].normal));
        }

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

typedef struct TextLayout {
    JkVec2 offset;
    JkVec2 dimensions;
} TextLayout;

static TextLayout text_layout_monospace(Environment *env, JkBuffer text, float scale)
{
    TextLayout result = {0};

    result.offset.x = 0;
    result.dimensions.x = env->assets->font_monospace_advance_width * text.size;

    // Layout looks more natural if we weight the descenders less than ascenders when considering
    // font "height". This is probably because we imagine the descenders dipping below the baseline.
    float descent = 0.4 * env->assets->font_descent;

    result.offset.y = -env->assets->font_ascent;
    result.dimensions.y = descent - env->assets->font_ascent;

    result.offset = jk_vec2_mul(scale, result.offset);
    result.dimensions = jk_vec2_mul(scale, result.dimensions);

    return result;
}

static void draw_text(
        JkShapesRenderer *renderer, JkBuffer text, JkVec2 cursor, float scale, JkColor color)
{
    for (int64_t i = 0; i < text.size; i++) {
        cursor.x += jk_shapes_draw(
                renderer, text.data[i] + ASCII_TO_SHAPE_OFFSET, cursor, scale, color);
    }
}

void render(JkContext *context, Environment *env)
{
    jk_context = context;

    static JkIntRect volatile tiles_rect_shared;
    static TileArray volatile tiles_shared;

    _Alignas(64) static int32_t volatile next_tile_index;

    JkArenaScope scratch = {0};
    JkArenaScope triangle_scratch = {0};
    Input input = {0};
    JkIntVec2 dimensions = {0};

    JK_CHANNEL_NARROW(0)
    {
        input = env->input;

        if (jk_key_pressed(&input.keyboard, JK_KEY_1)) {
            env->record_state = (env->record_state + 1) % RECORD_STATE_COUNT;

            switch (env->record_state) {
            case RECORD_STATE_NONE: {
            } break;

            case RECORD_STATE_RECORDING: {
                env->recording->initial = env->state;
                env->recording->count = 0;
            } break;

            case RECORD_STATE_PLAYBACK: {
                env->playback_cursor = 0;
            } break;

            case RECORD_STATE_COUNT: {
                jk_log(JK_LOG_WARNING, JKS("Invalid env->record_state"));
            } break;
            }
        }

        if (jk_key_pressed(&input.keyboard, JK_KEY_F3)) {
            JK_FLAG_SET(env->flags,
                    ENV_FLAG_DEBUG_DISPLAY,
                    !JK_FLAG_GET(env->flags, ENV_FLAG_DEBUG_DISPLAY));
        }

        switch (env->record_state) {
        case RECORD_STATE_NONE: {
        } break;

        case RECORD_STATE_RECORDING: {
            if (env->recording->count < JK_ARRAY_COUNT(env->recording->inputs)) {
                env->recording->inputs[env->recording->count++] = input;
            }
        } break;

        case RECORD_STATE_PLAYBACK: {
            if (env->playback_cursor == 0) {
                env->state = env->recording->initial;
            }
            input = env->recording->inputs[env->playback_cursor];
            env->playback_cursor = (env->playback_cursor + 1) % env->recording->count;
        } break;

        case RECORD_STATE_COUNT: {
            jk_log(JK_LOG_WARNING, JKS("Invalid env->record_state"));
        } break;
        }

        if (jk_key_pressed(&input.keyboard, JK_KEY_R)) {
            JK_FLAG_SET(env->state.flags, FLAG_INITIALIZED, 0);
        }

        if (jk_key_pressed(&input.keyboard, JK_KEY_T)) {
            JK_FLAG_SET(env->state.flags, FLAG_INITIALIZED, 0);
            env->state.test_frames_remaining = 240;
        }

        if (!JK_FLAG_GET(env->state.flags, FLAG_INITIALIZED)) {
            env->state.flags = JK_MASK(FLAG_INITIALIZED);
            env->state.frame_id = 0;
            env->state.camera_yaw = camera_rot_angle_init;
            env->state.camera_pitch = 0;
            env->state.camera_position = jk_vec3_to_2(camera_position_init);

            jk_profile_reset();
        }

        dimensions =
                0 < env->state.test_frames_remaining ? (JkIntVec2){1902, 970} : input.dimensions;

        JkVec3Array vertices;
        JK_ARRAY_FROM_SPAN(vertices, env->assets, env->assets->vertices);
        JkVec2Array texcoords;
        JK_ARRAY_FROM_SPAN(texcoords, env->assets, env->assets->texcoords);
        ObjectArray objects;
        JK_ARRAY_FROM_SPAN(objects, env->assets, env->assets->objects);

        jk_profile_frame_begin();

        if (env->state.test_frames_remaining <= 0) {
            float mouse_sensitivity = 0.4 * DELTA_TIME;
            env->state.camera_yaw +=
                    jk_remainder_f32(mouse_sensitivity * -input.mouse.delta.x, 2 * JK_PI);
            env->state.camera_pitch =
                    JK_CLAMP(env->state.camera_pitch + mouse_sensitivity * -input.mouse.delta.y,
                            -JK_PI / 2,
                            JK_PI / 2);
        }

        JkVec4 yaw_quat = jk_quat_angle_axis(env->state.camera_yaw, (JkVec3){0, 0, 1});

        if (env->state.test_frames_remaining <= 0) {
            JkVec3 camera_move = {0};
            if (jk_key_down(&input.keyboard, JK_KEY_W)) {
                camera_move.y += 1;
            }
            if (jk_key_down(&input.keyboard, JK_KEY_S)) {
                camera_move.y -= 1;
            }
            if (jk_key_down(&input.keyboard, JK_KEY_A)) {
                camera_move.x -= 1;
            }
            if (jk_key_down(&input.keyboard, JK_KEY_D)) {
                camera_move.x += 1;
            }
            camera_move = jk_vec3_mul(
                    5 * DELTA_TIME, jk_vec3_normalized(jk_quat_rotate(yaw_quat, camera_move)));

            JkVec3 move_start = jk_vec2_to_3(env->state.camera_position, camera_position_init.z);
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

                    move_against_box(&move, world_matrix, object_id);
                }
            } while (!jk_vec3_equal(prev_p1, move.s.p1, 0.0001));
            env->state.camera_position = jk_vec3_to_2(move.s.p1);
        }

        JkTransform camera_transform = {
            .translation = jk_vec2_to_3(env->state.camera_position, camera_position_init.z),
            .rotation = jk_quat_mul(
                    yaw_quat, jk_quat_angle_axis(env->state.camera_pitch, (JkVec3){1, 0, 0})),
            .scale = {1, 1, 1},
        };

        float near_clip = 0.2f;
        JkMat4 clip_space_matrix = jk_transform_to_mat4_inv(camera_transform);
        clip_space_matrix = jk_mat4_mul(
                jk_mat4_conversion_to((JkCoordinateSystem){JK_RIGHT, JK_UP, JK_BACKWARD}),
                clip_space_matrix);
        clip_space_matrix = jk_mat4_mul(
                jk_mat4_perspective(dimensions, JK_PI / 3, near_clip), clip_space_matrix);

        JkMat4 pixel_matrix = jk_mat4_translate((JkVec3){1, -1, 0});
        pixel_matrix =
                jk_mat4_mul(jk_mat4_scale((JkVec3){dimensions.x / 2.0f, -dimensions.y / 2.0f, 1}),
                        pixel_matrix);

        scratch = jk_arena_scratch_begin();
        triangle_scratch = jk_arena_scratch_begin_not(scratch.arena);

        JkIntRect tiles_rect;
        tiles_rect.min = (JkIntVec2){0};
        for (int64_t i = 0; i < 2; i++) {
            tiles_rect.max.v[i] = JK_ALIGN_UP(dimensions.v[i], TILE_SIDE_LENGTH) / TILE_SIDE_LENGTH;
        }
        TileArray tiles;
        tiles.count = tiles_rect.max.x * tiles_rect.max.y;
        tiles.e = jk_arena_push_zero(scratch.arena, sizeof(*tiles.e) * tiles.count);

        tiles_rect_shared = tiles_rect;
        tiles_shared = tiles;

        for (ObjectId object_id = {1}; object_id.i < objects.count; object_id.i++) {
            Object *object = objects.e + object_id.i;
            JkArenaScope object_scope = jk_arena_scope_begin(scratch.arena);

            JkMat4 world_matrix = jk_mat4_i;
            for (ObjectId parent_id = object_id; parent_id.i;
                    parent_id = objects.e[parent_id.i].parent) {
                Object *parent = objects.e + parent_id.i;
                world_matrix = jk_mat4_mul(jk_transform_to_mat4(parent->transform), world_matrix);
            }

            JkVec3 *world_vertices =
                    jk_arena_push(scratch.arena, vertices.count * JK_SIZEOF(*world_vertices));
            for (int64_t i = 0; i < vertices.count; i++) {
                world_vertices[i] = jk_mat4_mul_point(world_matrix, vertices.e[i]);
            }

            JkVec4 *clip_space_vertices =
                    jk_arena_push(scratch.arena, vertices.count * JK_SIZEOF(*clip_space_vertices));
            for (int64_t i = 0; i < vertices.count; i++) {
                clip_space_vertices[i] =
                        jk_mat4_mul_vec4(clip_space_matrix, jk_vec3_to_4(world_vertices[i], 1));
            }

            FaceArray faces;
            JK_ARRAY_FROM_SPAN(faces, env->assets, object->faces);
            Bitmap texture = bitmap_from_span(env, object->texture);

            for (int64_t face_index = 0; face_index < faces.count; face_index++) {
                JkArenaScope face_scope = jk_arena_scope_begin(scratch.arena);
                Face face = faces.e[face_index];

                JkVec2 uv[3];
                if (object->repeat_size) {
                    JkVec3 local_points[3];
                    for (int64_t i = 0; i < 3; i++) {
                        local_points[i] = jk_vec3_mul(1 / object->repeat_size,
                                jk_vec3_hadamard_prod(
                                        vertices.e[face.v[i]], object->transform.scale));
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
                TexturedVertexArray vs = {.e = jk_arena_pointer_current(scratch.arena)};
                for (int64_t i = 0; i < 3; i++) {
                    int64_t b_i = (i + 1) % 3;
                    JkVec4 a = clip_space_vertices[face.v[i]];
                    JkVec4 b = clip_space_vertices[face.v[b_i]];
                    b32 a_inside = !!(a.z < a.w);
                    b32 b_inside = !!(b.z < b.w);
                    if (a_inside != b_inside) { // Crosses clip plane, add interpolated vertex
                        float t = (near_clip - a.w) / (b.w - a.w);
                        add_textured_vertex(scratch.arena,
                                pixel_matrix,
                                jk_vec4_lerp(a, b, t),
                                jk_vec2_lerp(uv[i], uv[b_i], t));
                    }
                    if (b_inside) {
                        add_textured_vertex(scratch.arena, pixel_matrix, b, uv[b_i]);
                    }
                }
                vs.count = (TexturedVertex *)jk_arena_pointer_current(scratch.arena) - vs.e;

                // Triangulate the resulting polygon
                for (int64_t vertex_index = 2; vertex_index < vs.count; vertex_index++) {
                    int64_t indexes[3] = {0, vertex_index - 1, vertex_index};
                    Triangle tri;
                    for (int64_t i = 0; i < 3; i++) {
                        tri.v[i] = vs.e[indexes[i]].v;
                        tri.t[i] = vs.e[indexes[i]].t;
                    }
                    if (clockwise(tri)) {
                        JkVec3 tile_coords[3];
                        for (int64_t i = 0; i < 3; i++) {
                            tile_coords[i] = jk_vec3_mul(1.0f / TILE_SIDE_LENGTH, tri.v[i]);
                        }
                        JkIntRect coverage = jk_int_rect_intersect(
                                triangle_bounding_box(
                                        tile_coords[0], tile_coords[1], tile_coords[2]),
                                tiles_rect);
                        for (int32_t y = coverage.min.y; y < coverage.max.y; y++) {
                            for (int32_t x = coverage.min.x; x < coverage.max.x; x++) {
                                Tile *tile = tiles.e + (tiles_rect.max.x * y + x);

                                TriangleNode *new_node =
                                        jk_arena_push(triangle_scratch.arena, sizeof(*new_node));
                                new_node->tri = tri;
                                new_node->texture = texture;
                                new_node->next = tile->head;
                                tile->head = new_node;
                            }
                        }
                    }
                }

                jk_arena_scope_end(face_scope);
            }

            jk_arena_scope_end(object_scope);
        }

        next_tile_index = 0;
    }

    jk_channel_sync();

    JkI256 bg = jk_i256_broadcast_i32(
            *(int32_t *)(0 < env->state.test_frames_remaining ? &test_bg : &normal_bg));

    {
        JkIntRect tiles_rect = tiles_rect_shared;
        TileArray tiles = tiles_shared;
        int32_t tile_index;
        while ((tile_index = jk_atomic_add(&next_tile_index, 1)) < tiles.count) {
            Tile *tile = tiles.e + tile_index;

            JkIntVec2 tile_coord = {tile_index % tiles_rect.max.x, tile_index / tiles_rect.max.x};
            JkIntRect bounding_box;
            for (int64_t i = 0; i < 2; i++) {
                bounding_box.min.v[i] = TILE_SIDE_LENGTH * tile_coord.v[i];
                bounding_box.max.v[i] = bounding_box.min.v[i] + TILE_SIDE_LENGTH;
            }

            for (int32_t sample_index = 0; sample_index < SAMPLE_COUNT; sample_index++) {
                for (int32_t y = bounding_box.min.y; y < bounding_box.max.y; y++) {
                    for (int32_t x = bounding_box.min.x; x < bounding_box.max.x; x += 8) {
                        int32_t pixel_index =
                                PIXEL_COUNT * sample_index + DRAW_BUFFER_SIDE_LENGTH * y + x;
                        jk_i256_store(env->draw_buffer + pixel_index, bg);
                        jk_f32x8_store(env->z_buffer + pixel_index, jk_f32x8_zero());
                    }
                }
            }

            for (TriangleNode *node = tile->head; node; node = node->next) {
                triangle_fill(env, node, bounding_box);
            }

            for (int32_t y = bounding_box.min.y; y < bounding_box.max.y; y++) {
                for (int32_t x = bounding_box.min.x; x < bounding_box.max.x; x += 8) {
                    JkI256 channels[3] = {jk_i256_zero(), jk_i256_zero(), jk_i256_zero()};
                    for (int64_t sample_index = 0; sample_index < SAMPLE_COUNT; sample_index++) {
                        JkI256 sample = jk_i256_load(env->draw_buffer
                                + (PIXEL_COUNT * sample_index + DRAW_BUFFER_SIDE_LENGTH * y + x));
                        JkI256 byte_mask = jk_i256_broadcast_i32(0xff);
                        channels[0] = jk_i256_add_i32(channels[0], jk_i256_and(sample, byte_mask));
                        channels[1] = jk_i256_add_i32(channels[1],
                                jk_i256_and(
                                        JK_I256_SHIFT_RIGHT_SIGN_FILL_I32(sample, 8), byte_mask));
                        channels[2] = jk_i256_add_i32(channels[2],
                                jk_i256_and(
                                        JK_I256_SHIFT_RIGHT_SIGN_FILL_I32(sample, 16), byte_mask));
                    }
                    for (int32_t channel_index = 0; channel_index < 3; channel_index++) {
                        channels[channel_index] =
                                JK_I256_SHIFT_RIGHT_SIGN_FILL_I32(channels[channel_index], 2);
                    }

                    JkI256 color = channels[0];
                    color = jk_i256_or(color, JK_I256_SHIFT_LEFT_I32(channels[1], 8));
                    color = jk_i256_or(color, JK_I256_SHIFT_LEFT_I32(channels[2], 16));

                    jk_i256_store(env->draw_buffer + (DRAW_BUFFER_SIDE_LENGTH * y + x), color);
                }
            }
        }
    }

    if (jk_context->channel.index == 0) {
        JK_FLAG_SET(env->flags, ENV_FLAG_RUNNING, !env->shutdown_requested);
    }
    jk_channel_sync();

    JK_CHANNEL_NARROW(0)
    {
        jk_profile_frame_end();

        if (jk_key_pressed(&input.keyboard, JK_KEY_P)) {
            JK_ARENA_SCRATCH(log_scratch)
            {
                jk_log(JK_LOG_INFO,
                        jk_profile_report(log_scratch.arena, env->estimate_cpu_frequency(100)));
            }
        }

        if (0 < env->state.test_frames_remaining) {
            if (--env->state.test_frames_remaining == 0) {
                JK_ARENA_SCRATCH(log_scratch)
                {
                    jk_log(JK_LOG_INFO,
                            jk_profile_report(log_scratch.arena, env->estimate_cpu_frequency(100)));
                }
            }
        }

        if (JK_FLAG_GET(env->flags, ENV_FLAG_DEBUG_DISPLAY)) {
            JkShapesRenderer renderer;
            JkShapeArray shapes = (JkShapeArray){
                .count = JK_ARRAY_COUNT(env->assets->shapes), .e = env->assets->shapes};
            float pixels_per_unit = JK_MIN(dimensions.x, dimensions.y) / 64.0f;
            JkVec2 ui_dimensions =
                    jk_vec2_mul(1.0f / pixels_per_unit, jk_vec2_from_int(dimensions));
            jk_shapes_renderer_init(&renderer, pixels_per_unit, env->assets, shapes, scratch.arena);

            float padding = 0.5f;
            float text_scale = 0.005f;
            JkBuffer frame_id_text = JK_FORMAT(scratch.arena, jkfu(env->state.frame_id));
            TextLayout layout = text_layout_monospace(env, frame_id_text, text_scale);
            JkVec2 top_left = {(ui_dimensions.x - padding) - layout.dimensions.x, padding};
            draw_text(&renderer,
                    frame_id_text,
                    jk_vec2_add(top_left, layout.offset),
                    text_scale,
                    (JkColor){255, 255, 255, 255});

            JkShapesDrawCommandArray draw_commands = jk_shapes_draw_commands_get(&renderer);
            JkIntRect screen_rect = {.max = dimensions};
            for (int64_t i = 0; i < draw_commands.count; i++) {
                JkShapesDrawCommand *command = draw_commands.e + i;
                JkIntRect rect = jk_int_rect_intersect(screen_rect, command->rect);
                for (JkIntVec2 pos = rect.min; pos.y < rect.max.y; pos.y++) {
                    for (pos.x = rect.min.x; pos.x < rect.max.x; pos.x++) {
                        int32_t index = DRAW_BUFFER_SIDE_LENGTH * pos.y + pos.x;
                        uint8_t alpha = 255;
                        if (command->alpha_map) {
                            JkIntVec2 pos_in_rect = jk_int_vec2_sub(pos, command->rect.min);
                            int32_t width = (command->rect.max.x - command->rect.min.x);
                            alpha = command->alpha_map[pos_in_rect.y * width + pos_in_rect.x];
                        }
                        env->draw_buffer[index] =
                                blend_alpha(command->color, env->draw_buffer[index], alpha);
                    }
                }
            }
        }

        jk_arena_scope_end(scratch);
        jk_arena_scope_end(triangle_scratch);
        env->state.frame_id++;
    }
}
