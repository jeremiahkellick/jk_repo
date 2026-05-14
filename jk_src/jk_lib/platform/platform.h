#ifndef JK_PLATFORM_H
#define JK_PLATFORM_H

#include <jk_src/jk_lib/jk_lib.h>
#include <stdio.h>

// USER DEFINED
JK_PUBLIC int32_t jk_platform_entry_point(int32_t argc, char **argv);

// ---- OS-specific definitions begin ------------------------------------------

#ifdef _WIN32

#include <windows.h>

#if JK_PLATFORM_DESKTOP_APP
JK_GLOBAL_DECLARE HINSTANCE jk_platform_hinstance;
#endif

typedef SYNCHRONIZATION_BARRIER JkPlatformBarrier;

#else

#include <pthread.h>

typedef struct JkPlatformBarrier {
    int64_t needed;
    int64_t called;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} JkPlatformBarrier;

#endif

// ---- OS-specific definitions end --------------------------------------------

// ---- OS functions begin -----------------------------------------------------

JK_PUBLIC int64_t jk_platform_file_size(JkBuffer path);

JK_PUBLIC int64_t jk_platform_page_size(void);

typedef enum JkAllocType {
    JK_ALLOC_RESERVE,
    JK_ALLOC_COMMIT,
    JK_ALLOC_TYPE_COUNT,
} JkAllocType;

JK_PUBLIC JkBuffer jk_platform_memory_alloc(JkAllocType type, int64_t size);

JK_PUBLIC b32 jk_platform_memory_commit(void *address, int64_t size);

JK_PUBLIC void jk_platform_memory_free(JkBuffer memory);

JK_PUBLIC uint64_t jk_platform_page_fault_count_get(void);

JK_PUBLIC uint64_t jk_platform_os_timer_get(void);

JK_PUBLIC int64_t jk_platform_os_timer_frequency(void);

JK_PUBLIC void jk_platform_set_working_directory_to_executable_directory(void);

JK_PUBLIC int jk_platform_exec(JkBufferArray command);

JK_PUBLIC void jk_platform_sleep(int64_t milliseconds);

JK_PUBLIC b32 jk_platform_ensure_directory_exists(char *directory_path);

JK_PUBLIC b32 jk_platform_create_directory(JkBuffer path);

JK_PUBLIC JkBuffer jk_platform_stack_trace(JkBuffer buffer, int64_t skip, int64_t indent);

JK_PUBLIC void jk_platform_print(JkBuffer string);

JK_PUBLIC b32 jk_platform_barrier_init(JkPlatformBarrier *b, int64_t needed);

JK_PUBLIC void jk_platform_barrier_wait(JkPlatformBarrier *b);

JK_PUBLIC void jk_platform_barrier_destroy(JkPlatformBarrier *b);

// ---- OS functions end -------------------------------------------------------

// ---- ISA functions begin ----------------------------------------------------

JK_PUBLIC double jk_platform_fma_64(double a, double b, double c);

// ---- ISA functions end ------------------------------------------------------

// ---- Virtual arena begin ------------------------------------------------------------

JK_PUBLIC JkArena jk_platform_arena_virtual_init(int64_t virtual_size);

JK_PUBLIC void jk_platform_arena_virtual_release(JkArena *arena);

// ---- Virtual arena end --------------------------------------------------------------

// ---- Repetition test begin --------------------------------------------------

typedef enum JkPlatformRepetitionTestState {
    JK_REPETITION_TEST_UNINITIALIZED,
    JK_REPETITION_TEST_RUNNING,
    JK_REPETITION_TEST_COMPLETE,
    JK_REPETITION_TEST_ERROR,
} JkPlatformRepetitionTestState;

typedef enum JkPlatformRepetitionTestValueType {
    JK_PLATFORM_REPETITION_TEST_VALUE_COUNT,
    JK_PLATFORM_REPETITION_TEST_VALUE_CPU_TIME,
    JK_PLATFORM_REPETITION_TEST_VALUE_BYTE_COUNT,
    JK_PLATFORM_REPETITION_TEST_VALUE_PAGE_FAULT_COUNT,

    JK_PLATFORM_REPETITION_TEST_VALUE_TYPE_COUNT,
} JkPlatformRepetitionTestValueType;

typedef union JkPlatformRepetitionTestSample {
    int64_t v[JK_PLATFORM_REPETITION_TEST_VALUE_TYPE_COUNT];
    struct {
        int64_t count;
        uint64_t cpu_time;
        int64_t byte_count;
        uint64_t page_fault_count;
    };
} JkPlatformRepetitionTestSample;

typedef enum JkPlatformRepetitionTestSampleType {
    JK_PLATFORM_REPETITION_TEST_SAMPLE_CURRENT,
    JK_PLATFORM_REPETITION_TEST_SAMPLE_MIN,
    JK_PLATFORM_REPETITION_TEST_SAMPLE_MAX,
    JK_PLATFORM_REPETITION_TEST_SAMPLE_TOTAL,
    JK_PLATFORM_REPETITION_TEST_SAMPLE_TYPE_COUNT,
} JkPlatformRepetitionTestSampleType;

