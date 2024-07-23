#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <jk_gen/single_translation_unit.h>

// #jk_build nasm jk_src/perfaware/part3/windows_cache_tests.asm

// #jk_build dependencies_begin
#include <jk_src/jk_lib/jk_lib.h>
#include <jk_src/jk_lib/platform/platform.h>
#include <jk_src/jk_lib/profile/profile.h>
// #jk_build dependencies_end

void read_asm(size_t size, void *data);

#define TEST_COUNT 4

static char *names[] = {"L1", "L2", "L3", "Main memory"};

// Sizes chosen based on Skylake cache sizes
static size_t sizes[JK_ARRAY_COUNT(names)] = {
    4096, // L1
    128 * 1024, // L2
    512 * 1024, // L3
    64 * 1024 * 1024, // Main memory
};

static JkRepetitionTest tests[JK_ARRAY_COUNT(names)];

int main(int argc, char **argv)
{
    jk_platform_init();
    uint64_t frequency = jk_cpu_timer_frequency_estimate(100);

    void *data = jk_platform_memory_alloc(sizes[JK_ARRAY_COUNT(names) - 1]);

    while (true) {
        for (size_t i = 0; i < JK_ARRAY_COUNT(names); i++) {
            JkRepetitionTest *test = &tests[i];

            printf("\n%s\n", names[i]);

            jk_repetition_test_run_wave(test, sizes[i], frequency, 10);
            while (jk_repetition_test_running(test)) {
                jk_repetition_test_time_begin(test);
                read_asm(sizes[i], data);
                jk_repetition_test_time_end(test);
                jk_repetition_test_count_bytes(test, sizes[i]);
            }
            if (test->state == JK_REPETITION_TEST_ERROR) {
                fprintf(stderr, "%s: Error encountered during repetition test\n", argv[0]);
                exit(1);
            }
        }
    }

    return 0;
}
