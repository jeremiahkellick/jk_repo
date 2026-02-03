#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// #jk_build single_translation_unit

// #jk_build nasm jk_src/perfaware/part3/windows_cache_test_conflict.asm

// #jk_build dependencies_begin
#include <jk_src/jk_lib/platform/platform.h>
// #jk_build dependencies_end

void read_asm_conflict(int64_t outer_loop_iterations, int64_t inner_loop_iterations, void *data);

static int64_t sizes[] = {
    // Within a single L1 set
    4 * 64,
    8 * 64,

    // Within a single L2 set
    12 * 64,
    16 * 64,

    // Would normally be comfortably within L1
    8 * 1024,

    // Would normally be comfortably within L2
    64 * 1024,

    // L3
    1 * 1024 * 1024,
};

static JkPlatformRepetitionTest tests[JK_ARRAY_COUNT(sizes)];

#define BUFFER_SIZE (1 * 1024 * 1024 * 1024)

int32_t jk_platform_entry_point(int32_t argc, char **argv)
{
    int64_t frequency = jk_platform_cpu_timer_frequency_estimate(100);

    JkBuffer buffer = jk_platform_memory_alloc(JK_ALLOC_COMMIT, BUFFER_SIZE);

    for (;;) {
        for (int64_t i = 0; i < JK_ARRAY_COUNT(tests); i++) {
            JkPlatformRepetitionTest *test = &tests[i];

            if (sizes[i] < 1024 * 1024) {
                printf("\n%.2f KiB\n", (double)sizes[i] / 1024.0);
            } else if (sizes[i] < 1024 * 1024 * 1024) {
                printf("\n%.2f MiB\n", (double)sizes[i] / (1024.0 * 1024.0));
            } else {
                printf("\n%.2f GiB\n", (double)sizes[i] / (1024 * 1024.0 * 1024.0));
            }

            int64_t inner_loop_iterations = sizes[i] / 256;
            int64_t outer_loop_iterations = BUFFER_SIZE / sizes[i];
            int64_t byte_count = outer_loop_iterations * sizes[i];
            jk_platform_repetition_test_run_wave(test, byte_count, frequency, 10);
            while (jk_platform_repetition_test_running(test)) {
                jk_platform_repetition_test_time_begin(test);
                read_asm_conflict(outer_loop_iterations, inner_loop_iterations, buffer.data);
                jk_platform_repetition_test_time_end(test);
                jk_platform_repetition_test_count_bytes(test, byte_count);
            }
            if (test->state == JK_REPETITION_TEST_ERROR) {
                fprintf(stderr, "%s: Error encountered during repetition test\n", argv[0]);
                exit(1);
            }
        }
    }

    return 0;
}
