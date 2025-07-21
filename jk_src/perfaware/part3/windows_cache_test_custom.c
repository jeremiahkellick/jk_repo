#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// #jk_build single_translation_unit

// #jk_build nasm jk_src/perfaware/part3/windows_cache_test_custom.asm

// #jk_build dependencies_begin
#include <jk_src/jk_lib/platform/platform.h>
// #jk_build dependencies_end

void read_asm_custom(uint64_t outer_loop_iterations, uint64_t inner_loop_iterations, void *data);

static uint64_t sizes[] = {
    // L1 boundary
    24 * 1024,
    28 * 1024,
    32 * 1024,
    36 * 1024,
    40 * 1024,
    44 * 1024,
    48 * 1024,

    // L2 boundary
    248 * 1024,
    252 * 1024,
    256 * 1024,
    260 * 1024,
    264 * 1024,
    268 * 1024,
    272 * 1024,

    // L3 boundary
    2 * 1024 * 1024,
    3 * 1024 * 1024,
    4 * 1024 * 1024,
    5 * 1024 * 1024,
    6 * 1024 * 1024,
    7 * 1024 * 1024,
    8 * 1024 * 1024,
    9 * 1024 * 1024,
    10 * 1024 * 1024,
    11 * 1024 * 1024,
    12 * 1024 * 1024,
    13 * 1024 * 1024,
    14 * 1024 * 1024,
    15 * 1024 * 1024,
    16 * 1024 * 1024,

    32 * 1024 * 1024,
    64 * 1024 * 1024,
    128 * 1024 * 1024,

    1 * 1024 * 1024 * 1024,
};

static JkPlatformRepetitionTest tests[JK_ARRAY_COUNT(sizes)];

#define BUFFER_SIZE (sizes[JK_ARRAY_COUNT(sizes) - 1])

int main(int argc, char **argv)
{
    uint64_t frequency = jk_platform_cpu_timer_frequency_estimate(100);

    void *data = jk_platform_memory_alloc(BUFFER_SIZE);

    for (;;) {
        for (size_t i = 0; i < JK_ARRAY_COUNT(tests); i++) {
            JkPlatformRepetitionTest *test = &tests[i];

            if (sizes[i] < 1024 * 1024) {
                printf("\n%.2f KiB\n", (double)sizes[i] / 1024.0);
            } else if (sizes[i] < 1024 * 1024 * 1024) {
                printf("\n%.2f MiB\n", (double)sizes[i] / (1024.0 * 1024.0));
            } else {
                printf("\n%.2f GiB\n", (double)sizes[i] / (1024 * 1024.0 * 1024.0));
            }

            uint64_t inner_loop_iterations = sizes[i] / 256;
            uint64_t outer_loop_iterations = BUFFER_SIZE / sizes[i];
            uint64_t byte_count = outer_loop_iterations * sizes[i];
            jk_platform_repetition_test_run_wave(test, byte_count, frequency, 10);
            while (jk_platform_repetition_test_running(test)) {
                jk_platform_repetition_test_time_begin(test);
                read_asm_custom(outer_loop_iterations, inner_loop_iterations, data);
                jk_platform_repetition_test_time_end(test);
                jk_platform_repetition_test_count_bytes(test, byte_count);
            }
            if (test->state == JK_REPETITION_TEST_ERROR) {
                fprintf(stderr, "%s: Error encountered during repetition test\n", argv[0]);
                exit(1);
            }
        }
    }
}
