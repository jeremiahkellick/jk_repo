#include <stdio.h>
#include <windows.h>

// #jk_build single_translation_unit

// #jk_build dependencies_begin
#include <jk_src/jk_lib/platform/platform.h>
// #jk_build dependencies_end

#define STARTING_SIZE (8llu * 1024)

static char *program_name = "!! program_name not yet overwritten with argv[0] !!";

static uint64_t just_read(
        JkPlatformRepetitionTest *test, char *file_name, size_t file_size, size_t buffer_size)
{
    FILE *file = fopen(file_name, "rb");
    if (file) {
        uint8_t *buffer = jk_platform_memory_alloc(buffer_size);
        if (buffer) {
            size_t remaining_size = file_size;
            while (remaining_size) {
                size_t read_size = buffer_size;
                if (read_size > remaining_size) {
                    read_size = remaining_size;
                }
                if (fread(buffer, read_size, 1, file) == 1) {
                    jk_platform_repetition_test_count_bytes(test, read_size);
                } else {
                    jk_platform_repetition_test_error(test, "Failed to read file");
                }
                remaining_size -= read_size;
            }

            jk_platform_memory_free(buffer, buffer_size);
        } else {
            jk_platform_repetition_test_error(test, "Failed to allocate memory");
        }

        fclose(file);
    } else {
        jk_platform_repetition_test_error(test, "Failed to open file");
    }

    return 0;
}

static uint64_t sum(JkBuffer buffer)
{
    uint64_t *source = (uint64_t *)buffer.data;
    uint64_t sum0 = 0;
    uint64_t sum1 = 0;
    uint64_t sum2 = 0;
    uint64_t sum3 = 0;
    size_t count = buffer.size / (4 * sizeof(uint64_t));
    while (count--) {
        sum0 += source[0];
        sum0 += source[1];
        sum0 += source[2];
        sum0 += source[3];
        source += 4;
    }
    return sum0 + sum1 + sum2 + sum3;
}

static uint64_t read_and_sum(
        JkPlatformRepetitionTest *test, char *file_name, size_t file_size, size_t buffer_size)
{
    uint64_t result = 0;

    FILE *file = fopen(file_name, "rb");
    if (file) {
        uint8_t *buffer = jk_platform_memory_alloc(buffer_size);
        if (buffer) {
            size_t remaining_size = file_size;
            while (remaining_size) {
                size_t read_size = buffer_size;
                if (read_size > remaining_size) {
                    read_size = remaining_size;
                }
                if (fread(buffer, read_size, 1, file) == 1) {
                    result += sum((JkBuffer){.size = read_size, .data = buffer});
                    jk_platform_repetition_test_count_bytes(test, read_size);
                } else {
                    jk_platform_repetition_test_error(test, "Failed to read file");
                }
                remaining_size -= read_size;
            }

            jk_platform_memory_free(buffer, buffer_size);
        } else {
            jk_platform_repetition_test_error(test, "Failed to allocate memory");
        }

        fclose(file);
    } else {
        jk_platform_repetition_test_error(test, "Failed to open file");
    }

    return result;
}

typedef struct LockedBuffer {
    JkBuffer buffer;
    HANDLE sem_write;
    HANDLE sem_read;
    uint8_t padding[32];
} LockedBuffer;

typedef struct SumThreadData {
    LockedBuffer locks[2];
    uint64_t iterations;
    uint64_t result;
} SumThreadData;

static DWORD sum_thread(LPVOID ptr)
{
    SumThreadData *data = ptr;

    int i = 0;
    while (data->iterations--) {
        LockedBuffer *lock = &data->locks[i];

        WaitForSingleObject(lock->sem_read, ULONG_MAX);

        uint64_t *source = (uint64_t *)lock->buffer.data;
        uint64_t sum0 = 0;
        uint64_t sum1 = 0;
        uint64_t sum2 = 0;
        uint64_t sum3 = 0;
        size_t count = lock->buffer.size / (4 * sizeof(uint64_t));
        while (count--) {
            sum0 += source[0];
            sum0 += source[1];
            sum0 += source[2];
            sum0 += source[3];
            source += 4;
        }
        data->result += sum0 + sum1 + sum2 + sum3;

        ReleaseSemaphore(lock->sem_write, 1, NULL);

        i = !i;
    }

    return 0;
}