typedef struct JkPlatformRepetitionTest {
    JkPlatformRepetitionTestState state;
    int64_t target_byte_count;
    int64_t frequency;
    int64_t try_for_clocks;
    int64_t block_open_count;
    int64_t block_close_count;
    uint64_t last_found_min_time;
    union {
        JkPlatformRepetitionTestSample samples[JK_PLATFORM_REPETITION_TEST_SAMPLE_TYPE_COUNT];
        struct {
            JkPlatformRepetitionTestSample current;
            JkPlatformRepetitionTestSample min;
            JkPlatformRepetitionTestSample max;
            JkPlatformRepetitionTestSample total;
        };
    };
} JkPlatformRepetitionTest;

JK_PUBLIC void jk_platform_repetition_test_run_wave(JkPlatformRepetitionTest *test,
        int64_t target_byte_count,
        int64_t frequency,
        int64_t seconds_to_try);

JK_PUBLIC void jk_platform_repetition_test_time_begin(JkPlatformRepetitionTest *test);

JK_PUBLIC void jk_platform_repetition_test_time_end(JkPlatformRepetitionTest *test);

JK_PUBLIC double jk_platform_repetition_test_bandwidth(
        JkPlatformRepetitionTestSample values, int64_t frequency);

JK_PUBLIC b32 jk_platform_repetition_test_running_baseline(
        JkPlatformRepetitionTest *test, JkPlatformRepetitionTest *baseline);

JK_PUBLIC b32 jk_platform_repetition_test_running(JkPlatformRepetitionTest *test);

JK_PUBLIC void jk_platform_repetition_test_count_bytes(
        JkPlatformRepetitionTest *test, int64_t bytes);

JK_PUBLIC void jk_platform_repetition_test_error(JkPlatformRepetitionTest *test, char *message);

// ---- Repetition test end ----------------------------------------------------

// ---- Command line arguments parsing begin -----------------------------------

typedef struct JkOption {
    /**
     * Character used as the short-option flag. The null character means there is no short form of
     * this option. An option must have some way to refer to it. If this is the null character,
     * long_name must not be null.
     */
    char flag;

    /**
     * The long name of this option. NULL means there is no long name for this option. An option
     * must have some way to refer to it. If this is NULL, flag must not be the null character.
     */
    char *long_name;

    /** Name of the argument for this option. NULL if this option does not accept an argument. */
    char *arg_name;

    /** Description of this option used to print help text */
    char *description;
} JkOption;

typedef struct JkOptionResult {
    b32 present;
    char *arg;
    JkBuffer buf;
} JkOptionResult;

typedef struct JkOptionsParseResult {
    /** Pointer to the first operand (first non-option argument) */
    char **operands;
    int64_t operand_count;
    b32 usage_error;
} JkOptionsParseResult;

JK_PUBLIC void jk_options_parse(int argc,
        char **argv,
        JkOption *options_in,
        JkOptionResult *options_out,
        int64_t option_count,
        JkOptionsParseResult *result);

JK_PUBLIC void jk_options_print_help(FILE *file, JkOption *options, int option_count);

JK_PUBLIC double jk_parse_double(JkBuffer number_string);

// ---- Command line arguments parsing end -------------------------------------

JK_PUBLIC void jk_platform_thread_init_channel(JkChannel channel);

JK_PUBLIC void jk_platform_thread_init(void);

JK_PUBLIC JkLog *jk_platform_log_create(int64_t size);

JK_PUBLIC int64_t jk_platform_page_size_round_up(int64_t n);

JK_PUBLIC int64_t jk_platform_page_size_round_down(int64_t n);

JK_PUBLIC JkBuffer jk_platform_file_read(JkArena *arena, JkBuffer path);

JK_PUBLIC b32 jk_platform_file_write(JkBuffer path, JkBuffer contents);

JK_PUBLIC JkBuffer jk_platform_file_read_full(JkArena *arena, char *file_name);

JK_PUBLIC JkBufferArray jk_platform_file_read_lines(JkArena *arena, char *file_name);

JK_PUBLIC b32 jk_platform_write_as_c_byte_array(
        JkBuffer buffer, JkBuffer file_path, JkBuffer array_name);

JK_PUBLIC int64_t jk_platform_cpu_timer_frequency_estimate(int64_t milliseconds_to_wait);

JK_PUBLIC void jk_platform_profile_end_and_print(void);

JK_PUBLIC void jk_platform_print_bytes_int64(FILE *file, char *format, int64_t byte_count);

JK_PUBLIC void jk_platform_print_bytes_double(FILE *file, char *format, double byte_count);

#endif
