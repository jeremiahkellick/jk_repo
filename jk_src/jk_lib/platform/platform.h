#ifndef JK_PLATFORM_H
#define JK_PLATFORM_H

#include <wchar.h>

#include <jk_src/jk_lib/jk_lib.h>

// ---- Copy-paste from windows headers to avoid importing windows.h begin -----

#ifndef GUID_DEFINED
#define GUID_DEFINED
#if defined(__midl)
typedef struct {
    unsigned long Data1;
    unsigned short Data2;
    unsigned short Data3;
    byte Data4[8];
} GUID;
#else
typedef struct _GUID {
    unsigned long Data1;
    unsigned short Data2;
    unsigned short Data3;
    unsigned char Data4[8];
} GUID;
#endif
#endif

// ---- Copy-paste from windows headers to avoid importing windows.h end -------

typedef uint32_t b32;

// ---- OS functions begin -----------------------------------------------------

JK_PUBLIC void jk_platform_init(void);

JK_PUBLIC size_t jk_platform_file_size(char *file_name);

JK_PUBLIC size_t jk_platform_page_size(void);

JK_PUBLIC void *jk_platform_memory_reserve(size_t size);

JK_PUBLIC b32 jk_platform_memory_commit(void *address, size_t size);

JK_PUBLIC void *jk_platform_memory_alloc(size_t size);

JK_PUBLIC void jk_platform_memory_free(void *address, size_t size);

JK_PUBLIC void jk_platform_console_utf8_enable(void);

JK_PUBLIC uint64_t jk_platform_large_pages_try_enable(void);

JK_PUBLIC uint64_t jk_platform_large_page_size(void);

JK_PUBLIC uint64_t jk_platform_page_fault_count_get(void);

JK_PUBLIC uint64_t jk_platform_os_timer_get(void);

JK_PUBLIC uint64_t jk_platform_os_timer_frequency(void);

// -------- Performance-monitoring counters begin ------------------------------

typedef wchar_t *JkPlatformPmcNameArray[];

#define JK_PLATFORM_PMC_SOURCES_MAX 16

typedef struct JkPlatformPmcMapping {
    size_t count;
    uint32_t sources[JK_PLATFORM_PMC_SOURCES_MAX];
} JkPlatformPmcMapping;

typedef struct JkPlatformPmcTracer {
    uint64_t count;
    uint64_t session;
    uint64_t trace;
    uint64_t provider;
    uint64_t registration;
    GUID guid;
    uint32_t thread_id;
    void *thread_handle;
    b32 ended;
} JkPlatformPmcTracer;

typedef struct JkPlatformPmcResult {
    uint64_t hit_count;
    uint64_t counters[JK_PLATFORM_PMC_SOURCES_MAX];
} JkPlatformPmcResult;

typedef struct JkPlatformPmcZone {
    JkPlatformPmcResult result;
    uint64_t marker_processed_count;
    uint64_t count;
    b32 closed;
    b32 complete;
    void *complete_event;

    uint64_t padding[4];

    uint64_t marker_issued_count;
} JkPlatformPmcZone;

_STATIC_ASSERT(offsetof(JkPlatformPmcZone, marker_issued_count)
                - offsetof(JkPlatformPmcZone, marker_processed_count)
        >= 64);

JK_PUBLIC JkPlatformPmcMapping jk_platform_pmc_map_names(
        JkPlatformPmcNameArray names, size_t count);

JK_PUBLIC b32 jk_platform_pmc_mapping_valid(JkPlatformPmcMapping mapping);

JK_PUBLIC void jk_platform_pmc_trace_begin(
        JkPlatformPmcTracer *tracer, JkPlatformPmcMapping *mapping);

JK_PUBLIC void jk_platform_pmc_trace_end(JkPlatformPmcTracer *tracer);

JK_PUBLIC void jk_platform_pmc_zone_open(JkPlatformPmcTracer *tracer, JkPlatformPmcZone *zone);

