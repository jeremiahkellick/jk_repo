// #jk_build single_translation_unit

// #jk_build dependencies_begin
#include <jk_src/jk_lib/gzip/gzip.h>
#include <jk_src/jk_lib/hash_table/hash_table.h>
#include <jk_src/jk_lib/platform/platform.h>
#include <jk_src/pikuma/graphics/graphics.h>
#include <jk_src/stb/stb_truetype.h>
// #jk_build dependencies_end

static JkBuffer file_path = JKSI("../jk_assets/pikuma/graphics/terrain.fbx");
static JkCoordinateSystem coordinate_system = {JK_LEFT, JK_BACKWARD, JK_UP};

static JkMat4 conversion_matrix;
static JkMat4 inv_conversion_matrix;

typedef enum TransformType {
    TRANSFORM_TRANSLATION,
    TRANSFORM_ROTATION,
    TRANSFORM_SCALE,
    TRANSFORM_TYPE_COUNT,
} TransformType;

typedef struct Thing Thing;

typedef struct Link {
    struct Link *next;
    Thing *thing;
} Link;

typedef enum ThingFlag {
    THING_FLAG_HAS_PARENT,
    THING_FLAG_MODEL,
    THING_FLAG_SCALE,
    THING_FLAG_COLLIDE,
    THING_FLAG_WALKABLE,
} ThingFlag;

struct Thing {
    uint64_t flags;

    Link *first_child;

    JkVec3 transform[TRANSFORM_TYPE_COUNT];

    JkSpan faces;
    BitmapSpan image;

    int64_t vertices_base;
    JkInt32Array vertex_indexes;
    int64_t texcoords_base;
    JkInt32Array texcoord_indexes;

    float repeat_size;
};

#define ARBITRARY_CAP 100000

static Link links[ARBITRARY_CAP];
static int64_t link_count = 0;
static Thing things[ARBITRARY_CAP];
static int64_t thing_count = 0;
static JkHashTable *fbx_id_map;
static JkHashTable *bitmap_map;

static Thing *thing_new(int64_t fbx_id)
{
    JK_DEBUG_ASSERT(thing_count < ARBITRARY_CAP);
    Thing *result = things + (thing_count++);
    jk_hash_table_put(fbx_id_map, fbx_id, (int64_t)result);
    return result;
}

static Thing *thing_by_fbx_id(int64_t fbx_id)
{
    Thing *result = 0;
    JkHashTableValue *value = jk_hash_table_get(fbx_id_map, fbx_id);
    if (value) {
        result = (Thing *)*value;
    }
    return result;
}

void thing_connect(int64_t child_fbx_id, int64_t parent_fbx_id)
{
    Thing *child = thing_by_fbx_id(child_fbx_id);
    Thing *parent = thing_by_fbx_id(parent_fbx_id);

    if (child && parent) {
        JK_DEBUG_ASSERT(link_count < ARBITRARY_CAP);
        Link *link = links + (link_count++);
        link->next = parent->first_child;
        link->thing = child;
        parent->first_child = link;
        JK_FLAG_SET(child->flags, THING_FLAG_HAS_PARENT, 1);
    }
}

