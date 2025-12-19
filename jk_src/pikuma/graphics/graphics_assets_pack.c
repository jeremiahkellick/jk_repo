#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// #jk_build single_translation_unit

// #jk_build dependencies_begin
#include <jk_src/jk_lib/gzip/gzip.h>
#include <jk_src/jk_lib/hash_table/hash_table.h>
#include <jk_src/jk_lib/platform/platform.h>
#include <jk_src/pikuma/graphics/graphics.h>
// #jk_build dependencies_end

JkBuffer file_path = JKSI("../jk_assets/pikuma/graphics/models.fbx");

typedef struct ThingId {
    int64_t i;
} ThingId;

typedef enum ThingFlag {
    THING_FLAG_HAS_PARENT,
    THING_FLAG_MODEL,
} ThingFlag;

typedef struct Thing {
    uint64_t flags;

    ThingId kid;
    ThingId bro;

    JkTransform transform;
    JkSpan faces;
    BitmapSpan image;

    int64_t vertices_base;
    JkInt32Array vertex_indexes;
    int64_t texcoords_base;
    JkInt32Array texcoord_indexes;
} Thing;

#define MAX_THINGS 100000

static Thing things[MAX_THINGS];
static int64_t thing_count = 1;
static JkHashTable *fbx_id_map;
static JkHashTable *bitmap_map;

static ThingId thing_new(int64_t fbx_id)
{
    JK_DEBUG_ASSERT(thing_count < MAX_THINGS);
    ThingId result = {thing_count++};
    jk_hash_table_put(fbx_id_map, fbx_id, result.i);
    return result;
}

static ThingId thing_by_fbx_id(int64_t fbx_id)
{
    ThingId result = {0};
    JkHashTableValue *id = jk_hash_table_get(fbx_id_map, fbx_id);
    if (id) {
        result.i = *id;
    }
    return result;
}

static Thing *thing_get(ThingId id)
{
    JK_DEBUG_ASSERT(0 <= id.i && id.i < thing_count);
    return things + id.i;
}