JK_PUBLIC void jk_platform_pmc_zone_close(JkPlatformPmcTracer *tracer, JkPlatformPmcZone *zone);

JK_PUBLIC void jk_platform_pmc_zone_collection_start(
        JkPlatformPmcTracer *tracer, JkPlatformPmcZone *zone);

JK_PUBLIC void jk_platform_pmc_zone_collection_stop(
        JkPlatformPmcTracer *tracer, JkPlatformPmcZone *zone);

JK_PUBLIC b32 jk_platform_pmc_zone_result_ready(
        JkPlatformPmcTracer *tracer, JkPlatformPmcZone *zone);

JK_PUBLIC JkPlatformPmcResult jk_platform_pmc_zone_result_get(
        JkPlatformPmcTracer *tracer, JkPlatformPmcZone *zone);

JK_PUBLIC JkPlatformPmcResult jk_platform_pmc_zone_result_wait(
        JkPlatformPmcTracer *tracer, JkPlatformPmcZone *zone);

// -------- Performance-monitoring counters end --------------------------------

// ---- OS functions end -------------------------------------------------------

// ---- Compiler functions begin -----------------------------------------------

JK_PUBLIC uint64_t jk_platform_cpu_timer_get(void);

// ---- Compiler functions end -------------------------------------------------

// ---- Arena begin ------------------------------------------------------------

typedef struct JkPlatformArena {
    size_t virtual_size;
    size_t size;
    size_t pos;
    uint8_t *address;
} JkPlatformArena;

typedef enum JkPlatformArenaInitResult {
    JK_PLATFORM_ARENA_INIT_SUCCESS,
    JK_PLATFORM_ARENA_INIT_FAILURE,
} JkPlatformArenaInitResult;

JK_PUBLIC JkPlatformArenaInitResult jk_platform_arena_init(
        JkPlatformArena *arena, size_t virtual_size);

JK_PUBLIC void jk_platform_arena_terminate(JkPlatformArena *arena);

JK_PUBLIC void *jk_platform_arena_push(JkPlatformArena *arena, size_t size);

JK_PUBLIC void *jk_platform_arena_push_zero(JkPlatformArena *arena, size_t size);

typedef enum JkPlatformArenaPopResult {
    JK_PLATFORM_ARENA_POP_SUCCESS,
    JK_PLATFORM_ARENA_POP_TRIED_TO_POP_MORE_THAN_POS,
} JkPlatformArenaPopResult;

JK_PUBLIC JkPlatformArenaPopResult jk_platform_arena_pop(JkPlatformArena *arena, size_t size);

// ---- Arena end --------------------------------------------------------------

// ---- Profile begin ----------------------------------------------------------

#ifndef JK_PLATFORM_PROFILE_DISABLE
#define JK_PLATFORM_PROFILE_DISABLE 0
#endif

#if JK_PLATFORM_PROFILE_DISABLE

#define JK_PLATFORM_PROFILE_ZONE_BANDWIDTH_BEGIN(...)
#define JK_PLATFORM_PROFILE_ZONE_TIME_BEGIN(...)
#define JK_PLATFORM_PROFILE_ZONE_END(...)

#else

typedef struct JkPlatformProfileEntry {
    char *name;
    uint64_t elapsed_exclusive;
    uint64_t elapsed_inclusive;
    uint64_t hit_count;
    uint64_t byte_count;
    uint64_t depth;

#ifndef NDEBUG
    int64_t active_count;
#endif

    b32 seen;
} JkPlatformProfileEntry;

typedef struct JkPlatformProfileTiming {
    uint64_t saved_elapsed_inclusive;
    JkPlatformProfileEntry *parent;
    uint64_t start;

#ifndef NDEBUG
    JkPlatformProfileEntry *entry;
    b32 ended;
#endif
} JkPlatformProfileTiming;

JK_PUBLIC void jk_platform_profile_zone_begin(JkPlatformProfileTiming *timing,
        JkPlatformProfileEntry *entry,
        char *name,
        uint64_t byte_count);

