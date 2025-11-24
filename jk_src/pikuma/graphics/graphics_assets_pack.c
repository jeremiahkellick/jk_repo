#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// #jk_build single_translation_unit

// #jk_build dependencies_begin
#include <jk_src/jk_lib/gzip/gzip.h>
#include <jk_src/jk_lib/platform/platform.h>
#include <jk_src/pikuma/graphics/graphics.h>
// #jk_build dependencies_end

char file_name[] = "../jk_assets/pikuma/graphics/models.fbx";

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

typedef struct Context {
    JkArena *arena;
    JkArena *verts_arena;
    JkArena *faces_arena;
} Context;

void process_fbx_nodes(Context *c, JkBuffer file, uint64_t pos)
{
    while (0 < pos && pos < file.size) {
        JkFbxNode *node = (JkFbxNode *)(file.data + pos);
        uint64_t pos_children =
                (node->name - file.data) + node->name_length + node->property_list_size;
        JkBuffer name = {.size = node->name_length, .data = node->name};

        if (jk_buffer_compare(name, JKS("Vertices")) == 0) {
            uint8_t type = *(node->name + node->name_length);
            if (type == 'd') {
                JkFbxArray *array = (JkFbxArray *)(node->name + node->name_length + 1);
                double *coords = 0;
                uint64_t byte_count = array->length * sizeof(*coords);
                if (array->encoding == 0) {
                    coords = (double *)array->contents;
                } else if (array->encoding == 1) {
                    JkBuffer compressed_data = {
                        .size = array->compressed_length, .data = array->contents};
                    JkBuffer decompressed_data =
                            jk_zlib_decompress(c->arena, compressed_data, byte_count);
                    if (decompressed_data.size == byte_count) {
                        coords = (double *)decompressed_data.data;
                    } else {
                        fprintf(stderr, "process_fbx_nodes: Failed to decompress verticies\n");
                    }
                } else {
                    fprintf(stderr, "process_fbx_nodes: Unrecognized array encoding\n");
                }
                if (coords) {
                    uint32_t vertex_count = array->length / 3;
                    JkVector3 *vertices =
                            jk_arena_push(c->verts_arena, vertex_count * sizeof(*vertices));
                    for (uint32_t vertex_index = 0; vertex_index < vertex_count; vertex_index++) {
                        for (uint32_t coord_index = 0; coord_index < 3; coord_index++) {
                            vertices[vertex_index].coords[coord_index] =
                                    coords[vertex_index * 3 + coord_index];
                        }
                    }
                }
            } else {
                fprintf(stderr, "process_fbx_nodes: Unrecognized vertices data type\n");
            }
        } else if (jk_buffer_compare(name, JKS("PolygonVertexIndex")) == 0) {
            uint8_t type = *(node->name + node->name_length);
            if (type == 'i') {
                JkFbxArray *array = (JkFbxArray *)(node->name + node->name_length + 1);
                int32_t *indexes = 0;
                uint64_t byte_count = array->length * sizeof(*indexes);
                if (array->encoding == 0) {
                    indexes = (int32_t *)array->contents;
                } else if (array->encoding == 1) {
                    JkBuffer compressed_data = {
                        .size = array->compressed_length, .data = array->contents};
                    JkBuffer decompressed_data =
                            jk_zlib_decompress(c->arena, compressed_data, byte_count);
                    if (decompressed_data.size == byte_count) {
                        indexes = (int32_t *)decompressed_data.data;
                    } else {
                        fprintf(stderr, "process_fbx_nodes: Failed to decompress vertex indexes\n");
                    }
                } else {
                    fprintf(stderr, "process_fbx_nodes: Unrecognized array encoding\n");
                }
                if (indexes) {
                    int32_t *index_ptr = indexes;
                    while (index_ptr - indexes < array->length) {
                        Face *face = jk_arena_push_zero(c->faces_arena, sizeof(*face));
                        uint32_t i = 0;
                        b32 end_of_polygon = 0;
                        while (index_ptr - indexes < array->length && !end_of_polygon) {
                            int32_t index = *index_ptr++;
                            if (index < 0) {
                                if (i != 2) {
                                    fprintf(stderr,
                                            "process_fbx_nodes: Encountered non-triangle "
                                            "polygon\n");
                                }
                                end_of_polygon = 1;
                                index = ~index;
                            }
                            if (i < JK_ARRAY_COUNT(face->v)) {
                                face->v[i++] = index;
                            } else {
                                fprintf(stderr,
                                        "process_fbx_nodes: Encountered non-triangle polygon\n");
                            }
                        }
                    }
                }
            } else {
                fprintf(stderr, "process_fbx_nodes: Unrecognized vertex indexes data type\n");
            }
        } else if (jk_buffer_compare(name, JKS("Objects")) == 0) {
            process_fbx_nodes(c, file, pos_children);
        } else if (jk_buffer_compare(name, JKS("Geometry")) == 0) {
            c->verts_arena->pos = 0;
            c->faces_arena->pos = 0;
            process_fbx_nodes(c, file, pos_children);
        }

        pos = node->end_offset;
    }
}

void append_arena_as_span(JkArena *dest, JkArena *src, JkSpan *span)
{
    span->size = src->pos;
    span->offset = dest->pos;
    uint8_t *data = jk_arena_push(dest, span->size);
    jk_memcpy(data, src->root->memory.data, span->size);
}

int main(int argc, char **argv)
{
    jk_print = jk_platform_print_stdout;
    jk_platform_set_working_directory_to_executable_directory();

    Context context;
    Context *c = &context;

    JkPlatformArenaVirtualRoot arena_root;
    JkArena arena = jk_platform_arena_virtual_init(&arena_root, 8 * JK_GIGABYTE);
    c->arena = &arena;

    JkPlatformArenaVirtualRoot verts_arena_root;
    JkArena verts_arena = jk_platform_arena_virtual_init(&verts_arena_root, 1 * JK_GIGABYTE);
    c->verts_arena = &verts_arena;

    JkPlatformArenaVirtualRoot faces_arena_root;
    JkArena faces_arena = jk_platform_arena_virtual_init(&faces_arena_root, 1 * JK_GIGABYTE);
    c->faces_arena = &faces_arena;

    JkPlatformArenaVirtualRoot result_arena_root;
    JkArena result_arena = jk_platform_arena_virtual_init(&result_arena_root, 1 * JK_GIGABYTE);

    JkBuffer file = jk_platform_file_read_full(&arena, file_name);
    JkFbxHeader *header = (JkFbxHeader *)file.data;

    if (jk_buffer_compare((JkBuffer){.size = sizeof(header->magic), .data = header->magic},
                JKS("Kaydara FBX Binary  "))
            != 0) {
        fprintf(stderr, "'%s': unrecognized file format\n", file_name);
        exit(1);
    }

    process_fbx_nodes(c, file, header->first_node - file.data);

    Assets *assets = jk_arena_push(&result_arena, sizeof(*assets));
    append_arena_as_span(&result_arena, c->verts_arena, &assets->vertices);
    append_arena_as_span(&result_arena, c->faces_arena, &assets->faces);

    char *binary_file_name = "graphics_assets";
    FILE *binary_file = fopen(binary_file_name, "wb");
    if (binary_file) {
        fwrite(result_arena.root->memory.data, result_arena.pos, 1, binary_file);
    } else {
        fprintf(stderr,
                "%s: Failed to open '%s': %s\n",
                argv[0],
                binary_file_name,
                strerror(errno));
    }

    return 0;
}