void thing_connect(int64_t kid_fbx_id, int64_t parent_fbx_id)
{
    ThingId kid_id = thing_by_fbx_id(kid_fbx_id);
    ThingId parent_id = thing_by_fbx_id(parent_fbx_id);

    if (kid_id.i && parent_id.i) {
        Thing *kid = thing_get(kid_id);
        Thing *parent = thing_get(parent_id);

        kid->bro = parent->kid;
        parent->kid = kid_id;
        JK_FLAG_SET(kid->flags, THING_FLAG_HAS_PARENT, 1);
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

static BitmapSpan error_bitmap(Context *c)
{
    BitmapSpan result = {
        .dimensions = {2, 2},
        .offset = c->result_arena->pos - c->result_arena->base,
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
                c->scratch_arena, jk_buffer_to_null_terminated(c->scratch_arena, image_path));

        b32 valid = 1;
        BitmapHeader *header = 0;
        valid = valid && JK_SIZEOF(BitmapHeader) <= image_file.size;
        if (valid) {
            header = (BitmapHeader *)image_file.data;
        }

        valid = valid && header->identifier == 0x4d42 && header->bits_per_pixel == 24
                && header->compression_method == 0 && 0 < header->width && header->width <= 4096
                && 0 < header->height && header->height <= 4096
                && header->data_size == (JK_SIZEOF(JkColor3) * header->width * header->height)
                && (header->data_offset + header->data_size) <= image_file.size;

        /*
        Color *pixels = (Color *)(image_file.data + header->offset);
        for (int32_t y = 0; y < ATLAS_HEIGHT; y++) {
            int32_t atlas_y = ATLAS_HEIGHT - y - 1;
            for (int32_t x = 0; x < ATLAS_WIDTH; x++) {
                global_chess.atlas[atlas_y * ATLAS_WIDTH + x] = pixels[y * ATLAS_WIDTH + x].a;
            }
        }
        */

        if (valid) {
            span->dimensions.x = header->width;
            span->dimensions.y = header->height;
            span->offset = c->result_arena->pos - c->result_arena->base;
            jk_memcpy(jk_arena_push(c->result_arena, header->data_size),
                    image_file.data + header->data_offset,
                    header->data_size);
        } else {
            fprintf(stderr, "bitmap_get: Invalid image file format, expects 32-bit BMP\n");
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
        int64_t byte_count = array->length * JK_SIZEOF(*result.items);
        if (array->encoding == 0) {
            result.items = (double *)array->contents;
        } else if (array->encoding == 1) {
            JkBuffer compressed_data = {.size = array->compressed_length, .data = array->contents};
            JkBuffer decompressed_data =
                    jk_zlib_decompress(c->scratch_arena, compressed_data, byte_count);
            if (decompressed_data.size == byte_count) {
                result.items = (double *)decompressed_data.data;
            } else {
                fprintf(stderr, "read_doubles: Failed to decompress\n");
            }
        } else {
            fprintf(stderr, "read_doubles: Unrecognized array encoding\n");
        }
        if (result.items) {
            result.count = array->length;
        }
    } else {
        fprintf(stderr, "read_doubles: expected type 'd', got '%c'\n", type);
    }
    return result;
}

static JkInt32Array read_ints(Context *c, JkFbxNode *node)
{
    JkInt32Array result = {0};
    uint8_t type = *(node->name + node->name_length);
    if (0 < node->property_count && type == 'i') {
        JkFbxArray *array = (JkFbxArray *)(node->name + node->name_length + 1);
        int64_t byte_count = array->length * JK_SIZEOF(*result.items);
        if (array->encoding == 0) {
            result.items = (int32_t *)array->contents;
        } else if (array->encoding == 1) {
            JkBuffer compressed_data = {.size = array->compressed_length, .data = array->contents};
            JkBuffer decompressed_data =
                    jk_zlib_decompress(c->scratch_arena, compressed_data, byte_count);
            if (decompressed_data.size == byte_count) {
                result.items = (int32_t *)decompressed_data.data;
            } else {
                fprintf(stderr, "read_ints: Failed to decompress\n");
            }
        } else {
            fprintf(stderr, "read_ints: Unrecognized array encoding\n");
        }
        if (result.items) {
            result.count = array->length;
        }
    } else {
        fprintf(stderr, "read_ints: expected type 'i', got '%c'\n", type);
    }
    return result;
}

static void process_fbx_nodes(Context *c, JkBuffer file, int64_t pos, ThingId thing_id)
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
                    vertices[vertex_index].coords[coord_index] =
                            coords.items[vertex_index * 3 + coord_index];
                }
            }
        } else if (jk_buffer_compare(name, JKS("UV")) == 0) {
            JkDoubleArray coords = read_doubles(c, node);
            int64_t texcoord_count = coords.count / 2;
            JkVec2 *texcoords =
                    jk_arena_push(c->texcoords_arena, texcoord_count * JK_SIZEOF(*texcoords));
            for (int64_t texcoord_index = 0; texcoord_index < texcoord_count; texcoord_index++) {
                for (int64_t coord_index = 0; coord_index < 2; coord_index++) {
                    texcoords[texcoord_index].coords[coord_index] =
                            coords.items[texcoord_index * 2 + coord_index];
                }
            }
        } else if (is_vertex_indexes || is_texcoord_indexes) {
            if (thing_id.i) {
                Thing *thing = thing_get(thing_id);
                JkInt32Array indexes = read_ints(c, node);
                if (is_vertex_indexes) {
                    thing->vertex_indexes = indexes;
                } else {
                    thing->texcoord_indexes = indexes;
                }
            } else {
                fprintf(stderr, "process_fbx_nodes: Nothing to write indexes to\n");
            }
        } else if (jk_buffer_compare(name, JKS("C")) == 0) {
            b32 valid = 1;
            int64_t kid_fbx_id = -1;
            int64_t parent_fbx_id = -1;

            int64_t cursor = node->name_length;

            valid = valid && 3 <= node->property_count && node->name[cursor++] == 'S';
            if (valid) {
                JkFbxString *string = (JkFbxString *)(node->name + cursor);
                cursor += JK_SIZEOF(string->length) + string->length;
            }

            valid = valid && node->name[cursor++] == 'L';
            if (valid) {
                kid_fbx_id = *(int64_t *)(node->name + cursor);
                cursor += JK_SIZEOF(kid_fbx_id);
            }

            valid = valid && node->name[cursor++] == 'L';
            if (valid) {
                parent_fbx_id = *(int64_t *)(node->name + cursor);
                thing_connect(kid_fbx_id, parent_fbx_id);
            } else {
                fprintf(stderr, "process_fbx_nodes: Problem parsing connection\n");
            }
        } else if (jk_buffer_compare(name, JKS("RelativeFilename")) == 0) {
            int64_t cursor = node->name_length;
            if (1 <= node->property_count && node->name[cursor++] == 'S') {
                JkFbxString *string = (JkFbxString *)(node->name + cursor);
                JkBuffer image_file_name = {.size = string->length, .data = string->data};

                if (thing_id.i) {
                    Thing *thing = thing_get(thing_id);
                    thing->image = bitmap_get(c, image_file_name);
                }
            } else {
                fprintf(stderr, "process_fbx_nodes: Problem parsing RelativeFilename\n");
            }
        } else if (jk_buffer_compare(name, JKS("Connections")) == 0
                || jk_buffer_compare(name, JKS("LayerElementUV")) == 0
                || jk_buffer_compare(name, JKS("Objects")) == 0) {
            process_fbx_nodes(c, file, pos_children, thing_id);
        } else if (jk_buffer_compare(name, JKS("Geometry")) == 0
                || jk_buffer_compare(name, JKS("Material")) == 0 || is_model
                || jk_buffer_compare(name, JKS("Texture")) == 0) {
            uint8_t type = *(node->name + node->name_length);
            if (0 < node->property_count && type == 'L') {
                int64_t fbx_id = *(int64_t *)(node->name + node->name_length + 1);
                ThingId new_thing_id = thing_new(fbx_id);
                Thing *thing = thing_get(new_thing_id);
                JK_FLAG_SET(thing->flags, THING_FLAG_MODEL, is_model);
                thing->vertices_base = c->verts_arena->pos / JK_SIZEOF(JkVec3);
                thing->texcoords_base = c->texcoords_arena->pos / JK_SIZEOF(JkVec2);
                process_fbx_nodes(c, file, pos_children, new_thing_id);
            } else {
                fprintf(stderr, "read_doubles: For object ID, expected type 'L', got '%c'\n", type);
            }
        }

        pos = node->end_offset;
    }
}

