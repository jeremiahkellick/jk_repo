#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <jk_src/jk_lib/hash.c>

#define DEFAULT_FILE_PATH "./coords.json"

#define DEFAULT_PAIR_COUNT 1000
#define DEFAULT_PAIR_COUNT_STRING "1,000"
#define MAX_PAIR_COUNT 1000000000
#define MAX_PAIR_COUNT_STRING "1,000,000,000"

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

int main(int argc, char **argv)
{
    program_name = argv[0];

    // Parse arguments
    int pair_count = DEFAULT_PAIR_COUNT;
    unsigned seed = jk_hash_uint32((uint32_t)time(NULL));
    char *file_path = DEFAULT_FILE_PATH;
    {
        char *pair_count_string = NULL;
        char *seed_string = NULL;
        bool help = false;
        bool usage_error = false;
        bool options_ended = false;
        unsigned non_option_arguments = 0;
        for (int i = 1; i < argc; i++) {
            if (argv[i][0] == '-' && argv[i][1] != '\0' && !options_ended) { // Option argument
                if (argv[i][1] == '-') {
                    if (argv[i][2] == '\0') { // -- encountered
                        options_ended = true;
                    } else { // Double hyphen option
                        char *name = &argv[i][2];
                        int end = 0;
                        while (name[end] != '=' && name[end] != '\0') {
                            end++;
                        }
                        if (strncmp(name, "help", end) == 0) {
                            help = true;
                        } else if (strncmp(name, "pair-count", end) == 0) {
                            if (name[end] == '=' && name[end + 1] != '\0') {
                                pair_count_string = &name[end + 1];
                            } else {
                                pair_count_string = argv[++i];
                            }
                        } else if (strncmp(name, "seed", end) == 0) {
                            if (name[end] == '=' && name[end + 1] != '\0') {
                                seed_string = &name[end + 1];
                            } else {
                                seed_string = argv[++i];
                            }
                        } else {
                            fprintf(stderr, "%s: Invalid option '%s'\n", program_name, argv[i]);
                            usage_error = true;
                        }
                    }
                } else { // Single-hypen option(s)
                    bool has_argument = false;
                    for (char *c = &argv[i][1]; *c != '\0' && !has_argument; c++) {
                        switch (*c) {
                        case 'n':
                            has_argument = true;
                            pair_count_string = ++c;
                            if (*pair_count_string == '\0') {
                                pair_count_string = argv[++i];
                            }
                            break;
                        case 's':
                            has_argument = true;
                            seed_string = ++c;
                            if (*seed_string == '\0') {
                                seed_string = argv[++i];
                            }
                            break;
                        default:
                            fprintf(stderr,
                                    "%s: Invalid option '%c' in '%s'\n",
                                    program_name,
                                    *c,
                                    argv[i]);
                            usage_error = true;
                            break;
                        }
                    }
                }
                if (i >= argc) {
                    fprintf(stderr,
                            "%s: Option '%s' missing required argument\n",
                            program_name,
                            argv[i - 1]);
                    usage_error = true;
                }
            } else { // Regular argument
                non_option_arguments++;
                file_path = argv[i];
            }
        }
        if (!help && non_option_arguments > 1) {
            fprintf(stderr,
                    "%s: Expected 0-1 non-option arguments, got %u\n",
                    program_name,
                    non_option_arguments);
            usage_error = true;
        }
        // Convert pair_count_string to integer
        if (pair_count_string) {
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
                    usage_error = true;
                    break;
                }
            }
        }
        // Convert seed string to seed
        if (seed_string) {
            seed = hash_string(seed_string);
        }
        if (help || usage_error) {
            printf("NAME\n"
                   "\thaversine_generator - generates JSON for haversine coordinate pairs\n\n"
                   "SYNOPSIS\n"
                   "\thaversine_generator [-n PAIR_COUNT] [-s SEED] [FILE]\n\n"
                   "DESCRIPTION\n"
                   "\thaversine_generator generates JSON for randomly-selected haversine\n"
                   "\tcoordinate pairs, then writes it to FILE. FILE defaults to " DEFAULT_FILE_PATH
                   "\n\n"
                   "OPTIONS\n"
                   "\t--help\tDisplay this help text and exit.\n\n"
                   "\t-n PAIR_COUNT, --pair-count=PAIR_COUNT\n"
                   "\t\tGenerate PAIR_COUNT coordinate pairs. Must be a positive integer\n"
                   "\t\tless than " MAX_PAIR_COUNT_STRING ". Permits commas, single quotes, or\n"
                   "\t\tunderscores as digit separators which are ignored. Defaults to\n"
                   "\t\t" DEFAULT_PAIR_COUNT_STRING ".\n\n"
                   "\t-s SEED, --seed=SEED\n"
                   "\t\tUsed to seed the random number generator. Defaults to the\n"
                   "\t\tcurrent time\n\n");
            exit(usage_error);
        }
    }

    printf("pair_count: %d\nseed: %u\nfile_path: %s\n", pair_count, seed, file_path);

    return 0;
}
