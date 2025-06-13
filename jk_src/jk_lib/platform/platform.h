#ifndef JK_PLATFORM_H
#define JK_PLATFORM_H

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

JK_PUBLIC void jk_platform_set_working_directory_to_executable_directory(void);

// ---- OS functions end -------------------------------------------------------

// ---- Compiler functions begin -----------------------------------------------

JK_PUBLIC uint64_t jk_platform_cpu_timer_get(void);

// ---- Compiler functions end -------------------------------------------------

// ---- Arena begin ------------------------------------------------------------

typedef struct JkPlatformArena {
    uint64_t virtual_size;
    uint64_t size;
    uint64_t pos;
    uint8_t *address;
} JkPlatformArena;

typedef enum JkPlatformArenaInitResult {
    JK_PLATFORM_ARENA_INIT_SUCCESS,
    JK_PLATFORM_ARENA_INIT_FAILURE,
} JkPlatformArenaInitResult;

JK_PUBLIC JkPlatformArenaInitResult jk_platform_arena_init(
        JkPlatformArena *arena, uint64_t virtual_size);

JK_PUBLIC void jk_platform_arena_terminate(JkPlatformArena *arena);

JK_PUBLIC void *jk_platform_arena_push(JkPlatformArena *arena, uint64_t size);

JK_PUBLIC void *jk_platform_arena_push_zero(JkPlatformArena *arena, uint64_t size);

typedef enum JkPlatformArenaPopResult {
    JK_PLATFORM_ARENA_POP_SUCCESS,
    JK_PLATFORM_ARENA_POP_TRIED_TO_POP_MORE_THAN_POS,
} JkPlatformArenaPopResult;

JK_PUBLIC JkPlatformArenaPopResult jk_platform_arena_pop(JkPlatformArena *arena, uint64_t size);

JK_PUBLIC void *jk_platform_arena_pointer_get(JkPlatformArena *arena);

JK_PUBLIC void jk_platform_arena_pointer_set(JkPlatformArena *arena, void *pointer);

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

typedef enum JkPlatformProfileFrameType {
    JK_PLATFORM_PROFILE_FRAME_CURRENT,
    JK_PLATFORM_PROFILE_FRAME_MIN,
    JK_PLATFORM_PROFILE_FRAME_MAX,
    JK_PLATFORM_PROFILE_FRAME_TOTAL,
    JK_PLATFORM_PROFILE_FRAME_TYPE_COUNT,
} JkPlatformProfileFrameType;

typedef enum JkPlatformProfileMetric {
    JK_PLATFORM_PROFILE_METRIC_ELAPSED_EXCLUSIVE,
    JK_PLATFORM_PROFILE_METRIC_ELAPSED_INCLUSIVE,
    JK_PLATFORM_PROFILE_METRIC_HIT_COUNT,
    JK_PLATFORM_PROFILE_METRIC_BYTE_COUNT,
    JK_PLATFORM_PROFILE_METRIC_DEPTH,
    JK_PLATFORM_PROFILE_METRIC_COUNT,
} JkPlatformProfileMetric;

typedef union JkPlatformProfileZoneFrame {
    uint64_t a[JK_PLATFORM_PROFILE_METRIC_COUNT];
    struct {
        uint64_t elapsed_exclusive;
        uint64_t elapsed_inclusive;
        uint64_t hit_count;
        uint64_t byte_count;
        uint64_t depth;
    };
} JkPlatformProfileZoneFrame;

typedef struct JkPlatformProfileZone {
    char *name;
    JkPlatformProfileZoneFrame frames[JK_PLATFORM_PROFILE_FRAME_TYPE_COUNT];

#ifndef NDEBUG
    int64_t active_count;
#endif

    b32 seen;
} JkPlatformProfileZone;

typedef struct JkPlatformProfileTiming {
    uint64_t saved_elapsed_inclusive;
    JkPlatformProfileZone *parent;
    uint64_t start;

#ifndef NDEBUG
    JkPlatformProfileZone *zone;
    b32 ended;
#endif
} JkPlatformProfileTiming;

JK_PUBLIC void jk_platform_profile_zone_begin(JkPlatformProfileTiming *timing,
        JkPlatformProfileZone *zone,
        char *name,
        uint64_t byte_count);

JK_PUBLIC void jk_platform_profile_zone_end(JkPlatformProfileTiming *timing);

#define JK_PLATFORM_PROFILE_ZONE_BANDWIDTH_BEGIN(identifier, byte_count)          \
    JkPlatformProfileTiming jk_platform_profile_timing__##identifier;             \
    do {                                                                          \
        static JkPlatformProfileZone jk_platform_profile_time_begin_zone;         \
        jk_platform_profile_zone_begin(&jk_platform_profile_timing__##identifier, \
                &jk_platform_profile_time_begin_zone,                             \
                #identifier,                                                      \
                byte_count);                                                      \
    } while (0)