static ObjectId object_new(JkArena *objects_arena)
{
    Object *object = jk_arena_push(objects_arena, JK_SIZEOF(*object));
    return (ObjectId){object - (Object *)(objects_arena->root->memory.data + objects_arena->base)};
}

static Object *object_get(JkArena *objects_arena, ObjectId id)
{
    return (Object *)(objects_arena->root->memory.data + objects_arena->base) + id.i;
}

static void process_thing(JkArena *objects_arena, ThingId thing_id, ObjectId object_id)
{
    if (!thing_id.i) {
        return;
    }

    Thing *thing = thing_get(thing_id);

    if (JK_FLAG_GET(thing->flags, THING_FLAG_MODEL)) {
        object_id = object_new(objects_arena);
    }

    if (object_id.i) {
        Object *object = object_get(objects_arena, object_id);
        if (thing->faces.size) {
            object->faces = thing->faces;
        }
        if (thing->image.offset) {
            object->texture = thing->image;
        }
    }

    for (ThingId kid = thing->kid; kid.i; kid = thing_get(kid)->bro) {
        process_thing(objects_arena, kid, object_id);
    }
}

JkSpan child_arena_span(JkArena *parent, JkArena *child)
{
    return (JkSpan){
        .size = child->pos - child->base,
        .offset = child->base - parent->base,
    };
}

JkSpan append_arena(JkArena *dest, JkArena *src)
{
    JkSpan result = {
        .size = src->pos - src->base,
        .offset = dest->pos - dest->base,
    };
    uint8_t *data = jk_arena_push(dest, result.size);
    jk_memcpy(data, src->root->memory.data + src->base, result.size);
    return result;
}

