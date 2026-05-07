#include "graphics.h"

// #jk_build library
// #jk_build export render
// #jk_build single_translation_unit

// #jk_build dependencies_begin
#include <jk_src/jk_lib/jk_lib.h>
#include <jk_src/jk_shapes/jk_shapes.h>
// #jk_build dependencies_end

#define NEAR_CLIP 0.2f
#define SPEED 5.0f

#define Q16_EPSILON (1 << 10)
#define EPSILON 0x1.0p-14
#define SYNTHETIC_OFFSET (1 << 13)

#define NAV_STEP_HEIGHT jk_q16_from_f32(0.75f)
#define NAV_HEIGHT jk_q16_from_f32(1.875f)

static float const nav_density = 0.125f;
static JkIntVec2 const nav_dimensions = {32, 32};

static JkColor normal_bg = {.r = CLEAR_COLOR_R, .g = CLEAR_COLOR_G, .b = CLEAR_COLOR_B, .a = 255};
static JkColor test_bg = {.r = 0x27, .g = 0x27, .b = 0x16, .a = 255};

static float const player_radius = 0.33f;
static float const player_height = 1.75f;
static float const player_eye_height = 1.4f;
static JkVec3 light_dir = {-1, 4, -1};
static int32_t rotation_seconds = 8;

static float sample_offsets[2][SAMPLE_COUNT] = {
    {0.5, 0x0.4p0f, 0x0.Ep0f, 0x0.6p0f},
    {0.5, 0x0.4p0f, 0x0.6p0f, 0x0.Ep0f},
};

_Alignas(32) static float lane_offsets[2][LANE_COUNT] = {
    {0, 1, 2, 3, 4, 5, 6, 7},
    {0, 0, 0, 0, 0, 0, 0, 0},
};

// ---- Xiaolin Wu's line algorithm begin --------------------------------------

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

// ---- Xiaolin Wu's line algorithm end ----------------------------------------

