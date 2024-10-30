#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <jk_gen/single_translation_unit.h>

// #jk_build nasm jk_src/perfaware/part3/conditional_nop.asm

// #jk_build dependencies_begin
#include <jk_src/jk_lib/platform/platform.h>
// #jk_build dependencies_end

void conditional_nop(size_t size, void *data);

typedef enum Pattern {
    PATTERN_NONE,
    PATTERN_ALL,
    PATTERN_EVERY_SECOND,
    PATTERN_EVERY_THIRD,
    PATTERN_EVERY_FOURTH,
    PATTERN_RANDOM,

    PATTERN_COUNT,
} Pattern;

char *pattern_names[PATTERN_COUNT] = {
    "None",
    "All",
    "Every second",
    "Every third",
    "Every fourth",
    "Random",
};

static void fill_with_pattern(JkBuffer buffer, Pattern pattern)
{
    for (size_t i = 0; i < buffer.size; i++) {
        uint8_t value = 0;

        switch (pattern) {
        case PATTERN_NONE:
        case PATTERN_COUNT: {
        }; break;

        case PATTERN_ALL: {
            value = 1;
        }; break;

        case PATTERN_EVERY_SECOND: {
            value = (i % 2) == 0;
        }; break;

        case PATTERN_EVERY_THIRD: {
            value = (i % 3) == 0;
        }; break;

        case PATTERN_EVERY_FOURTH: {
            value = (i % 4) == 0;
        }; break;

        case PATTERN_RANDOM: {
            value = (uint8_t)rand();
        }; break;
        }

        buffer.data[i] = value;
    }
}

static JkPlatformRepetitionTest tests[PATTERN_COUNT];

int main(int argc, char **argv)
{
    srand((unsigned)time(NULL));

    JkBuffer buffer = {
        .size = 1024 * 1024 * 1024,
        .data = malloc(buffer.size),
    };
    if (!buffer.data) {
        fprintf(stderr, "%s: Failed to allocate memory\n", argv[0]);
        exit(1);
    }

    jk_platform_init();
    uint64_t frequency = jk_platform_cpu_timer_frequency_estimate(100);

    for (;;) {
        for (Pattern pattern = 0; pattern < PATTERN_COUNT; pattern++) {
            fill_with_pattern(buffer, pattern);

            JkPlatformRepetitionTest *test = &tests[pattern];

            printf("\n%s\n", pattern_names[pattern]);

            jk_platform_repetition_test_run_wave(test, buffer.size, frequency, 10);
            while (jk_platform_repetition_test_running(test)) {
                jk_platform_repetition_test_time_begin(test);
                conditional_nop(buffer.size, buffer.data);
                jk_platform_repetition_test_time_end(test);
                jk_platform_repetition_test_count_bytes(test, buffer.size);
            }
            if (test->state == JK_REPETITION_TEST_ERROR) {
                fprintf(stderr, "%s: Error encountered during repetition test\n", argv[0]);
                exit(1);
            }
        }
    }
}