JK_PUBLIC void jk_platform_profile_zone_end(JkPlatformProfileTiming *timing);

#define JK_PLATFORM_PROFILE_ZONE_BANDWIDTH_BEGIN(identifier, byte_count)          \
    JkPlatformProfileTiming jk_platform_profile_timing__##identifier;             \
    do {                                                                          \
        static JkPlatformProfileEntry jk_platform_profile_time_begin_entry;       \
        jk_platform_profile_zone_begin(&jk_platform_profile_timing__##identifier, \
                &jk_platform_profile_time_begin_entry,                            \
                #identifier,                                                      \
                byte_count);                                                      \
    } while (0)

#define JK_PLATFORM_PROFILE_ZONE_TIME_BEGIN(identifier) \
    JK_PLATFORM_PROFILE_ZONE_BANDWIDTH_BEGIN(identifier, 0)

#define JK_PLATFORM_PROFILE_ZONE_END(identifier) \
    jk_platform_profile_zone_end(&jk_platform_profile_timing__##identifier);

#endif

JK_PUBLIC void jk_platform_profile_begin(void);

JK_PUBLIC void jk_platform_profile_end_and_print(void);

typedef enum JkPlatformRepetitionTestState {
    JK_REPETITION_TEST_UNINITIALIZED,
    JK_REPETITION_TEST_RUNNING,
    JK_REPETITION_TEST_COMPLETE,
    JK_REPETITION_TEST_ERROR,
} JkPlatformRepetitionTestState;

typedef enum JkPlatformRepValue {
    JK_REP_VALUE_TEST_COUNT,
    JK_REP_VALUE_CPU_TIMER,
    JK_REP_VALUE_BYTE_COUNT,
    JK_REP_VALUE_PAGE_FAULT_COUNT,

    JK_REP_VALUE_COUNT,
} JkPlatformRepValue;

typedef struct JkPlatformRepValues {
    uint64_t v[JK_REP_VALUE_COUNT];
} JkPlatformRepValues;

typedef struct JkPlatformRepetitionTest {
    JkPlatformRepetitionTestState state;
    uint64_t target_byte_count;
    uint64_t frequency;
    uint64_t try_for_clocks;
    uint64_t block_open_count;
    uint64_t block_close_count;
    uint64_t last_found_min_time;
    JkPlatformRepValues current;
    JkPlatformRepValues min;
    JkPlatformRepValues max;
    JkPlatformRepValues total;
} JkPlatformRepetitionTest;

JK_PUBLIC void jk_platform_repetition_test_run_wave(JkPlatformRepetitionTest *test,
        uint64_t target_byte_count,
        uint64_t frequency,
        uint64_t seconds_to_try);

JK_PUBLIC void jk_platform_repetition_test_time_begin(JkPlatformRepetitionTest *test);

JK_PUBLIC void jk_platform_repetition_test_time_end(JkPlatformRepetitionTest *test);

JK_PUBLIC double jk_platform_repetition_test_bandwidth(
        JkPlatformRepValues values, uint64_t frequency);

JK_PUBLIC b32 jk_platform_repetition_test_running(JkPlatformRepetitionTest *test);

JK_PUBLIC void jk_platform_repetition_test_count_bytes(
        JkPlatformRepetitionTest *test, uint64_t bytes);

JK_PUBLIC void jk_platform_repetition_test_error(JkPlatformRepetitionTest *test, char *message);

// ---- Profile end ------------------------------------------------------------

JK_PUBLIC size_t jk_platform_page_size_round_up(size_t n);

JK_PUBLIC size_t jk_platform_page_size_round_down(size_t n);

JK_PUBLIC JkBuffer jk_platform_file_read_full(char *file_name, JkPlatformArena *storage);

JK_PUBLIC uint64_t jk_platform_cpu_timer_frequency_estimate(uint64_t milliseconds_to_wait);

#endif
