#ifndef JK_COMMAND_LINE_OPTIONS_H
#define JK_COMMAND_LINE_OPTIONS_H

#include <stdbool.h>
#include <stdint.h>

typedef struct JkOption {
    /**
     * Character used as the short-option flag. The null character means there is no short form of
     * this option. An option must have some way to refer to it. If this is the null character,
     * long_name must not be null.
     */
    char flag;

    /**
     * The long name of this option. NULL means there is no long name for this option. An option
     * must have some way to refer to it. If this is NULL, flag must not be the null character.
     */
    char *long_name;

    /** Name of the argument for this option. NULL if this option does not accept an argument. */
    char *arg_name;

    /** Description of this option used to print help text */
    char *description;
} JkOption;

typedef union JkOptionResult {
    bool present;
    char *arg;
} JkOptionResult;

typedef struct JkOptionsParseResult {
    /** Pointer to the first operand (first non-option argument) */
    char **operands;
    size_t operand_count;
    bool usage_error;
} JkOptionsParseResult;

void jk_options_parse(int argc,
        char **argv,
        JkOption *options_in,
        JkOptionResult *options_out,
        size_t option_count,
        JkOptionsParseResult *result);

void jk_options_print_help(FILE *file, JkOption *options, int option_count);

#endif
