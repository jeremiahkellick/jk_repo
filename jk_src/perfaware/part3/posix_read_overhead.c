#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// #jk_build single_translation_unit

// #jk_build dependencies_begin
#include <jk_src/jk_lib/platform/platform.h>
// #jk_build dependencies_end

typedef struct ReadParams {
    JkBuffer dest;
    char *file_name;
    b32 alloc;
} ReadParams;

typedef void ReadFunction(JkPlatformRepetitionTest *test, ReadParams params);

static void handle_allocation(ReadParams *params)
{
    if (params->alloc) {
        params->dest.data = mmap(
                NULL, params->dest.size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    }
}

static void handle_deallocation(ReadParams *params)
{
    if (params->alloc) {
        munmap(params->dest.data, params->dest.size);
    }
}

static void write_to_all_bytes(JkPlatformRepetitionTest *test, ReadParams params)
{
    while (jk_platform_repetition_test_running(test)) {
        handle_allocation(&params);

        jk_platform_repetition_test_time_begin(test);
        for (size_t i = 0; i < params.dest.size; i++) {
            params.dest.data[i] = (uint8_t)i;
        }
        jk_platform_repetition_test_time_end(test);

        jk_platform_repetition_test_count_bytes(test, params.dest.size);

        handle_deallocation(&params);
    }
}

static void write_to_all_bytes_backwards(JkPlatformRepetitionTest *test, ReadParams params)
{
    while (jk_platform_repetition_test_running(test)) {
        handle_allocation(&params);

        jk_platform_repetition_test_time_begin(test);
        for (size_t i = 0; i < params.dest.size; i++) {
            params.dest.data[params.dest.size - 1 - i] = (uint8_t)i;
        }
        jk_platform_repetition_test_time_end(test);

        jk_platform_repetition_test_count_bytes(test, params.dest.size);

        handle_deallocation(&params);
    }
}

static void read_via_fread(JkPlatformRepetitionTest *test, ReadParams params)
{
    while (jk_platform_repetition_test_running(test)) {
        handle_allocation(&params);
        FILE *file = fopen(params.file_name, "rb");
        if (!file) {
            jk_platform_repetition_test_error(test, "read_via_fread: Failed to open file\n");
            handle_deallocation(&params);
            continue;
        }

        jk_platform_repetition_test_time_begin(test);
        size_t result = fread(params.dest.data, params.dest.size, 1, file);
        jk_platform_repetition_test_time_end(test);

        if (result == 1) {
            jk_platform_repetition_test_count_bytes(test, params.dest.size);
        } else {
            jk_platform_repetition_test_error(test, "read_via_fread: fread failed\n");
        }
        fclose(file);
        handle_deallocation(&params);
    }
}

static void read_via_read(JkPlatformRepetitionTest *test, ReadParams params)
{
    while (jk_platform_repetition_test_running(test)) {
        handle_allocation(&params);
        int file = open(params.file_name, O_RDONLY);
        if (file == -1) {
            jk_platform_repetition_test_error(test, "read_via_read: Failed to open file: %s\n");
            handle_deallocation(&params);
            continue;
        }

        jk_platform_repetition_test_time_begin(test);
        int result = read(file, params.dest.data, params.dest.size);
        jk_platform_repetition_test_time_end(test);

        if (result == -1) {
            jk_platform_repetition_test_error(test, "read_via_read: read failed\n");
        } else {
            jk_platform_repetition_test_count_bytes(test, params.dest.size);
        }

        close(file);
        handle_deallocation(&params);
    }
}

typedef struct TestCandidate {
    char *name;
    ReadFunction *function;
} TestCandidate;

static TestCandidate candidates[] = {
    {"Write to all bytes", write_to_all_bytes},
    {"Write to all bytes backwards", write_to_all_bytes_backwards},
    {"fread", read_via_fread},
    {"read", read_via_read},
};

// tests[alloc][i]
static JkPlatformRepetitionTest tests[2][JK_ARRAY_COUNT(candidates)];

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "%s: Expected 1 file argument, got %d\n", argv[0], argc - 1);
        exit(1);
    }

    ReadParams params = {.file_name = argv[1], .dest = {.size = jk_platform_file_size(argv[1])}};
    params.dest.data =
            mmap(NULL, params.dest.size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (!params.dest.data) {
        fprintf(stderr, "%s: Failed to allocate memory\n", argv[0]);
        exit(1);
    }

    uint64_t frequency = jk_platform_cpu_timer_frequency_estimate(100);

    for (;;) {
        for (size_t i = 0; i < JK_ARRAY_COUNT(candidates); i++) {
            for (int alloc = 0; alloc < 2; alloc++) {
                params.alloc = alloc;
                JkPlatformRepetitionTest *test = &tests[params.alloc][i];
                printf("\n%s%s\n", candidates[i].name, params.alloc ? " w/ alloc" : "");
                jk_platform_repetition_test_run_wave(test, params.dest.size, frequency, 10);
                candidates[i].function(test, params);
                if (test->state == JK_REPETITION_TEST_ERROR) {
                    fprintf(stderr, "%s: Error encountered during repetition test\n", argv[0]);
                    exit(1);
                }
            }
        }
    }
}
