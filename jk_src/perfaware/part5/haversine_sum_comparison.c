#include "jk_src/jk_lib/jk_lib.h"
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

static JkOption opts[OPT_COUNT] = {
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

static double haversine_sum_1(HaversineContext context)
{
    double sum = 0.0;
    for (size_t i = 0; i < context.pair_count; i++) {
        double lat1 = RADIANS_PER_DEGREE * context.pairs[i].v[Y0];
        double lat2 = RADIANS_PER_DEGREE * context.pairs[i].v[Y1];
        double lon1_deg = context.pairs[i].v[X0];
        double lon2_deg = context.pairs[i].v[X1];

        double d_lat = lat2 - lat1;
        double d_lon = RADIANS_PER_DEGREE * (lon2_deg - lon1_deg);

        double a = square(jk_sin(d_lat / 2.0))
                + jk_cos(lat1) * jk_cos(lat2) * square(jk_sin(d_lon / 2.0));
        double distance = jk_asin(jk_sqrt(a));

        sum += distance;
    }
    return (2.0 * EARTH_RADIUS * sum) / context.pair_count;
}

static double haversine_sum_2(HaversineContext context)
{
    double sum = 0.0;
    for (size_t i = 0; i < context.pair_count; i++) {
        double lat1 = RADIANS_PER_DEGREE * context.pairs[i].v[Y0];
        double lat2 = RADIANS_PER_DEGREE * context.pairs[i].v[Y1];
        double lon1_deg = context.pairs[i].v[X0];
        double lon2_deg = context.pairs[i].v[X1];

        double d_lat = lat2 - lat1;
        double d_lon = RADIANS_PER_DEGREE * (lon2_deg - lon1_deg);

        double lat1_cos = jk_sin_core(lat1 + JK_PI / 2.0);
        double lat2_cos = jk_sin_core(lat2 + JK_PI / 2.0);

        double a = square(jk_sin(d_lat / 2.0)) + lat1_cos * lat2_cos * square(jk_sin(d_lon / 2.0));
        double distance = jk_asin(jk_sqrt(a));

        sum += distance;
    }
    return (2.0 * EARTH_RADIUS * sum) / context.pair_count;
}

static double haversine_sum_3(HaversineContext context)
{
    double sum = 0.0;
    for (size_t i = 0; i < context.pair_count; i++) {
        double lat1 = RADIANS_PER_DEGREE * context.pairs[i].v[Y0];
        double lat2 = RADIANS_PER_DEGREE * context.pairs[i].v[Y1];
        double lon1_deg = context.pairs[i].v[X0];
        double lon2_deg = context.pairs[i].v[X1];

        double d_lat = lat2 - lat1;
        double d_lon = RADIANS_PER_DEGREE * (lon2_deg - lon1_deg);

        double lat1_cos = jk_sin_core(lat1 + JK_PI / 2.0);
        double lat2_cos = jk_sin_core(lat2 + JK_PI / 2.0);

        double a = square(jk_sin(d_lat / 2.0)) + lat1_cos * lat2_cos * square(jk_sin(d_lon / 2.0));

        b32 in_asin_standard_range = a <= 0.5;
        double x_squared = in_asin_standard_range ? a : 1.0 - a;
        double asin_core_result = jk_asin_core(x_squared) * jk_sqrt(x_squared);
        double distance =
                in_asin_standard_range ? asin_core_result : (JK_PI / 2.0) - asin_core_result;

        sum += distance;
    }
    return (2.0 * EARTH_RADIUS * sum) / context.pair_count;
}

static double haversine_sum_4(HaversineContext context)
{
    double sum = 0.0;
    for (size_t i = 0; i < context.pair_count; i++) {
        double lat1 = RADIANS_PER_DEGREE * context.pairs[i].v[Y0];
        double lat2 = RADIANS_PER_DEGREE * context.pairs[i].v[Y1];
        double lon1_deg = context.pairs[i].v[X0];
        double lon2_deg = context.pairs[i].v[X1];

        double d_lat = lat2 - lat1;
        double d_lon = RADIANS_PER_DEGREE * (lon2_deg - lon1_deg);

        double lat1_cos = jk_sin_core(lat1 + JK_PI / 2.0);
        double lat2_cos = jk_sin_core(lat2 + JK_PI / 2.0);

        double half_d_lat_sin = jk_sin_core(jk_abs_64(d_lat / 2.0));
        double half_d_lon_sin = jk_sin_core(jk_abs_64(d_lon / 2.0));

        double a = square(half_d_lat_sin) + lat1_cos * lat2_cos * square(half_d_lon_sin);

        b32 in_asin_standard_range = a <= 0.5;
        double x_squared = in_asin_standard_range ? a : 1.0 - a;
        double asin_core_result = jk_asin_core(x_squared) * jk_sqrt(x_squared);
        double distance =
                in_asin_standard_range ? asin_core_result : (JK_PI / 2.0) - asin_core_result;

        sum += distance;
    }
    return (2.0 * EARTH_RADIUS * sum) / context.pair_count;
}

static double haversine_sum_5(HaversineContext context)
{
    double sum = 0.0;
    for (size_t i = 0; i < context.pair_count; i++) {
        double lat1_deg = context.pairs[i].v[Y0];
        double lat2_deg = context.pairs[i].v[Y1];
        double lon1_deg = context.pairs[i].v[X0];
        double lon2_deg = context.pairs[i].v[X1];

        double half_d_lat = (RADIANS_PER_DEGREE / 2.0) * (lat2_deg - lat1_deg);
        double half_d_lon = (RADIANS_PER_DEGREE / 2.0) * (lon2_deg - lon1_deg);

        double lat1_cos = jk_sin_core(RADIANS_PER_DEGREE * lat1_deg + JK_PI / 2.0);
        double lat2_cos = jk_sin_core(RADIANS_PER_DEGREE * lat2_deg + JK_PI / 2.0);

        double half_d_lat_sin = jk_sin_core(jk_abs_64(half_d_lat));
        double half_d_lon_sin = jk_sin_core(jk_abs_64(half_d_lon));

        double a = lat1_cos * lat2_cos * square(half_d_lon_sin) + square(half_d_lat_sin);

        b32 in_asin_standard_range = a <= 0.5;
        double x_squared = in_asin_standard_range ? a : 1.0 - a;
        double asin_core_result = jk_asin_core(x_squared) * jk_sqrt(x_squared);
        double distance =
                in_asin_standard_range ? asin_core_result : (JK_PI / 2.0) - asin_core_result;

        sum += distance;
    }
    return (2.0 * EARTH_RADIUS * sum) / context.pair_count;
}

static double haversine_sum_6(HaversineContext context)
{
    double sum = 0.0;
    for (size_t i = 0; i < context.pair_count; i++) {
        double lat1_deg = context.pairs[i].v[Y0];
        double lat2_deg = context.pairs[i].v[Y1];
        double lon1_deg = context.pairs[i].v[X0];
        double lon2_deg = context.pairs[i].v[X1];

        double half_d_lat = (RADIANS_PER_DEGREE / 2.0) * (lat2_deg - lat1_deg);
        double half_d_lon = (RADIANS_PER_DEGREE / 2.0) * (lon2_deg - lon1_deg);

        double lat1_cos =
                jk_sin_core(jk_platform_fma_64(RADIANS_PER_DEGREE, lat1_deg, JK_PI / 2.0));
        double lat2_cos =
                jk_sin_core(jk_platform_fma_64(RADIANS_PER_DEGREE, lat2_deg, JK_PI / 2.0));

        double half_d_lat_sin = jk_sin_core(jk_abs_64(half_d_lat));
        double half_d_lon_sin = jk_sin_core(jk_abs_64(half_d_lon));

        double a = jk_platform_fma_64(
                lat1_cos * lat2_cos, square(half_d_lon_sin), square(half_d_lat_sin));

        b32 in_asin_standard_range = a <= 0.5;
        double x_squared = in_asin_standard_range ? a : 1.0 - a;
        double asin_core_result = jk_asin_core(x_squared) * jk_sqrt(x_squared);
        double distance =
                in_asin_standard_range ? asin_core_result : (JK_PI / 2.0) - asin_core_result;

        sum += distance;
    }
    return (2.0 * EARTH_RADIUS * sum) / context.pair_count;
}

typedef struct TestFunction {
    char *name;
    HaversineSumFunction call;
} TestFunction;

TestFunction functions[] = {
    {"Reference", haversine_reference_sum},
    {"Custom math", haversine_sum},
    {"1", haversine_sum_1},
    {"2", haversine_sum_2},
    {"3", haversine_sum_3},
    {"4", haversine_sum_4},
    {"5", haversine_sum_5},
    {"6", haversine_sum_6},
};

JkPlatformRepetitionTest tests[JK_ARRAY_COUNT(functions)];

int main(int argc, char **argv)
{
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

    JkPlatformArenaVirtualRoot arena_root;
    JkArena storage = jk_platform_arena_virtual_init(&arena_root, (size_t)1 << 35);

    HaversineContext context = haversine_setup(json_file_name, answers_file_name, &storage);

    uint64_t frequency = jk_platform_cpu_timer_frequency_estimate(100);

    for (uint64_t i = 0; i < JK_ARRAY_COUNT(functions); i++) {
        JkPlatformRepetitionTest *test = &tests[i];

        printf("\n%s\n", functions[i].name);

        jk_platform_repetition_test_run_wave(
                test, context.pair_count * sizeof(context.pairs[0]), frequency, 10);
        b32 passed = 1;
        while (jk_platform_repetition_test_running_baseline(test, tests + 0)) {
            jk_platform_repetition_test_time_begin(test);
            double sum = functions[i].call(context);
            jk_platform_repetition_test_count_bytes(
                    test, context.pair_count * sizeof(context.pairs[0]));
            jk_platform_repetition_test_time_end(test);
            if (context.answers && !jk_float64_equal(sum, context.sum_answer, 0.00000001f)) {
                passed = 0;
            }
        }
        if (!passed) {
            fprintf(stderr, "WARNING: sum didn't match answer file sum\n");
        }
    }

    return 0;
}
