#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <jk_gen/single_translation_unit.h>

// #jk_build nasm jk_src/perfaware/part3/windows_cache_test_custom.asm

// #jk_build dependencies_begin
#include <jk_src/jk_lib/platform/platform.h>
// #jk_build dependencies_end

void read_asm_custom(uint64_t outer_loop_iterations, uint64_t inner_loop_iterations, void *data);

static uint64_t sizes[] = {
    // L1
    16 * 1024,

    // L2
    128 * 1024,

    // L3
    1 * 1024 * 1024,

    // Main memory
    1 * 1024 * 1024 * 1024,
};

static uint64_t offsets[] = {0, 1, 8, 16, 32, 48, 63};

static JkPlatformRepetitionTest tests[JK_ARRAY_COUNT(sizes)][JK_ARRAY_COUNT(offsets)];

#define BUFFER_SIZE (sizes[JK_ARRAY_COUNT(sizes) - 1])

int main(int argc, char **argv)
{
    jk_platform_init();
    uint64_t frequency = jk_platform_cpu_timer_frequency_estimate(100);

    void *data = jk_platform_memory_alloc(BUFFER_SIZE + 4096);

    for (;;) {
        for (int i = 0; i < JK_ARRAY_COUNT(tests); i++) {
            for (int j = 0; j < JK_ARRAY_COUNT(offsets); j++) {
                JkPlatformRepetitionTest *test = &tests[i][j];

                if (sizes[i] < 1024 * 1024) {
                    printf("\n%.2f KiB", (double)sizes[i] / 1024.0);
                } else if (sizes[i] < 1024 * 1024 * 1024) {
                    printf("\n%.2f MiB", (double)sizes[i] / (1024.0 * 1024.0));
                } else {
                    printf("\n%.2f GiB", (double)sizes[i] / (1024 * 1024.0 * 1024.0));
                }
                printf(", offset %llu\n", (long long)offsets[j]);

                uint64_t inner_loop_iterations = sizes[i] / 256;
                uint64_t outer_loop_iterations = BUFFER_SIZE / sizes[i];
                uint64_t byte_count = outer_loop_iterations * sizes[i];
                jk_platform_repetition_test_run_wave(test, byte_count, frequency, 10);
                while (jk_platform_repetition_test_running(test)) {
                    jk_platform_repetition_test_time_begin(test);
                    read_asm_custom(outer_loop_iterations,
                            inner_loop_iterations,
                            (char *)data + offsets[j]);
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
}
