#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// #jk_build single_translation_unit

// #jk_build nasm jk_src/perfaware/part3/windows_mov_size_tests.asm

// #jk_build dependencies_begin
#include <jk_src/jk_lib/platform/platform.h>
// #jk_build dependencies_end

void read_4x2(int64_t size, void *data);
void read_8x2(int64_t size, void *data);
void read_16x2(int64_t size, void *data);
void read_32x1(int64_t size, void *data);
void read_32x2(int64_t size, void *data);
void read_32x3(int64_t size, void *data);
void read_32x4(int64_t size, void *data);

typedef struct TestCandidate {
    char *name;
    void (*function)(int64_t, void *);
} TestCandidate;

static TestCandidate candidates[] = {
    {"read_4x2", read_4x2},
    {"read_8x2", read_8x2},
    {"read_16x2", read_16x2},
    {"read_32x2", read_32x2},

    //  {"read_32x1", read_32x1},
    //  {"read_32x2", read_32x2},
    //  {"read_32x3", read_32x3},
    //  {"read_32x4", read_32x4},
};

static JkPlatformRepetitionTest tests[JK_ARRAY_COUNT(candidates)];

int32_t jk_platform_entry_point(int32_t argc, char **argv)
{
    int64_t frequency = jk_platform_cpu_timer_frequency_estimate(100);

    JkBuffer buffer = jk_platform_memory_alloc(JK_ALLOC_COMMIT, jk_platform_page_size());
    int64_t mov_count = 1024 * 1024 * 1024;

    for (;;) {
        for (int64_t i = 0; i < JK_ARRAY_COUNT(candidates); i++) {
            JkPlatformRepetitionTest *test = &tests[i];

            printf("\n%s\n", candidates[i].name);

            jk_platform_repetition_test_run_wave(test, mov_count, frequency, 10);
            while (jk_platform_repetition_test_running(test)) {
                jk_platform_repetition_test_time_begin(test);
                candidates[i].function(mov_count, buffer.data);
                jk_platform_repetition_test_time_end(test);
                jk_platform_repetition_test_count_bytes(test, mov_count);
            }
            if (test->state == JK_REPETITION_TEST_ERROR) {
                fprintf(stderr, "%s: Error encountered during repetition test\n", argv[0]);
                exit(1);
            }
        }
    }

    return 0;
}
