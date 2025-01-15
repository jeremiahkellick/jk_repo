#include <stdio.h>
#include <stdlib.h>

#include <jk_gen/perfaware/part3/page_fault_stats.stu.h>

// #jk_build dependencies_begin
#include <jk_src/jk_lib/jk_lib.h>
#include <jk_src/jk_lib/platform/platform.h>
// #jk_build dependencies_end

typedef enum Opt {
    OPT_HELP,
    OPT_REVERSE,
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
        .flag = 'r',
        .long_name = "reverse",
        .arg_name = NULL,
        .description = "\n\t\tTouch the pages in reverse order (highest address to lowest)\n",
    },
};

JkOptionResult opt_results[OPT_COUNT] = {0};

JkOptionsParseResult opts_parse = {0};

int main(int argc, char **argv)
{
    int page_count = 0;
    jk_options_parse(argc, argv, opts, opt_results, OPT_COUNT, &opts_parse);
    if (opts_parse.operand_count == 1) {
        page_count = jk_parse_positive_integer(opts_parse.operands[0]);
        if (page_count < 0) {
            fprintf(stderr,
                    "%s: Invalid PAGE_COUNT: Expected a positive integer, got '%s'\n",
                    argv[0],
                    opts_parse.operands[0]);
            opts_parse.usage_error = true;
        }
    } else if (!opt_results[OPT_HELP].present) {
        fprintf(stderr, "%s: Expected 1 operand, got %zu\n", argv[0], opts_parse.operand_count);
        opts_parse.usage_error = true;
    }
    if (opt_results[OPT_HELP].present || opts_parse.usage_error) {
        printf("NAME\n"
               "\twindows_page_fault_stats - print some page fault stats in CSV format\n\n"
               "SYNOPSIS\n"
               "\twindows_page_fault_stats PAGE_COUNT\n\n"
               "DESCRIPTION\n"
               "\twindows_page_fault_stats progressively allocates an increasing number of\n"
               "\tpages up to PAGE_COUNT and writes to them, printing out changes to the\n"
               "\tpage fault counter reported by the Windows.\n\n");
        jk_options_print_help(stdout, opts, OPT_COUNT);
        exit(opts_parse.usage_error);
    }

    jk_platform_init();

    uint64_t page_size = jk_platform_page_size();
    JkBuffer buffer = {.size = (uint64_t)page_count * page_size};
    for (int touch_page_count = 0; touch_page_count < page_count; touch_page_count++) {
        buffer.data = jk_platform_memory_alloc(buffer.size);
        if (!buffer.data) {
            continue;
        }

        uint64_t count_before = jk_platform_page_fault_count_get();
        uint64_t touch_size = touch_page_count * page_size;
        for (size_t i = 0; i < touch_size; i++) {
            size_t index = opt_results[OPT_REVERSE].present ? touch_size - 1 - i : i;
            buffer.data[index] = (uint8_t)index;
        }
        uint64_t count_after = jk_platform_page_fault_count_get();

        uint64_t fault_count = count_after - count_before;
        printf("%d, %d, %llu, %lld\n",
                page_count,
                touch_page_count,
                (long long)fault_count,
                (long long)(fault_count - (uint64_t)touch_page_count));

        jk_platform_memory_free(buffer.data, buffer.size);
    }
}
