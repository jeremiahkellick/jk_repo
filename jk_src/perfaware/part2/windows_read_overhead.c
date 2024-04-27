#include <fcntl.h>
#include <io.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

// #jk_build dependencies_begin
#include <jk_src/jk_lib/jk_lib.h>
#include <jk_src/jk_lib/platform/platform.h>
#include <jk_src/jk_lib/profile/profile.h>
// #jk_build dependencies_end

typedef struct ReadParams {
    JkBuffer dest;
    char *file_name;
    bool malloc;
} ReadParams;

typedef struct JkRepetitionTest JkRepetitionTest;

typedef void ReadFunction(JkRepetitionTest *test, ReadParams params);

static void handle_allocation(ReadParams *params)
{
    if (params->malloc) {
        params->dest.data = malloc(params->dest.size);
    }
}

static void handle_deallocation(ReadParams *params)
{
    if (params->malloc) {
        free(params->dest.data);
    }
}

static void read_via_fread(JkRepetitionTest *test, ReadParams params)
{
    while (jk_repetition_test_running(test)) {
        handle_allocation(&params);
        FILE *file = fopen(params.file_name, "rb");
        if (!file) {
            jk_repetition_test_error(test);
            handle_deallocation(&params);
            continue;
        }

        jk_repetition_test_time_begin(test);
        size_t result = fread(params.dest.data, params.dest.size, 1, file);
        jk_repetition_test_time_end(test);

        if (result == 1) {
            jk_repetition_test_count_bytes(test, params.dest.size);
        } else {
            jk_repetition_test_error(test);
        }
        fclose(file);
        handle_deallocation(&params);
    }
}

static void read_via_read(JkRepetitionTest *test, ReadParams params)
{
    while (jk_repetition_test_running(test)) {
        handle_allocation(&params);
        int file = _open(params.file_name, _O_BINARY | _O_RDONLY);
        if (file == -1) {
            jk_repetition_test_error(test);
            handle_deallocation(&params);
            continue;
        }

        uint8_t *dest = params.dest.data;
        uint64_t size_remaining = params.dest.size;
        while (size_remaining) {
            int read_size = INT_MAX;
            if ((uint64_t)read_size > size_remaining) {
                read_size = (int)size_remaining;
            }

            jk_repetition_test_time_begin(test);
            int result = _read(file, dest, read_size);
            jk_repetition_test_time_end(test);

            if (result != (int)read_size) {
                jk_repetition_test_error(test);
                break;
            }

            jk_repetition_test_count_bytes(test, read_size);
            size_remaining -= read_size;
            dest += read_size;
        }

        _close(file);
        handle_deallocation(&params);
    }
}

static void read_via_read_file(JkRepetitionTest *test, ReadParams params)
{
    while (jk_repetition_test_running(test)) {
        handle_allocation(&params);
        HANDLE file = CreateFileA(params.file_name,
                GENERIC_READ,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                0,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                0);
        if (file == INVALID_HANDLE_VALUE) {
            jk_repetition_test_error(test);
            handle_deallocation(&params);
            continue;
        }

        uint8_t *dest = params.dest.data;
        uint64_t size_remaining = params.dest.size;
        while (size_remaining) {
            DWORD read_size = UINT_MAX;
            if ((uint64_t)read_size > size_remaining) {
                read_size = (DWORD)size_remaining;
            }

            DWORD bytes_read = 0;
            jk_repetition_test_time_begin(test);
            BOOL result = ReadFile(file, dest, read_size, &bytes_read, 0);
            jk_repetition_test_time_end(test);

            if (!result || bytes_read != read_size) {
                jk_repetition_test_error(test);
                break;
            }

            jk_repetition_test_count_bytes(test, read_size);
            size_remaining -= read_size;
            dest += read_size;
        }

        CloseHandle(file);
        handle_deallocation(&params);
    }
}

typedef struct TestCandidate {
    char *name;
    ReadFunction *function;
    JkRepetitionTest test;
} TestCandidate;

TestCandidate candidates[] = {
    {"fread", read_via_fread},
    {"_read", read_via_read},
    {"ReadFile", read_via_read_file},
};

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "%s: Expected 1 file argument, got %d\n", argv[0], argc - 1);
        exit(1);
    }

    ReadParams params = {.file_name = argv[1], .dest = {.size = jk_platform_file_size(argv[1])}};
    params.dest.data = malloc(params.dest.size);
    if (!params.dest.data) {
        fprintf(stderr, "%s: Failed to allocate memory\n", argv[0]);
        exit(1);
    }

    uint64_t frequency = jk_cpu_timer_frequency_estimate(100);

    while (true) {
        for (size_t i = 0; i < JK_ARRAY_COUNT(candidates); i++) {
            printf("\n%s%s\n", candidates[i].name, params.malloc ? " w/ malloc" : "");
            jk_repetition_test_init(&candidates[i].test, params.dest.size, frequency, 10);
            candidates[i].function(&candidates[i].test, params);
        }
        params.malloc = !params.malloc;
    }

    return 0;
}
