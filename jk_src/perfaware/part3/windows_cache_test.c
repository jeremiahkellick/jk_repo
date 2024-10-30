#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <jk_gen/single_translation_unit.h>

// #jk_build nasm jk_src/perfaware/part3/windows_cache_test.asm

// #jk_build dependencies_begin
#include <jk_src/jk_lib/platform/platform.h>
// #jk_build dependencies_end

void read_asm(size_t byte_count, size_t size, void *data);

static JkPlatformRepetitionTest tests[17];

#define STARTING_SIZE (16 * 1024)
#define MAX_SIZE (STARTING_SIZE << (JK_ARRAY_COUNT(tests) - 1))

int main(int argc, char **argv)
{
    jk_platform_init();
    uint64_t frequency = jk_platform_cpu_timer_frequency_estimate(100);

    void *data = jk_platform_memory_alloc(MAX_SIZE);

    for (;;) {
        size_t size = STARTING_SIZE;
        for (size_t i = 0; i < JK_ARRAY_COUNT(tests); i++, size *= 2) {
            JkPlatformRepetitionTest *test = &tests[i];

            if (size < 1024 * 1024) {
                printf("\n%.2f KiB\n", (double)size / 1024.0);
            } else if (size < 1024 * 1024 * 1024) {
                printf("\n%.2f MiB\n", (double)size / (1024.0 * 1024.0));
            } else {
                printf("\n%.2f GiB\n", (double)size / (1024 * 1024.0 * 1024.0));
            }

            jk_platform_repetition_test_run_wave(test, MAX_SIZE, frequency, 10);
            while (jk_platform_repetition_test_running(test)) {
                jk_platform_repetition_test_time_begin(test);
                read_asm(MAX_SIZE, size, data);
                jk_platform_repetition_test_time_end(test);
                jk_platform_repetition_test_count_bytes(test, MAX_SIZE);
            }
            if (test->state == JK_REPETITION_TEST_ERROR) {
                fprintf(stderr, "%s: Error encountered during repetition test\n", argv[0]);
                exit(1);
            }
        }
    }
}
