#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <jk_gen/single_translation_unit.h>

// #jk_build nasm jk_src/perfaware/part3/windows_non_temporal.asm

// #jk_build dependencies_begin
#include <jk_src/jk_lib/jk_lib.h>
#include <jk_src/jk_lib/platform/platform.h>
#include <jk_src/jk_lib/profile/profile.h>
// #jk_build dependencies_end

void test_control(uint64_t input_size, uint64_t output_size, void *data);
void test_non_temporal(uint64_t input_size, uint64_t output_size, void *data);

typedef struct Function {
    void (*ptr)(uint64_t, uint64_t, void *);
    char *name;
} Function;

static Function functions[] = {
    {.ptr = test_control, .name = "Control"},
    {.ptr = test_non_temporal, .name = "Non-temporal"},
};

static JkRepetitionTest tests[JK_ARRAY_COUNT(functions)];

int main(int argc, char **argv)
{
    jk_platform_init();
    uint64_t frequency = jk_cpu_timer_frequency_estimate(100);

    uint64_t input_size = 16llu * 1024;
    uint64_t output_size = 1024llu * 1024 * 1024;

    void *data = jk_platform_memory_alloc(input_size + output_size);

    for (uint32_t i = 0; i < input_size / sizeof(uint32_t); i++) {
        ((uint32_t *)data)[i] = i;
    }

    while (true) {
        for (size_t i = 0; i < JK_ARRAY_COUNT(functions); i++) {
            Function *function = &functions[i];
            JkRepetitionTest *test = &tests[i];

            printf("\n%s\n", function->name);

            jk_repetition_test_run_wave(test, output_size, frequency, 10);
            while (jk_repetition_test_running(test)) {
                jk_repetition_test_time_begin(test);
                function->ptr(input_size, output_size, data);
                jk_repetition_test_time_end(test);
                jk_repetition_test_count_bytes(test, output_size);
            }
            if (test->state == JK_REPETITION_TEST_ERROR) {
                fprintf(stderr, "%s: Error encountered during repetition test\n", argv[0]);
                exit(1);
            }
        }
    }

    return 0;
}
