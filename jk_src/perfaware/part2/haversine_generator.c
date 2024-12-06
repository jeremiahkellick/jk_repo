#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <jk_gen/single_translation_unit.h>

// #jk_build dependencies_begin
#include <jk_src/jk_lib/jk_lib.h>
#include <jk_src/perfaware/part2/haversine_lib.h>
// #jk_build dependencies_end

#define DEFAULT_JSON_FILE_PATH "./coords.json"
#define DEFAULT_ANSWER_FILE_PATH "./answers.f64"

#define DEFAULT_PAIR_COUNT 1000
#define DEFAULT_PAIR_COUNT_STRING "1,000"
#define MAX_PAIR_COUNT 1000000000
#define MAX_PAIR_COUNT_STRING "1,000,000,000"

#define MIN_CLUSTER_COUNT 1
#define MIN_CLUSTER_COUNT_STRING "1"
#define MAX_CLUSTER_COUNT 1000000000
#define MAX_CLUSTER_COUNT_STRING "1,000,000,000"

char *program_name = "<program_name global should be overwritten with argv[0]>";

typedef union Bits32 {
    unsigned u;
    char c[4];
} Bits32;

typedef struct Cluster {
    double x_min;
    double y_min;
} Cluster;

static unsigned hash_string(char *string)
{
    unsigned hash = 0;
    while (*string != '\0') {
        Bits32 bits = {0};
        for (size_t i = 0; i < sizeof(bits.c) && *string != '\0'; i++, string++) {
            bits.c[i] = *string;
        }
        hash = jk_hash_uint32(hash ^ bits.u);
    }
    return hash;
}

static double random_within(double min, double radius)
{
    return min + ((double)rand() / (double)RAND_MAX) * radius;
}

Cluster random_cluster(void)
{
    Cluster cluster = {
        .x_min = random_within(0.0, 360.0 - 45.0),
        .y_min = random_within(0.0, 180.0 - 45.0),
    };
    return cluster;
}

typedef enum Opt {
    OPT_CLUSTER_COUNT,
    OPT_HELP,
    OPT_PAIR_COUNT,
    OPT_SEED,
    OPT_COUNT,
} Opt;

JkOption opts[OPT_COUNT] = {
    {
        .flag = 'c',
        .long_name = "clusters",
        .arg_name = "CLUSTER_COUNT",
        .description = "\n"
                       "\t\tAs PAIR_COUNT gets large, the expected sums for even very\n"
                       "\t\tdifferent data converge to similar values. This option helps to\n"
                       "\t\tcounteract that effect by dividing the randomly selected points\n"
                       "\t\tinto CLUSTER_COUNT different areas and placing x0,y0 and x1,y1\n"
                       "\t\tof each pair in different clusters. Each area limits the x and y\n"
                       "\t\tcoordinates to two randomly-selected 45-degree arcs.\n"
                       "\t\tCLUSTER_COUNT must be a positive integer greater "
                       "than " MIN_CLUSTER_COUNT_STRING " and less\n"
                       "\t\tthan " MAX_CLUSTER_COUNT_STRING ".\n",
    },
    {
        .flag = '\0',
        .long_name = "help",
        .arg_name = NULL,
        .description = "\tDisplay this help text and exit.\n",
    },
    {
        .flag = 'n',
        .long_name = "pair-count",
        .arg_name = "PAIR_COUNT",
        .description =
                "\n"
                "\t\tGenerate PAIR_COUNT coordinate pairs. Must be a positive integer\n"
                "\t\tless than " MAX_PAIR_COUNT_STRING ". Permits commas, single quotes, or\n"
                "\t\tunderscores as digit separators which are ignored. Defaults to\n"
                "\t\t" DEFAULT_PAIR_COUNT_STRING ".\n",
    },
    {
        .flag = 's',
        .long_name = "seed",
        .arg_name = "SEED",
        .description = "\n"
                       "\t\tUsed to seed the random number generator. Defaults to the\n"
                       "\t\tcurrent time\n",
    },
};

JkOptionResult opt_results[OPT_COUNT] = {0};

JkOptionsParseResult opts_parse = {0};

