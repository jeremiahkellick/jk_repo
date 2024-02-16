#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <jk_src/jk_lib/command_line/options.c>
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
    char *file_path = DEFAULT_FILE_PATH;
    {
        jk_options_parse(argc, argv, opts, opt_results, OPT_COUNT, &opts_parse);
        if (opts_parse.operand_count == 1) {
            file_path = opts_parse.operands[0];
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
                   "\thaversine_generator [-n PAIR_COUNT] [-s SEED] [FILE]\n\n"
                   "DESCRIPTION\n"
                   "\thaversine_generator generates JSON for randomly-selected haversine\n"
                   "\tcoordinate pairs, then writes it to FILE. FILE defaults to " DEFAULT_FILE_PATH
                   "\n\n");
            jk_options_print_help(stdout, opts, OPT_COUNT);
            exit(opts_parse.usage_error);
        }
    }

    printf("pair_count: %d\nseed: %u\nfile_path: %s\n", pair_count, seed, file_path);

    return 0;
}