int main(int argc, char **argv)
{
    jk_print = jk_platform_print_stdout;
    jk_platform_set_working_directory_to_executable_directory();

    Context context;
    Context *c = &context;
    c->directory = jk_path_directory(file_path);

    JkPlatformArenaVirtualRoot result_arena_root;
    JkArena result_arena = jk_platform_arena_virtual_init(&result_arena_root, 8 * JK_GIGABYTE);
    c->result_arena = &result_arena;

    JkPlatformArenaVirtualRoot verts_arena_root;
    JkArena verts_arena = jk_platform_arena_virtual_init(&verts_arena_root, 1 * JK_GIGABYTE);
    c->verts_arena = &verts_arena;

    JkPlatformArenaVirtualRoot texcoords_arena_root;
    JkArena texcoords_arena =
            jk_platform_arena_virtual_init(&texcoords_arena_root, 1 * JK_GIGABYTE);
    c->texcoords_arena = &texcoords_arena;

    JkPlatformArenaVirtualRoot scratch_arena_root;
    JkArena scratch_arena = jk_platform_arena_virtual_init(&scratch_arena_root, 1 * JK_GIGABYTE);
    c->scratch_arena = &scratch_arena;

    JkBuffer file = jk_platform_file_read_full(&scratch_arena, (char *)file_path.data);
    JkFbxHeader *header = (JkFbxHeader *)file.data;

    fbx_id_map = jk_hash_table_create_capacity(32);
    bitmap_map = jk_hash_table_create_capacity(32);

    if (jk_buffer_compare((JkBuffer){.size = JK_SIZEOF(header->magic), .data = header->magic},
                JKS("Kaydara FBX Binary  "))
            != 0) {
        fprintf(stderr, "'%s': unrecognized file format\n", (char *)file_path.data);
        exit(1);
    }

    Assets *assets = jk_arena_push(&result_arena, JK_SIZEOF(*assets));

    process_fbx_nodes(c, file, header->first_node - file.data, (ThingId){0});

    assets->vertices = append_arena(&result_arena, c->verts_arena);
    assets->texcoords = append_arena(&result_arena, c->texcoords_arena);

    // Process faces
    for (ThingId id = {1}; id.i < thing_count; id.i++) {
        Thing *thing = thing_get(id);
        int64_t faces_base = result_arena.pos;
        int64_t i = 0;
        while (i < thing->vertex_indexes.count) {
            Face *face = jk_arena_push_zero(&result_arena, JK_SIZEOF(*face));
            int64_t point_index = 0;
            b32 end_of_polygon = 0;
            while (i < thing->vertex_indexes.count && !end_of_polygon) {
                int32_t vertex_index = thing->vertex_indexes.items[i];
                if (vertex_index < 0) {
                    if (point_index != 2) {
                        fprintf(stderr,
                                "process_fbx_nodes: Encountered non-triangle "
                                "polygon\n");
                    }
                    end_of_polygon = 1;
                    vertex_index = ~vertex_index;
                }
                if (point_index < JK_ARRAY_COUNT(face->v)) {
                    face->v[point_index] = thing->vertices_base + vertex_index;
                    if (i < thing->texcoord_indexes.count) {
                        face->t[point_index] =
                                thing->texcoords_base + thing->texcoord_indexes.items[i];
                    }
                    point_index++;
                } else {
                    fprintf(stderr, "process_fbx_nodes: Encountered non-triangle polygon\n");
                }
                i++;
            }
        }
        if (faces_base < result_arena.pos) {
            thing->faces.offset = faces_base - result_arena.base;
            thing->faces.size = result_arena.pos - faces_base;
        }
    }

    JkArena objects_arena = jk_arena_child_get(&result_arena);
    jk_arena_push(&objects_arena, JK_SIZEOF(Object)); // Push nil object
    for (ThingId id = {1}; id.i < thing_count; id.i++) {
        if (!JK_FLAG_GET(thing_get(id)->flags, THING_FLAG_HAS_PARENT)) {
            process_thing(&objects_arena, id, (ObjectId){0});
        }
    }

    assets->objects = child_arena_span(&result_arena, &objects_arena);
    jk_arena_child_commit(&result_arena, &objects_arena);

    char *binary_file_name = "graphics_assets";
    FILE *binary_file = fopen(binary_file_name, "wb");
    if (binary_file) {
        fwrite(result_arena.root->memory.data + result_arena.base,
                result_arena.pos - result_arena.base,
                1,
                binary_file);
    } else {
        fprintf(stderr,
                "%s: Failed to open '%s': %s\n",
                argv[0],
                binary_file_name,
                strerror(errno));
    }

    jk_platform_write_as_c_byte_array(jk_arena_as_buffer(&result_arena),
            JKS("../jk_gen/pikuma/graphics/assets.c"),
            JKS("assets_byte_array"));

    return 0;
}
