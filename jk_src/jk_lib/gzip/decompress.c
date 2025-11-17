#include <stdio.h>

// #jk_build single_translation_unit

// #jk_build dependencies_begin
#include <jk_src/jk_lib/gzip/gzip.h>
#include <jk_src/jk_lib/platform/platform.h>
// #jk_build dependencies_end

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "%s: Usage error\n", argv[0]);
        fprintf(stderr, "%s FILE_NAME\n", argv[0]);
        return 1;
    }

    jk_print = jk_platform_print_stdout;

    JkPlatformArenaVirtualRoot arena_root;
    JkArena arena = jk_platform_arena_virtual_init(&arena_root, 64 * JK_GIGABYTE);
    JkBuffer file = jk_platform_file_read_full(&arena, argv[1]);

    JkGzipDecompressResult result = jk_gzip_decompress(&arena, file);
    if (!result.name.size) {
        result.name = JK_FORMAT(&arena, jkfn("decompressed_"), jkfn(argv[0]));
    }

    char *output_file_name = jk_buffer_to_null_terminated(&arena, result.name);
    FILE *output_file = fopen(output_file_name, "wb");
    if (output_file) {
        fwrite(result.contents.data, 1, result.contents.size, output_file);
    } else {
        fprintf(stderr, "%s: failed to open output file '%s'\n", argv[0], output_file_name);
    }

    return 0;
}
