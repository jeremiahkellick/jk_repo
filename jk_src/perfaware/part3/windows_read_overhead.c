#include <fcntl.h>
#include <io.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

// #jk_build single_translation_unit
// #jk_build link Advapi32

// #jk_build dependencies_begin
#include <jk_src/jk_lib/platform/platform.h>
// #jk_build dependencies_end

typedef enum Alloc {
    NONE,
    MALLOC,
    VIRTUAL_ALLOC,
    VIRTUAL_ALLOC_LARGE_PAGES,
    ALLOC_COUNT,
} Alloc;

typedef struct ReadParams {
    JkBuffer dest;
    char *file_name;
    Alloc alloc;
} ReadParams;

typedef void ReadFunction(JkPlatformRepetitionTest *test, ReadParams params);

static int64_t large_page_size;

static void *global_buffer;

static b32 handle_allocation(JkPlatformRepetitionTest *test, ReadParams *params)
{
    switch (params->alloc) {
    case NONE:
    case ALLOC_COUNT: {
        params->dest.data = global_buffer;
    } break;

    case MALLOC: {
        params->dest.data = malloc(params->dest.size);
        if (!params->dest.data) {
            jk_platform_repetition_test_error(test, "malloc failed\n");
            return 0;
        }
    } break;

    case VIRTUAL_ALLOC:
    case VIRTUAL_ALLOC_LARGE_PAGES: {
        int64_t size = params->dest.size;
        DWORD flAllocationType = MEM_COMMIT | MEM_RESERVE;

        if (params->alloc == VIRTUAL_ALLOC_LARGE_PAGES) {
            if (large_page_size > 0) {
                flAllocationType |= MEM_LARGE_PAGES;
                size = (size + large_page_size - 1) & ~(large_page_size - 1);
            } else {
                jk_platform_repetition_test_error(test, "No large page support\n");
                return 0;
            }
        }

        params->dest.data = VirtualAlloc(NULL, size, flAllocationType, PAGE_READWRITE);
        if (!params->dest.data) {
            jk_platform_repetition_test_error(test, "VirtualAlloc failed\n");
            return 0;
        }
    } break;
    }

    return 1;
}

static void handle_deallocation(JkPlatformRepetitionTest *test, ReadParams *params)
{
    switch (params->alloc) {
    case NONE:
    case ALLOC_COUNT: {
    } break;

    case MALLOC: {
        free(params->dest.data);
    } break;

    case VIRTUAL_ALLOC:
    case VIRTUAL_ALLOC_LARGE_PAGES: {
        VirtualFree(params->dest.data, 0, MEM_RELEASE);
    } break;
    }
}

static void write_to_all_bytes(JkPlatformRepetitionTest *test, ReadParams params)
{
    while (jk_platform_repetition_test_running(test)) {
        if (!handle_allocation(test, &params)) {
            continue;
        }

        jk_platform_repetition_test_time_begin(test);
        for (int64_t i = 0; i < params.dest.size; i++) {
            params.dest.data[i] = (uint8_t)i;
        }
        jk_platform_repetition_test_time_end(test);

        jk_platform_repetition_test_count_bytes(test, params.dest.size);

        handle_deallocation(test, &params);
    }
}

static void write_to_all_bytes_backwards(JkPlatformRepetitionTest *test, ReadParams params)
{
    while (jk_platform_repetition_test_running(test)) {
        if (!handle_allocation(test, &params)) {
            continue;
        }

        jk_platform_repetition_test_time_begin(test);
        for (int64_t i = 0; i < params.dest.size; i++) {
            params.dest.data[params.dest.size - 1 - i] = (uint8_t)i;
        }
        jk_platform_repetition_test_time_end(test);

        jk_platform_repetition_test_count_bytes(test, params.dest.size);

        handle_deallocation(test, &params);
    }
}

