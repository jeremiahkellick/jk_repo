#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// #jk_build single_translation_unit

// #jk_build nasm jk_src/perfaware/part3/windows_port_tests.asm

// #jk_build dependencies_begin
#include <jk_src/jk_lib/platform/platform.h>
// #jk_build dependencies_end

void read_1x(size_t size, void *data);
void read_2x(size_t size, void *data);
void read_3x(size_t size, void *data);
void read_4x(size_t size, void *data);
void write_1x(size_t size, void *data);
void write_2x(size_t size, void *data);
void write_3x(size_t size, void *data);
void write_4x(size_t size, void *data);

typedef struct TestCandidate {
    char *name;
    void (*function)(size_t, void *);
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

int main(int argc, char **argv)
{
    uint8_t data[8] = {0};
    size_t mov_count = 1024 * 1024 * 1024;

    uint64_t frequency = jk_platform_cpu_timer_frequency_estimate(100);

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
