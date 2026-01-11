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

static JkVec3 camera_translation_init = {0, 8, 0};
static float camera_rot_angle_init = JK_PI;
static JkVec3 camera_rot_axis_init = {0, 0, 1};

static JkTransform camera_transform;

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

static PixelIndex pixel_index_by_coords(State *state, int32_t x, int32_t y)
{
    return (PixelIndex){DRAW_BUFFER_SIDE_LENGTH * y + x + 1};
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
    int32_t x = texcoord.x * texture.dimensions.x;
    int32_t y = texcoord.y * texture.dimensions.y;
    x = JK_MOD(x, texture.dimensions.x);
    y = JK_MOD(y, texture.dimensions.y);
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

            int32_t alpha = (int32_t)(JK_ABS((coverage[x - bounds.min.x] + acc) * 255.0f));
            if (255 < alpha) {
                alpha = 255;
            }

            if (0 < alpha) {
                JkVec2 point = {x + 0.5, y + 0.5};
                float total_area = jk_vec2_cross(jk_vec2_sub(verts_2d[1], verts_2d[0]),
                        jk_vec2_sub(verts_2d[2], verts_2d[0]));
                float weight[3];
                for (int64_t i = 0; i < 2; i++) {
                    int64_t a = (i + 1) % 3;
                    int64_t b = (i + 2) % 3;
                    float area = jk_vec2_cross(
                            jk_vec2_sub(verts_2d[a], point), jk_vec2_sub(verts_2d[b], point));
                    weight[i] = area / total_area;
                }
                weight[2] = 1 - weight[0] - weight[1];

                JkVec2 texcoord = {0};
                float z = 0;
                for (int64_t i = 0; i < 3; i++) {
                    texcoord = jk_vec2_add(texcoord, jk_vec2_mul(weight[i], tri.t[i]));
                    z += weight[i] * tri.v[i].z;
                }
                texcoord.x /= z;
                texcoord.y /= z;

                JkColor pixel_color = texture_lookup(tri.texture, texcoord);
                pixel_color.a = (uint8_t)alpha;

                PixelIndex head_index = pixel_index_by_coords(state, x, y);
                PixelIndex *head_next = next_get(state, head_index);
                PixelIndex new_pixel_index = pixel_alloc(state);
                if (!pixel_index_nil(new_pixel_index)) {
                    Pixel new_pixel = pixel_get(state, new_pixel_index);

                    *new_pixel.color = pixel_color;
                    *new_pixel.z = z;
                    *new_pixel.next = *head_next;

                    *head_next = new_pixel_index;
                }
            }
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
        return -1;
    } else if (b_avg_z < a_avg_z) {
        return 1;
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

static b32 ran = 0;

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

        camera_transform.translation = camera_translation_init;
        camera_transform.rotation = jk_quat_angle_axis(camera_rot_angle_init, camera_rot_axis_init);
        camera_transform.scale = (JkVec3){1, 1, 1};
    }

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
    camera_move = jk_vec3_mul(5 * DELTA_TIME,
            jk_vec3_normalized(jk_quat_rotate(camera_transform.rotation, camera_move)));

    camera_transform.translation = jk_vec3_add(camera_transform.translation, camera_move);

    state->pixel_count = PIXEL_COUNT / 2;

    // Clear next links
    for (int32_t y = 0; y < state->dimensions.y; y++) {
        jk_memset(state->next_buffer + (y * DRAW_BUFFER_SIDE_LENGTH),
                0,
                JK_SIZEOF(PixelIndex) * state->dimensions.x + 1);
    }

    // int32_t rotation_ticks = rotation_seconds * state->os_timer_frequency;
    // float angle = 2 * JK_PI * ((float)(state->os_time % rotation_ticks) / (float)rotation_ticks);
    // JkVec3 light_dir_n = jk_vec3_normalized(light_dir);

    JkMat4 ndc_matrix = jk_transform_to_mat4_inv(camera_transform);
    ndc_matrix = jk_mat4_mul(
            jk_mat4_conversion_to((JkCoordinateSystem){JK_RIGHT, JK_UP, JK_BACKWARD}), ndc_matrix);
    ndc_matrix = jk_mat4_mul(jk_mat4_perspective(state->dimensions, JK_PI / 2, 0.05f), ndc_matrix);

    JkMat4 pixel_matrix = jk_mat4_translate((JkVec3){1, -1, 0});
    pixel_matrix = jk_mat4_mul(
            jk_mat4_scale((JkVec3){state->dimensions.x / 2.0f, -state->dimensions.y / 2.0f, 1}),
            pixel_matrix);

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

        JkVec3 *screen_vertices =
                jk_arena_push(&object_arena, vertices.count * JK_SIZEOF(*screen_vertices));
        for (int64_t i = 0; i < vertices.count; i++) {
            JkVec4 vec4 = jk_mat4_mul_vec4(ndc_matrix, jk_vec3_to_4(world_vertices[i], 1));
            screen_vertices[i] = jk_mat4_mul_point(pixel_matrix, jk_vec4_perspective_divide(vec4));
        }

        FaceArray faces;
        JK_ARRAY_FROM_SPAN(faces, assets, object->faces);
        Bitmap texture = bitmap_from_span(assets, object->texture);

        JkInt64Array face_ids;
        JK_ARENA_PUSH_ARRAY(&object_arena, face_ids, faces.count);
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
                triangle_fill(&object_arena, state, tri);
            }
        }
    }

    PixelIndex list[16];
    for (int32_t y = 0; y < state->dimensions.y; y++) {
        for (int32_t x = 0; x < state->dimensions.x; x++) {
            Pixel pixel = pixel_get(state, pixel_index_by_coords(state, x, y));
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
        }
    }
}
