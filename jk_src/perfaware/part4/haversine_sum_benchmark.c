#include <math.h>
#include <stdlib.h>

// #jk_build single_translation_unit

// #jk_build dependencies_begin
#include <jk_src/jk_lib/platform/platform.h>
#include <jk_src/perfaware/part2/haversine_lib.h>
// #jk_build dependencies_end

typedef struct Test {
    char *name;
    HaversineSumFunction compute;
    HaversineSumVerifyFunction verify;
} Test;

static Test tests[] = {
    {.name = "Reference", .compute = haversine_reference_sum, .verify = haversine_reference_verify},
};

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
    jk_platform_profile_begin();

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
                   "\thaversine_sum_benchmark - benchmarks different haversine sum loops\n\n"
                   "SYNOPSIS\n"
                   "\thaversine_sum_benchmark JSON_FILE [ANSWER_FILE]\n\n");
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

    uint64_t frequency = jk_platform_cpu_timer_frequency_estimate(100);
    HaversineContext context = haversine_setup(json_file_name, answers_file_name, &storage);
    JkPlatformRepetitionTest tester = {0};

    for (uint64_t i = 0; i < JK_ARRAY_COUNT(tests); i++) {
        printf("\n%s\n", tests[i].name);
        jk_platform_repetition_test_run_wave(
                &tester, context.pair_count * sizeof(context.pairs[0]), frequency, 10);

        uint64_t individual_error_count = tests[i].verify(context);
        uint64_t sum_error_count = 0;
        while (jk_platform_repetition_test_running(&tester)) {
            jk_platform_repetition_test_time_begin(&tester);
            double sum = tests[i].compute(context);
            jk_platform_repetition_test_count_bytes(
                    &tester, context.pair_count * sizeof(context.pairs[0]));
            jk_platform_repetition_test_time_end(&tester);

            sum_error_count += !approximately_equal(sum, context.sum_answer);
        }

        if (individual_error_count || sum_error_count) {
            fprintf(stderr,
                    "WARNING: %llu haversines mismatched, %llu sums mismatched\n",
                    (unsigned long long)individual_error_count,
                    (unsigned long long)sum_error_count);
        }
    }

    jk_platform_profile_end_and_print();

    return 0;
}