int main(int argc, char **argv)
{
    program_name = argv[0];

    // Parse arguments
    int cluster_count = -1;
    int pair_count = DEFAULT_PAIR_COUNT;
    unsigned seed = jk_hash_uint32((uint32_t)time(NULL));
    char *json_file_path = DEFAULT_JSON_FILE_PATH;
    char *answer_file_path = DEFAULT_ANSWER_FILE_PATH;
    {
        jk_options_parse(argc, argv, opts, opt_results, OPT_COUNT, &opts_parse);
        if (opts_parse.operand_count > 2 && !opt_results[OPT_HELP].present) {
            fprintf(stderr,
                    "%s: Expected 0-2 operands, got %zu\n",
                    program_name,
                    opts_parse.operand_count);
            opts_parse.usage_error = true;
        } else {
            if (opts_parse.operand_count >= 1) {
                json_file_path = opts_parse.operands[0];
            }
            if (opts_parse.operand_count >= 2) {
                answer_file_path = opts_parse.operands[1];
            }
        }
        if (opt_results[OPT_CLUSTER_COUNT].present) {
            cluster_count = jk_parse_positive_integer(opt_results[OPT_CLUSTER_COUNT].arg);
            if (cluster_count < 0) {
                fprintf(stderr,
                        "%s: Invalid argument for option -c (--clusters): Expected a positive "
                        "integer, got '%s'\n",
                        program_name,
                        opt_results[OPT_CLUSTER_COUNT].arg);
                opts_parse.usage_error = true;
            } else if (cluster_count <= MIN_CLUSTER_COUNT || cluster_count >= MAX_CLUSTER_COUNT) {
                fprintf(stderr,
                        "%s: Invalid argument for option -c (--clusters): Expected integer greater "
                        "than %s and less than %s\n",
                        program_name,
                        MIN_CLUSTER_COUNT_STRING,
                        MAX_CLUSTER_COUNT_STRING);
                opts_parse.usage_error = true;
            }
        }
        if (opt_results[OPT_PAIR_COUNT].present) {
            pair_count = jk_parse_positive_integer(opt_results[OPT_PAIR_COUNT].arg);
            if (pair_count < 0) {
                fprintf(stderr,
                        "%s: Invalid argument for option -n (--pair-count): Expected a positive "
                        "integer, got '%s'\n",
                        program_name,
                        opt_results[OPT_PAIR_COUNT].arg);
                opts_parse.usage_error = true;
            }
            if (pair_count >= MAX_PAIR_COUNT) {
                fprintf(stderr,
                        "%s: Invalid argument for option -n (--pair-count): Expected integer less "
                        "than %s\n",
                        program_name,
                        MAX_PAIR_COUNT_STRING);
                opts_parse.usage_error = true;
            }
        }
        // Convert seed string to seed
        if (opt_results[OPT_SEED].present) {
            seed = hash_string(opt_results[OPT_SEED].arg);
        }
        if (opt_results[OPT_HELP].present || opts_parse.usage_error) {
            printf("NAME\n"
                   "\thaversine_generator - generates JSON for haversine coordinate pairs\n\n"
                   "SYNOPSIS\n"
                   "\thaversine_generator [-c CLUSTER_COUNT] [-n PAIR_COUNT] [-s SEED]\n"
                   "\t\t[JSON_FILE] [ANSWER_FILE]\n\n"
                   "DESCRIPTION\n"
                   "\thaversine_generator generates JSON for randomly-selected haversine\n"
                   "\tcoordinate pairs, then writes the JSON to JSON_FILE. It also writes the\n"
                   "\thaversine distances for each pair in binary to ANSWER_FILE. That file\n"
                   "\twill contain one 64-bit floating point value for each pair. JSON_FILE\n"
                   "\tdefaults to " DEFAULT_JSON_FILE_PATH
                   ". ANSWER_FILE defaults to " DEFAULT_ANSWER_FILE_PATH "\n\n");
            jk_options_print_help(stdout, opts, OPT_COUNT);
            exit(opts_parse.usage_error);
        }
    }

    srand(seed);

    FILE *json_file = fopen(json_file_path, "wb");
    if (json_file == NULL) {
        fprintf(stderr,
                "%s: Failed to open file '%s': %s",
                program_name,
                json_file_path,
                strerror(errno));
        exit(1);
    }
    FILE *answers_file = fopen(answer_file_path, "wb");
    if (answers_file == NULL) {
        fprintf(stderr,
                "%s: Failed to open file '%s': %s",
                program_name,
                answer_file_path,
                strerror(errno));
        fclose(json_file);
        exit(1);
    }

    fprintf(json_file, "{\"pairs\": [\n");

    double x_radius;
    double y_radius;
    int points_per_cluster;
    Cluster cluster0 = {0};
    Cluster cluster1 = {0};

    if (cluster_count == -1) {
        x_radius = 360.0;
        y_radius = 180.0;
        points_per_cluster = INT_MAX;
    } else {
        x_radius = 45.0;
        y_radius = 45.0;
        points_per_cluster = pair_count * 2 / cluster_count;
        if (points_per_cluster < 1) {
            points_per_cluster = 1;
        }

        cluster0 = random_cluster();
        cluster1 = random_cluster();
    }

    int remaining_in_cluster = points_per_cluster;

    double sum = 0.0;
    double sum_coefficient = 1.0 / (double)pair_count;
    for (int i = 0; i < pair_count; i++) {
        double x0 = random_within(cluster0.x_min, x_radius);
        double y0 = random_within(cluster0.y_min, y_radius);
        double x1 = random_within(cluster1.x_min, x_radius);
        double y1 = random_within(cluster1.y_min, y_radius);

        fprintf(json_file,
                "%s    {\"x0\": %.16f, \"y0\": %.16f, \"x1\": %.16f, \"y1\": %.16f}",
                i == 0 ? "" : ",\n",
                x0,
                y0,
                x1,
                y1);

        double distance = haversine_reference(x0, y0, x1, y1, EARTH_RADIUS);
        sum += distance * sum_coefficient;
        fwrite(&distance, sizeof(double), 1, answers_file);

        remaining_in_cluster -= 2;
        if (remaining_in_cluster <= 0) {
            remaining_in_cluster = points_per_cluster;
            cluster0 = random_cluster();
            cluster1 = random_cluster();
        }
    }
    fwrite(&sum, sizeof(double), 1, answers_file);

    fprintf(json_file, "\n]}\n");

    if (opt_results[OPT_SEED].present) {
        printf("Seed: %s\n", opt_results[OPT_SEED].arg);
    }
    printf("Pair count: %d\n", pair_count);
    printf("Expected sum: %.16f\n", sum);

    fclose(answers_file);
    fclose(json_file);

    return 0;
}