#define JK_PLATFORM_PROFILE_ZONE_TIME_BEGIN(identifier) \
    JK_PLATFORM_PROFILE_ZONE_BANDWIDTH_BEGIN(identifier, 0)

#define JK_PLATFORM_PROFILE_ZONE_END(identifier) \
    jk_platform_profile_zone_end(&jk_platform_profile_timing__##identifier);

#endif

JK_PUBLIC void jk_platform_profile_frame_begin(void);

JK_PUBLIC void jk_platform_profile_frame_end(void);

JK_PUBLIC void jk_platform_profile_print_custom(
        void (*print)(void *data, char *format, ...), void *data);

JK_PUBLIC void jk_platform_profile_frame_end_and_print_custom(
        void (*print)(void *data, char *format, ...), void *data);

JK_PUBLIC void jk_platform_profile_print(void);

JK_PUBLIC void jk_platform_profile_frame_end_and_print(void);

// ---- Profile end ------------------------------------------------------------

// ---- Repetition test begin --------------------------------------------------

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

// ---- Repetition test end ----------------------------------------------------

// ---- File formats begin -----------------------------------------------------

#ifdef _MSC_VER
#pragma pack(push, 1)
typedef struct JkBitmapHeader {
#else
typedef struct __attribute__((packed)) JkBitmapHeader {
#endif
    uint16_t identifier;
    uint32_t size;
    uint32_t reserved;
    uint32_t offset;
    uint32_t info_header_size;
    uint32_t width;
    uint32_t height;
} JkBitmapHeader;
#ifdef _MSC_VER
#pragma pack(pop)
#endif

#ifdef _MSC_VER
#pragma pack(push, 1)
typedef struct JkRiffChunk {
#else
typedef struct __attribute__((packed)) JkRiffChunk {
#endif
    uint32_t id;
    uint32_t size;
    uint8_t data[];
} JkRiffChunk;
#ifdef _MSC_VER
#pragma pack(pop)
#endif

#ifdef _MSC_VER
#pragma pack(push, 1)
typedef struct JkRiffChunkMain {
#else
typedef struct __attribute__((packed)) JkRiffChunkMain {
#endif
    uint32_t id;
    uint32_t size;
    uint32_t form_type;
    uint8_t chunk_first[];
} JkRiffChunkMain;
#ifdef _MSC_VER
#pragma pack(pop)
#endif

#ifdef _MSC_VER
#pragma pack(push, 1)
typedef struct JkWavFormat {
#else
typedef struct __attribute__((packed)) JkWavFormat {
#endif
    uint16_t format_tag;
    uint16_t channel_count;
    uint32_t samples_per_second;
    uint32_t average_bytes_per_second;
    uint16_t block_align;
    uint16_t bits_per_sample;
    uint16_t size;
    uint16_t valid_bits_per_sample;
    uint32_t channel_mask;
    uint8_t sub_format[16];
} JkWavFormat;
#ifdef _MSC_VER
#pragma pack(pop)
#endif

#define JK_RIFF_ID(c0, c1, c2, c3)                                          \
    (((uint32_t)(c0) << 0) | ((uint32_t)(c1) << 8) | ((uint32_t)(c2) << 16) \
            | ((uint32_t)(c3) << 24))

typedef enum RiffId {
    JK_RIFF_ID_RIFF = JK_RIFF_ID('R', 'I', 'F', 'F'),
    JK_RIFF_ID_WAV = JK_RIFF_ID('W', 'A', 'V', 'E'),
    JK_RIFF_ID_FMT = JK_RIFF_ID('f', 'm', 't', ' '),
    JK_RIFF_ID_DATA = JK_RIFF_ID('d', 'a', 't', 'a'),
} RiffId;

#define JK_WAV_FORMAT_PCM 0x1

JK_PUBLIC b32 jk_riff_chunk_valid(JkRiffChunkMain *chunk_main, JkRiffChunk *chunk);

JK_PUBLIC JkRiffChunk *jk_riff_chunk_next(JkRiffChunk *chunk);

// ---- File formats end -------------------------------------------------------

JK_PUBLIC size_t jk_platform_page_size_round_up(size_t n);

JK_PUBLIC size_t jk_platform_page_size_round_down(size_t n);

JK_PUBLIC JkBuffer jk_platform_file_read_full(JkPlatformArena *storage, char *file_name);

JK_PUBLIC JkBufferArray jk_platform_file_read_lines(JkPlatformArena *arena, char *file_name);

JK_PUBLIC uint64_t jk_platform_cpu_timer_frequency_estimate(uint64_t milliseconds_to_wait);

#endif
