#include <stdlib.h>

// #jk_build single_translation_unit

// #jk_build dependencies_begin
#include <jk_src/jk_lib/platform/platform.h>
#include <jk_src/perfaware/part2/haversine_lib.h>
#include <jk_src/perfaware/part4/custom_math_functions.h>
// #jk_build dependencies_end

#define RADIANS_PER_DEGREE (JK_PI / 180.0)

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

int32_t jk_platform_entry_point(int32_t argc, char **argv)
{
    jk_profile_frame_begin();

    program_name = argv[0];

    // Parse command line arguments
    {
        jk_options_parse(argc, argv, opts, opt_results, OPT_COUNT, &opts_parse);
        if ((opts_parse.operand_count < 1 || opts_parse.operand_count > 2)
                && !opt_results[OPT_HELP].present) {
            fprintf(stderr,
                    "%s: Expected 1-2 operands, got %lld\n",
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

    JkPlatformArenaVirtualRoot arena_root;
    JkArena storage = jk_platform_arena_virtual_init(&arena_root, (int64_t)1 << 35);

    HaversineContext context = haversine_setup(json_file_name, answers_file_name, &storage);

    JK_PROFILE_ZONE_BANDWIDTH_BEGIN(sum, context.pair_count * JK_SIZEOF(context.pairs[0]));
    double sum = 0.0;
    for (int64_t i = 0; i < context.pair_count; i++) {
        double lat1 = RADIANS_PER_DEGREE * context.pairs[i].v[Y0];
        double lat2 = RADIANS_PER_DEGREE * context.pairs[i].v[Y1];
        double lon1_deg = context.pairs[i].v[X0];
        double lon2_deg = context.pairs[i].v[X1];

        double dLat = lat2 - lat1;
        double dLon = RADIANS_PER_DEGREE * (lon2_deg - lon1_deg);

        double a = square(jk_sin(dLat / 2.0))
                + jk_cos(lat1) * jk_cos(lat2) * square(jk_sin(dLon / 2.0));
        double distance = jk_asin(jk_sqrt(a));

#if JK_BUILD_MODE != JK_RELEASE
        if (context.answers) {
            JK_ASSERT(approximately_equal(2.0 * EARTH_RADIUS * distance, context.answers[i]));
        }
#endif

        sum += distance;
    }
    sum = (2.0 * EARTH_RADIUS * sum) / context.pair_count;
    JK_PROFILE_ZONE_END(sum);

    printf("Pair count: %lld\n", (long long)context.pair_count);
    printf("Haversine sum: %.16f\n", sum);

    if (context.answers) {
        printf("\nReference sum: %.16f\n", context.sum_answer);
        printf("Difference: %.16f\n\n", sum - context.sum_answer);
    }

    jk_platform_profile_end_and_print();

    return 0;
}
