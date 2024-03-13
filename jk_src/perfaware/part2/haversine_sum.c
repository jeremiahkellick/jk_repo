#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <jk_gen/single_translation_unit.h>

// #jk_build dependencies_begin
#include <jk_src/jk_lib/command_line/options.h>
#include <jk_src/jk_lib/json/json.h>
#include <jk_src/jk_lib/metrics/metrics.h>
#include <jk_src/perfaware/part2/haversine_reference.h>
// #jk_build dependencies_end

typedef enum Coordinate {
    X0,
    Y0,
    X1,
    Y1,
    COORDINATE_COUNT,
} Coordinate;

char *coordinate_names[COORDINATE_COUNT] = {
    "x0",
    "y0",
    "x1",
    "y1",
};

static size_t stream_read_file(void *file, size_t byte_count, void *buffer)
{
    FILE *file_internal = file;
    return fread(buffer, 1, byte_count, file_internal);
}

static int stream_seek_relative_file(void *file, long offset)
{
    FILE *file_internal = file;
    return fseek(file_internal, offset, SEEK_CUR);
}

static bool approximately_equal(double a, double b)
{
    double diff = a - b;
    return diff > -0.0001 && diff < 0.0001;
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

char *program_name = "<program_name global should be overwritten with argv[0]>";

int main(int argc, char **argv)
{
    uint64_t time_start = jk_cpu_timer_get();

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
            opts_parse.usage_error = true;
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
    char *answer_file_name = NULL;
    if (opts_parse.operand_count > 1) {
        answer_file_name = opts_parse.operands[1];
    }

    FILE *json_file = fopen(json_file_name, "rb");
    if (json_file == NULL) {
        fprintf(stderr,
                "%s: Failed to open '%s': %s\n",
                program_name,
                json_file_name,
                strerror(errno));
        exit(1);
    }

    FILE *answer_file = NULL;
    if (answer_file_name) {
        answer_file = fopen(answer_file_name, "rb");
        if (answer_file == NULL) {
            fprintf(stderr,
                    "%s: Failed to open '%s': %s\n",
                    program_name,
                    answer_file_name,
                    strerror(errno));
            exit(1);
        }
    }

    JkArena storage;
    JkArena tmp_storage;
    jk_arena_init(&storage, (size_t)1 << 35);
    jk_arena_init(&tmp_storage, (size_t)1 << 35);

    uint64_t time_setup = jk_cpu_timer_get();

    JkJsonParseData json_parse_data;
    JkJson *json = jk_json_parse(&storage,
            &tmp_storage,
            stream_read_file,
            stream_seek_relative_file,
            json_file,
            &json_parse_data);

    if (json == NULL) {
        fprintf(stderr, "%s: Failed to parse JSON\n", program_name);
        exit(1);
    }
    if (!(json->type == JK_JSON_COLLECTION
                && json->u.collection.type == JK_JSON_COLLECTION_OBJECT)) {
        fprintf(stderr, "%s: JSON was not an object\n", program_name);
        exit(1);
    }
    JkJson *pairs_json = jk_json_member_get(&json->u.collection, "pairs");
    if (pairs_json == NULL) {
        fprintf(stderr, "%s: JSON object did not have a \"pairs\" member\n", program_name);
        exit(1);
    }
    if (!(pairs_json->type == JK_JSON_COLLECTION
                && pairs_json->u.collection.type == JK_JSON_COLLECTION_ARRAY)) {
        fprintf(stderr, "%s: JSON object \"pairs\" member was not an array\n", program_name);
        exit(1);
    }
    JkJson **pairs = pairs_json->u.collection.elements;
    size_t pair_count = pairs_json->u.collection.count;

    uint64_t time_json_parsed = jk_cpu_timer_get();

    double sum = 0.0;
    double sum_coefficient = 1.0 / (double)pair_count;
    double coords[COORDINATE_COUNT];
    for (size_t i = 0; i < pair_count; i++) {
        if (!(pairs[i]->type == JK_JSON_COLLECTION
                    && pairs[i]->u.collection.type == JK_JSON_COLLECTION_OBJECT)) {
            fprintf(stderr,
                    "%s: An element of the \"pairs\" array was not an object\n",
                    program_name);
            exit(1);
        }
        for (int j = 0; j < COORDINATE_COUNT; j++) {
            JkJson *value = jk_json_member_get(&pairs[i]->u.collection, coordinate_names[j]);
            if (value == NULL) {
                fprintf(stderr,
                        "%s: An object in the \"pairs\" array did not have an \"%s\" member\n",
                        program_name,
                        coordinate_names[j]);
                exit(1);
            }
            if (value->type != JK_JSON_NUMBER) {
                fprintf(stderr,
                        "%s: Found an object in \"pairs\" where \"%s\" was not a number\n",
                        program_name,
                        coordinate_names[j]);
            }
            coords[j] = value->u.number;
        }
        double distance =
                haversine_reference(coords[X0], coords[Y0], coords[X1], coords[Y1], EARTH_RADIUS);

        if (answer_file) {
            double answer;
            if (!fread(&answer, sizeof(answer), 1, answer_file)) {
                fprintf(stderr, "%s: Expected '%s' to be bigger\n", program_name, answer_file_name);
                exit(1);
            }
            assert(approximately_equal(distance, answer));
        }

        sum += distance * sum_coefficient;
    }

    uint64_t time_summed = jk_cpu_timer_get();

    printf("Pair count: %zu\n", pair_count);
    printf("Haversine sum: %.16f\n", sum);

    if (answer_file) {
        double ref_sum;
        if (!fread(&ref_sum, sizeof(ref_sum), 1, answer_file)) {
            fprintf(stderr, "%s: Expected '%s' to be bigger\n", program_name, answer_file_name);
            exit(1);
        }

        printf("\nReference sum: %.16f\n", ref_sum);
        printf("Difference: %.16f\n\n", sum - ref_sum);
    }

    uint64_t time_misc_output_done = jk_cpu_timer_get();

    uint64_t timer_frequency = jk_cpu_timer_frequency_estimate(100);

    uint64_t elapsed_total = time_misc_output_done - time_start;

    uint64_t elapsed_setup = time_setup - time_start;
    uint64_t elapsed_json_parse = time_json_parsed - time_setup;
    uint64_t elapsed_sum = time_summed - time_json_parsed;
    uint64_t elapsed_mixed_output = time_misc_output_done - time_summed;

    printf("Total time: %.4fms (CPU frequency %llu)\n",
            (double)elapsed_total * 1000.0 / (double)timer_frequency,
            timer_frequency);
    printf("\tSetup: %llu (%f%%)\n",
            elapsed_setup,
            (double)elapsed_setup / (double)elapsed_total * 100.0);
    printf("\tParse JSON: %llu (%f%%)\n",
            elapsed_json_parse,
            (double)elapsed_json_parse / (double)elapsed_total * 100.0);
    printf("\tSum: %llu (%f%%)\n",
            elapsed_sum,
            (double)elapsed_sum / (double)elapsed_total * 100.0);
    printf("\tMisc output: %llu (%f%%)\n",
            elapsed_mixed_output,
            (double)elapsed_mixed_output / (double)elapsed_total * 100.0);

    return 0;
}
