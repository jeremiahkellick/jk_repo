#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// #jk_build single_translation_unit

// #jk_build nasm jk_src/perfaware/part3/windows_write_loop.asm

// #jk_build dependencies_begin
#include <jk_src/jk_lib/platform/platform.h>
// #jk_build dependencies_end

void buffer_loop_mov(int64_t size, void *data);
void buffer_loop_cmp(int64_t size, void *data);
void buffer_loop_dec(int64_t size, void *data);
void buffer_loop_nop(int64_t size, void *data);
void buffer_loop_nop_3(int64_t size, void *data);
void buffer_loop_nop_9(int64_t size, void *data);
void loop_predictable(int64_t size, void *data);
void loop_unpredictable(int64_t size, void *data);

typedef struct TestCandidate {
    char *name;
    void (*function)(int64_t, void *);
} TestCandidate;

static TestCandidate candidates[] = {
    {"loop_predictable", loop_predictable},
    {"loop_unpredictable", loop_unpredictable},
};

// tests[malloc][i]
static JkPlatformRepetitionTest tests[JK_ARRAY_COUNT(candidates)];

int32_t jk_platform_entry_point(int32_t argc, char **argv)
{
    int64_t size = 1024 * 1024 * 1024;
    void *data = malloc(size);
    if (!data) {
        fprintf(stderr, "%s: Failed to allocate memory\n", argv[0]);
        exit(1);
    }

    int64_t frequency = jk_platform_cpu_timer_frequency_estimate(100);

    for (;;) {
        for (int64_t i = 0; i < JK_ARRAY_COUNT(candidates); i++) {
            JkPlatformRepetitionTest *test = &tests[i];

            printf("\n%s\n", candidates[i].name);

            jk_platform_repetition_test_run_wave(test, size, frequency, 10);
            while (jk_platform_repetition_test_running(test)) {
                jk_platform_repetition_test_time_begin(test);
                candidates[i].function(size, data);
                jk_platform_repetition_test_time_end(test);
                jk_platform_repetition_test_count_bytes(test, size);
            }
            if (test->state == JK_REPETITION_TEST_ERROR) {
                fprintf(stderr, "%s: Error encountered during repetition test\n", argv[0]);
                exit(1);
            }
        }
    }

    return 0;
}
