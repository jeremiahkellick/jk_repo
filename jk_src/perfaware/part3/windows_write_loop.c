#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <jk_gen/single_translation_unit.h>

// #jk_build dependencies_begin
#include <jk_src/jk_lib/jk_lib.h>
#include <jk_src/jk_lib/platform/platform.h>
#include <jk_src/jk_lib/profile/profile.h>
// #jk_build dependencies_end

void buffer_loop_mov(size_t size, void *data);
void buffer_loop_nop(size_t size, void *data);
void buffer_loop_cmp(size_t size, void *data);
void buffer_loop_dec(size_t size, void *data);

typedef struct TestCandidate {
    char *name;
    void (*function)(size_t, void *);
} TestCandidate;

static TestCandidate candidates[] = {
    {"buffer_loop_mov", buffer_loop_mov},
    {"buffer_loop_nop", buffer_loop_nop},
    {"buffer_loop_cmp", buffer_loop_cmp},
    {"buffer_loop_dec", buffer_loop_dec},
};

// tests[malloc][i]
static JkRepetitionTest tests[JK_ARRAY_COUNT(candidates)];

int main(int argc, char **argv)
{
    size_t size = 1024 * 1024 * 1024;
    void *data = malloc(size);
    if (!data) {
        fprintf(stderr, "%s: Failed to allocate memory\n", argv[0]);
        exit(1);
    }

    jk_platform_init();
    uint64_t frequency = jk_cpu_timer_frequency_estimate(100);

    while (true) {
        for (size_t i = 0; i < JK_ARRAY_COUNT(candidates); i++) {
            JkRepetitionTest *test = &tests[i];

            printf("\n%s\n", candidates[i].name);

            jk_repetition_test_run_wave(test, size, frequency, 10);
            while (jk_repetition_test_running(test)) {
                jk_repetition_test_time_begin(test);
                candidates[i].function(size, data);
                jk_repetition_test_time_end(test);
                jk_repetition_test_count_bytes(test, size);
            }
            if (test->state == JK_REPETITION_TEST_ERROR) {
                fprintf(stderr, "%s: Error encountered during repetition test\n", argv[0]);
                exit(1);
            }
        }
    }

    return 0;
}