#if _MSC_VER && !__clang__
#pragma pack(push, 1)
typedef struct BitmapHeader {
#else
typedef struct __attribute__((packed)) BitmapHeader {
#endif
    uint16_t identifier;
    uint32_t size;
    uint32_t reserved;
    uint32_t data_offset;
    uint32_t dib_header_size;
    int32_t width;
    int32_t height;
    uint16_t color_plane_count;
    uint16_t bits_per_pixel;
    uint32_t compression_method;
    uint32_t data_size;
    int32_t h_pixels_per_meter;
    int32_t v_pixels_per_meter;
    uint32_t color_count;
    uint32_t important_color_count;
} BitmapHeader;
#if _MSC_VER && !__clang__
#pragma pack(pop)
#endif

#if _MSC_VER && !__clang__
#pragma pack(push, 1)
typedef struct JkFbxNode {
#else
typedef struct __attribute__((packed)) JkFbxNode {
#endif
    uint32_t end_offset;
    uint32_t property_count;
    uint32_t property_list_size;
    uint8_t name_length;
    uint8_t name[1];
} JkFbxNode;
#if _MSC_VER && !__clang__
#pragma pack(pop)
#endif

#if _MSC_VER && !__clang__
#pragma pack(push, 1)
typedef struct JkFbxHeader {
#else
typedef struct __attribute__((packed)) JkFbxHeader {
#endif
    uint8_t magic[20];
    uint8_t unknown[3];
    uint32_t version;
    uint8_t first_node[1];
} JkFbxHeader;
#if _MSC_VER && !__clang__
#pragma pack(pop)
#endif

#if _MSC_VER && !__clang__
#pragma pack(push, 1)
typedef struct JkFbxArray {
#else
typedef struct __attribute__((packed)) JkFbxArray {
#endif
    uint32_t length;
    b32 encoding;
    uint32_t compressed_length;
    uint8_t contents[1];
} JkFbxArray;
#if _MSC_VER && !__clang__
#pragma pack(pop)
#endif

#if _MSC_VER && !__clang__
#pragma pack(push, 1)
typedef struct JkFbxString {
#else
typedef struct __attribute__((packed)) JkFbxString {
#endif
    uint32_t length;
    uint8_t data[1];
} JkFbxString;
#if _MSC_VER && !__clang__
#pragma pack(pop)
#endif

typedef struct Context {
    JkBuffer directory;
    JkArena *result_arena;
    JkArena *verts_arena;
    JkArena *texcoords_arena;
    JkArena *scratch_arena;
} Context;

static JkBuffer fbx_prop_string_read(uint8_t *base, int64_t *cursor)
{
    JkFbxString *string = (JkFbxString *)(base + *cursor);
    *cursor += JK_SIZEOF(string->length) + string->length;
    return (JkBuffer){.size = string->length, .data = string->data};
}

static BitmapSpan error_bitmap(Context *c)
{
    BitmapSpan result = {
        .dimensions = {2, 2},
        .offset = c->result_arena->pos,
    };
    JkColor *pixels = jk_arena_push(
            c->result_arena, JK_SIZEOF(*pixels) * result.dimensions.x * result.dimensions.y);
    for (int64_t i = 0; i < result.dimensions.x * result.dimensions.y; i++) {
        pixels[i].r = 255;
        pixels[i].g = 0;
        pixels[i].b = 255;
        pixels[i].a = 255;
    }
    return result;
}

static BitmapSpan bitmap_get(Context *c, JkBuffer image_file_name)
{
    JkHashTableKey hash = jk_buffer_hash(image_file_name);
    int64_t *value = jk_hash_table_get(bitmap_map, hash);
    if (value) {
        return *(BitmapSpan *)(*value);
    } else {
        BitmapSpan *span = jk_arena_push(c->scratch_arena, JK_SIZEOF(*span));
        jk_hash_table_put(bitmap_map, hash, (JkHashTableValue)span);

        // Load image data
        JkBuffer image_path =
                JK_FORMAT(c->scratch_arena, jkfs(c->directory), jkfn("/"), jkfs(image_file_name));
        JkBuffer image_file = jk_platform_file_read_full(
                c->scratch_arena, jk_null_terminated_from_buffer(c->scratch_arena, image_path));

        b32 valid = 1;
        int64_t source_size = 0;
        BitmapHeader *header = 0;
        valid = valid && JK_SIZEOF(BitmapHeader) <= image_file.size;
        if (valid) {
            header = (BitmapHeader *)image_file.data;
            source_size = JK_SIZEOF(JkColor3) * header->width * header->height;
        }

        valid = valid && header->identifier == 0x4d42 && header->bits_per_pixel == 24
                && header->compression_method == 0 && 0 < header->width && header->width <= 4096
                && 0 < header->height && header->height <= 4096
                && (header->data_offset + source_size) <= image_file.size;

        if (valid) {
            span->dimensions.x = header->width;
            span->dimensions.y = header->height;
            span->offset = c->result_arena->pos;
            JkColor3 *src_pointer = (JkColor3 *)(image_file.data + header->data_offset);
            JkColor3 *src_endpoint =
                    (JkColor3 *)(image_file.data + header->data_offset + source_size);
            JkColor *dest_pointer = jk_arena_push(
                    c->result_arena, JK_SIZEOF(JkColor) * span->dimensions.x * span->dimensions.y);
            while (src_pointer < src_endpoint) {
                *dest_pointer++ = jk_color4_from_3(*src_pointer++, 0xff);
            }
        } else {
            jk_log(JK_LOG_ERROR, JKS("bitmap_get: Invalid image file format, expects 24-bit BMP"));
            *span = error_bitmap(c);
        }

        return *span;
    }
}

static JkDoubleArray read_doubles(Context *c, JkFbxNode *node)
{
    JkDoubleArray result = {0};
    uint8_t type = *(node->name + node->name_length);
    if (0 < node->property_count && type == 'd') {
        JkFbxArray *array = (JkFbxArray *)(node->name + node->name_length + 1);
        int64_t byte_count = array->length * JK_SIZEOF(*result.e);
        if (array->encoding == 0) {
            result.e = (double *)array->contents;
        } else if (array->encoding == 1) {
            JkBuffer compressed_data = {.size = array->compressed_length, .data = array->contents};
            JkBuffer decompressed_data =
                    jk_zlib_decompress(c->scratch_arena, compressed_data, byte_count);
            if (decompressed_data.size == byte_count) {
                result.e = (double *)decompressed_data.data;
            } else {
                jk_log(JK_LOG_ERROR, JKS("read_doubles: Failed to decompress"));
            }
        } else {
            jk_log(JK_LOG_ERROR, JKS("read_doubles: Unrecognized array encoding"));
        }
        if (result.e) {
            result.count = array->length;
        }
    } else {
        JK_LOGF(JK_LOG_ERROR,
                jkfn("read_doubles: expected type 'd', got '"),
                jkfc(type),
                jkfn("'"));
    }
    return result;
}

static JkInt32Array read_ints(Context *c, JkFbxNode *node)
{
    JkInt32Array result = {0};
    uint8_t type = *(node->name + node->name_length);
    if (0 < node->property_count && type == 'i') {
        JkFbxArray *array = (JkFbxArray *)(node->name + node->name_length + 1);
        int64_t byte_count = array->length * JK_SIZEOF(*result.e);
        if (array->encoding == 0) {
            result.e = (int32_t *)array->contents;
        } else if (array->encoding == 1) {
            JkBuffer compressed_data = {.size = array->compressed_length, .data = array->contents};
            JkBuffer decompressed_data =
                    jk_zlib_decompress(c->scratch_arena, compressed_data, byte_count);
            if (decompressed_data.size == byte_count) {
                result.e = (int32_t *)decompressed_data.data;
            } else {
                jk_log(JK_LOG_ERROR, JKS("read_ints: Failed to decompress"));
            }
        } else {
            jk_log(JK_LOG_ERROR, JKS("read_ints: Unrecognized array encoding"));
        }
        if (result.e) {
            result.count = array->length;
        }
    } else {
        JK_LOGF(JK_LOG_ERROR, jkfn("read_ints: expected type 'i', got '"), jkfc(type), jkfn("'"));
    }
    return result;
}

static void process_fbx_nodes(Context *c, JkBuffer file, int64_t pos, Thing *thing)
{
    while (0 < pos && pos < file.size) {
        JkFbxNode *node = (JkFbxNode *)(file.data + pos);
        int64_t pos_children =
                (node->name - file.data) + node->name_length + node->property_list_size;
        JkBuffer name = {.size = node->name_length, .data = node->name};

        b32 is_vertex_indexes = jk_buffer_compare(name, JKS("PolygonVertexIndex")) == 0;
        b32 is_texcoord_indexes = jk_buffer_compare(name, JKS("UVIndex")) == 0;
        b32 is_model = jk_buffer_compare(name, JKS("Model")) == 0;

        if (jk_buffer_compare(name, JKS("Vertices")) == 0) {
            JkDoubleArray coords = read_doubles(c, node);
            int64_t vertex_count = coords.count / 3;
            JkVec3 *vertices = jk_arena_push(c->verts_arena, vertex_count * JK_SIZEOF(*vertices));
            for (int64_t vertex_index = 0; vertex_index < vertex_count; vertex_index++) {
                for (int64_t coord_index = 0; coord_index < 3; coord_index++) {
                    vertices[vertex_index].v[coord_index] =
                            coords.e[vertex_index * 3 + coord_index];
                }
                vertices[vertex_index] =
                        jk_mat4_mul_point(conversion_matrix, vertices[vertex_index]);
            }
        } else if (jk_buffer_compare(name, JKS("UV")) == 0) {
            JkDoubleArray coords = read_doubles(c, node);
            int64_t texcoord_count = coords.count / 2;
            JkVec2 *texcoords =
                    jk_arena_push(c->texcoords_arena, texcoord_count * JK_SIZEOF(*texcoords));
            for (int64_t texcoord_index = 0; texcoord_index < texcoord_count; texcoord_index++) {
                for (int64_t coord_index = 0; coord_index < 2; coord_index++) {
                    texcoords[texcoord_index].v[coord_index] =
                            coords.e[texcoord_index * 2 + coord_index];
                }
            }
        } else if (is_vertex_indexes || is_texcoord_indexes) {
            if (thing) {
                JkInt32Array indexes = read_ints(c, node);
                if (is_vertex_indexes) {
                    thing->vertex_indexes = indexes;
                } else {
                    thing->texcoord_indexes = indexes;
                }
            } else {
                jk_log(JK_LOG_ERROR, JKS("process_fbx_nodes: Nothing to write indexes to"));
            }
        } else if (jk_buffer_compare(name, JKS("C")) == 0) {
            b32 valid = 1;
            int64_t child_fbx_id = -1;
            int64_t parent_fbx_id = -1;

            int64_t cursor = node->name_length;

            valid = valid && 3 <= node->property_count && node->name[cursor++] == 'S';
            if (valid) {
                fbx_prop_string_read(node->name, &cursor);
            }

            valid = valid && node->name[cursor++] == 'L';
            if (valid) {
                child_fbx_id = *(int64_t *)(node->name + cursor);
                cursor += JK_SIZEOF(child_fbx_id);
            }

            valid = valid && node->name[cursor++] == 'L';
            if (valid) {
                parent_fbx_id = *(int64_t *)(node->name + cursor);
                thing_connect(child_fbx_id, parent_fbx_id);
            } else {
                jk_log(JK_LOG_ERROR, JKS("process_fbx_nodes: Problem parsing connection"));
            }
        } else if (jk_buffer_compare(name, JKS("RelativeFilename")) == 0) {
            int64_t cursor = node->name_length;
            if (1 <= node->property_count && node->name[cursor++] == 'S') {
                JkFbxString *string = (JkFbxString *)(node->name + cursor);
                JkBuffer image_file_name = {.size = string->length, .data = string->data};

                if (thing) {
                    thing->image = bitmap_get(c, image_file_name);
                }
            } else {
                jk_log(JK_LOG_ERROR, JKS("process_fbx_nodes: Problem parsing RelativeFilename"));
            }
        } else if (jk_buffer_compare(name, JKS("P")) == 0) {
            b32 proceed = 1;
            b32 has_repeat_size = 0;
            b32 has_collide = 0;
            b32 has_walkable = 0;

            TransformType type = 0;

            int64_t cursor = node->name_length;
            proceed =
                    proceed && !!thing && 3 <= node->property_count && node->name[cursor++] == 'S';
            if (proceed) {
                JkBuffer string = fbx_prop_string_read(node->name, &cursor);
                if (jk_buffer_compare(string, JKS("Lcl Translation")) == 0) {
                    type = TRANSFORM_TRANSLATION;
                } else if (jk_buffer_compare(string, JKS("Lcl Rotation")) == 0) {
                    type = TRANSFORM_ROTATION;
                } else if (jk_buffer_compare(string, JKS("Lcl Scaling")) == 0) {
                    type = TRANSFORM_SCALE;
                } else if (jk_buffer_compare(string, JKS("repeat_size")) == 0) {
                    has_repeat_size = 1;
                } else if (jk_buffer_compare(string, JKS("collide")) == 0) {
                    has_collide = 1;
                } else if (jk_buffer_compare(string, JKS("walkable")) == 0) {
                    has_walkable = 1;
                } else {
                    proceed = 0;
                }
            }

            // Skip type label and flags properties
            for (int64_t i = 0; i < 3 && proceed; i++) {
                proceed = proceed && node->name[cursor++] == 'S';
                fbx_prop_string_read(node->name, &cursor);
            }

            if (proceed && has_repeat_size) {
                if (node->name[cursor++] == 'D') {
                    thing->repeat_size = *(double *)(node->name + cursor);
                }
                proceed = 0;
            }

            if (proceed && has_collide) {
                if (node->name[cursor++] == 'I') {
                    int32_t collide = *(int32_t *)(node->name + cursor);
                    JK_FLAG_SET(thing->flags, THING_FLAG_COLLIDE, collide);
                }
                proceed = 0;
            }

            if (proceed && has_walkable) {
                if (node->name[cursor++] == 'I') {
                    int32_t walkable = *(int32_t *)(node->name + cursor);
                    JK_FLAG_SET(thing->flags, THING_FLAG_WALKABLE, walkable);
                }
                proceed = 0;
            }

            proceed = proceed && 7 <= node->property_count;

            if (proceed) {
                for (int64_t i = 0; i < 3 && proceed; i++) {
                    proceed = proceed && node->name[cursor++] == 'D';
                    if (proceed) {
                        thing->transform[type].v[i] = *(double *)(node->name + cursor);
                        cursor += sizeof(double);
                    }
                }

                if (proceed && type == TRANSFORM_SCALE) {
                    JK_FLAG_SET(thing->flags, THING_FLAG_SCALE, 1);
                }
            }
        } else if (jk_buffer_compare(name, JKS("Connections")) == 0
                || jk_buffer_compare(name, JKS("LayerElementUV")) == 0
                || jk_buffer_compare(name, JKS("Objects")) == 0) {
            process_fbx_nodes(c, file, pos_children, thing);
        } else if (jk_buffer_compare(name, JKS("Properties70")) == 0) {
            process_fbx_nodes(c, file, pos_children, thing);
        } else if (jk_buffer_compare(name, JKS("Geometry")) == 0
                || jk_buffer_compare(name, JKS("Material")) == 0 || is_model
                || jk_buffer_compare(name, JKS("Texture")) == 0) {
            uint8_t type = *(node->name + node->name_length);
            if (0 < node->property_count && type == 'L') {
                int64_t fbx_id = *(int64_t *)(node->name + node->name_length + 1);
                Thing *new_thing = thing_new(fbx_id);
                JK_FLAG_SET(new_thing->flags, THING_FLAG_MODEL, is_model);
                new_thing->vertices_base = c->verts_arena->pos / JK_SIZEOF(JkVec3);
                new_thing->texcoords_base = c->texcoords_arena->pos / JK_SIZEOF(JkVec2);
                process_fbx_nodes(c, file, pos_children, new_thing);
            } else {
                JK_LOGF(JK_LOG_ERROR,
                        jkfn("read_doubles: For object ID, expected type 'L', got '"),
                        jkfc(type),
                        jkfn("'"));
            }
        }

        pos = node->end_offset;
    }
}

static ObjectId object_new(JkArenaScope objects_scope)
{
    Object *object = jk_arena_push(objects_scope.arena, JK_SIZEOF(*object));
    return (ObjectId){object - (Object *)(objects_scope.arena->memory.data + objects_scope.base)};
}

static Object *object_get(JkArenaScope objects_scope, ObjectId id)
{
    return (Object *)(objects_scope.arena->memory.data + objects_scope.base) + id.i;
}

static void process_thing(JkArenaScope objects_scope, Thing *thing, ObjectId object_id)
{
    if (!thing) {
        return;
    }

    if (JK_FLAG_GET(thing->flags, THING_FLAG_MODEL)) {
        ObjectId parent = object_id;
        object_id = object_new(objects_scope);
        Object *object = object_get(objects_scope, object_id);
        object->parent = parent;

        JkMat4 local_matrix = inv_conversion_matrix;
        if (JK_FLAG_GET(thing->flags, THING_FLAG_SCALE)) {
            local_matrix =
                    jk_mat4_mul(jk_mat4_scale(thing->transform[TRANSFORM_SCALE]), local_matrix);
        }
        local_matrix = jk_mat4_mul(
                jk_mat4_rotate_x(thing->transform[TRANSFORM_ROTATION].x * (JK_PI / 180)),
                local_matrix);
        local_matrix = jk_mat4_mul(
                jk_mat4_rotate_y(thing->transform[TRANSFORM_ROTATION].y * (JK_PI / 180)),
                local_matrix);
        local_matrix = jk_mat4_mul(
                jk_mat4_rotate_z(thing->transform[TRANSFORM_ROTATION].z * (JK_PI / 180)),
                local_matrix);
        local_matrix = jk_mat4_mul(
                jk_mat4_translate(thing->transform[TRANSFORM_TRANSLATION]), local_matrix);
        local_matrix = jk_mat4_mul(conversion_matrix, local_matrix);

        // Strip translation values from matrix
        for (int64_t i = 0; i < 3; i++) {
            object->transform.translation.v[i] = local_matrix.e[i][3];
            local_matrix.e[i][3] = 0;
        }

        // Strip scale from matrix
        for (int64_t j = 0; j < 3; j++) {
            JkVec3 v = {local_matrix.e[0][j], local_matrix.e[1][j], local_matrix.e[2][j]};
            float magnitude = jk_vec3_magnitude(v);
            object->transform.scale.v[j] = magnitude;
            for (int64_t i = 0; i < 3; i++) {
                local_matrix.e[i][j] = v.v[i] / magnitude;
            }
        }

        object->transform.rotation = jk_quat_from_mat4(local_matrix);
    }

    if (object_id.i) {
        Object *object = object_get(objects_scope, object_id);
        if (thing->faces.size) {
            object->faces = thing->faces;
        }
        if (thing->image.offset) {
            object->texture = thing->image;
        }
        if (thing->repeat_size) {
            object->repeat_size = thing->repeat_size;
        }
        if (JK_FLAG_GET(thing->flags, THING_FLAG_COLLIDE)) {
            JK_FLAG_SET(object->flags, OBJ_COLLIDE, 1);
        }
        if (JK_FLAG_GET(thing->flags, THING_FLAG_WALKABLE)) {
            JK_FLAG_SET(object->flags, OBJ_WALKABLE, 1);
        }
    }

    for (Link *child = thing->first_child; child; child = child->next) {
        process_thing(objects_scope, child->thing, object_id);
    }
}

JkSpan arena_scope_span(JkArenaScope scope)
{
    return (JkSpan){
        .size = scope.arena->pos - scope.base,
        .offset = scope.base,
    };
}

JkSpan append_arena(JkArena *dest, JkArena *src)
{
    JkSpan result = {.size = src->pos, .offset = dest->pos};
    uint8_t *data = jk_arena_push(dest, result.size);
    jk_memcpy(data, src->memory.data, result.size);
    return result;
}

int32_t jk_platform_entry_point(int32_t argc, char **argv)
{
    jk_platform_set_working_directory_to_executable_directory();

    conversion_matrix = jk_mat4_conversion_from(coordinate_system);
    inv_conversion_matrix = jk_mat4_transpose(conversion_matrix);

    Context context;
    Context *c = &context;
    c->directory = jk_path_directory(file_path);

    JkArena result_arena = jk_platform_arena_virtual_init(8 * JK_GIGABYTE);
    c->result_arena = &result_arena;

    JkArena verts_arena = jk_platform_arena_virtual_init(1 * JK_GIGABYTE);
    c->verts_arena = &verts_arena;

    JkArena texcoords_arena = jk_platform_arena_virtual_init(1 * JK_GIGABYTE);
    c->texcoords_arena = &texcoords_arena;

    JkArena scratch_arena = jk_platform_arena_virtual_init(1 * JK_GIGABYTE);
    c->scratch_arena = &scratch_arena;

    JkBuffer file = jk_platform_file_read_full(&scratch_arena, (char *)file_path.data);
    JkFbxHeader *header = (JkFbxHeader *)file.data;

    fbx_id_map = jk_hash_table_create_capacity(32);
    bitmap_map = jk_hash_table_create_capacity(32);

    if (jk_buffer_compare((JkBuffer){.size = JK_SIZEOF(header->magic), .data = header->magic},
                JKS("Kaydara FBX Binary  "))
            != 0) {
        JK_LOGF(JK_LOG_ERROR, jkfn("'"), jkfs(file_path), jkfn("': unrecognized file format"));
        return 1;
    }

    Assets *assets = jk_arena_push(&result_arena, JK_SIZEOF(*assets));

    // Fill out the rest of the shapes array with font data
    JK_ARENA_SCOPE(&scratch_arena)
    {
        char *ttf_file_name = "../jk_assets/pikuma/graphics/Inconsolata-Regular.ttf";
        JkBuffer ttf_file = jk_platform_file_read_full(&scratch_arena, ttf_file_name);
        if (!ttf_file.size) {
            JK_LOGF(JK_LOG_ERROR, jkfn("Failed to read file '"), jkfn(ttf_file_name), jkfn("'"));
            return 1;
        }

        stbtt_fontinfo font;

        stbtt_InitFont(&font, ttf_file.data, stbtt_GetFontOffsetForIndex(ttf_file.data, 0));

        {
            int32_t advance_width;
            stbtt_GetCodepointHMetrics(&font, ' ', &advance_width, 0);
            assets->shapes[1].advance_width = (float)advance_width;
        }

        assets->font_ascent = jk_infinity_f32.f32;
        assets->font_descent = -jk_infinity_f32.f32;
        for (int64_t shape_index = 2; shape_index < JK_ARRAY_COUNT(assets->shapes); shape_index++) {
            JkShape *shape = assets->shapes + shape_index;
            int32_t codepoint = shape_index - ASCII_TO_SHAPE_OFFSET;

            int32_t x0, x1, y0, y1;
            stbtt_GetCodepointBox(&font, codepoint, &x0, &y0, &x1, &y1);

            // Since the font uses y values that grow from the bottom. Negate and swap y0 and y1.
            int32_t tmp = y0;
            y0 = -y1;
            y1 = -tmp;

            shape->offset.x = (float)x0;
            shape->offset.y = (float)y0;
            shape->dimensions.x = (float)(x1 - x0);
            shape->dimensions.y = (float)(y1 - y0);

            int32_t advance_width;
            stbtt_GetCodepointHMetrics(&font, codepoint, &advance_width, 0);
            shape->advance_width = (float)advance_width;

            if (y0 < assets->font_ascent) {
                assets->font_ascent = (float)y0;
            }
            if (assets->font_descent < y1) {
                assets->font_descent = (float)y1;
            }
            if (codepoint == 'A') {
                assets->font_monospace_advance_width = (float)advance_width;
            }

            stbtt_vertex *verticies;
            int64_t command_count = stbtt_GetCodepointShape(&font, codepoint, &verticies);
            shape->commands.size = JK_SIZEOF(JkShapesPenCommand) * command_count;
            shape->commands.offset = result_arena.pos;
            JkShapesPenCommand *commands = jk_arena_push_zero(&result_arena, shape->commands.size);
            for (int64_t i = 0; i < command_count; i++) {
                switch (verticies[i].type) {
                case STBTT_vmove:
                case STBTT_vline: {
                    commands[i].type = verticies[i].type == STBTT_vmove
                            ? JK_SHAPES_PEN_COMMAND_MOVE
                            : JK_SHAPES_PEN_COMMAND_LINE;
                    commands[i].v[0].x = (float)verticies[i].x;
                    commands[i].v[0].y = (float)-verticies[i].y;
                } break;

                case STBTT_vcurve: {
                    commands[i].type = JK_SHAPES_PEN_COMMAND_CURVE_QUADRATIC;
                    commands[i].v[0].x = (float)verticies[i].cx;
                    commands[i].v[0].y = (float)-verticies[i].cy;
                    commands[i].v[1].x = (float)verticies[i].x;
                    commands[i].v[1].y = (float)-verticies[i].y;
                } break;

                case STBTT_vcubic: {
                    commands[i].type = JK_SHAPES_PEN_COMMAND_CURVE_CUBIC;
                    commands[i].v[0].x = (float)verticies[i].cx;
                    commands[i].v[0].y = (float)-verticies[i].cy;
                    commands[i].v[1].x = (float)verticies[i].cx1;
                    commands[i].v[1].y = (float)-verticies[i].cy1;
                    commands[i].v[2].x = (float)verticies[i].x;
                    commands[i].v[2].y = (float)-verticies[i].y;
                } break;

                default: {
                    JK_ASSERT(0 && "Unsupported vertex type");
                } break;
                }
            }
        }
    }

    process_fbx_nodes(c, file, header->first_node - file.data, 0);

    assets->vertices = append_arena(&result_arena, c->verts_arena);
    assets->texcoords = append_arena(&result_arena, c->texcoords_arena);

    // Process faces
    for (int64_t thing_index = 0; thing_index < thing_count; thing_index++) {
        Thing *thing = things + thing_index;
        int64_t faces_base = result_arena.pos;
        int64_t i = 0;
        while (i < thing->vertex_indexes.count) {
            Face *face = jk_arena_push_zero(&result_arena, JK_SIZEOF(*face));
            int64_t point_index = 0;
            b32 end_of_polygon = 0;
            while (i < thing->vertex_indexes.count && !end_of_polygon) {
                int32_t vertex_index = thing->vertex_indexes.e[i];
                if (vertex_index < 0) {
                    if (point_index != 2) {
                        fprintf(stderr, "process_fbx_nodes: Encountered non-triangle polygon");
                    }
                    end_of_polygon = 1;
                    vertex_index = ~vertex_index;
                }
                if (point_index < JK_ARRAY_COUNT(face->v)) {
                    face->v[point_index] = thing->vertices_base + vertex_index;
                    if (i < thing->texcoord_indexes.count) {
                        face->t[point_index] = thing->texcoords_base + thing->texcoord_indexes.e[i];
                    }
                    point_index++;
                } else {
                    jk_log(JK_LOG_ERROR,
                            JKS("process_fbx_nodes: Encountered non-triangle polygon"));
                }
                i++;
            }
        }
        if (faces_base < result_arena.pos) {
            thing->faces.offset = faces_base;
            thing->faces.size = result_arena.pos - faces_base;
        }
    }

    JkArenaScope objects_scope = jk_arena_scope_begin(&result_arena);
    jk_arena_push(objects_scope.arena, JK_SIZEOF(Object)); // Push nil object
    for (int64_t i = 0; i < thing_count; i++) {
        Thing *thing = things + i;
        if (!JK_FLAG_GET(thing->flags, THING_FLAG_HAS_PARENT)) {
            process_thing(objects_scope, thing, (ObjectId){0});
        }
    }
    assets->objects = arena_scope_span(objects_scope);

    jk_platform_file_write(JKS("graphics_assets"), jk_buffer_from_arena(&result_arena));

    jk_platform_write_as_c_byte_array(jk_buffer_from_arena(&result_arena),
            JKS("../jk_gen/pikuma/graphics/assets.c"),
            JKS("assets_byte_array"));

    return 0;
}
