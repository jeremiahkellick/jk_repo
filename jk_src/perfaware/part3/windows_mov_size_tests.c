#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// #jk_build single_translation_unit

// #jk_build nasm jk_src/perfaware/part3/windows_mov_size_tests.asm

// #jk_build dependencies_begin
#include <jk_src/jk_lib/platform/platform.h>
// #jk_build dependencies_end

void read_4x2(size_t size, void *data);
void read_8x2(size_t size, void *data);
void read_16x2(size_t size, void *data);
void read_32x1(size_t size, void *data);
void read_32x2(size_t size, void *data);
void read_32x3(size_t size, void *data);
void read_32x4(size_t size, void *data);

typedef struct TestCandidate {
    char *name;
    void (*function)(size_t, void *);
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

int main(int argc, char **argv)
{
    uint64_t frequency = jk_platform_cpu_timer_frequency_estimate(100);

    void *data = jk_platform_memory_alloc(jk_platform_page_size());
    size_t mov_count = 1024 * 1024 * 1024;

    for (;;) {
        for (size_t i = 0; i < JK_ARRAY_COUNT(candidates); i++) {
            JkPlatformRepetitionTest *test = &tests[i];

            printf("\n%s\n", candidates[i].name);

            jk_platform_repetition_test_run_wave(test, mov_count, frequency, 10);
            while (jk_platform_repetition_test_running(test)) {
                jk_platform_repetition_test_time_begin(test);
                candidates[i].function(mov_count, data);
                jk_platform_repetition_test_time_end(test);
                jk_platform_repetition_test_count_bytes(test, mov_count);
            }
            if (test->state == JK_REPETITION_TEST_ERROR) {
                fprintf(stderr, "%s: Error encountered during repetition test\n", argv[0]);
                exit(1);
            }
        }
    }
}
