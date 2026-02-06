#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// #jk_build single_translation_unit

// #jk_build nasm jk_src/perfaware/part3/windows_cache_test_custom.asm

// #jk_build dependencies_begin
#include <jk_src/jk_lib/platform/platform.h>
// #jk_build dependencies_end

void read_asm_custom(int64_t outer_loop_iterations, int64_t inner_loop_iterations, void *data);
void write_asm_custom(int64_t outer_loop_iterations, int64_t inner_loop_iterations, void *data);

static int64_t sizes[] = {
    // L1 boundary
    24 * 1024,
    28 * 1024,
    32 * 1024,
    36 * 1024,
    40 * 1024,
    44 * 1024,
    48 * 1024,

    // L2 boundary
    160 * 1024,
    176 * 1024,
    192 * 1024,
    208 * 1024,
    224 * 1024,
    240 * 1024,
    256 * 1024,
    272 * 1024,
    288 * 1024,
    304 * 1024,
    320 * 1024,
    336 * 1024,
    352 * 1024,

    // L3 boundary
    2 * 1024 * 1024,
    3 * 1024 * 1024,
    4 * 1024 * 1024,
    5 * 1024 * 1024,
    6 * 1024 * 1024,
    7 * 1024 * 1024,
    8 * 1024 * 1024,
    9 * 1024 * 1024,
    10 * 1024 * 1024,
    11 * 1024 * 1024,
    12 * 1024 * 1024,
    13 * 1024 * 1024,
    14 * 1024 * 1024,
    15 * 1024 * 1024,
    16 * 1024 * 1024,

    32 * 1024 * 1024,
    64 * 1024 * 1024,
    128 * 1024 * 1024,

    1 * 1024 * 1024 * 1024,
};

typedef struct TestFunction {
    char *name;
    void (*call)(int64_t outer_loop_iterations, int64_t inner_loop_iterations, void *data);
} TestFunction;

TestFunction functions[] = {
    {"Read", read_asm_custom},
    {"Write", write_asm_custom},
};

static JkPlatformRepetitionTest tests[JK_ARRAY_COUNT(sizes)][JK_ARRAY_COUNT(functions)];

#define BUFFER_SIZE (sizes[JK_ARRAY_COUNT(sizes) - 1])

int32_t jk_platform_entry_point(int32_t argc, char **argv)
{
    int64_t frequency = jk_platform_cpu_timer_frequency_estimate(100);

    JkBuffer buffer = jk_platform_memory_alloc(JK_ALLOC_COMMIT, BUFFER_SIZE);

    for (int64_t size_index = 0; size_index < JK_ARRAY_COUNT(tests); size_index++) {
        for (int64_t func_index = 0; func_index < JK_ARRAY_COUNT(functions); func_index++) {
            JkPlatformRepetitionTest *test = &tests[size_index][func_index];

            printf("\n");
            jk_platform_print_bytes_int64(stdout, "%.0f", sizes[size_index]);
            printf(", %s\n", functions[func_index].name);

            int64_t inner_loop_iterations = sizes[size_index] / 256;
            int64_t outer_loop_iterations = BUFFER_SIZE / sizes[size_index];
            int64_t byte_count = outer_loop_iterations * sizes[size_index];
            jk_platform_repetition_test_run_wave(test, byte_count, frequency, 5);
            while (jk_platform_repetition_test_running(test)) {
                jk_platform_repetition_test_time_begin(test);
                functions[func_index].call(
                        outer_loop_iterations, inner_loop_iterations, buffer.data);
                jk_platform_repetition_test_time_end(test);
                jk_platform_repetition_test_count_bytes(test, byte_count);
            }
            if (test->state == JK_REPETITION_TEST_ERROR) {
                fprintf(stderr, "%s: Error encountered during repetition test\n", argv[0]);
                exit(1);
            }
        }
    }

    printf("\nSize");
    for (int64_t i = 0; i < JK_ARRAY_COUNT(functions); i++) {
        printf(",%s", functions[i].name);
    }
    printf("\n");
    for (int64_t size_index = 0; size_index < JK_ARRAY_COUNT(tests); size_index++) {
        jk_platform_print_bytes_int64(stdout, "%.0f", sizes[size_index]);
        for (int64_t func_index = 0; func_index < JK_ARRAY_COUNT(functions); func_index++) {
            printf(",%.3f",
                    jk_platform_repetition_test_bandwidth(
                            tests[size_index][func_index].min, frequency)
                            / (1024.0 * 1024.0 * 1024.0));
        }
        printf("\n");
    }

    return 0;
}
