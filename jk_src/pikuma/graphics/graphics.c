#include "graphics.h"

// #jk_build library
// #jk_build export render
// #jk_build single_translation_unit

// #jk_build dependencies_begin
#include <jk_src/jk_lib/jk_lib.h>
#include <jk_src/jk_shapes/jk_shapes.h>
// #jk_build dependencies_end

#define NEAR_CLIP 0.2f

#define EPSILON 0x1.51b717p-14f

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

static uint8_t color_multiply(uint8_t a, uint8_t b)
{
    return ((uint32_t)a * (uint32_t)b) / 255;
}

static void plot(JkColor *draw_buffer, JkColor color, int32_t x, int32_t y, float brightness)
{
    int32_t brightness_i = (int32_t)(brightness * 255.0f);
    if (brightness_i > 255) {
        brightness_i = 255;
    }
    draw_buffer[y * DRAW_BUFFER_SIDE_LENGTH + x] = jk_color_alpha_blend(color,
            draw_buffer[y * DRAW_BUFFER_SIDE_LENGTH + x],
            color_multiply(color.a, (uint8_t)brightness_i));
}

static void draw_line(Environment *env, JkColor color, JkVec2 a, JkVec2 b)
{
    if (!clip_to_draw_region(env->input.dimensions, &a, &b)) {
        return;
    }

    JkColor *draw_buffer = env->draw_buffer;

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

typedef struct Q16Triangle {
    JkQ16Vec2 v[3];
    int32_t z[3];
} Q16Triangle;

typedef struct TexturedTriangle {
    JkVec3 v[3];
    JkVec2 t[3];
} TexturedTriangle;

typedef struct TriangleArray {
    int64_t count;
    TexturedTriangle *e;
} TriangleArray;

typedef struct TriangleNode {
    struct TriangleNode *next;
    Bitmap texture;
    TexturedTriangle tri;
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

static JkIntRect q16_triangle_bounding_box(Q16Triangle tri)
{
    return (JkIntRect){
        .min.x = jk_q16_to_i32(JK_MIN3(tri.v[0].x, tri.v[1].x, tri.v[2].x)),
        .min.y = jk_q16_to_i32(JK_MIN3(tri.v[0].y, tri.v[1].y, tri.v[2].y)),
        .max.x = jk_q16_to_i32(JK_MAX3(tri.v[0].x, tri.v[1].x, tri.v[2].x)) + 1,
        .max.y = jk_q16_to_i32(JK_MAX3(tri.v[0].y, tri.v[1].y, tri.v[2].y)) + 1,
    };
}

static JkIntRect triangle_bounding_box(JkVec3 v0, JkVec3 v1, JkVec3 v2)
{
    return (JkIntRect){
        .min.x = jk_floor_f32(JK_MIN3(v0.x, v1.x, v2.x)),
        .min.y = jk_floor_f32(JK_MIN3(v0.y, v1.y, v2.y)),
        .max.x = jk_floor_f32(JK_MAX3(v0.x, v1.x, v2.x)) + 1,
        .max.y = jk_floor_f32(JK_MAX3(v0.y, v1.y, v2.y)) + 1,
    };
}

static b32 clockwise_left_handed(JkVec3 v0, JkVec3 v1, JkVec3 v2)
{
    return (v1.x - v0.x) * (v2.y - v0.y) - (v1.y - v0.y) * (v2.x - v0.x) > 0;
}

typedef struct NavRing {
    struct NavRing *neighbors[4];
    int64_t vertex_count;
    JkVec3 vertices[8];
} NavRing;

typedef struct NavRingArray {
    int64_t count;
    NavRing *e;
} NavRingArray;

typedef enum NavContactFlag {
    NAV_CONTACT_UP,
    NAV_CONTACT_FLAG_COUNT,
} NavContactFlag;

typedef struct NavContact {
    struct NavContact *next;

    uint32_t flags;
    int32_t z;
    NavRing *rings[4];
} NavContact;

typedef struct NavContacts {
    NavContact *e[4];
} NavContacts;

static JK_READONLY NavRing nil_ring = {
    .neighbors = {&nil_ring, &nil_ring, &nil_ring, &nil_ring},
};
static JK_READONLY NavContact nil_contact = {
    .next = &nil_contact,
    .rings = {&nil_ring, &nil_ring, &nil_ring, &nil_ring},
    .z = INT32_MIN,
};

static b32 nav_contacts_any(NavContacts contacts)
{
    b32 result = 0;
    for (int64_t i = 0; i < 4; i++) {
        if (contacts.e[i] != &nil_contact) {
            result = 1;
        }
    }
    return result;
}

typedef enum NavInterpolant {
    NAV_BARYCENTRIC_0,
    NAV_BARYCENTRIC_1,
    NAV_BARYCENTRIC_2,
    NAV_Z,
    NAV_INTERPOLANT_COUNT,
} NavInterpolant;

typedef struct NavInterpolants {
    int32_t e[NAV_INTERPOLANT_COUNT];
} NavInterpolants;

static void nav_triangle_rasterize(
        JkArena *arena, NavContact **nav_contacts, JkIntVec2 nav_dimensions, Q16Triangle tri)
{
    JkIntRect b = (JkIntRect){.max = nav_dimensions};
    JkIntRect bounds = jk_int_rect_intersect(b, q16_triangle_bounding_box(tri));
    if (!(bounds.min.x < bounds.max.x && bounds.min.y < bounds.max.y)) {
        return;
    }

    uint32_t flags = 0;
    int32_t up = jk_q16_vec2_cross(
            jk_q16_vec2_sub(tri.v[1], tri.v[0]), jk_q16_vec2_sub(tri.v[2], tri.v[0]));
    if (up < 0) {
        JK_SWAP(tri.v[1], tri.v[2], JkQ16Vec2);
    } else if (0 < up) {
        JK_FLAG_SET(flags, NAV_CONTACT_UP, 1);
    } else {
        return;
    }

    // Initialize interpolants and their deltas
    int32_t barycentric_divisor = 0;
    NavInterpolants deltas[2] = {0};
    NavInterpolants interpolants_row = {0};
    for (int64_t i = 0; i < 3; i++) {
        int64_t a = (i + 1) % 3;
        int64_t b = (i + 2) % 3;
        int32_t cross = jk_q16_vec2_cross(tri.v[a], tri.v[b]);
        barycentric_divisor += cross;
        int32_t x_delta = tri.v[a].y - tri.v[b].y;
        int32_t y_delta = tri.v[b].x - tri.v[a].x;
        deltas[0].e[NAV_BARYCENTRIC_0 + i] = x_delta;
        deltas[1].e[NAV_BARYCENTRIC_0 + i] = y_delta;
        b32 top = tri.v[b].x < tri.v[a].x && tri.v[a].y == tri.v[b].y;
        b32 left = tri.v[b].y < tri.v[a].y;
        int32_t bias = (top || left) ? 0 : -1;
        interpolants_row.e[NAV_BARYCENTRIC_0 + i] =
                (x_delta * bounds.min.x) + (y_delta * bounds.min.y) + cross + bias;
    }
    if (barycentric_divisor == 0) {
        return;
    }
    for (int64_t i = 0; i < 3; i++) {
        for (int64_t axis_index = 0; axis_index < 2; axis_index++) {
            deltas[axis_index].e[NAV_Z] +=
                    jk_q16_div(jk_q16_mul(deltas[axis_index].e[NAV_BARYCENTRIC_0 + i], tri.z[i]),
                            barycentric_divisor);
        }
        interpolants_row.e[NAV_Z] +=
                jk_q16_div(jk_q16_mul(interpolants_row.e[NAV_BARYCENTRIC_0 + i], tri.z[i]),
                        barycentric_divisor);
    }

    for (int32_t y = bounds.min.y; y < bounds.max.y; y++) {
        NavInterpolants interpolants = interpolants_row;
        for (int32_t x = bounds.min.x; x < bounds.max.x; x++) {
            if (0 <= (interpolants.e[NAV_BARYCENTRIC_0] | interpolants.e[NAV_BARYCENTRIC_1]
                        | interpolants.e[NAV_BARYCENTRIC_2])) {
                NavContact *contact = jk_arena_push(arena, sizeof(*contact));
                *contact = nil_contact;
                contact->flags = flags;
                contact->z = interpolants.e[NAV_Z];

                NavContact **link = nav_contacts + (nav_dimensions.x * y + x);
                while (*link && contact->z < (*link)->z) {
                    link = &(*link)->next;
                }

                contact->next = *link;
                *link = contact;
            }

            for (int64_t i = 0; i < NAV_INTERPOLANT_COUNT; i++) {
                interpolants.e[i] += deltas[0].e[i];
            }
        }

        for (int64_t i = 0; i < NAV_INTERPOLANT_COUNT; i++) {
            interpolants_row.e[i] += deltas[1].e[i];
        }
    }
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
    TexturedTriangle tri = node->tri;
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

static Bitmap bitmap_from_span(Environment *env, BitmapSpan span)
{
    return (Bitmap){
        .dimensions = span.dimensions,
        .memory = (JkColor *)((uint8_t *)env->assets + span.offset),
    };
}

static void add_textured_vertex(JkArena *arena, JkMat4 screen_from_ndc, JkVec4 v, JkVec2 t)
{
    TexturedVertex *new = jk_arena_push(arena, sizeof(*new));
    new->v = jk_mat4_mul_point(screen_from_ndc, jk_vec4_perspective_divide(v));
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

static void move_against_box(Move *move, JkMat4 world_from_local, ObjectId id)
{
    static float const padding = 0.5;

    static JkVec3 const local_extents[3] = {
        {0.5, 0, 0},
        {0, 0.5, 0},
        {0, 0, 0.5},
    };

    JkVec3 origin = jk_mat4_mul_point(world_from_local, (JkVec3){0, 0, 0});

    // Compute all 6 planes
    Plane planes[6];
    for (int64_t extent = 0; extent < 3; extent++) {
        JkVec3 transformed = jk_mat4_mul_normal(world_from_local, local_extents[extent]);
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

static JkMat4 object_compute_world_from_local(ObjectArray objects, ObjectId id)
{
    JkMat4 result = jk_mat4_i;
    for (ObjectId parent_id = id; parent_id.i; parent_id = objects.e[parent_id.i].parent) {
        Object *parent = objects.e + parent_id.i;
        result = jk_mat4_mul(jk_transform_to_mat4(parent->transform), result);
    }
    return result;
}

typedef struct ScreenFromWorldResult {
    b32 clipped;
    JkVec3 v;
} ScreenFromWorldResult;

static void draw_world_segment(Environment *env,
        JkMat4 screen_from_ndc,
        JkMat4 clip_from_world,
        JkColor color,
        JkVec3 a,
        JkVec3 b)
{
    JkVec4 clip[2] = {
        jk_mat4_mul_vec4(clip_from_world, jk_vec3_to_4(a, 1)),
        jk_mat4_mul_vec4(clip_from_world, jk_vec3_to_4(b, 1)),
    };
    b32 inside[2];
    for (int64_t i = 0; i < 2; i++) {
        inside[i] = !!(clip[i].z < clip[i].w);
    }
    if (!inside[0]) {
        if (!inside[1]) {
            return;
        }
        JK_SWAP(clip[0], clip[1], JkVec4);
    }
    if (inside[0] != inside[1]) { // Crosses clip plane, add interpolated vertex
        float t = (NEAR_CLIP - clip[0].w) / (clip[1].w - clip[0].w);
        clip[1] = jk_vec4_lerp(clip[0], clip[1], t);
    }
    JkVec2 screen[2];
    for (int64_t i = 0; i < 2; i++) {
        screen[i] = jk_vec3_to_2(
                jk_mat4_mul_point(screen_from_ndc, jk_vec4_perspective_divide(clip[i])));
    }
    draw_line(env, color, screen[0], screen[1]);
}

void render(JkContext *context, Environment *env)
{
    jk_context = context;

    static JkIntRect volatile tiles_rect_shared;
    static TileArray volatile tiles_shared;

    _Alignas(64) static int32_t volatile next_tile_index;

    JkArenaScope scratch0 = {0};
    JkArenaScope scratch1 = {0};
    Input input = {0};
    JkIntVec2 dimensions = {0};

    JkMat4 clip_from_world = jk_mat4_i;
    JkMat4 screen_from_ndc = jk_mat4_i;

    float nav_density = 0.25f;
    int32_t nav_step_height = jk_q16_from_f32(0.75f);
    int32_t nav_height = jk_q16_from_f32(1.875f);
    JkIntVec2 nav_dimensions = {32, 32};
    JkVec2 nav_origin = jk_vec2_sub(jk_vec2_mul(1 / nav_density, env->state.camera_position),
            jk_vec2_mul(0.5, jk_vec2_from_int(nav_dimensions)));
    nav_origin.x = jk_floor_f32(nav_origin.x);
    nav_origin.y = jk_floor_f32(nav_origin.y);
    NavContact **nav_contacts = 0;
    NavRingArray nav_rings = {0};

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
                    if (JK_FLAG_GET(objects.e[object_id.i].flags, OBJ_COLLIDE)) {
                        JkMat4 world_from_local =
                                object_compute_world_from_local(objects, object_id);

                        move_against_box(&move, world_from_local, object_id);
                    }
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

        clip_from_world = jk_transform_to_mat4_inv(camera_transform);
        clip_from_world = jk_mat4_mul(
                jk_mat4_conversion_to((JkCoordinateSystem){JK_RIGHT, JK_UP, JK_BACKWARD}),
                clip_from_world);
        clip_from_world =
                jk_mat4_mul(jk_mat4_perspective(dimensions, JK_PI / 3, NEAR_CLIP), clip_from_world);

        JkMat4 nav_from_world = jk_mat4_scale((JkVec3){1 / nav_density, 1 / nav_density, 1});
        nav_from_world = jk_mat4_mul(
                jk_mat4_translate(jk_vec2_to_3(jk_vec2_mul(-1, nav_origin), 0)), nav_from_world);

        screen_from_ndc = jk_mat4_translate((JkVec3){1, -1, 0});
        screen_from_ndc =
                jk_mat4_mul(jk_mat4_scale((JkVec3){dimensions.x / 2.0f, -dimensions.y / 2.0f, 1}),
                        screen_from_ndc);

        scratch0 = jk_arena_scratch_begin();
        scratch1 = jk_arena_scratch_begin_not(scratch0.arena);

        // ---- Navigation begin ----------------------------------------------

        nav_contacts = jk_arena_push(
                scratch0.arena, nav_dimensions.x * nav_dimensions.y * JK_SIZEOF(NavContact *));
        for (int64_t i = 0; i < nav_dimensions.x * nav_dimensions.y; i++) {
            nav_contacts[i] = &nil_contact;
        }

        JkArenaScope nav_rasterize_scope = jk_arena_scope_begin(scratch1.arena);

        // Rasterize in initial set of contacts
        for (ObjectId object_id = {1}; object_id.i < objects.count; object_id.i++) {
            Object *object = objects.e + object_id.i;

            FaceArray faces;
            JK_ARRAY_FROM_SPAN(faces, env->assets, object->faces);

            JkMat4 world_from_local = object_compute_world_from_local(objects, object_id);
            JkMat4 nav_from_local = jk_mat4_mul(nav_from_world, world_from_local);

            JkQ16Vec2 *nav_vertices =
                    jk_arena_push(scratch1.arena, vertices.count * JK_SIZEOF(*nav_vertices));
            int32_t *nav_zs = jk_arena_push(scratch1.arena, vertices.count * JK_SIZEOF(*nav_zs));
            for (int64_t i = 0; i < vertices.count; i++) {
                JkVec3 vert_f32 = jk_mat4_mul_point(nav_from_local, vertices.e[i]);
                nav_vertices[i] = jk_q16_vec2_from_f32(jk_vec3_to_2(vert_f32));
                nav_zs[i] = jk_q16_from_f32(vert_f32.z);
            }

            // Process faces in world space for navigation grid
            for (int64_t face_index = 0; face_index < faces.count; face_index++) {
                Face face = faces.e[face_index];
                Q16Triangle triangle = {0};
                for (int64_t i = 0; i < 3; i++) {
                    triangle.v[i] = nav_vertices[face.v[i]];
                    triangle.z[i] = nav_zs[face.v[i]];
                }
                nav_triangle_rasterize(scratch0.arena, nav_contacts, nav_dimensions, triangle);
            }
        }

        jk_arena_scope_end(nav_rasterize_scope);

        // Remove invalid contacts
        for (int64_t i = 0; i < nav_dimensions.x * nav_dimensions.y; i++) {
            int32_t ceiling = INT32_MAX / 2;
            int64_t solid_count = 0;
            NavContact **link = nav_contacts + i;
            NavContact *contact;
            while ((contact = *link) != &nil_contact) {
                b32 valid = 1;

                valid = valid && solid_count <= 0;
                valid = valid && (solid_count < 0 || nav_height < ceiling - contact->z);
                if (JK_FLAG_GET(contact->flags, NAV_CONTACT_UP)) {
                    solid_count++;
                } else {
                    solid_count--;
                    ceiling = contact->z - jk_q16_from_f32(0.00048828125f);
                    valid = 0;
                }

                if (valid) {
                    link = &contact->next;
                } else {
                    *link = contact->next;
                }
            }
        }

        scratch0.arena->pos = JK_ALIGN_UP(scratch0.arena->pos, 8);
        JkArenaScope ring_scope = jk_arena_scope_begin(scratch0.arena);

        for (JkIntVec2 pos = {0}; pos.y < nav_dimensions.y - 1; pos.y++) {
            for (pos.x = 0; pos.x < nav_dimensions.x - 1; pos.x++) {
                JkVec2 world_pos[4];
                world_pos[0] =
                        jk_vec2_mul(nav_density, jk_vec2_add(jk_vec2_from_int(pos), nav_origin));
                world_pos[1] = jk_vec2_add(world_pos[0], (JkVec2){nav_density, 0});
                world_pos[2] = jk_vec2_add(world_pos[0], (JkVec2){nav_density, nav_density});
                world_pos[3] = jk_vec2_add(world_pos[0], (JkVec2){0, nav_density});

                NavContacts contacts = {
                    nav_contacts[nav_dimensions.x * pos.y + pos.x],
                    nav_contacts[nav_dimensions.x * pos.y + (pos.x + 1)],
                    nav_contacts[nav_dimensions.x * (pos.y + 1) + (pos.x + 1)],
                    nav_contacts[nav_dimensions.x * (pos.y + 1) + pos.x],
                };
                while (nav_contacts_any(contacts)) {
                    NavRing ring = nil_ring;

                    int64_t max_index = 0;
                    for (int64_t i = 1; i < 4; i++) {
                        if (contacts.e[max_index]->z < contacts.e[i]->z) {
                            max_index = i;
                        }
                    }
                    int32_t max_z = contacts.e[max_index]->z;
                    int32_t min_z = max_z - nav_step_height;

                    NavContacts candidates = contacts;
                    b32 found[4] = {0};
                    for (int64_t i = 0; i < 4; i++) {
                        for (; !found[i] && candidates.e[i] != &nil_contact;
                                candidates.e[i] = candidates.e[i]->next) {
                            if (min_z < candidates.e[i]->z && candidates.e[i]->z <= max_z) {
                                found[i] = 1;
                                ring.vertices[ring.vertex_count++] = jk_vec2_to_3(
                                        world_pos[i], jk_q16_to_f32(candidates.e[i]->z));
                            }
                        }
                    }

                    if (2 < ring.vertex_count) {
                        NavRing *new_ring = jk_arena_push(scratch0.arena, sizeof(*new_ring));
                        *new_ring = ring;

                        for (int64_t i = 0; i < 4; i++) {
                            if (found[i]) {
                                contacts.e[i] = candidates.e[i];
                            }
                        }
                    } else {
                        contacts.e[max_index] = contacts.e[max_index]->next;
                    }
                }
            }
        }

        JK_ARRAY_FROM_ARENA_SCOPE(nav_rings, ring_scope);

        // ---- Navigation end ------------------------------------------------

        JkIntRect tiles_rect;
        tiles_rect.min = (JkIntVec2){0};
        for (int64_t i = 0; i < 2; i++) {
            tiles_rect.max.v[i] = JK_ALIGN_UP(dimensions.v[i], TILE_SIDE_LENGTH) / TILE_SIDE_LENGTH;
        }
        TileArray tiles;
        tiles.count = tiles_rect.max.x * tiles_rect.max.y;
        tiles.e = jk_arena_push_zero(scratch0.arena, sizeof(*tiles.e) * tiles.count);

        tiles_rect_shared = tiles_rect;
        tiles_shared = tiles;

        for (ObjectId object_id = {1}; object_id.i < objects.count; object_id.i++) {
            Object *object = objects.e + object_id.i;
            JkArenaScope object_scope = jk_arena_scope_begin(scratch0.arena);

            FaceArray faces;
            JK_ARRAY_FROM_SPAN(faces, env->assets, object->faces);
            Bitmap texture = bitmap_from_span(env, object->texture);

            JkMat4 world_from_local = object_compute_world_from_local(objects, object_id);
            JkMat4 clip_from_local = jk_mat4_mul(clip_from_world, world_from_local);

            // jk_mat4_mul_vec4(clip_from_local, jk_vec3_to_4(vertices.e[i], 1));

            // Clip and bin faces for later rendering
            for (int64_t face_index = 0; face_index < faces.count; face_index++) {
                JkArenaScope face_scope = jk_arena_scope_begin(scratch0.arena);
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
                TexturedVertexArray vs = {.e = jk_arena_pointer_current(scratch0.arena)};
                for (int64_t i = 0; i < 3; i++) {
                    int64_t b_i = (i + 1) % 3;
                    JkVec4 a = jk_mat4_mul_vec4(
                            clip_from_local, jk_vec3_to_4(vertices.e[face.v[i]], 1));
                    JkVec4 b = jk_mat4_mul_vec4(
                            clip_from_local, jk_vec3_to_4(vertices.e[face.v[b_i]], 1));
                    b32 a_inside = !!(a.z < a.w);
                    b32 b_inside = !!(b.z < b.w);
                    if (a_inside != b_inside) { // Crosses clip plane, add interpolated vertex
                        float t = (NEAR_CLIP - a.w) / (b.w - a.w);
                        add_textured_vertex(scratch0.arena,
                                screen_from_ndc,
                                jk_vec4_lerp(a, b, t),
                                jk_vec2_lerp(uv[i], uv[b_i], t));
                    }
                    if (b_inside) {
                        add_textured_vertex(scratch0.arena, screen_from_ndc, b, uv[b_i]);
                    }
                }
                vs.count = (TexturedVertex *)jk_arena_pointer_current(scratch0.arena) - vs.e;

                // Triangulate the resulting polygon
                for (int64_t vertex_index = 2; vertex_index < vs.count; vertex_index++) {
                    int64_t indexes[3] = {0, vertex_index - 1, vertex_index};
                    TexturedTriangle tri;
                    for (int64_t i = 0; i < 3; i++) {
                        tri.v[i] = vs.e[indexes[i]].v;
                        tri.t[i] = vs.e[indexes[i]].t;
                    }
                    if (!clockwise_left_handed(tri.v[0], tri.v[1], tri.v[2])) {
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
                                        jk_arena_push(scratch1.arena, sizeof(*new_node));
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
            for (int64_t ring_index = 0; ring_index < nav_rings.count; ring_index++) {
                NavRing ring = nav_rings.e[ring_index];
                for (int64_t i = 0; i < ring.vertex_count; i++) {
                    draw_world_segment(env,
                            screen_from_ndc,
                            clip_from_world,
                            (JkColor){.r = 0, .g = 80, .b = 0, .a = 255},
                            ring.vertices[i],
                            ring.vertices[(i + 1) % ring.vertex_count]);
                }
            }

            JkShapesRenderer renderer;
            JkShapeArray shapes = (JkShapeArray){
                .count = JK_ARRAY_COUNT(env->assets->shapes), .e = env->assets->shapes};
            float pixels_per_unit = JK_MIN(dimensions.x, dimensions.y) / 64.0f;
            JkVec2 ui_dimensions =
                    jk_vec2_mul(1.0f / pixels_per_unit, jk_vec2_from_int(dimensions));
            jk_shapes_renderer_init(
                    &renderer, pixels_per_unit, env->assets, shapes, scratch0.arena);

            float padding = 0.5f;
            float text_scale = 0.005f;
            JkBuffer frame_id_text = JK_FORMAT(scratch0.arena, jkfu(env->state.frame_id));
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

        jk_arena_scope_end(scratch0);
        jk_arena_scope_end(scratch1);
        env->state.frame_id++;
    }
}
