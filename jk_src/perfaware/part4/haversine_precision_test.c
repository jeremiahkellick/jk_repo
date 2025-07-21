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

int main(int argc, char **argv)
{
    jk_platform_profile_frame_begin();

    // Parse command line arguments
    {
        jk_options_parse(argc, argv, opts, opt_results, OPT_COUNT, &opts_parse);
        if ((opts_parse.operand_count < 1 || opts_parse.operand_count > 2)
                && !opt_results[OPT_HELP].present) {
            fprintf(stderr,
                    "%s: Expected 1-2 operands, got %zu\n",
                    argv[0],
                    opts_parse.operand_count);
            opts_parse.usage_error = 1;
        }
        if (opt_results[OPT_HELP].present || opts_parse.usage_error) {
            printf("NAME\n"
                   "\thaversine_precision_test - tests precision of haversine w/ custom math\n\n"
                   "SYNOPSIS\n"
                   "\thaversine_precision_test JSON_FILE [ANSWER_FILE]\n\n");
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
    JkArena storage = jk_platform_arena_virtual_init(&arena_root, (size_t)1 << 35);

    HaversineContext context = haversine_setup(json_file_name, answers_file_name, &storage);

    uint64_t error_count = haversine_verify(context);

    double sum_reference = 0.0;
    double sum = 0.0;
    double sum_coefficient = 1.0 / (double)context.pair_count;
    uint64_t diff_count = 0;
    double diff_total = 0.0;
    double diff_max = 0.0;

    for (uint64_t i = 0; i < context.pair_count; i++) {
        HaversinePair pair = context.pairs[i];
        double value_reference =
                haversine_reference(pair.v[X0], pair.v[Y0], pair.v[X1], pair.v[Y1], EARTH_RADIUS);
        double value = haversine(pair.v[X0], pair.v[Y0], pair.v[X1], pair.v[Y1], EARTH_RADIUS);
        sum_reference += sum_coefficient * value_reference;
        sum += sum_coefficient * value;

        double diff = jk_abs_64(sum - sum_reference);
        diff_count++;
        diff_total += diff;
        if (diff_max < diff) {
            diff_max = diff;
        }
    }

    b32 sum_mismatched = !approximately_equal(sum, context.sum_answer);

    if (error_count) {
        fprintf(stderr, "WARNING: %llu haversines mismatched\n", (unsigned long long)error_count);
    }
    if (sum_mismatched) {
        fprintf(stderr, "WARNING: haversine sum mismatched\n");
    }

    printf("%+.24f Haversine sum\n", sum);

    printf("\n%+.24f Reference sum\n", sum_reference);
    printf("%+.24f Difference\n", sum - sum_reference);

    if (context.answers) {
        printf("\n%+.24f Answers file sum\n", context.sum_answer);
        printf("%+.24f Difference\n", sum - context.sum_answer);
    }

    printf("\n%+.24f Max error\n", diff_max);
    printf("%+.24f Average error\n", diff_total / (double)diff_count);

    printf("\n");
    jk_platform_profile_frame_end_and_print();

    return 0;
}
