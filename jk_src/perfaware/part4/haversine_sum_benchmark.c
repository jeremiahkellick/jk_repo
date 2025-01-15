#include <jk_gen/perfaware/part4/haversine_sum_benchmark.stu.h>

// #jk_build dependencies_begin
#include <jk_src/jk_lib/platform/platform.h>
#include <jk_src/perfaware/part2/haversine_lib.h>
// #jk_build dependencies_end

typedef struct Domain {
    char *name;
    double min;
    double max;
} Domain;

typedef enum DomainType {
    DOMAIN_SQRT,
    DOMAIN_ASIN,
    DOMAIN_SIN,
    DOMAIN_COS,

    DOMAIN_COUNT,
} MathFunction;

static Domain domains[DOMAIN_COUNT] = {
    {.name = "sqrt", .min = INFINITY, .max = -INFINITY},
    {.name = "asin", .min = INFINITY, .max = -INFINITY},
    {.name = "sin", .min = INFINITY, .max = -INFINITY},
    {.name = "cos", .min = INFINITY, .max = -INFINITY},
};

JK_PUBLIC void print_tracked_domains(void)
{
    printf("\n");
    for (int i = 0; i < DOMAIN_COUNT; i++) {
        Domain *d = &domains[i];
        printf("%s: %.3f-%.3f\n", d->name, d->min, d->max);
    }
    printf("\n");
}

static void update_domain(Domain *domain, double value)
{
    if (value < domain->min) {
        domain->min = value;
    }
    if (value > domain->max) {
        domain->max = value;
    }
}

static double tracked_sqrt(double value)
{
    update_domain(&domains[DOMAIN_SQRT], value);
    return sqrt(value);
}

static double tracked_asin(double value)
{
    update_domain(&domains[DOMAIN_ASIN], value);
    return asin(value);
}

static double tracked_sin(double value)
{
    update_domain(&domains[DOMAIN_SIN], value);
    return sqrt(value);
}

static double tracked_cos(double value)
{
    update_domain(&domains[DOMAIN_COS], value);
    return cos(value);
}

JK_PUBLIC double haversine_track_domains(
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

JK_PUBLIC double haversine_track_domains_sum(HaversineContext context)
{
    double sum = 0.0;
    double sum_coefficient = 1.0 / (double)context.pair_count;

    for (uint64_t i = 0; i < context.pair_count; i++) {
        HaversinePair pair = context.pairs[i];
        sum += sum_coefficient
                * haversine_track_domains(
                        pair.v[X0], pair.v[Y0], pair.v[X1], pair.v[Y1], EARTH_RADIUS);
    }

    print_tracked_domains();

    return sum;
}

typedef struct Test {
    char *name;
    HaversineSumFunction compute;
    HaversineSumVerifyFunction verify;
} Test;

static Test tests[] = {
    {.name = "Reference", .compute = haversine_reference_sum, .verify = haversine_reference_verify},
    {.name = "Tracked domains",
        .compute = haversine_track_domains_sum,
        .verify = haversine_reference_verify},
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