typedef struct Q16Triangle {
    JkQ16Vec3 v[3];
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
    int32_t texture_id;
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

static JkIntRect segment_bounding_box(JkQ16Vec2 v0, JkQ16Vec2 v1, int32_t radius)
{
    JkQ16Vec2 min = {.x = JK_MIN(v0.x, v1.x) - radius, .y = JK_MIN(v0.y, v1.y) - radius};
    JkQ16Vec2 max = {.x = JK_MAX(v0.x, v1.x) + radius, .y = JK_MAX(v0.y, v1.y) + radius};
    return (JkIntRect){
        .min = {jk_i32_from_q16_floor(min.x), jk_i32_from_q16_floor(min.y)},
        .max = {jk_i32_from_q16_ceil(max.x) + 1, jk_i32_from_q16_ceil(max.y) + 1},
    };
}

static JkIntRect q16_triangle_bounding_box(Q16Triangle tri)
{
    return (JkIntRect){
        .min.x = jk_i32_from_q16_ceil(JK_MIN3(tri.v[0].x, tri.v[1].x, tri.v[2].x)),
        .min.y = jk_i32_from_q16_ceil(JK_MIN3(tri.v[0].y, tri.v[1].y, tri.v[2].y)),
        .max.x = jk_i32_from_q16_floor(JK_MAX3(tri.v[0].x, tri.v[1].x, tri.v[2].x)) + 1,
        .max.y = jk_i32_from_q16_floor(JK_MAX3(tri.v[0].y, tri.v[1].y, tri.v[2].y)) + 1,
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

typedef struct NavRing NavRing;
typedef struct NavContact NavContact;

typedef enum NavContactFlag {
    NAV_CONTACT_UP,
    NAV_CONTACT_FLAG_COUNT,
} NavContactFlag;

struct NavContact {
    struct NavContact *next;

    uint32_t flags;
    int32_t z;
    NavRing *rings[4]; // 0 top-left, 1 top-right, 2 bottom-right, 3 bottom-left
};

typedef struct NavContactArray {
    int64_t count;
    NavContact *e;
} NavContactArray;

typedef struct NavContacts {
    NavContact *e[4];
} NavContacts;

typedef enum NavRingFlag {
    NAV_RING_FOUND_EDGE_UP,
    NAV_RING_FOUND_EDGE_RIGHT,
    NAV_RING_FOUND_EDGE_DOWN,
    NAV_RING_FOUND_EDGE_LEFT,
    NAV_RING_ENQUEUED,
    NAV_RING_HAS_CORNER,
    NAV_RING_DRAWN,
    NAV_RING_FLAG_COUNT,
} NavRingFlag;

struct NavRing {
    struct NavRing *neighbors[4]; // 0 up, 1 right, 2 down, 3 left

    NavContact *corners[4]; // 0 top-left, 1 top-right, 2 bottom-right, 3 bottom-left
    JkIntVec2 pos;
    JkQ16Vec3 found_points[4];
    JkQ16Vec3 corner;
    int8_t vertex_count;
    JkVec3 vertices[8];
    uint32_t flags;
};

typedef struct NavRingArray {
    int64_t count;
    NavRing *e;
} NavRingArray;

typedef struct NavPoint {
    JkVec3 p;
    float distance_sqr;
    NavRing *ring;
    int64_t triangle_index;
} NavPoint;

typedef struct NavEdge {
    struct NavEdge *next;
    JkQ16Vec3 v[2];
} NavEdge;

static JK_READONLY NavContact nil_contact;

static JK_READONLY NavRing nil_ring = {
    .flags = JK_MASK(NAV_RING_ENQUEUED) | JK_MASK(NAV_RING_DRAWN),
    .neighbors = {&nil_ring, &nil_ring, &nil_ring, &nil_ring},
    .corners = {&nil_contact, &nil_contact, &nil_contact, &nil_contact},
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

static JkIntVec2 nav_corner_offset[4] = {
    {0, 0},
    {1, 0},
    {1, 1},
    {0, 1},
};

static JkQ16Vec2 synthetic_edge_offset[4] = {
    {0, SYNTHETIC_OFFSET},
    {-SYNTHETIC_OFFSET, 0},
    {0, -SYNTHETIC_OFFSET},
    {SYNTHETIC_OFFSET, 0},
};

static JkQ16Vec2 nav_corner_pos(NavRing *ring, int64_t corner_index)
{
    JkIntVec2 result_i32 = jk_int_vec2_add(ring->pos, nav_corner_offset[corner_index]);
    return jk_q16_vec2_from_i32(result_i32);
}

static JkVec3 world_from_nav(JkVec2 nav_origin, JkQ16Vec3 v)
{
    JkVec2 xy = jk_vec2_mul(
            nav_density, jk_vec2_add(jk_vec2_from_q16(jk_q16_vec2_from_3(v)), nav_origin));
    return jk_vec3_from_2(xy, jk_f32_from_q16(v.z));
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

typedef struct NavTriangle {
    uint32_t flags;
    JkIntRect bounds;
    NavInterpolants interpolants;
    NavInterpolants deltas[2];
} NavTriangle;

typedef struct NavTriangleArray {
    int64_t count;
    NavTriangle *e;
} NavTriangleArray;

static void nav_triangle_setup(JkArena *arena, JkIntVec2 nav_dimensions, Q16Triangle tri)
{
    NavTriangle *result = jk_arena_push_zero(arena, sizeof(*result));

    JkIntRect nav_area_bounds = (JkIntRect){
        .min = {1, 1},
        .max = jk_int_vec2_sub(nav_dimensions, (JkIntVec2){1, 1}),
    };
    result->bounds = jk_int_rect_intersect(nav_area_bounds, q16_triangle_bounding_box(tri));
    if (!(result->bounds.min.x < result->bounds.max.x
                && result->bounds.min.y < result->bounds.max.y)) {
        jk_arena_pop(arena, sizeof(*result));
        return;
    }

    JkQ16Vec2 verts_2d[3];
    for (int64_t i = 0; i < 3; i++) {
        verts_2d[i] = jk_q16_vec2_from_3(tri.v[i]);
    }

    int32_t up = jk_q16_vec2_cross(
            jk_q16_vec2_sub(verts_2d[1], verts_2d[0]), jk_q16_vec2_sub(verts_2d[2], verts_2d[0]));
    if (up < 0) {
        JK_SWAP(verts_2d[1], verts_2d[2], JkQ16Vec2);
    } else if (0 < up) {
        JK_FLAG_SET(result->flags, NAV_CONTACT_UP, 1);
    } else {
        jk_arena_pop(arena, sizeof(*result));
        return;
    }

    // Initialize interpolants and their deltas
    int32_t barycentric_divisor = 0;
    for (int64_t i = 0; i < 3; i++) {
        int64_t a = (i + 1) % 3;
        int64_t b = (i + 2) % 3;
        int32_t cross = jk_q16_vec2_cross(verts_2d[a], verts_2d[b]);
        barycentric_divisor += cross;
        int32_t x_delta = verts_2d[a].y - verts_2d[b].y;
        int32_t y_delta = verts_2d[b].x - verts_2d[a].x;
        result->deltas[0].e[NAV_BARYCENTRIC_0 + i] = x_delta;
        result->deltas[1].e[NAV_BARYCENTRIC_0 + i] = y_delta;
        b32 top = verts_2d[b].x < verts_2d[a].x && verts_2d[a].y == verts_2d[b].y;
        b32 left = verts_2d[b].y < verts_2d[a].y;
        int32_t bias = (top || left) ? 0 : -1;
        result->interpolants.e[NAV_BARYCENTRIC_0 + i] = cross + bias;
    }
    if (barycentric_divisor == 0) {
        jk_arena_pop(arena, sizeof(*result));
        return;
    }
    for (int64_t i = 0; i < 3; i++) {
        for (int64_t axis_index = 0; axis_index < 2; axis_index++) {
            result->deltas[axis_index].e[NAV_Z] += jk_q16_div(
                    jk_q16_mul(result->deltas[axis_index].e[NAV_BARYCENTRIC_0 + i], tri.v[i].z),
                    barycentric_divisor);
        }
        result->interpolants.e[NAV_Z] += jk_q16_mul(
                jk_q16_div(result->interpolants.e[NAV_BARYCENTRIC_0 + i], barycentric_divisor),
                tri.v[i].z);
    }
}

static NavInterpolants nav_triangle_sample(NavTriangle *edge_functions, JkQ16Vec2 pos)
{
    NavInterpolants result;
    for (NavInterpolant i = 0; i < NAV_INTERPOLANT_COUNT; i++) {
        result.e[i] = jk_q16_mul(pos.x, edge_functions->deltas[0].e[i])
                + jk_q16_mul(pos.y, edge_functions->deltas[1].e[i])
                + edge_functions->interpolants.e[i];
    }
    return result;
}

static void nav_triangle_generate_contact(
        JkArena *arena, NavContact **head, NavInterpolants *interpolants, uint32_t flags)
{
    if (0 <= (interpolants->e[NAV_BARYCENTRIC_0] | interpolants->e[NAV_BARYCENTRIC_1]
                | interpolants->e[NAV_BARYCENTRIC_2])) {
        NavContact *contact = jk_arena_push(arena, sizeof(*contact));
        *contact = nil_contact;
        contact->flags = flags;
        contact->z = interpolants->e[NAV_Z];

        NavContact **link = head;
        while (*link && contact->z < (*link)->z) {
            link = &(*link)->next;
        }

        contact->next = *link;
        *link = contact;
    }
}

static void draw_world_segment(Environment *env,
        JkMat4 screen_from_ndc,
        JkMat4 clip_from_world,
        JkColor color,
        JkVec3 a,
        JkVec3 b)
{
    JkVec4 clip[2] = {
        jk_mat4_mul_vec4(clip_from_world, jk_vec4_from_3(a, 1)),
        jk_mat4_mul_vec4(clip_from_world, jk_vec4_from_3(b, 1)),
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
        screen[i] = jk_vec2_from_3(
                jk_mat4_mul_point(screen_from_ndc, jk_vec4_perspective_divide(clip[i])));
    }
    draw_line(env, color, screen[0], screen[1]);
}

static void nav_draw_rings(Environment *env,
        JkMat4 screen_from_ndc,
        JkMat4 clip_from_world,
        JkColor nav_color,
        NavRing *ring)
{
    if (JK_FLAG_GET(ring->flags, NAV_RING_DRAWN)) {
        return;
    }
    JK_FLAG_SET(ring->flags, NAV_RING_DRAWN, 1);

    for (int64_t i = 2; i < ring->vertex_count; i++) {
        draw_world_segment(env,
                screen_from_ndc,
                clip_from_world,
                nav_color,
                ring->vertices[0],
                ring->vertices[i - 1]);
        draw_world_segment(env,
                screen_from_ndc,
                clip_from_world,
                nav_color,
                ring->vertices[i - 1],
                ring->vertices[i]);
        if (i == ring->vertex_count - 1) {
            draw_world_segment(env,
                    screen_from_ndc,
                    clip_from_world,
                    nav_color,
                    ring->vertices[i],
                    ring->vertices[0]);
        }
    }

    for (int64_t i = 0; i < 4; i++) {
        nav_draw_rings(env, screen_from_ndc, clip_from_world, nav_color, ring->neighbors[i]);
    }
}

static int32_t distance_to_edge_sqr(JkQ16Vec3 p, NavEdge *edge)
{
    JkQ16Vec3 v01 = jk_q16_vec3_sub(edge->v[1], edge->v[0]);
    JkQ16Vec3 v0p = jk_q16_vec3_sub(p, edge->v[0]);
    int32_t dot = jk_q16_vec3_dot(v01, v0p);
    if (dot <= 0) {
        return jk_q16_vec3_magnitude_sqr(v0p);
    }
    int32_t mag = jk_q16_vec3_magnitude_sqr(v01);
    if (mag <= dot) {
        return jk_q16_vec3_distance_sqr(edge->v[1], p);
    } else {
        return jk_q16_vec3_magnitude_sqr(v0p) - jk_q16_div(jk_q16_mul(dot, dot), mag);
    }
}

static void nav_remove_invalid_contacts(JkIntVec2 nav_dimensions,
        NavEdge **nav_edges,
        JkQ16Vec2 pos,
        NavContact **head,
        int32_t nav_player_radius_sqr)
{
    int32_t ceiling = INT32_MAX / 2;
    int64_t solid_count = 0;
    NavContact **link = head;
    NavContact *contact;
    while ((contact = *link) != &nil_contact) {
        b32 valid = 1;

        valid = valid && solid_count <= 0;
        valid = valid && (solid_count < 0 || NAV_HEIGHT < ceiling - contact->z);
        if (JK_FLAG_GET(contact->flags, NAV_CONTACT_UP)) {
            solid_count++;
        } else {
            solid_count--;
            ceiling = contact->z - jk_q16_from_f32(0.00048828125f);
            valid = 0;
        }

        if (nav_edges) {
            JkIntVec2 corner_pos = jk_int_vec2_from_q16_floor(pos);
            int32_t index = nav_dimensions.x * corner_pos.y + corner_pos.x;
            for (NavEdge *edge = nav_edges[index]; edge; edge = edge->next) {
                // Remove contacts within player_radius
                JkQ16Vec3 contact_pos = jk_q16_vec3_from_2(pos, contact->z);
                if (distance_to_edge_sqr(contact_pos, edge) < nav_player_radius_sqr) {
                    valid = 0;
                }
            }
        }

        if (valid) {
            link = &contact->next;
        } else {
            *link = contact->next;
        }
    }
}

static int32_t nav_sample(JkArena *arena,
        NavTriangleArray nav_triangles,
        NavEdge **nav_edges,
        int32_t min_z,
        int32_t max_z,
        JkQ16Vec2 pos,
        int32_t nav_player_radius_sqr)
{
    JkArenaScope scope = jk_arena_scope_begin(arena);

    NavContact *head = &nil_contact;
    for (int64_t i = 0; i < nav_triangles.count; i++) {
        NavInterpolants interpolants = nav_triangle_sample(nav_triangles.e + i, pos);
        nav_triangle_generate_contact(arena, &head, &interpolants, nav_triangles.e[i].flags);
    }

    nav_remove_invalid_contacts(nav_dimensions, nav_edges, pos, &head, nav_player_radius_sqr);

    int32_t result = INT32_MIN;
    for (NavContact *contact = head; contact != &nil_contact; contact = contact->next) {
        if (min_z < contact->z && contact->z <= max_z) {
            result = contact->z;
            break;
        }
    }

    jk_arena_scope_end(scope);
    return result;
}

static JkQ16Vec2 get_midpoint(JkQ16Vec2 a, JkQ16Vec2 b)
{
    return (JkQ16Vec2){.x = (a.x + b.x) >> 1, .y = (a.y + b.y) >> 1};
}

static JkQ16Vec3 nav_binary_search(JkArena *arena,
        NavTriangleArray nav_triangles,
        NavEdge **nav_edges,
        JkQ16Vec3 inside_point,
        JkQ16Vec2 outside_point,
        int32_t nav_player_radius_sqr)
{
    int32_t min_z = inside_point.z - (NAV_STEP_HEIGHT >> 1);
    int32_t max_z = inside_point.z + (NAV_STEP_HEIGHT >> 1);

    // Binary search for walkable point closest to outside contact
    for (int64_t i = 0; i < 8; i++) {
        JkQ16Vec2 midpoint = get_midpoint(jk_q16_vec2_from_3(inside_point), outside_point);
        int32_t z = nav_sample(
                arena, nav_triangles, nav_edges, min_z, max_z, midpoint, nav_player_radius_sqr);
        if (z == INT32_MIN) { // miss
            outside_point = midpoint;
        } else { // hit
            inside_point = jk_q16_vec3_from_2(midpoint, z);
        }
    }

    return inside_point;
}

static void nav_add_edge(JkArena *arena,
        NavEdge **nav_edges,
        NavContact **nav_contacts,
        JkIntVec2 nav_dimensions,
        int32_t nav_player_radius,
        int32_t nav_player_radius_sqr,
        JkQ16Vec3 v0,
        JkQ16Vec3 v1)
{
    JkIntRect nav_area_bounds = (JkIntRect){.max = nav_dimensions};
    JkIntRect bounds = jk_int_rect_intersect(
            segment_bounding_box(jk_q16_vec2_from_3(v0), jk_q16_vec2_from_3(v1), nav_player_radius),
            nav_area_bounds);
    for (JkIntVec2 pos = {.y = bounds.min.y}; pos.y < bounds.max.y; pos.y++) {
        for (pos.x = bounds.min.x; pos.x < bounds.max.x; pos.x++) {
            int32_t index = nav_dimensions.x * pos.y + pos.x;
            NavEdge *edge = jk_arena_push(arena, JK_SIZEOF(*edge));
            edge->v[0] = v0;
            edge->v[1] = v1;
            edge->next = nav_edges[index];
            nav_edges[index] = edge;

            // Remove contacts within player_radius
            JkQ16Vec2 q16_pos = jk_q16_vec2_from_i32(pos);
            NavContact **link = nav_contacts + index;
            for (NavContact *contact; (contact = *link) != &nil_contact;) {
                JkQ16Vec3 contact_pos = jk_q16_vec3_from_2(q16_pos, contact->z);
                if (distance_to_edge_sqr(contact_pos, edge) < nav_player_radius_sqr) {
                    *link = contact->next;
                } else {
                    link = &contact->next;
                }
            }
        }
    }
}

static void nav_triangle_rasterize(
        JkArena *arena, NavContact **nav_contacts, JkIntVec2 nav_dimensions, NavTriangle *tri)
{
    NavInterpolants interpolants_row;
    for (NavInterpolant i = 0; i < NAV_INTERPOLANT_COUNT; i++) {
        interpolants_row.e[i] = tri->bounds.min.x * tri->deltas[0].e[i]
                + tri->bounds.min.y * tri->deltas[1].e[i] + tri->interpolants.e[i];
    }

    for (int32_t y = tri->bounds.min.y; y < tri->bounds.max.y; y++) {
        NavInterpolants interpolants = interpolants_row;
        for (int32_t x = tri->bounds.min.x; x < tri->bounds.max.x; x++) {
            NavContact **head = nav_contacts + (nav_dimensions.x * y + x);
            nav_triangle_generate_contact(arena, head, &interpolants, tri->flags);

            for (int64_t i = 0; i < NAV_INTERPOLANT_COUNT; i++) {
                interpolants.e[i] += tri->deltas[0].e[i];
            }
        }

        for (int64_t i = 0; i < NAV_INTERPOLANT_COUNT; i++) {
            interpolants_row.e[i] += tri->deltas[1].e[i];
        }
    }
}

static NavRingArray nav_find_rings(JkArena *arena, NavContact **nav_contacts)
{
    JkArenaScope ring_array_scope = jk_arena_scope_begin(arena);

    for (JkIntVec2 pos = {0}; pos.y < nav_dimensions.y - 1; pos.y++) {
        for (pos.x = 0; pos.x < nav_dimensions.x - 1; pos.x++) {
            NavContacts contacts = {
                nav_contacts[nav_dimensions.x * pos.y + pos.x],
                nav_contacts[nav_dimensions.x * pos.y + (pos.x + 1)],
                nav_contacts[nav_dimensions.x * (pos.y + 1) + (pos.x + 1)],
                nav_contacts[nav_dimensions.x * (pos.y + 1) + pos.x],
            };
            while (nav_contacts_any(contacts)) {
                NavRing ring = nil_ring;
                ring.pos = pos;
                ring.flags = 0;

                int64_t max_index = 0;
                for (int64_t i = 1; i < 4; i++) {
                    if (contacts.e[max_index]->z < contacts.e[i]->z) {
                        max_index = i;
                    }
                }
                int32_t original_min_z = contacts.e[max_index]->z - NAV_STEP_HEIGHT;

                // Potentially expand the min_z a bit to avoid the awkward case where one lower
                // corner barely makes it and the other one barely doesn't
                int32_t min_z = original_min_z;
                for (int64_t i = 0; i < 4; i++) {
                    if (original_min_z < contacts.e[i]->z) {
                        min_z = JK_MIN(min_z, contacts.e[i]->z - (NAV_STEP_HEIGHT >> 2));
                    }
                }

                uint8_t mask = 0;
                for (int64_t i = 0; i < 4; i++) {
                    if (min_z < contacts.e[i]->z) {
                        ring.corners[i] = contacts.e[i];
                        mask |= (1 << i);
                    }
                }

                // If at least one corner is present and it's NOT a diagonal pattern
                if (mask && mask != 0x5 && mask != 0xa) {
                    NavRing *self = jk_arena_push(arena, sizeof(*self));
                    *self = ring;

                    for (int64_t i = 0; i < 4; i++) {
                        int64_t prev = JK_MOD(i - 1, 4);
                        int64_t next = JK_MOD(i + 1, 4);
                        int64_t opposite = JK_MOD(i + 2, 4);

                        NavContact *corner = self->corners[i];
                        if (corner != &nil_contact) {
                            corner->rings[opposite] = self;

                            // Find neighbors
                            NavRing *neighbor0 = corner->rings[prev];
                            if (neighbor0 != &nil_ring
                                    && neighbor0->corners[opposite] == self->corners[prev]) {
                                self->neighbors[prev] = neighbor0;
                                neighbor0->neighbors[next] = self;
                            }
                            NavRing *neighbor1 = corner->rings[next];
                            if (neighbor1 != &nil_ring
                                    && neighbor1->corners[opposite] == self->corners[next]) {
                                self->neighbors[i] = neighbor1;
                                neighbor1->neighbors[opposite] = self;
                            }

                            contacts.e[i] = contacts.e[i]->next;
                        }
                    }
                } else {
                    contacts.e[max_index] = contacts.e[max_index]->next;
                }
            }
        }
    }

    NavRingArray result;
    JK_ARRAY_FROM_ARENA_SCOPE(result, ring_array_scope);
    return result;
}

typedef struct NavRingFindEdgesResult {
    uint8_t is_edge;
    int8_t count;
    JkQ16Vec3 vertices[8];
} NavRingFindEdgesResult;

static void nav_ring_find_edges(JkArena *arena,
        NavTriangleArray nav_triangles,
        NavEdge **nav_edges,
        int32_t nav_player_radius_sqr,
        NavRing *ring)
{
    int64_t segment_count = 0;
    JkQ16Vec3 segments[2][2];
    int64_t first_inside = -1;
    for (int64_t edge_index = 0; edge_index < 4; edge_index++) {
        if (first_inside == -1 && ring->corners[edge_index] != &nil_contact) {
            first_inside = edge_index;
        }

        int64_t next_index = JK_MOD(edge_index + 1, 4);

        int64_t outside_count = 0;
        int64_t inside_index = next_index;
        int64_t outside_index = edge_index;
        if (ring->corners[edge_index] == &nil_contact) {
            outside_count++;
        }
        if (ring->corners[next_index] == &nil_contact) {
            outside_count++;
            JK_SWAP(inside_index, outside_index, int64_t);
        }

        if (outside_count == 1) {
            JkQ16Vec3 inside_point = jk_q16_vec3_from_2(
                    nav_corner_pos(ring, inside_index), ring->corners[inside_index]->z);
            JkQ16Vec2 outside_point = nav_corner_pos(ring, outside_index);

            if (!JK_FLAG_GET(ring->flags, NAV_RING_FOUND_EDGE_UP + edge_index)) {
                ring->found_points[edge_index] = nav_binary_search(arena,
                        nav_triangles,
                        nav_edges,
                        inside_point,
                        outside_point,
                        nav_player_radius_sqr);

                JK_FLAG_SET(ring->flags, NAV_RING_FOUND_EDGE_UP + edge_index, 1);

                NavRing *neighbor = ring->neighbors[edge_index];
                if (neighbor != &nil_ring) {
                    int64_t opposite_index = JK_MOD(edge_index + 2, 4);
                    neighbor->found_points[opposite_index] = ring->found_points[edge_index];
                    JK_FLAG_SET(neighbor->flags, NAV_RING_FOUND_EDGE_UP + opposite_index, 1);
                }
            }

            JkQ16Vec3 synthetic_inside = jk_q16_vec3_add(
                    inside_point, jk_q16_vec3_from_2(synthetic_edge_offset[edge_index], 0));
            JkQ16Vec2 synthetic_outisde =
                    jk_q16_vec2_add(outside_point, synthetic_edge_offset[edge_index]);

            JkQ16Vec3 synthetic_point = nav_binary_search(arena,
                    nav_triangles,
                    nav_edges,
                    synthetic_inside,
                    synthetic_outisde,
                    nav_player_radius_sqr);

            int64_t segment_index = segment_count++;
            segments[segment_index][0] = ring->found_points[edge_index];
            segments[segment_index][1] = synthetic_point;
        }
    }

    // Find corner
    JK_DEBUG_ASSERT(segment_count == 0 || segment_count == 2);
    if (segment_count == 2) {
        JkQ16Vec3 delta0 = jk_q16_vec3_sub(segments[0][1], segments[0][0]);
        JkQ16Vec3 delta1 = jk_q16_vec3_sub(segments[1][1], segments[1][0]);
        JkQ16Vec3 normal = jk_q16_vec3_cross(delta1, (JkQ16Vec3){0, 0, jk_q16_from_i32(1)});

        int32_t delta0_dot = jk_q16_vec3_dot(normal, delta0);
        if (Q16_EPSILON < JK_ABS(delta0_dot)) {
            JK_FLAG_SET(ring->flags, NAV_RING_HAS_CORNER, 1);

            JkQ16Vec3 tmp = jk_q16_vec3_sub(segments[0][0], segments[1][0]);
            int32_t p0_dot = jk_q16_vec3_dot(normal, tmp);
            int32_t t = jk_q16_div(-p0_dot, delta0_dot);
            ring->corner = jk_q16_vec3_lerp(segments[0][0], segments[0][1], t);
        }
    }
}

// ---- NavRingQueue begin ------------------------------------------------------------

typedef struct NavRingQueue {
    int64_t mask;
    int64_t start;
    int64_t end;
    NavRing **rings;
} NavRingQueue;

static NavRingQueue q_new(JkArena *arena, int64_t capacity)
{
    JK_DEBUG_ASSERT(jk_is_power_of_two(capacity));
    return (NavRingQueue){
        .mask = capacity - 1,
        .rings = jk_arena_push(arena, JK_SIZEOF(NavRing *) * capacity),
    };
}

static void q_enqueue(NavRingQueue *q, NavRing *ring)
{
    q->rings[q->end++ & q->mask] = ring;
    JK_FLAG_SET(ring->flags, NAV_RING_ENQUEUED, 1);
}

static NavRing *q_dequeue(NavRingQueue *q)
{
    if (q->start < q->end) {
        return q->rings[q->start++ & q->mask];
    } else {
        return 0;
    }
}

// ---- NavRingQueue end --------------------------------------------------------------

typedef enum SampleInterpolant {
    S_BARYCENTRIC_0,
    S_BARYCENTRIC_1,
    S_BARYCENTRIC_2,
    S_Z,
    SAMPLE_INTERPOLANT_COUNT,
} SampleInterpolant;

typedef struct SampleInterpolants {
    JkF32x8 e[SAMPLE_INTERPOLANT_COUNT];
} SampleInterpolants;

typedef enum PixelInterpolant {
    P_U,
    P_V,
    PIXEL_INTERPOLANT_COUNT,
} PixelInterpolant;

typedef struct PixelInterpolants {
    JkF32x8 e[PIXEL_INTERPOLANT_COUNT];
} PixelInterpolants;

typedef struct ColorF32x8x4 {
    JkF32x8 e[4];
} ColorF32x8x4;

static JkF32x8 channel_extract(JkI256 color, int32_t channel_index)
{
    switch (channel_index) {
    case 1: {
        color = JK_I256_SHIFT_RIGHT_ZERO_FILL_I32(color, 8);
    } break;

    case 2: {
        color = JK_I256_SHIFT_RIGHT_ZERO_FILL_I32(color, 16);
    } break;

    case 3: {
        color = JK_I256_SHIFT_RIGHT_ZERO_FILL_I32(color, 24);
    } break;

    default: {
    } break;
    }
    return jk_f32x8_from_i32x8(jk_i256_and(color, jk_i256_broadcast_i32(0xff)));
}

static JkF32x8 bilerp(JkI256 *colors, int32_t channel_index, JkF32x8 xfrac, JkF32x8 yfrac)
{
    JkF32x8 colorsf[4];
    for (int64_t i = 0; i < 4; i++) {
        colorsf[i] = channel_extract(colors[i], channel_index);
    }
    JkF32x8 top = jk_f32x8_lerp(colorsf[0], colorsf[1], xfrac);
    JkF32x8 bottom = jk_f32x8_lerp(colorsf[2], colorsf[3], xfrac);
    return jk_f32x8_lerp(top, bottom, yfrac);
}

static ColorF32x8x4 color_broadcast(JkColor color)
{
    ColorF32x8x4 result;
    for (int64_t i = 0; i < 4; i++) {
        result.e[i] = jk_f32x8_broadcast(color.v[i]);
    }
    return result;
}

static void disjoint_over(ColorF32x8x4 *fg, ColorF32x8x4 bg)
{
    bg.e[3] = jk_f32x8_min(bg.e[3], jk_f32x8_sub(jk_f32x8_broadcast(1), fg->e[3]));
    JkF32x8 alpha = jk_f32x8_add(fg->e[3], bg.e[3]);
    for (int64_t i = 0; i < 3; i++) {
        JkF32x8 fg_chan = jk_f32x8_mul(fg->e[i], fg->e[3]);
        JkF32x8 bg_chan = jk_f32x8_mul(bg.e[i], bg.e[3]);
        fg->e[i] = jk_f32x8_div(
                jk_f32x8_add(fg_chan, bg_chan), jk_f32x8_max(alpha, jk_f32x8_broadcast(0.001)));
    }
    fg->e[3] = alpha;
}

static void triangle_fill(
        Environment *env, TexturedTriangle *tri, Texture *texture, JkIntRect bounding_box)
{
    JkIntRect bounds = jk_int_rect_intersect(
            bounding_box, triangle_bounding_box(tri->v[0], tri->v[1], tri->v[2]));
    if (!(bounds.min.x < bounds.max.x && bounds.min.y < bounds.max.y)) {
        return;
    }
    bounds.min.x &= ~(8 - 1);

    ColorF32x8x4 bg = color_broadcast(texture->bg);
    ColorF32x8x4 tex_colors[4];
    for (int64_t i = 0; i < 4; i++) {
        tex_colors[i] = color_broadcast(jk_color4_from_3(texture->colors[i], 0x00));
    }

    JkVec2 verts_2d[3];
    for (int64_t i = 0; i < 3; i++) {
        verts_2d[i] = jk_vec2_from_3(tri->v[i]);
    }

    SampleInterpolants s_interpolants_row[SAMPLE_COUNT] = {0};
    PixelInterpolants p_interpolants_row = {0};
    float deltas[2][SAMPLE_INTERPOLANT_COUNT + PIXEL_INTERPOLANT_COUNT] = {0};

    JkF32x8 init_pos[2][SAMPLE_COUNT];
    for (int64_t axis_index = 0; axis_index < 2; axis_index++) {
        JkF32x8 pixel_coord = jk_f32x8_add(jk_f32x8_broadcast(bounds.min.v[axis_index]),
                jk_f32x8_load(lane_offsets[axis_index]));
        for (int64_t sample_index = 0; sample_index < SAMPLE_COUNT; sample_index++) {
            init_pos[axis_index][sample_index] = jk_f32x8_add(
                    pixel_coord, jk_f32x8_broadcast(sample_offsets[axis_index][sample_index]));
        }
    }

    float barycentric_divisor = 0;
    for (int64_t i = 0; i < 3; i++) {
        int64_t a = (i + 1) % 3;
        int64_t b = (i + 2) % 3;
        float cross = jk_vec2_cross(verts_2d[a], verts_2d[b]);
        barycentric_divisor += cross;
        float x_delta = verts_2d[a].y - verts_2d[b].y;
        float y_delta = verts_2d[b].x - verts_2d[a].x;
        deltas[0][S_BARYCENTRIC_0 + i] = 8 * x_delta;
        deltas[1][S_BARYCENTRIC_0 + i] = y_delta;
        JkF32x8 cross_wide = jk_f32x8_broadcast(cross);
        JkF32x8 x_delta_wide = jk_f32x8_broadcast(x_delta);
        JkF32x8 y_delta_wide = jk_f32x8_broadcast(y_delta);
        for (int64_t sample_index = 0; sample_index < SAMPLE_COUNT; sample_index++) {
            JkF32x8 coord =
                    jk_f32x8_add(cross_wide, jk_f32x8_mul(x_delta_wide, init_pos[0][sample_index]));
            coord = jk_f32x8_add(coord, jk_f32x8_mul(y_delta_wide, init_pos[1][sample_index]));
            s_interpolants_row[sample_index].e[S_BARYCENTRIC_0 + i] = coord;
        }
    }

    JkF32x8 barycentric_divisor_wide = jk_f32x8_broadcast(barycentric_divisor);
    for (int64_t i = 0; i < 3; i++) {
        for (int64_t axis_index = 0; axis_index < 2; axis_index++) {
            deltas[axis_index][S_BARYCENTRIC_0 + i] /= barycentric_divisor;
            deltas[axis_index][S_Z] += deltas[axis_index][S_BARYCENTRIC_0 + i] * tri->v[i].z;

            deltas[axis_index][SAMPLE_INTERPOLANT_COUNT + P_U] +=
                    deltas[axis_index][S_BARYCENTRIC_0 + i] * tri->t[i].x;
            deltas[axis_index][SAMPLE_INTERPOLANT_COUNT + P_V] +=
                    deltas[axis_index][S_BARYCENTRIC_0 + i] * tri->t[i].y;
        }
        for (int64_t sample_index = 0; sample_index < SAMPLE_COUNT; sample_index++) {
            s_interpolants_row[sample_index].e[S_BARYCENTRIC_0 + i] =
                    jk_f32x8_div(s_interpolants_row[sample_index].e[S_BARYCENTRIC_0 + i],
                            barycentric_divisor_wide);
            s_interpolants_row[sample_index].e[S_Z] =
                    jk_f32x8_add(s_interpolants_row[sample_index].e[S_Z],
                            jk_f32x8_mul(s_interpolants_row[sample_index].e[S_BARYCENTRIC_0 + i],
                                    jk_f32x8_broadcast(tri->v[i].z)));
        }
        p_interpolants_row.e[P_U] = jk_f32x8_add(p_interpolants_row.e[P_U],
                jk_f32x8_mul(s_interpolants_row[0].e[S_BARYCENTRIC_0 + i],
                        jk_f32x8_broadcast(tri->t[i].x)));
        p_interpolants_row.e[P_V] = jk_f32x8_add(p_interpolants_row.e[P_V],
                jk_f32x8_mul(s_interpolants_row[0].e[S_BARYCENTRIC_0 + i],
                        jk_f32x8_broadcast(tri->t[i].y)));
    }

    float inv_deriv_z[2] = {
        deltas[0][S_Z] / 8,
        deltas[1][S_Z],
    };

    float inv_deriv[4]; // Usage: inv_deriv[2 * axis + tex_axis]
    inv_deriv[0] = deltas[0][SAMPLE_INTERPOLANT_COUNT + P_U] / 8; // dU/dx
    inv_deriv[1] = deltas[0][SAMPLE_INTERPOLANT_COUNT + P_V] / 8; // dV/dx
    inv_deriv[2] = deltas[1][SAMPLE_INTERPOLANT_COUNT + P_U]; // dU/dy
    inv_deriv[3] = deltas[1][SAMPLE_INTERPOLANT_COUNT + P_V]; // dV/dy

    for (int32_t y = bounds.min.y; y < bounds.max.y; y++) {
        PixelInterpolants p_interpolants = p_interpolants_row;
        SampleInterpolants s_interpolants_col[SAMPLE_COUNT];
        jk_memcpy(s_interpolants_col, s_interpolants_row, sizeof(s_interpolants_row));
        for (int32_t x = bounds.min.x; x < bounds.max.x; x += 8) {
            b32 found_color = 0;
            ColorF32x8x4 pixel_color = color_broadcast((JkColor){0});
            for (int64_t sample_index = 0; sample_index < SAMPLE_COUNT; sample_index++) {
                SampleInterpolants *s_interpolants = s_interpolants_col + sample_index;
                JkF32x8 outside_triangle =
                        jk_f32x8_to_mask(jk_f32x8_or(s_interpolants->e[S_BARYCENTRIC_0],
                                jk_f32x8_or(s_interpolants->e[S_BARYCENTRIC_1],
                                        s_interpolants->e[S_BARYCENTRIC_2])));
                if (!jk_f32x8_all(outside_triangle)) {
                    int32_t index = PIXEL_COUNT * sample_index + DRAW_BUFFER_SIDE_LENGTH * y + x;
                    JkF32x8 z_buffer = jk_f32x8_load(env->z_buffer + index);
                    JkF32x8 in_front = jk_f32x8_less_than(z_buffer, s_interpolants->e[S_Z]);
                    JkF32x8 visible = jk_f32x8_andnot(outside_triangle, in_front);
                    if (jk_f32x8_any(visible)) {
                        jk_f32x8_store(env->z_buffer + index,
                                jk_f32x8_blend(z_buffer, s_interpolants->e[S_Z], visible));

                        if (!found_color) {
                            found_color = 1;
                            JkF32x8 inv_z = jk_f32x8_div(
                                    jk_f32x8_broadcast(1), s_interpolants_col[0].e[S_Z]);

                            JkF32x8 uv[2];
                            JkF32x8 frac[2];
                            JkI256 coords[2][2];
                            for (int32_t axis = 0; axis < 2; axis++) {
                                uv[axis] = jk_f32x8_mul(p_interpolants.e[P_U + axis], inv_z);

                                JkF32x8 tex = jk_f32x8_mul(jk_f32x8_broadcast(TEXTURE_SIDE_LENGTH),
                                        jk_f32x8_sub(uv[axis], jk_f32x8_floor(uv[axis])));
                                frac[axis] = jk_f32x8_sub(tex, jk_f32x8_floor(tex));
                                coords[axis][0] = jk_i256_and(jk_i32x8_from_f32x8_truncate(tex),
                                        jk_i256_broadcast_i32(TEXTURE_MASK));
                                coords[axis][1] = jk_i256_and(
                                        jk_i256_add_i32(coords[axis][0], jk_i256_broadcast_i32(1)),
                                        jk_i256_broadcast_i32(TEXTURE_MASK));
                            }

                            JkF32x8 pixel_size = jk_f32x8_broadcast(0);
                            for (int32_t axis = 0; axis < 2; axis++) {
                                for (int32_t tex_axis = 0; tex_axis < 2; tex_axis++) {
                                    JkF32x8 dUV =
                                            jk_f32x8_broadcast(inv_deriv[2 * axis + tex_axis]);
                                    JkF32x8 dZ = jk_f32x8_broadcast(inv_deriv_z[axis]);
                                    JkF32x8 deriv = jk_f32x8_mul(inv_z,
                                            jk_f32x8_sub(dUV, jk_f32x8_mul(uv[tex_axis], dZ)));
                                    pixel_size = jk_f32x8_add(pixel_size, jk_f32x8_abs(deriv));
                                }
                            }
                            pixel_size = jk_f32x8_mul(pixel_size, jk_f32x8_broadcast(0.5));

                            JkI256 dist[4];
                            for (int32_t row_i = 0; row_i < 2; row_i++) {
                                JkI256 row =
                                        JK_I256_SHIFT_LEFT_I32(coords[1][row_i], TEXTURE_POW_2);
                                for (int32_t col_i = 0; col_i < 2; col_i++) {
                                    dist[2 * row_i + col_i] = jk_i256_from_f32x8_reinterpret(
                                            jk_f32x8_gather(texture->data,
                                                    jk_i256_add_i32(row, coords[0][col_i])));
                                }
                            }

                            for (int64_t channel_index = 0;
                                    jk_f32x8_any(jk_f32x8_less_than(
                                            pixel_color.e[3], jk_f32x8_broadcast(1)))
                                    && channel_index < 4;
                                    channel_index++) {
                                JkF32x8 distance = bilerp(dist, channel_index, frac[0], frac[1]);
                                JkF32x8 dir = jk_f32x8_sub(
                                        jk_f32x8_mul(jk_f32x8_broadcast(2.0f / 255), distance),
                                        jk_f32x8_broadcast(1));
                                JkF32x8 spread_pixels = jk_f32x8_mul(
                                        jk_f32x8_broadcast(SDF_SPREAD / TEXTURE_SIDE_LENGTH),
                                        jk_f32x8_reciprocal_approx(pixel_size));

                                ColorF32x8x4 color = tex_colors[channel_index];
                                color.e[3] = jk_f32x8_add(
                                        jk_f32x8_broadcast(0.5), jk_f32x8_mul(dir, spread_pixels));
                                color.e[3] = jk_f32x8_max(color.e[3], jk_f32x8_broadcast(0));
                                color.e[3] = jk_f32x8_min(color.e[3], jk_f32x8_broadcast(1));
                                disjoint_over(&pixel_color, color);
                            }
                            if (jk_f32x8_any(jk_f32x8_less_than(
                                        pixel_color.e[3], jk_f32x8_broadcast(1)))) {
                                disjoint_over(&pixel_color, bg);
                            }
                        }

                        JkI256 color_i32 = jk_i32x8_from_f32x8_truncate(pixel_color.e[0]);
                        color_i32 = jk_i256_or(color_i32,
                                JK_I256_SHIFT_LEFT_I32(
                                        jk_i32x8_from_f32x8_truncate(pixel_color.e[1]), 8));
                        color_i32 = jk_i256_or(color_i32,
                                JK_I256_SHIFT_LEFT_I32(
                                        jk_i32x8_from_f32x8_truncate(pixel_color.e[2]), 16));

                        JkF32x8 color_buffer = jk_f32x8_load((float *)(env->draw_buffer + index));
                        jk_f32x8_store((float *)(env->draw_buffer + index),
                                jk_f32x8_blend(color_buffer,
                                        jk_f32x8_from_i256_reinterpret(color_i32),
                                        visible));
                    }
                }

                for (int64_t i = 0; i < SAMPLE_INTERPOLANT_COUNT; i++) {
                    s_interpolants->e[i] =
                            jk_f32x8_add(s_interpolants->e[i], jk_f32x8_broadcast(deltas[0][i]));
                }
            }

            for (int64_t i = 0; i < PIXEL_INTERPOLANT_COUNT; i++) {
                p_interpolants.e[i] = jk_f32x8_add(p_interpolants.e[i],
                        jk_f32x8_broadcast(deltas[0][SAMPLE_INTERPOLANT_COUNT + i]));
            }
        }

        for (int64_t sample_index = 0; sample_index < SAMPLE_COUNT; sample_index++) {
            for (int64_t i = 0; i < SAMPLE_INTERPOLANT_COUNT; i++) {
                s_interpolants_row[sample_index].e[i] = jk_f32x8_add(
                        s_interpolants_row[sample_index].e[i], jk_f32x8_broadcast(deltas[1][i]));
            }
        }
        for (int64_t i = 0; i < PIXEL_INTERPOLANT_COUNT; i++) {
            p_interpolants_row.e[i] = jk_f32x8_add(p_interpolants_row.e[i],
                    jk_f32x8_broadcast(deltas[1][SAMPLE_INTERPOLANT_COUNT + i]));
        }
    }
}

static void add_textured_vertex(JkArena *arena, JkMat4 screen_from_ndc, JkVec4 v, JkVec2 t)
{
    TexturedVertex *new = jk_arena_push(arena, sizeof(*new));
    new->v = jk_mat4_mul_point(screen_from_ndc, jk_vec4_perspective_divide(v));
    new->t = jk_vec2_mul(new->v.z, t);
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
        result = jk_mat4_mul(jk_mat4_from_transform(parent->transform), result);
    }
    return result;
}

typedef struct ScreenFromWorldResult {
    b32 clipped;
    JkVec3 v;
} ScreenFromWorldResult;

static NavPoint closest_point_on_ring(JkVec3 p, NavRing *ring)
{
    NavPoint result = {.distance_sqr = jk_infinity_f32.f32, .ring = ring};
    for (int64_t i = 2; i < ring->vertex_count; i++) {
        JkVec3 candidate = jk_closest_point_on_triangle(
                p, ring->vertices[0], ring->vertices[i - 1], ring->vertices[i]);
        float candidate_distance_sqr = jk_vec3_distance_squared(p, candidate);
        if (candidate_distance_sqr < result.distance_sqr) {
            result.p = candidate;
            result.distance_sqr = candidate_distance_sqr;
            result.triangle_index = i;
        }
    }
    return result;
}

void render(JkContext *context, Environment *env)
{
    JkColor text_color = {255, 255, 255, 255};

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

    NavContact **nav_contacts = 0;
    NavRingArray nav_rings = {0};
    NavPoint start = {.distance_sqr = jk_infinity_f32.f32};

    TextureArray textures;
    JK_ARRAY_FROM_SPAN(textures, env->assets, env->assets->textures);

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
            env->state.camera_yaw = 5 * JK_PI / 4;
            env->state.camera_pitch = 0;
            env->state.player_position = (JkVec3){0};

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

        scratch0 = jk_arena_scratch_begin();
        scratch1 = jk_arena_scratch_begin_not(scratch0.arena);

        // ---- Navigation begin ----------------------------------------------

        JkVec2 nav_origin = jk_vec2_sub(
                jk_vec2_mul(1 / nav_density, jk_vec2_from_3(env->state.player_position)),
                jk_vec2_mul(0.5, jk_vec2_from_i32(nav_dimensions)));
        nav_origin.x = jk_floor_f32(nav_origin.x);
        nav_origin.y = jk_floor_f32(nav_origin.y);

        JkMat4 nav_from_world = jk_mat4_scale((JkVec3){1 / nav_density, 1 / nav_density, 1});
        nav_from_world = jk_mat4_mul(
                jk_mat4_translate(jk_vec3_from_2(jk_vec2_mul(-1, nav_origin), 0)), nav_from_world);

        int32_t nav_player_radius = jk_q16_from_f32(player_radius / nav_density);
        int32_t nav_player_radius_sqr = jk_q16_mul(nav_player_radius, nav_player_radius);

        JkArenaScope build_navmesh_scope = jk_arena_scope_begin(scratch0.arena);

        // Collect navigation triangles
        JkArenaScope nav_triangle_transform_scope = jk_arena_scope_begin(scratch1.arena);

        for (ObjectId object_id = {1}; object_id.i < objects.count; object_id.i++) {
            Object *object = objects.e + object_id.i;

            FaceArray faces;
            JK_ARRAY_FROM_SPAN(faces, env->assets, object->faces);

            JkMat4 world_from_local = object_compute_world_from_local(objects, object_id);
            JkMat4 nav_from_local = jk_mat4_mul(nav_from_world, world_from_local);

            JkQ16Vec3 *nav_vertices =
                    jk_arena_push(scratch1.arena, vertices.count * JK_SIZEOF(*nav_vertices));
            for (int64_t i = 0; i < vertices.count; i++) {
                JkVec3 vert_f32 = jk_mat4_mul_point(nav_from_local, vertices.e[i]);
                nav_vertices[i] = jk_q16_vec3_from_f32(vert_f32);
            }

            // Process faces in world space for navigation grid
            for (int64_t face_index = 0; face_index < faces.count; face_index++) {
                Face face = faces.e[face_index];

                Q16Triangle triangle = {0};
                for (int64_t i = 0; i < 3; i++) {
                    triangle.v[i] = nav_vertices[face.v[i]];
                }

                nav_triangle_setup(scratch0.arena, nav_dimensions, triangle);
            }
        }

        jk_arena_scope_end(nav_triangle_transform_scope);

        NavTriangleArray nav_triangles;
        JK_ARRAY_FROM_ARENA_SCOPE(nav_triangles, build_navmesh_scope);

        // Rasterize navigation triangles
        nav_contacts = jk_arena_push(
                scratch0.arena, nav_dimensions.x * nav_dimensions.y * JK_SIZEOF(NavContact *));
        for (int64_t i = 0; i < nav_dimensions.x * nav_dimensions.y; i++) {
            nav_contacts[i] = &nil_contact;
        }

        JkArenaScope contacts_scope = jk_arena_scope_begin(scratch0.arena);
        for (int64_t i = 0; i < nav_triangles.count; i++) {
            nav_triangle_rasterize(
                    scratch0.arena, nav_contacts, nav_dimensions, nav_triangles.e + i);
        }
        NavContactArray nav_contacts_array;
        JK_ARRAY_FROM_ARENA_SCOPE(nav_contacts_array, contacts_scope);

        // Remove invalid contacts
        for (int64_t i = 0; i < nav_dimensions.x * nav_dimensions.y; i++) {
            nav_remove_invalid_contacts(
                    nav_dimensions, 0, (JkQ16Vec2){0}, nav_contacts + i, nav_player_radius_sqr);
        }

        scratch1.arena->pos = JK_ALIGN_UP(scratch1.arena->pos, 8);
        JkArenaScope pass1_scope = jk_arena_scope_begin(scratch1.arena);
        NavRingArray pass1_rings = nav_find_rings(scratch1.arena, nav_contacts);

        // Find edges
        NavEdge **nav_edges = jk_arena_push_zero(
                scratch0.arena, nav_dimensions.x * nav_dimensions.y * JK_SIZEOF(NavEdge *));
        for (int64_t ring_index = 0; ring_index < pass1_rings.count; ring_index++) {
            NavRing *ring = pass1_rings.e + ring_index;

            nav_ring_find_edges(scratch1.arena, nav_triangles, 0, nav_player_radius_sqr, ring);

            int64_t point_count = 0;
            JkQ16Vec3 points[3];
            for (int64_t edge_index = 0; edge_index < 4; edge_index++) {
                if (JK_FLAG_GET(ring->flags, NAV_RING_FOUND_EDGE_UP + edge_index)) {
                    points[point_count++] = ring->found_points[edge_index];
                }
                if (point_count == 1 && JK_FLAG_GET(ring->flags, NAV_RING_HAS_CORNER)) {
                    points[point_count++] = ring->corner;
                }
            }

            for (int64_t i = 1; i < point_count; i++) {
                nav_add_edge(scratch0.arena,
                        nav_edges,
                        nav_contacts,
                        nav_dimensions,
                        nav_player_radius,
                        nav_player_radius_sqr,
                        points[i - 1],
                        points[i]);
            }
        }

        jk_arena_scope_end(pass1_scope);

        for (int64_t contact_index = 0; contact_index < nav_contacts_array.count; contact_index++) {
            for (int64_t i = 0; i < 4; i++) {
                nav_contacts_array.e[contact_index].rings[i] = &nil_ring;
            }
        }

        nav_rings = nav_find_rings(scratch1.arena, nav_contacts);
        for (int64_t ring_index = 0; ring_index < nav_rings.count; ring_index++) {
            NavRing *ring = nav_rings.e + ring_index;

            nav_ring_find_edges(
                    scratch1.arena, nav_triangles, nav_edges, nav_player_radius_sqr, ring);

            int64_t first_inside = 0;
            while (ring->corners[first_inside] == &nil_contact) {
                first_inside++;
            }

            int64_t pivot = 0;
            JkQ16Vec3 points[JK_ARRAY_COUNT(ring->vertices)];
            for (int64_t i = 0; i < 4; i++) {
                int64_t edge_index = JK_MOD(i + first_inside, 4);

                if (ring->corners[edge_index] != &nil_contact) {
                    points[ring->vertex_count++] = jk_q16_vec3_from_2(
                            jk_q16_vec2_from_i32(
                                    jk_int_vec2_add(ring->pos, nav_corner_offset[edge_index])),
                            ring->corners[edge_index]->z);
                }
                if (JK_FLAG_GET(ring->flags, NAV_RING_FOUND_EDGE_UP + edge_index)) {
                    points[ring->vertex_count++] = ring->found_points[edge_index];
                    if (JK_FLAG_GET(ring->flags, NAV_RING_HAS_CORNER)) {
                        JK_FLAG_SET(ring->flags, NAV_RING_HAS_CORNER, 0);
                        pivot = ring->vertex_count++;
                        points[pivot] = ring->corner;
                    }
                }
            }

            if (pivot == 0 && 4 <= ring->vertex_count
                    && JK_ABS(points[1].z - points[3].z) < JK_ABS(points[0].z - points[2].z)) {
                pivot = 1;
            }

            for (int64_t dest = 0; dest < ring->vertex_count; dest++) {
                int64_t source = JK_MOD(dest + pivot, ring->vertex_count);
                ring->vertices[dest] = world_from_nav(nav_origin, points[source]);
            }
        }

        jk_arena_scope_end(build_navmesh_scope);

        {
            for (int64_t i = 0; i < nav_rings.count; i++) {
                NavRing *ring = nav_rings.e + i;
                NavPoint candidate = closest_point_on_ring(env->state.player_position, ring);
                if (candidate.distance_sqr < start.distance_sqr) {
                    start = candidate;
                }
            }
        }

        JkVec4 yaw_quat = jk_quat_angle_axis(env->state.camera_yaw, (JkVec3){0, 0, 1});

        JkVec3 target = start.p;
        if (env->state.test_frames_remaining <= 0) {
            JkVec3 forward = {0};
            if (jk_key_down(&input.keyboard, JK_KEY_W)) {
                forward.y += 1;
            }
            if (jk_key_down(&input.keyboard, JK_KEY_S)) {
                forward.y -= 1;
            }
            if (jk_key_down(&input.keyboard, JK_KEY_A)) {
                forward.x -= 1;
            }
            if (jk_key_down(&input.keyboard, JK_KEY_D)) {
                forward.x += 1;
            }
            forward = jk_quat_rotate(yaw_quat, forward);
            JkVec3 right = {-forward.y, forward.x};

            JkVec3 a = start.ring->vertices[0];
            JkVec3 b = start.ring->vertices[start.triangle_index - 1];
            JkVec3 c = start.ring->vertices[start.triangle_index];

            JkVec3 up = jk_vec3_cross(jk_vec3_sub(b, a), jk_vec3_sub(c, a));
            if (EPSILON < jk_vec3_magnitude_sqr(up)) {
                up = jk_vec3_normalized(up);
            } else {
                up = (JkVec3){0, 0, 1};
            }

            JkVec3 direction = jk_vec3_cross(right, up);
            if (jk_vec3_magnitude_sqr(direction) < EPSILON) {
                // project the forward vector onto the walk plane
                direction = jk_vec3_sub(forward,
                        jk_vec3_mul(jk_vec3_dot(forward, up) / jk_vec3_magnitude_sqr(up), up));
            }

            if (EPSILON < jk_vec3_magnitude_sqr(direction)) {
                direction = jk_vec3_normalized(direction);
                target = jk_vec3_add(target, jk_vec3_mul(SPEED * DELTA_TIME, direction));
            }
        }

        // Compute max depth
        float step_size = JK_SQRT_2 * nav_density;
        int64_t max_steps = jk_ceil_f32((SPEED * DELTA_TIME) / step_size);
        int64_t max_depth = 2 * max_steps + 1;

        JK_ARENA_SCOPE(scratch0.arena)
        {
            NavPoint destination = {.distance_sqr = jk_infinity_f32.f32};
            NavRingQueue q = q_new(scratch0.arena, 1024);
            q_enqueue(&q, start.ring);
            int64_t depth = 0;
            while (q.start < q.end && depth < max_depth) {
                int64_t depth_end = q.end;
                while (q.start < depth_end) {
                    NavRing *ring = q_dequeue(&q);

                    NavPoint candidate = closest_point_on_ring(target, ring);
                    if (candidate.distance_sqr <= destination.distance_sqr) {
                        destination = candidate;
                    }

                    for (int64_t i = 0; i < 4; i++) {
                        if (!JK_FLAG_GET(ring->neighbors[i]->flags, NAV_RING_ENQUEUED)) {
                            q_enqueue(&q, ring->neighbors[i]);
                        }
                    }
                }
                depth++;
            }
            env->state.player_position = destination.p;
        }

        // ---- Navigation end ------------------------------------------------

        JkTransform camera_transform = {
            .translation =
                    jk_vec3_add(env->state.player_position, (JkVec3){0, 0, player_eye_height}),
            .rotation = jk_quat_mul(
                    yaw_quat, jk_quat_angle_axis(env->state.camera_pitch, (JkVec3){1, 0, 0})),
            .scale = {1, 1, 1},
        };

        clip_from_world = jk_mat4_from_transform_inv(camera_transform);
        clip_from_world = jk_mat4_mul(
                jk_mat4_conversion_to((JkCoordinateSystem){JK_RIGHT, JK_UP, JK_BACKWARD}),
                clip_from_world);
        clip_from_world =
                jk_mat4_mul(jk_mat4_perspective(dimensions, JK_PI / 3, NEAR_CLIP), clip_from_world);

        screen_from_ndc = jk_mat4_translate((JkVec3){1, -1, 0});
        screen_from_ndc =
                jk_mat4_mul(jk_mat4_scale((JkVec3){dimensions.x / 2.0f, -dimensions.y / 2.0f, 1}),
                        screen_from_ndc);

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

            JkMat4 world_from_local = object_compute_world_from_local(objects, object_id);
            JkMat4 clip_from_local = jk_mat4_mul(clip_from_world, world_from_local);

            // jk_mat4_mul_vec4(clip_from_local, jk_vec4_from_3(vertices.e[i], 1));

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
                            clip_from_local, jk_vec4_from_3(vertices.e[face.v[i]], 1));
                    JkVec4 b = jk_mat4_mul_vec4(
                            clip_from_local, jk_vec4_from_3(vertices.e[face.v[b_i]], 1));
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
                                new_node->texture_id = object->texture_id;
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
                triangle_fill(env, &node->tri, textures.e + node->texture_id, bounding_box);
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
            JkColor nav_color = {.r = 0, .g = 255, .b = 0, .a = 255};
            nav_draw_rings(env, screen_from_ndc, clip_from_world, nav_color, start.ring);

            for (int64_t ring_index = 0; ring_index < nav_rings.count; ring_index++) {
            }

            JkShapesRenderer renderer;
            JkShapeArray shapes = (JkShapeArray){
                .count = JK_ARRAY_COUNT(env->assets->shapes), .e = env->assets->shapes};
            float pixels_per_unit = JK_MIN(dimensions.x, dimensions.y) / 64.0f;
            JkVec2 ui_dimensions =
                    jk_vec2_mul(1.0f / pixels_per_unit, jk_vec2_from_i32(dimensions));
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
                    text_color);

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
