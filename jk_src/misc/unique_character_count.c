#include "jk_src/jk_lib/jk_lib.h"
#include <stdio.h>
#include <stdlib.h>

// #jk_build single_translation_unit

// #jk_build dependencies_begin
#include <jk_src/jk_lib/hash_table/hash_table.h>
#include <jk_src/jk_lib/platform/platform.h>
// #jk_build dependencies_end

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

JkOptionResult opt_results[OPT_COUNT] = {0};

JkOptionsParseResult opts_parse = {0};

char *program_name = "<program_name global should be overwritten with argv[0]>";

int main(int argc, char **argv)
{
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
                   "\tunique_character_count - counts the number of unique characters in a file\n\n"
                   "SYNOPSIS\n"
                   "\tunique_character_count FILE\n\n");
            jk_options_print_help(stdout, opts, OPT_COUNT);
            exit(opts_parse.usage_error);
        }
    }

    JkPlatformArena storage;
    jk_platform_arena_init(&storage, (size_t)1 << 35);

    JkBuffer file = jk_platform_file_read_full(opts_parse.operands[0], &storage);
    JkHashTable *seen = jk_hash_table_create();
    uint64_t count = 0;
    uint64_t pos = 0;
    JkUtf8Codepoint codepoint;
    JkUtf8CodepointGetResult result;
    while ((result = jk_utf8_codepoint_get(file, &pos, &codepoint))
            == JK_UTF8_CODEPOINT_GET_SUCCESS) {
        uint32_t decoded = (uint32_t)jk_utf8_codepoint_decode(codepoint);
        if (!jk_hash_table_get(seen, decoded)) {
            jk_hash_table_put(seen, decoded, 1);
            count++;
        }
    }

    if (result == JK_UTF8_CODEPOINT_GET_UNEXPECTED_BYTE) {
        fprintf(stderr, "%s: Encountered unexpected byte\n", program_name);
        exit(1);
    }

    printf("%llu\n", (unsigned long long)count);

    return 0;
}
