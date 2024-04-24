#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <jk_gen/single_translation_unit.h>

// #jk_build dependencies_begin
#include <jk_src/jk_lib/jk_lib.h>
#include <jk_src/jk_lib/json/json.h>
#include <jk_src/jk_lib/profile/profile.h>
#include <jk_src/perfaware/part2/haversine_reference.h>
// #jk_build dependencies_end

typedef enum Coordinate {
    X0,
    Y0,
    X1,
    Y1,
    COORDINATE_COUNT,
} Coordinate;

typedef struct HaversinePair {
    double v[COORDINATE_COUNT];
} HaversinePair;

char *coordinate_names[COORDINATE_COUNT] = {
    "x0",
    "y0",
    "x1",
    "y1",
};

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
    jk_profile_begin();

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

    JkArena storage;
    jk_arena_init(&storage, (size_t)1 << 35);

    JkBuffer text = jk_file_read_full(json_file_name, &storage);
    JkBuffer answers = {0};
    if (answer_file_name) {
        answers = jk_file_read_full(answer_file_name, &storage);
    }

    JK_PROFILE_ZONE_TIME_BEGIN(parse_haversine_pairs);

    JK_PROFILE_ZONE_BANDWIDTH_BEGIN(parse_json, text.size);
    JkJson *json = jk_json_parse(text, &storage);
    JK_PROFILE_ZONE_END(parse_json);

    if (json == NULL) {
        fprintf(stderr, "%s: Failed to parse JSON\n", program_name);
        exit(1);
    }
    if (json->type != JK_JSON_OBJECT) {
        fprintf(stderr, "%s: JSON was not an object\n", program_name);
        exit(1);
    }
    JkJson *pairs_json = jk_json_member_get(json, "pairs");
    if (pairs_json == NULL) {
        fprintf(stderr, "%s: JSON object did not have a \"pairs\" member\n", program_name);
        exit(1);
    }
    if (pairs_json->type != JK_JSON_ARRAY) {
        fprintf(stderr, "%s: JSON object \"pairs\" member was not an array\n", program_name);
        exit(1);
    }

    size_t pair_count = pairs_json->child_count;
    size_t pairs_buffer_size = sizeof(HaversinePair) * pair_count;
    HaversinePair *pairs = jk_arena_push(&storage, pairs_buffer_size);

    if (answers.size) {
        assert(answers.size == sizeof(double) * (pair_count + 1));
    }

    JK_PROFILE_ZONE_TIME_BEGIN(lookup_and_convert);
    {
        size_t i = 0;
        for (JkJson *pair_json = pairs_json->first_child; pair_json;
                pair_json = pair_json->sibling, i++) {
            for (JkJson *coord_json = pair_json->first_child; coord_json;
                    coord_json = coord_json->sibling) {
                if (coord_json->name.size != 2) {
                    continue;
                }
                uint8_t x_or_y = coord_json->name.data[0] - 'x';
                uint8_t zero_or_one = coord_json->name.data[1] - '0';
                uint8_t j = (zero_or_one << 1) | x_or_y;
                if (x_or_y < 2 && zero_or_one < 2) {
                    pairs[i].v[j] = jk_json_parse_number(coord_json->value);
                }
            }
        }
    }
    JK_PROFILE_ZONE_END(lookup_and_convert);

    JK_PROFILE_ZONE_END(parse_haversine_pairs);

    JK_PROFILE_ZONE_BANDWIDTH_BEGIN(sum, pairs_buffer_size);
    double sum = 0.0;
    double sum_coefficient = 1.0 / (double)pair_count;
    for (size_t i = 0; i < pair_count; i++) {
        double distance = haversine_reference(
                pairs[i].v[X0], pairs[i].v[Y0], pairs[i].v[X1], pairs[i].v[Y1], EARTH_RADIUS);

#ifndef NDEBUG
        if (answers.size) {
            assert(approximately_equal(distance, JK_DATA_GET(answers.data, i, double)));
        }
#endif

        sum += distance * sum_coefficient;
    }
    JK_PROFILE_ZONE_END(sum);

    printf("Pair count: %zu\n", pair_count);
    printf("Haversine sum: %.16f\n", sum);

    if (answers.size) {
        double ref_sum = JK_DATA_GET(answers.data, pair_count, double);
        printf("\nReference sum: %.16f\n", ref_sum);
        printf("Difference: %.16f\n\n", sum - ref_sum);
    }

    jk_profile_end_and_print();

    return 0;
}