static void read_via_fread(JkPlatformRepetitionTest *test, ReadParams params)
{
    while (jk_platform_repetition_test_running(test)) {
        if (!handle_allocation(test, &params)) {
            continue;
        }
        FILE *file = fopen(params.file_name, "rb");
        if (!file) {
            jk_platform_repetition_test_error(test, "read_via_fread: Failed to open file\n");
            handle_deallocation(test, &params);
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
        handle_deallocation(test, &params);
    }
}

static void read_via_read(JkPlatformRepetitionTest *test, ReadParams params)
{
    while (jk_platform_repetition_test_running(test)) {
        if (!handle_allocation(test, &params)) {
            continue;
        }
        int file = _open(params.file_name, _O_BINARY | _O_RDONLY);
        if (file == -1) {
            jk_platform_repetition_test_error(test, "read_via_read: Failed to open file\n");
            handle_deallocation(test, &params);
            continue;
        }

        uint8_t *dest = params.dest.data;
        int64_t size_remaining = params.dest.size;
        while (size_remaining) {
            int read_size = INT_MAX;
            if (read_size > size_remaining) {
                read_size = (int)size_remaining;
            }

            jk_platform_repetition_test_time_begin(test);
            int result = _read(file, dest, read_size);
            jk_platform_repetition_test_time_end(test);

            if (result != (int)read_size) {
                jk_platform_repetition_test_error(test, "read_via_read: _read failed\n");
                break;
            }

            jk_platform_repetition_test_count_bytes(test, read_size);
            size_remaining -= read_size;
            dest += read_size;
        }

        _close(file);
        handle_deallocation(test, &params);
    }
}

static void read_via_read_file(JkPlatformRepetitionTest *test, ReadParams params)
{
    while (jk_platform_repetition_test_running(test)) {
        if (!handle_allocation(test, &params)) {
            continue;
        }
        HANDLE file = CreateFileA(params.file_name,
                GENERIC_READ,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                0,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                0);
        if (file == INVALID_HANDLE_VALUE) {
            jk_platform_repetition_test_error(test, "read_via_read_file: Failed to open file\n");
            handle_deallocation(test, &params);
            continue;
        }

        uint8_t *dest = params.dest.data;
        int64_t size_remaining = params.dest.size;
        while (size_remaining) {
            DWORD read_size = UINT_MAX;
            if ((int64_t)read_size > size_remaining) {
                read_size = (DWORD)size_remaining;
            }

            DWORD bytes_read = 0;
            jk_platform_repetition_test_time_begin(test);
            BOOL result = ReadFile(file, dest, read_size, &bytes_read, 0);
            jk_platform_repetition_test_time_end(test);

            if (!result || bytes_read != read_size) {
                jk_platform_repetition_test_error(test, "read_via_read_file: ReadFile failed\n");
                break;
            }

            jk_platform_repetition_test_count_bytes(test, read_size);
            size_remaining -= read_size;
            dest += read_size;
        }

        CloseHandle(file);
        handle_deallocation(test, &params);
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
    {"_read", read_via_read},
    {"ReadFile", read_via_read_file},
};

// tests[malloc][i]
static JkPlatformRepetitionTest tests[ALLOC_COUNT][JK_ARRAY_COUNT(candidates)];

int32_t jk_platform_entry_point(int32_t argc, char **argv)
{
    HANDLE process_token;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &process_token)) {
        TOKEN_PRIVILEGES privileges = {0};
        privileges.PrivilegeCount = 1;
        privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        if (LookupPrivilegeValueA(0, SE_LOCK_MEMORY_NAME, &privileges.Privileges[0].Luid)) {
            AdjustTokenPrivileges(process_token, 0, &privileges, 0, 0, 0);
            if (GetLastError() == ERROR_SUCCESS) {
                large_page_size = GetLargePageMinimum();
            }
        }
        CloseHandle(process_token);
    }

    if (argc != 2) {
        fprintf(stderr, "%s: Expected 1 file argument, got %d\n", argv[0], argc - 1);
        exit(1);
    }

    ReadParams params = {.file_name = argv[1], .dest = {.size = jk_platform_file_size(argv[1])}};
    global_buffer = malloc(params.dest.size);
    if (!global_buffer) {
        fprintf(stderr, "%s: Failed to allocate memory\n", argv[0]);
        exit(1);
    }

    int64_t frequency = jk_platform_cpu_timer_frequency_estimate(100);

    for (;;) {
        for (int64_t i = 0; i < JK_ARRAY_COUNT(candidates); i++) {
            for (params.alloc = 0; params.alloc < ALLOC_COUNT; params.alloc++) {
                JkPlatformRepetitionTest *test = &tests[params.alloc][i];

                printf("\n%s", candidates[i].name);
                switch (params.alloc) {
                case NONE:
                case ALLOC_COUNT: {
                } break;

                case MALLOC: {
                    printf(" (malloc)");
                } break;

                case VIRTUAL_ALLOC: {
                    printf(" (VirtualAlloc)");
                } break;
                case VIRTUAL_ALLOC_LARGE_PAGES: {
                    printf(" (VirtualAlloc w/ MEM_LARGE_PAGES)");
                } break;
                }
                printf("\n");

                jk_platform_repetition_test_run_wave(test, params.dest.size, frequency, 10);
                candidates[i].function(test, params);
                if (test->state == JK_REPETITION_TEST_ERROR) {
                    fprintf(stderr, "%s: Error encountered during repetition test\n", argv[0]);
                    exit(1);
                }
            }
        }
    }

    return 0;
}
