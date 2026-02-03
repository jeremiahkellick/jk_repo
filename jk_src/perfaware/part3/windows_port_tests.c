#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// #jk_build single_translation_unit

// #jk_build nasm jk_src/perfaware/part3/windows_port_tests.asm

// #jk_build dependencies_begin
#include <jk_src/jk_lib/platform/platform.h>
// #jk_build dependencies_end

void read_1x(int64_t size, void *data);
void read_2x(int64_t size, void *data);
void read_3x(int64_t size, void *data);
void read_4x(int64_t size, void *data);
void write_1x(int64_t size, void *data);
void write_2x(int64_t size, void *data);
void write_3x(int64_t size, void *data);
void write_4x(int64_t size, void *data);

typedef struct TestCandidate {
    char *name;
    void (*function)(int64_t, void *);
} TestCandidate;

static TestCandidate candidates[] = {
    {"read_1x", read_1x},
    {"read_2x", read_2x},
    {"read_3x", read_3x},
    {"read_4x", read_4x},
    {"write_1x", write_1x},
    {"write_2x", write_2x},
    {"write_3x", write_3x},
    {"write_4x", write_4x},
};

static JkPlatformRepetitionTest tests[JK_ARRAY_COUNT(candidates)];

int32_t jk_platform_entry_point(int32_t argc, char **argv)
{
    uint8_t data[8] = {0};
    int64_t mov_count = 1024 * 1024 * 1024;

    int64_t frequency = jk_platform_cpu_timer_frequency_estimate(100);

    for (;;) {
        for (int64_t i = 0; i < JK_ARRAY_COUNT(candidates); i++) {
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

    return 0;
}
