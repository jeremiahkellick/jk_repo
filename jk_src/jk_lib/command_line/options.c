#include <stdio.h>
#include <string.h>

#include "options.h"

static void jk_argv_swap_to_front(char **argv, char **arg)
{
    for (; arg > argv; arg--) {
        char *tmp = *arg;
        *arg = *(arg - 1);
        *(arg - 1) = tmp;
    }
}

void jk_options_parse(int argc,
        char **argv,
        JkOption *options_in,
        JkOptionResult *options_out,
        size_t option_count,
        JkOptionsParseResult *result)
{
    bool options_ended = false;
    result->operands = &argv[argc];
    result->operand_count = 0;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] != '\0' && !options_ended) { // Option argument
            bool i_plus_one_is_arg = false;
            if (argv[i][1] == '-') {
                if (argv[i][2] == '\0') { // -- encountered
                    options_ended = true;
                } else { // Double hyphen option
                    char *name = &argv[i][2];
                    int end = 0;
                    while (name[end] != '=' && name[end] != '\0') {
                        end++;
                    }
                    bool matched = false;
                    for (size_t j = 0; !matched && j < option_count; j++) {
                        if (options_in[j].long_name
                                && strncmp(name, options_in[j].long_name, end) == 0) {
                            matched = true;
                            options_out[j].present = true;

                            if (options_in[j].arg_name) {
                                if (name[end] == '=') {
                                    if (name[end + 1] != '\0') {
                                        options_out[j].arg = &name[end + 1];
                                    }
                                } else {
                                    i_plus_one_is_arg = true;
                                    options_out[j].arg = argv[i + 1];
                                }
                            } else {
                                if (name[end] == '=') {
                                    fprintf(stderr,
                                            "%s: Error in '%s': Option does not accept an "
                                            "argument\n",
                                            argv[0],
                                            argv[i]);
                                    result->usage_error = true;
                                }
                            }
                        }
                    }
                    if (!matched) {
                        fprintf(stderr, "%s: Invalid option '%s'\n", argv[0], argv[i]);
                        result->usage_error = true;
                    }
                }
            } else { // Single-hypen option(s)
                bool has_argument = false;
                for (char *c = &argv[i][1]; *c != '\0' && !has_argument; c++) {
                    bool matched = false;
                    for (size_t j = 0; !matched && j < option_count; j++) {
                        if (*c == options_in[j].flag) {
                            matched = true;
                            options_out[j].present = true;
                            has_argument = options_in[j].arg_name != NULL;

                            if (has_argument) {
                                options_out[j].arg = ++c;
                                if (options_out[j].arg[0] == '\0') {
                                    i_plus_one_is_arg = true;
                                    options_out[j].arg = argv[i + 1];
                                }
                            }
                        }
                    }
                    if (!matched) {
                        fprintf(stderr, "%s: Invalid option '%c' in '%s'\n", argv[0], *c, argv[i]);
                        result->usage_error = true;
                        break;
                    }
                }
            }
            if (&argv[i] > result->operands) {
                jk_argv_swap_to_front(result->operands, &argv[i]);
                result->operands++;
            }
            if (i_plus_one_is_arg) {
                if (argv[i + 1]) {
                    i++;
                    if (&argv[i] > result->operands) {
                        jk_argv_swap_to_front(result->operands, &argv[i]);
                        result->operands++;
                    }
                } else {
                    fprintf(stderr,
                            "%s: Option '%s' missing required argument\n",
                            argv[0],
                            argv[i - 1]);
                    result->usage_error = true;
                }
            }
        } else { // Regular argument
            result->operand_count++;
            if (&argv[i] < result->operands) {
                result->operands = &argv[i];
            }
        }
    }
}

void jk_options_print_help(FILE *file, JkOption *options, int option_count)
{
    fprintf(file, "OPTIONS\n");
    for (int i = 0; i < option_count; i++) {
        if (i != 0) {
            fprintf(file, "\n");
        }
        printf("\t");
        if (options[i].flag) {
            fprintf(file,
                    "-%c%s%s",
                    options[i].flag,
                    options[i].arg_name ? " " : "",
                    options[i].arg_name ? options[i].arg_name : "");
        }
        if (options[i].long_name) {
            fprintf(file,
                    "%s--%s%s%s",
                    options[i].flag ? ", " : "",
                    options[i].long_name,
                    options[i].arg_name ? "=" : "",
                    options[i].arg_name ? options[i].arg_name : "");
        }
        fprintf(file, "%s", options[i].description);
    }
}
