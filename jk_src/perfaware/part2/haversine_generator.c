#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <jk_src/jk_lib/command_line/options.c>
#include <jk_src/jk_lib/hash.c>

#define DEFAULT_JSON_FILE_PATH "./coords.json"
#define DEFAULT_ANSWER_FILE_PATH "./answers.f64"

#define DEFAULT_PAIR_COUNT 1000
#define DEFAULT_PAIR_COUNT_STRING "1,000"
#define MAX_PAIR_COUNT 1000000000
#define MAX_PAIR_COUNT_STRING "1,000,000,000"

#define EARTH_RADIUS 6372.8
#define PI 3.14159265358979323846264338327950288
#define DEGREES_TO_RADIANS (PI / 180.0)

char *program_name = "<program_name global should be overwritten with argv[0]>";

typedef union Bits32 {
    unsigned u;
    char c[4];
} Bits32;

static unsigned hash_string(char *string)
{
    unsigned hash = 0;
    while (*string != '\0') {
        Bits32 bits = {0};
        for (int i = 0; i < sizeof(bits.c) && *string != '\0'; i++, string++) {
            bits.c[i] = *string;
        }
        hash = jk_hash_uint32(hash ^ bits.u);
    }
    return hash;
}

static double random_between(double min, double max)
{
    return min + ((double)rand() / (double)RAND_MAX) * (max - min);
}

static double square(double A)
{
    double Result = (A * A);
    return Result;
}

static double radians_from_degrees(double Degrees)
{
    double Result = 0.01745329251994329577 * Degrees;
    return Result;
}

// NOTE(casey): EarthRadius is generally expected to be 6372.8
static double reference_haversine(double X0, double Y0, double X1, double Y1, double EarthRadius)
{
    /* NOTE(casey): This is not meant to be a "good" way to calculate the Haversine distance.
       Instead, it attempts to follow, as closely as possible, the formula used in the real-world
       question on which these homework exercises are loosely based.
    */

    double lat1 = Y0;
    double lat2 = Y1;
    double lon1 = X0;
    double lon2 = X1;

    double dLat = radians_from_degrees(lat2 - lat1);
    double dLon = radians_from_degrees(lon2 - lon1);
    lat1 = radians_from_degrees(lat1);
    lat2 = radians_from_degrees(lat2);

    double a = square(sin(dLat / 2.0)) + cos(lat1) * cos(lat2) * square(sin(dLon / 2));
    double c = 2.0 * asin(sqrt(a));

    double Result = EarthRadius * c;

    return Result;
}

typedef enum Opt {
    OPT_HELP,
    OPT_PAIR_COUNT,
    OPT_SEED,
    OPT_COUNT,
} Opt;

JkOption opts[OPT_COUNT] = {
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
    int pair_count = DEFAULT_PAIR_COUNT;
    unsigned seed = jk_hash_uint32((uint32_t)time(NULL));
    char *json_file_path = DEFAULT_JSON_FILE_PATH;
    char *answer_file_path = DEFAULT_ANSWER_FILE_PATH;
    {
        jk_options_parse(argc, argv, opts, opt_results, OPT_COUNT, &opts_parse);
        if (opts_parse.operand_count == 1) {
            json_file_path = opts_parse.operands[0];
        } else if (opts_parse.operand_count > 1 && !opt_results[OPT_HELP].present) {
            fprintf(stderr,
                    "%s: Expected 0-1 operands, got %zu\n",
                    program_name,
                    opts_parse.operand_count);
            opts_parse.usage_error = true;
        }
        // Convert pair_count_string to integer
        if (opt_results[OPT_PAIR_COUNT].present) {
            char *pair_count_string = opt_results[OPT_PAIR_COUNT].arg;
            int multiplier = 1;
            pair_count = 0;
            for (int i = (int)strlen(pair_count_string) - 1; i >= 0; i--) {
                if (isdigit(pair_count_string[i])) {
                    pair_count += (pair_count_string[i] - '0') * multiplier;
                    multiplier *= 10;
                } else if (!(pair_count_string[i] == ',' || pair_count_string[i] == '\''
                                   || pair_count_string[i] == '_')) {
                    // Error if character is not a digit or one of the permitted separators
                    fprintf(stderr,
                            "%s: Invalid value for option -n (--pair-count): expected a positive "
                            "integer, got '%s'\n",
                            program_name,
                            pair_count_string);
                    opts_parse.usage_error = true;
                    break;
                }
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
                   "\thaversine_generator [-n PAIR_COUNT] [-s SEED] [JSON_FILE] [ANSWER_FILE]\n\n"
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
    double sum = 0.0;
    for (int i = 0; i < pair_count; i++) {
        double x0 = random_between(-180.0, 180.0);
        double y0 = random_between(-90.0, 90.0);
        double x1 = random_between(-180.0, 180.0);
        double y1 = random_between(-90.0, 90.0);
        fprintf(json_file,
                "%s    {\"x0\": %.16f, \"y0\": %.16f, \"x1\": %.16f, \"y1\": %.16f}",
                i == 0 ? "" : ",\n",
                x0,
                y0,
                x1,
                y1);
        double distance = reference_haversine(x0, y0, x1, y1, EARTH_RADIUS);
        sum += distance;
        fwrite(&distance, sizeof(double), 1, answers_file);
    }
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
