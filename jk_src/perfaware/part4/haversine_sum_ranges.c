#include <math.h>
#include <stdlib.h>

// #jk_build single_translation_unit

// #jk_build dependencies_begin
#include <jk_src/jk_lib/platform/platform.h>
#include <jk_src/perfaware/part2/haversine_lib.h>
// #jk_build dependencies_end

typedef struct Interval {
    double min;
    double max;
} Interval;

typedef enum MathFunction {
    MATH_FUNCTION_SIN,
    MATH_FUNCTION_COS,
    MATH_FUNCTION_ASIN,
    MATH_FUNCTION_SQRT,

    MATH_FUNCTION_COUNT,
} MathFunction;

static char *interval_names[MATH_FUNCTION_COUNT] = {
    "sin",
    "cos",
    "asin",
    "sqrt",
};

static Interval intervals[MATH_FUNCTION_COUNT] = {
    {.min = INFINITY, .max = -INFINITY},
    {.min = INFINITY, .max = -INFINITY},
    {.min = INFINITY, .max = -INFINITY},
    {.min = INFINITY, .max = -INFINITY},
};

static void print_tracked_intervals(void)
{
    printf("\n");
    for (int i = 0; i < MATH_FUNCTION_COUNT; i++) {
        printf("%s: %.3f-%.3f\n", interval_names[i], intervals[i].min, intervals[i].max);
    }
    printf("\n");
}

static void interval_include(Interval *interval, double value)
{
    if (value < interval->min) {
        interval->min = value;
    }
    if (value > interval->max) {
        interval->max = value;
    }
}

static double tracked_sin(double value)
{
    interval_include(&intervals[MATH_FUNCTION_SIN], value);
    return sin(value);
}

static double tracked_cos(double value)
{
    interval_include(&intervals[MATH_FUNCTION_COS], value);
    return cos(value);
}

static double tracked_asin(double value)
{
    interval_include(&intervals[MATH_FUNCTION_ASIN], value);
    return asin(value);
}

static double tracked_sqrt(double value)
{
    interval_include(&intervals[MATH_FUNCTION_SQRT], value);
    return sqrt(value);
}

static double haversine_track_intervals(
        double X0, double Y0, double X1, double Y1, double EarthRadius)
{
    double lat1 = Y0;
    double lat2 = Y1;
    double lon1 = X0;
    double lon2 = X1;

    double dLat = radians_from_degrees(lat2 - lat1);
    double dLon = radians_from_degrees(lon2 - lon1);
    lat1 = radians_from_degrees(lat1);
    lat2 = radians_from_degrees(lat2);

    double a = square(tracked_sin(dLat / 2.0))
            + tracked_cos(lat1) * tracked_cos(lat2) * square(tracked_sin(dLon / 2));
    double c = 2.0 * tracked_asin(tracked_sqrt(a));

    double Result = EarthRadius * c;

    return Result;
}

static double haversine_track_intervals_sum(HaversineContext context)
{
    double sum = 0.0;
    double sum_coefficient = 1.0 / (double)context.pair_count;

    for (uint64_t i = 0; i < context.pair_count; i++) {
        HaversinePair pair = context.pairs[i];
        sum += sum_coefficient
                * haversine_track_intervals(
                        pair.v[X0], pair.v[Y0], pair.v[X1], pair.v[Y1], EARTH_RADIUS);
    }

    return sum;
}

static uint64_t haversine_track_intervals_verify(HaversineContext context)
{
    uint64_t error_count = 0;

    for (uint64_t i = 0; i < context.pair_count; i++) {
        HaversinePair pair = context.pairs[i];
        if (!approximately_equal(context.answers[i],
                    haversine_track_intervals(
                            pair.v[X0], pair.v[Y0], pair.v[X1], pair.v[Y1], EARTH_RADIUS))) {
            error_count++;
        }
    }

    return error_count;
}

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
                   "\thaversine_sum_ranges - finds the ranges of values used in math functions\n\n"
                   "SYNOPSIS\n"
                   "\thaversine_sum_ranges JSON_FILE [ANSWER_FILE]\n\n");
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

    uint64_t error_count = haversine_track_intervals_verify(context);
    double sum = haversine_track_intervals_sum(context);
    b32 sum_mismatched = !approximately_equal(sum, context.sum_answer);

    if (error_count) {
        fprintf(stderr, "WARNING: %llu haversines mismatched\n", (unsigned long long)error_count);
    }
    if (sum_mismatched) {
        fprintf(stderr, "WARNING: haversine sum mismatched\n");
    }

    print_tracked_intervals();

    jk_platform_profile_end_and_print();

    return 0;
}