static uint64_t read_and_sum_threads(
        JkPlatformRepetitionTest *test, char *file_name, size_t file_size, size_t buffer_size)
{
    size_t half_size = buffer_size / 2;
    SumThreadData thread_data = {.iterations = (file_size + half_size - 1) / half_size};

    FILE *file = fopen(file_name, "rb");
    if (file) {
        uint8_t *buffer = jk_platform_memory_alloc(buffer_size);
        if (buffer) {
            for (int i = 0; i < JK_ARRAY_COUNT(thread_data.locks); i++) {
                thread_data.locks[i].sem_write = CreateSemaphore(NULL, 1, 1, NULL);
                thread_data.locks[i].sem_read = CreateSemaphore(NULL, 0, 1, NULL);
                if (!thread_data.locks[i].sem_write || !thread_data.locks[i].sem_read) {
                    jk_platform_repetition_test_error(test, "Failed to create mutex");
                    return 0;
                }
            }

            DWORD threadId;
            HANDLE thread = CreateThread(NULL, 0, sum_thread, &thread_data, 0, &threadId);
            if (thread) {
                size_t remaining_size = file_size;
                int i = 0;
                while (remaining_size) {
                    LockedBuffer *lock = &thread_data.locks[i];

                    WaitForSingleObject(lock->sem_write, ULONG_MAX);

                    lock->buffer.size = half_size;
                    lock->buffer.data = i ? buffer + half_size : buffer;
                    if (lock->buffer.size > remaining_size) {
                        lock->buffer.size = remaining_size;
                    }

                    if (fread(lock->buffer.data, lock->buffer.size, 1, file) == 1) {
                        remaining_size -= lock->buffer.size;
                        jk_platform_repetition_test_count_bytes(test, lock->buffer.size);
                        ReleaseSemaphore(lock->sem_read, 1, NULL);
                    } else {
                        jk_platform_repetition_test_error(test, "Failed to read file");
                        break;
                    }

                    i = !i;
                }

                WaitForSingleObject(thread, ULONG_MAX);
                CloseHandle(thread);
            } else {
                jk_platform_repetition_test_error(test, "Failed to create thread");
            }

            for (int i = 0; i < JK_ARRAY_COUNT(thread_data.locks); i++) {
                if (thread_data.locks[i].sem_write) {
                    CloseHandle(thread_data.locks[i].sem_write);
                }
                if (thread_data.locks[i].sem_read) {
                    CloseHandle(thread_data.locks[i].sem_read);
                }
            }

            jk_platform_memory_free(buffer, buffer_size);
        } else {
            jk_platform_repetition_test_error(test, "Failed to allocate memory");
        }

        fclose(file);
    } else {
        jk_platform_repetition_test_error(test, "Failed to open file");
    }

    return thread_data.result;
}

typedef struct Function {
    char *name;
    uint64_t (*ptr)(
            JkPlatformRepetitionTest *test, char *file_name, size_t file_size, size_t buffer_size);
} Function;

static Function functions[] = {
    {.name = "Read and sum threads", .ptr = read_and_sum_threads},
    {.name = "Just read", .ptr = just_read},
    {.name = "Read and sum", .ptr = read_and_sum},
};

static JkPlatformRepetitionTest tests[13][JK_ARRAY_COUNT(functions)];

int main(int argc, char **argv)
{
    program_name = argv[0];

    if (argc != 2) {
        fprintf(stderr, "%s: Expected 1 file argument, got %d\n", argv[0], argc - 1);
        exit(1);
    }

    size_t file_size = jk_platform_file_size(argv[1]);
    uint64_t frequency = jk_platform_cpu_timer_frequency_estimate(100);

    JkPlatformArenaVirtualRoot arena_root;
    JkArena storage = jk_platform_arena_virtual_init(&arena_root, 64llu * 1024 * 1024 * 1024);
    JkBuffer full_file_buffer = jk_platform_file_read_full(&storage, argv[1]);
    uint64_t reference_sum = sum(full_file_buffer);
    jk_platform_arena_virtual_release(&arena_root);

    size_t buffer_size = STARTING_SIZE;
    for (size_t i = 0; i < JK_ARRAY_COUNT(tests); i++, buffer_size *= 2) {
        for (size_t j = 0; j < JK_ARRAY_COUNT(functions); j++) {
            JkPlatformRepetitionTest *test = &tests[i][j];

            printf("\nFunction: %s, buffer size: ", functions[j].name);
            jk_platform_print_bytes_uint64(stdout, "%.0f", buffer_size);
            printf("\n");

            jk_platform_repetition_test_run_wave(test, file_size, frequency, 10);
            uint8_t passed = 1;
            while (jk_platform_repetition_test_running(test)) {
                jk_platform_repetition_test_time_begin(test);
                uint64_t sum = functions[j].ptr(test, argv[1], file_size, buffer_size);
                if (sum != reference_sum) {
                    passed = 0;
                }
                jk_platform_repetition_test_time_end(test);
            }
            if (!passed) {
                fprintf(stderr, "WARNING: Checksum mismatch\n");
            }
        }
    }

    printf("\nSize");
    for (size_t i = 0; i < JK_ARRAY_COUNT(functions); i++) {
        printf(",%s", functions[i].name);
    }
    printf("\n");
    for (size_t i = 0, size = STARTING_SIZE; i < JK_ARRAY_COUNT(tests); i++, size *= 2) {
        jk_platform_print_bytes_uint64(stdout, "%.0f", size);
        for (size_t j = 0; j < JK_ARRAY_COUNT(functions); j++) {
            printf(",%.3f",
                    jk_platform_repetition_test_bandwidth(tests[i][j].min, frequency)
                            / (1024.0 * 1024.0 * 1024.0));
        }
        printf("\n");
    }
}
