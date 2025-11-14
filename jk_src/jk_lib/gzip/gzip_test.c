#include <errno.h>
#include <stdio.h>
#include <string.h>

// #jk_build single_translation_unit

// #jk_build dependencies_begin
#include <jk_src/jk_lib/gzip/gzip.h>
#include <jk_src/jk_lib/platform/platform.h>
// #jk_build dependencies_end

static uint32_t crc32(JkBuffer buffer)
{
    uint32_t result = 0xffffffff;
    for (uint64_t byte_index = 0; byte_index < buffer.size; byte_index++) {
        result ^= buffer.data[byte_index];
        for (uint8_t bit_index = 0; bit_index < 8; bit_index++) {
            result = (result >> 1) ^ ((result & 1) * 0xedb88320);
        }
    }
    return ~result;
}

static JkGzipDecompressResult expected_result = {
    .name = JKSI("test.txt"),
    .comment = {0},
    .contents = JKSI("There is a yummy, yummy"),
};

// clang-format off
static uint8_t gzip_bytes[] = {
    // GZIP header
    0x1f, 0x8b, 0x08, 0x08, 0x23, 0xb8, 0x0f, 0x69,
    0x00, 0x0b, 0x74, 0x65, 0x73, 0x74, 0x2e, 0x74,
    0x78, 0x74, 0x00,

    // ---- DEFLATE compresssed data begin -------------------------------------

    // uncompressed block
    0x00, 0x09, 0x00, 0xf6, 0xff, 0x54, 0x68, 0x65,
    0x72, 0x65, 0x20, 0x69, 0x73, 0x20,

    // block compressed with fixed Huffman codes
    0x4B, 0x54, 0xA8, 0x2C, 0xCD, 0xCD, 0xAD, 0xD4,
    0x51, 0x00, 0x53, 0x00,

    // ---- DEFLATE compresssed data end ---------------------------------------

    // GZIP trailer placeholder
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
// clang-format on

static JkBuffer gzip_buffer = {.size = sizeof(gzip_bytes), .data = gzip_bytes};

int main(int argc, char **argv)
{
    jk_print = jk_platform_print_stdout;

    b32 pass = 1;

    uint32_t crc = crc32(expected_result.contents);
    JkGzipTrailer *trailer =
            (JkGzipTrailer *)(gzip_bytes + (sizeof(gzip_bytes) - sizeof(JkGzipTrailer)));
    trailer->crc32 = crc;
    trailer->uncompressed_size = expected_result.contents.size;

    char *file_name = "test.txt.gz";
    FILE *file = fopen(file_name, "wb");
    if (file) {
        fwrite(gzip_buffer.data, 1, gzip_buffer.size, file);
        fclose(file);
    } else {
        pass = 0;
        printf("Failed to open '%s': %s\n", file_name, strerror(errno));
    }

    JkPlatformArenaVirtualRoot arena_root;
    JkArena arena = jk_platform_arena_virtual_init(&arena_root, 8 * JK_GIGABYTE);

    JkGzipDecompressResult result = jk_gzip_decompress(&arena, gzip_buffer);

    for (uint64_t i = 0; i < JK_ARRAY_COUNT(result.buffers); i++) {
        if (jk_buffer_compare(expected_result.buffers[i], result.buffers[i]) != 0) {
            pass = 0;
            JK_PRINT_FMT(&arena,
                    jkfn("Incorrect "),
                    jkfs(jk_gzip_buffer_names[i]),
                    jkfn(": Expected '"),
                    jkfs(expected_result.buffers[i]),
                    jkfn("', got '"),
                    jkfs(result.buffers[i]),
                    jkfn("'\n"));
        }
    }

    printf(pass ? "PASS\n" : "FAIL\n");
}
