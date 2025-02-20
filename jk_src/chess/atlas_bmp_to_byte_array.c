#include <errno.h>
#include <stdlib.h>
#include <string.h>

// #jk_build single_translation_unit

// #jk_build dependencies_begin
#include <jk_src/jk_lib/platform/platform.h>
// #jk_build dependencies_end

typedef struct Pixel {
    uint8_t ignored[3];
    uint8_t alpha;
} Pixel;

typedef enum Opt {
    OPT_HELP,
    OPT_COUNT,
} Opt;

JkOption opts[OPT_COUNT] = {
    {
        .flag = '\0',
        .long_name = "help",
        .arg_name = NULL,
        .description = "\tDisplay this help text and exit.\n",
    },
};

static JkOptionResult opt_results[OPT_COUNT] = {0};

static JkOptionsParseResult opts_parse = {0};

static char *program_name = "<program_name global should be overwritten with argv[0]>";

#define C_FILE_NAME "chess_atlas.c"

int main(int argc, char **argv)
{
    jk_platform_profile_begin();

    program_name = argv[0];

    // Parse command line arguments
    {
        jk_options_parse(argc, argv, opts, opt_results, OPT_COUNT, &opts_parse);
        if (opts_parse.operand_count != 1 && !opt_results[OPT_HELP].present) {
            fprintf(stderr,
                    "%s: Expected 1 operand, got %zu\n",
                    program_name,
                    opts_parse.operand_count);
            opts_parse.usage_error = 1;
        }
        if (opt_results[OPT_HELP].present || opts_parse.usage_error) {
            printf("NAME\n"
                   "\tatlas_bmp_to_byte_array - converts .bmp alpha channel to a C byte array\n\n"
                   "SYNOPSIS\n"
                   "\tatlas_bmp_to_byte_array FILE\n\n"
                   "DESCRIPTION\n"
                   "\tatlas_bmp_to_byte_array Reads FILE as a BMP. Assumes 4 bytes per pixel.\n"
                   "\tAssumes the last byte is the alpha channel. All other channels are\n"
                   "\tignored. The output is an array of alpha values. The Y order is\n"
                   "\treversed. In the BMP file format, the bottom-row pixels come first. In\n"
                   "\tresulting byte array, the top-row pixels comes first.\n\n"
                   "\tThe output is saved in " C_FILE_NAME " as a C byte array named\n"
                   "\tchess_atlas. CHESS_ATLAS_WIDTH and CHESS_ATLAS_HEIGHT are also defined\n"
                   "\tto the BMP's width and height.\n\n");
            jk_options_print_help(stdout, opts, OPT_COUNT);
            exit(opts_parse.usage_error);
        }
    }
    char *file_name = opts_parse.operands[0];

    JkPlatformArena storage;
    if (jk_platform_arena_init(&storage, (size_t)1 << 35) != JK_PLATFORM_ARENA_INIT_SUCCESS) {
        fprintf(stderr, "%s: Failed to initialized memory arena\n", program_name);
        exit(1);
    }

    JkBuffer bmp_file = jk_platform_file_read_full(file_name, &storage);
    if (bmp_file.size < sizeof(BitmapHeader)) {
        fprintf(stderr, "%s: Failed to load '%s' or file too small\n", program_name, file_name);
        exit(1);
    }

    FILE *c_file = fopen(C_FILE_NAME, "wb");
    if (!c_file) {
        fprintf(stderr,
                "%s: Failed to open '" C_FILE_NAME "' for writing: %s\n",
                program_name,
                strerror(errno));
        exit(1);
    }

    BitmapHeader *header = (BitmapHeader *)bmp_file.data;
    uint32_t width = header->width;
    uint32_t height = header->height;
    size_t atlas_size = width * height * sizeof(Pixel);
    if (bmp_file.size < header->offset + atlas_size) {
        fprintf(stderr,
                "%s: Error reading '%s': File smaller than implied by offset, width, and "
                "height\n",
                program_name,
                file_name);
        exit(1);
    }

    uint8_t *atlas = jk_platform_arena_push(&storage, atlas_size);
    if (!atlas) {
        fprintf(stderr, "%s: Failed to allocate atlas memory\n", program_name);
        exit(1);
    }

    Pixel *pixels = (Pixel *)(bmp_file.data + header->offset);
    for (uint32_t y = 0; y < height; y++) {
        uint32_t atlas_y = height - y - 1;
        for (uint32_t x = 0; x < width; x++) {
            atlas[atlas_y * width + x] = pixels[y * width + x].alpha;
        }
    }

    fprintf(c_file, "#include <stdint.h>\n\n");
    fprintf(c_file, "#define CHESS_ATLAS_WIDTH %u\n\n", width);
    fprintf(c_file, "#define CHESS_ATLAS_HEIGHT %u\n\n", height);
    fprintf(c_file, "uint8_t chess_atlas[%zu] = {\n", atlas_size);
    uint64_t atlas_index = 0;
    while (atlas_index < atlas_size) {
        fprintf(c_file, "   ");
        for (uint32_t i = 0; i < 16 && atlas_index < atlas_size; i++) {
            fprintf(c_file, " 0x%02x,", (uint32_t)atlas[atlas_index++]);
        }
        fprintf(c_file, "\n");
    }
    fprintf(c_file, "};\n");
}
