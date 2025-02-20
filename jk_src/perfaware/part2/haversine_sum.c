#include <errno.h>
#include <stdlib.h>

// #jk_build single_translation_unit

// #jk_build dependencies_begin
#include <jk_src/jk_lib/platform/platform.h>
#include <jk_src/perfaware/part2/haversine_lib.h>
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
    jk_platform_profile_begin();

    program_name = argv[0];

    // Parse command line arguments
    {
        jk_options_parse(argc, argv, opts, opt_results, OPT_COUNT, &opts_parse);
        if ((opts_parse.operand_count < 1 || opts_parse.operand_count > 2)
                && !opt_results[OPT_HELP].present) {
            fprintf(stderr,
                    "%s: Expected 1-2 operands, got %zu\n",
                    program_name,
                    opts_parse.operand_count);
            opts_parse.usage_error = 1;
        }
        if (opt_results[OPT_HELP].present || opts_parse.usage_error) {
            printf("NAME\n"
                   "\thaversine_sum - computes sum of haversine distances from a JSON file\n\n"
                   "SYNOPSIS\n"
                   "\thaversine_sum JSON_FILE [ANSWER_FILE]\n\n"
                   "DESCRIPTION\n"
                   "\thaversine_sum computes a sum of haversine distances based on the\n"
                   "\tcoordinate pairs in JSON_FILE. If ANSWER_FILE was provided, it also\n"
                   "\tvalidates the computation against ANSWER_FILE, a binary file which\n"
                   "\tshould contain one 64-bit floating point value for each coordinate pair\n"
                   "\tin JSON_FILE, plus the sum of all of them at the end.\n\n");
            jk_options_print_help(stdout, opts, OPT_COUNT);
            exit(opts_parse.usage_error);
        }
    }
    char *json_file_name = opts_parse.operands[0];
    char *answers_file_name = NULL;
    if (opts_parse.operand_count > 1) {
        answers_file_name = opts_parse.operands[1];
    }

    JkPlatformArena storage;
    jk_platform_arena_init(&storage, (size_t)1 << 35);

    HaversineContext context = haversine_setup(json_file_name, answers_file_name, &storage);

    JK_PLATFORM_PROFILE_ZONE_BANDWIDTH_BEGIN(sum, context.pair_count * sizeof(context.pairs[0]));
    double sum = 0.0;
    double sum_coefficient = 1.0 / (double)context.pair_count;
    for (size_t i = 0; i < context.pair_count; i++) {
        double distance = haversine_reference(context.pairs[i].v[X0],
                context.pairs[i].v[Y0],
                context.pairs[i].v[X1],
                context.pairs[i].v[Y1],
                EARTH_RADIUS);

#ifndef NDEBUG
        if (context.answers) {
            JK_ASSERT(approximately_equal(distance, context.answers[i]));
        }
#endif

        sum += distance * sum_coefficient;
    }
    JK_PLATFORM_PROFILE_ZONE_END(sum);

    printf("Pair count: %llu\n", context.pair_count);
    printf("Haversine sum: %.16f\n", sum);

    if (context.answers) {
        printf("\nReference sum: %.16f\n", context.sum_answer);
        printf("Difference: %.16f\n\n", sum - context.sum_answer);
    }

    jk_platform_profile_end_and_print();

    return 0;
}
