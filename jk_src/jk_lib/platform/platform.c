#include <errno.h>
#include <stdlib.h>
#include <string.h>

// #jk_build dependencies_begin
#include <jk_src/jk_lib/jk_lib.h>
// #jk_build dependencies_end

#include "platform.h"

// ---- OS functions begin -----------------------------------------------------

#ifdef _WIN32

#include <windows.h>

#define INITGUID // Causes definition of SystemTraceControlGuid in evntrace.h
#include <evntcons.h>
#include <evntrace.h>
#include <stdio.h>
#include <sys/stat.h>

typedef struct JkPlatformData {
    b32 initialized;
    uint64_t large_page_size;
    HANDLE process;
} JkPlatformData;

static JkPlatformData jk_platform_data;

JK_PUBLIC void jk_platform_init(void)
{
    JK_ASSERT(!jk_platform_data.initialized);
    jk_platform_data.initialized = 1;
    jk_platform_data.large_page_size = jk_platform_large_pages_try_enable();
    jk_platform_data.process = OpenProcess(PROCESS_QUERY_INFORMATION, 0, GetCurrentProcessId());
}

JK_PUBLIC size_t jk_platform_file_size(char *file_name)
{
    struct __stat64 info = {0};
    if (_stat64(file_name, &info)) {
        fprintf(stderr, "jk_platform_file_size: stat returned an error\n");
        return 0;
    }
    return (size_t)info.st_size;
}

JK_PUBLIC size_t jk_platform_page_size(void)
{
    return 4096;
}

JK_PUBLIC uint64_t jk_platform_large_pages_try_enable(void)
{
    static b32 has_been_called = 0;
    JK_ASSERT(!has_been_called);
    has_been_called = 1;

    uint64_t result = 0;

    HANDLE process_token;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &process_token)) {
        TOKEN_PRIVILEGES privileges = {0};
        privileges.PrivilegeCount = 1;
        privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        if (LookupPrivilegeValue(0, SE_LOCK_MEMORY_NAME, &privileges.Privileges[0].Luid)) {
            AdjustTokenPrivileges(process_token, 0, &privileges, 0, 0, 0);
            if (GetLastError() == ERROR_SUCCESS) {
                result = GetLargePageMinimum();
            }
        }
        CloseHandle(process_token);
    }

    return result;
}

JK_PUBLIC uint64_t jk_platform_large_page_size(void)
{
    return jk_platform_data.large_page_size;
}

JK_PUBLIC void *jk_platform_memory_reserve(size_t size)
{
    return VirtualAlloc(NULL, size, MEM_RESERVE, PAGE_NOACCESS);
}

JK_PUBLIC b32 jk_platform_memory_commit(void *address, size_t size)
{
    return VirtualAlloc(address, size, MEM_COMMIT, PAGE_READWRITE) != NULL;
}

JK_PUBLIC void *jk_platform_memory_alloc(size_t size)
{
    return VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}

JK_PUBLIC void *jk_platform_memory_alloc_large(size_t size)
{
    return VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE | MEM_LARGE_PAGES, PAGE_READWRITE);
}

JK_PUBLIC void jk_platform_memory_free(void *address, size_t size)
{
    // TODO: Consider how to deal with different freeing behavior between Windows and Unix
    VirtualFree(address, 0, MEM_RELEASE);
}

JK_PUBLIC void jk_platform_console_utf8_enable(void)
{
    SetConsoleOutputCP(CP_UTF8);
}

typedef struct _PROCESS_MEMORY_COUNTERS {
    DWORD cb;
    DWORD PageFaultCount;
    SIZE_T PeakWorkingSetSize;
    SIZE_T WorkingSetSize;
    SIZE_T QuotaPeakPagedPoolUsage;
    SIZE_T QuotaPagedPoolUsage;
    SIZE_T QuotaPeakNonPagedPoolUsage;
    SIZE_T QuotaNonPagedPoolUsage;
    SIZE_T PagefileUsage;
    SIZE_T PeakPagefileUsage;
} PROCESS_MEMORY_COUNTERS;

typedef BOOL (*GetProcessMemoryInfoPointer)(HANDLE, PROCESS_MEMORY_COUNTERS *, DWORD);

PROCESS_MEMORY_COUNTERS jk_process_memory_info_get()
{
    JK_DEBUG_ASSERT(jk_platform_data.initialized);

    static uint8_t initialized;
    static HINSTANCE library;
    static GetProcessMemoryInfoPointer GetProcessMemoryInfo;
    if (!initialized) {
        initialized = TRUE;
        library = LoadLibraryA("psapi.dll");
        if (library) {
            GetProcessMemoryInfo =
                    (GetProcessMemoryInfoPointer)GetProcAddress(library, "GetProcessMemoryInfo");
        } else {
            // TODO: log error
        }
    }

    PROCESS_MEMORY_COUNTERS memory_counters = {.cb = sizeof(memory_counters)};
    if (GetProcessMemoryInfo) {
        if (!GetProcessMemoryInfo(
                    jk_platform_data.process, &memory_counters, sizeof(memory_counters))) {
            // TODO: log error
        }
    } else {
        // TODO: log error
    }
    return memory_counters;
}

JK_PUBLIC uint64_t jk_platform_page_fault_count_get(void)
{
    PROCESS_MEMORY_COUNTERS memory_counters = jk_process_memory_info_get();
    return (uint64_t)memory_counters.PageFaultCount;
}

JK_PUBLIC uint64_t jk_platform_os_timer_get(void)
{
    LARGE_INTEGER value;
    QueryPerformanceCounter(&value);
    return value.QuadPart;
}

JK_PUBLIC uint64_t jk_platform_os_timer_frequency(void)
{
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    return freq.QuadPart;
}

// ---- Performance-monitoring counters begin ----------------------------------

static char global_jk_platform_pmc_trace_name[] = "jk_platform_pmc_trace";

typedef struct JkPlatformEventTraceConfig {
    EVENT_TRACE_PROPERTIES properties;
    char logger_name[1024];
} JkPlatformEventTraceConfig;

typedef enum JkPlatformPmcMarkerType {
    JK_PLATFORM_PMC_MARKER_START,
    JK_PLATFORM_PMC_MARKER_STOP,
} JkPlatformPmcMarkerType;

typedef struct JkPlatformPmcMarker {
    JkPlatformPmcMarkerType type;
    JkPlatformPmcZone *zone;
} JkPlatformPmcMarker;

typedef struct JkPlatformPmcMarkerEvent {
    EVENT_TRACE_HEADER header;
    JkPlatformPmcMarker marker;
} JkPlatformPmcMarkerEvent;

static GUID global_jk_platform_pmc_marker_events_guid = {
    0xe734482b, 0xbeee, 0x4ed2, {0x85, 0xc4, 0x08, 0x15, 0x73, 0x92, 0x27, 0x8c}};

static GUID global_jk_platform_pmc_guid_system = {
    0xce1dbfb4, 0x137e, 0x4da6, {0x87, 0xb0, 0x3f, 0x59, 0xaa, 0x10, 0x2c, 0xbc}};

JK_PUBLIC JkPlatformPmcMapping jk_platform_pmc_map_names(JkPlatformPmcNameArray names)
{
    JkPlatformPmcMapping result = {0};

    ULONG buffer_size;
    void *buffer = NULL;
    ULONG trace_query_1 = TraceQueryInformation(0, TraceProfileSourceListInfo, 0, 0, &buffer_size);
    ULONG trace_query_2 = ERROR_PROC_NOT_FOUND;

    if (trace_query_1 == ERROR_BAD_LENGTH) {
        buffer = malloc(buffer_size);
    } else {
        fprintf(stderr,
                "jk_platform_pmc_map_names: First TraceQueryInformation returned unexpected error "
                "code %lu\n",
                trace_query_1);
    }

    if (buffer) {
        trace_query_2 = TraceQueryInformation(
                0, TraceProfileSourceListInfo, buffer, buffer_size, &buffer_size);
    } else {
        fprintf(stderr, "jk_platform_pmc_map_names: Failed to allocate memory\n");
    }

    uint64_t names_count = 0;
    while (names.strings[names_count]) {
        names_count++;
    }

    if (trace_query_2 == ERROR_SUCCESS) {
        PROFILE_SOURCE_INFO *info = buffer;
        while (info->NextEntryOffset) {
            for (size_t i = 0; i < names_count; i++) {
                if (lstrcmpW(info->Description, names.strings[i]) == 0) {
                    result.sources[i] = info->Source;
                    result.count++;
                }
            }
            info = (PROFILE_SOURCE_INFO *)((uint8_t *)info + info->NextEntryOffset);
        }
    } else {
        fprintf(stderr,
                "jk_platform_pmc_map_names: Second TraceQueryInformation returned error code %lu\n",
                trace_query_2);
    }

    if (buffer) {
        free(buffer);
    }

    if (result.count == names_count) {
        return result;
    } else {
        return (JkPlatformPmcMapping){0};
    }
}

JK_PUBLIC b32 jk_platform_pmc_mapping_valid(JkPlatformPmcMapping mapping)
{
    return mapping.count > 0;
}

static DWORD jk_platform_pmc_thread(LPVOID trace)
{
    ULONG error_code = ProcessTrace((uint64_t *)&trace, 1, 0, 0);
    fprintf(stderr,
            "jk_platform_pmc_thread: ProcessTrace unexpectedly returned with error code %lu\n",
            error_code);
    return error_code;
}

static void jk_platform_event_record_callback(EVENT_RECORD *event)
{
    static JkPlatformPmcZone *zone;
    static uint64_t timestamp;
    static uint64_t counters[JK_PLATFORM_PMC_SOURCES_MAX];

    JkPlatformPmcTracer *tracer = event->UserContext;

    if (IsEqualGUID(&event->EventHeader.ProviderId, &global_jk_platform_pmc_marker_events_guid)) {
        JkPlatformPmcMarker marker = *(JkPlatformPmcMarker *)event->UserData;
        if (marker.type == JK_PLATFORM_PMC_MARKER_START) {
            zone = marker.zone;
        } else { // marker.type == JK_PLATFORM_PMC_MARKER_STOP
            marker.zone->result.time_elapsed += timestamp;
            for (int i = 0; i < tracer->count; i++) {
                marker.zone->result.counters[i] += counters[i];
            }

            _mm_mfence();

            marker.zone->result_ready = TRUE;
        }
    } else if (IsEqualGUID(&event->EventHeader.ProviderId, &global_jk_platform_pmc_guid_system)) {
        switch (event->EventHeader.EventDescriptor.Opcode) {
        case 51:
        case 52: {
            for (int i = 0; i < event->ExtendedDataCount; i++) {
                EVENT_HEADER_EXTENDED_DATA_ITEM *item = &event->ExtendedData[i];
                if (item->ExtType == EVENT_HEADER_EXT_TYPE_PMC_COUNTERS) {
                    timestamp = event->EventHeader.TimeStamp.QuadPart;
                    memcpy(counters, (void *)item->DataPtr, item->DataSize);
                    if (zone) {
                        JK_DEBUG_ASSERT(
                                tracer->count * sizeof(zone->result.counters[0]) == item->DataSize);
                        zone->result.time_elapsed -= timestamp;
                        for (int j = 0; j < tracer->count; j++) {
                            zone->result.counters[j] -= counters[j];
                        }
                        zone = 0;
                    }
                }
            }
        } break;

        default: {
        } break;
        }
    }
}

JK_PUBLIC void jk_platform_pmc_trace_begin(
        JkPlatformPmcTracer *tracer, JkPlatformPmcMapping *mapping)
{
    ULONG start_trace = ERROR_PROC_NOT_FOUND;
    tracer->session = 0;
    tracer->trace = INVALID_PROCESSTRACE_HANDLE;
    tracer->thread_handle = NULL;

    tracer->count = mapping->count;

    if (tracer->count > 0) {
        JkPlatformEventTraceConfig config = {0};
        config.properties.Wnode.BufferSize = sizeof(config);
        config.properties.LoggerNameOffset = offsetof(JkPlatformEventTraceConfig, logger_name);

        ControlTraceA(
                0, global_jk_platform_pmc_trace_name, &config.properties, EVENT_TRACE_CONTROL_STOP);

        config.properties.Wnode.ClientContext = 3;
        config.properties.Wnode.Flags = WNODE_FLAG_TRACED_GUID;
        config.properties.LogFileMode = EVENT_TRACE_REAL_TIME_MODE | EVENT_TRACE_SYSTEM_LOGGER_MODE;
        config.properties.EnableFlags = EVENT_TRACE_FLAG_SYSTEMCALL | EVENT_TRACE_FLAG_NO_SYSCONFIG;

        start_trace = StartTraceA(
                &tracer->session, global_jk_platform_pmc_trace_name, &config.properties);
        if (start_trace != ERROR_SUCCESS) {
            fprintf(stderr,
                    "jk_platform_pmc_trace_begin: StartTraceA returned error code %lu\n",
                    start_trace);
        }
    }

    if (start_trace == ERROR_SUCCESS) {
        ULONG trace_set_1 = TraceSetInformation(tracer->session,
                TracePmcCounterListInfo,
                mapping->sources,
                (ULONG)(mapping->count * sizeof(mapping->sources[0])));
        if (trace_set_1 != ERROR_SUCCESS) {
            fprintf(stderr,
                    "jk_platform_pmc_trace_begin: TraceSetInformation returned error code %lu\n",
                    trace_set_1);
        }

        CLASSIC_EVENT_ID event_ids[] = {
            {global_jk_platform_pmc_guid_system, 51},
            {global_jk_platform_pmc_guid_system, 52},
        };
        ULONG trace_set_2 = TraceSetInformation(
                tracer->session, TracePmcEventListInfo, event_ids, sizeof(event_ids));
        if (trace_set_2 != ERROR_SUCCESS) {
            fprintf(stderr,
                    "jk_platform_pmc_trace_begin: TraceSetInformation returned error code %lu\n",
                    trace_set_2);
        }

        EVENT_TRACE_LOGFILEA log_file = {
            .LoggerName = global_jk_platform_pmc_trace_name,
            .EventRecordCallback = jk_platform_event_record_callback,
            .ProcessTraceMode = PROCESS_TRACE_MODE_EVENT_RECORD | PROCESS_TRACE_MODE_RAW_TIMESTAMP
                    | PROCESS_TRACE_MODE_REAL_TIME,
            .Context = tracer,
        };
        tracer->trace = OpenTraceA(&log_file);
        if (tracer->trace == INVALID_PROCESSTRACE_HANDLE) {
            fprintf(stderr, "jk_platform_pmc_trace_begin: OpenTraceA failed\n");
        }
    }

    if (tracer->trace != INVALID_PROCESSTRACE_HANDLE) {
        tracer->thread_handle = CreateThread(0,
                0,
                jk_platform_pmc_thread,
                (void *)tracer->trace,
                0,
                (DWORD *)&tracer->thread_id);
        if (!tracer->thread_handle) {
            fprintf(stderr, "jk_platform_pmc_trace_begin: CreateThread failed\n");
        }
    }
}

JK_PUBLIC void jk_platform_pmc_trace_end(JkPlatformPmcTracer *tracer)
{
    JkPlatformEventTraceConfig config = {0};
    config.properties.Wnode.BufferSize = sizeof(config);
    config.properties.LoggerNameOffset = offsetof(JkPlatformEventTraceConfig, logger_name);

    ControlTraceA(
            0, global_jk_platform_pmc_trace_name, &config.properties, EVENT_TRACE_CONTROL_STOP);

    CloseHandle(tracer->thread_handle);
}

static void jk_platform_pmc_marker_event_issue(
        TRACEHANDLE provider, JkPlatformPmcMarkerType type, JkPlatformPmcZone *zone)
{
    if (!provider) {
        return;
    }

    JkPlatformPmcMarkerEvent event = {
        .header =
                {
                    .Size = sizeof(event),
                    .Flags = WNODE_FLAG_TRACED_GUID,
                    .Guid = global_jk_platform_pmc_marker_events_guid,
                },
        .marker =
                {
                    .type = type,
                    .zone = zone,
                },
    };
    ULONG status = TraceEvent(provider, &event.header);
    if (status != ERROR_SUCCESS) {
        fprintf(stderr,
                "jk_platform_pmc_marker_event_issue: TraceEvent returned error code %lu\n",
                status);
    }
}

JK_PUBLIC void jk_platform_pmc_zone_begin(JkPlatformPmcTracer *tracer, JkPlatformPmcZone *zone)
{
    memset(zone, 0, sizeof(*zone));
    if (tracer->thread_handle) {
        jk_platform_pmc_marker_event_issue(tracer->session, JK_PLATFORM_PMC_MARKER_START, zone);
    } else {
        zone->result_ready = TRUE;
    }
}

JK_PUBLIC void jk_platform_pmc_zone_end(JkPlatformPmcTracer *tracer, JkPlatformPmcZone *zone)
{
    jk_platform_pmc_marker_event_issue(tracer->session, JK_PLATFORM_PMC_MARKER_STOP, zone);
}

JK_PUBLIC b32 jk_platform_pmc_zone_result_ready(JkPlatformPmcZone *zone)
{
    return zone->result_ready;
}

JK_PUBLIC JkPlatformPmcResult jk_platform_pmc_zone_result_get(JkPlatformPmcZone *zone)
{
    return zone->result;
}

JK_PUBLIC JkPlatformPmcResult jk_platform_pmc_zone_result_wait(JkPlatformPmcZone *zone)
{
    while (!jk_platform_pmc_zone_result_ready(zone)) {
        _mm_pause();
    }

    _mm_mfence();

    return jk_platform_pmc_zone_result_get(zone);
}

// ---- Performance-monitoring counters end ------------------------------------

#else

#include <stdio.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

JK_PUBLIC size_t jk_platform_file_size(char *file_name)
{
    struct stat stat_struct = {0};
    if (stat(file_name, &stat_struct)) {
        fprintf(stderr, "jk_platform_file_size: stat returned an error\n");
        return 0;
    }
    return (size_t)stat_struct.st_size;
}

JK_PUBLIC size_t jk_platform_page_size(void)
{
    static size_t page_size = 0;
    if (page_size == 0) {
        page_size = getpagesize();
    }
    return page_size;
}

JK_PUBLIC void *jk_platform_memory_reserve(size_t size)
{
    return mmap(NULL, size, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0);
}

JK_PUBLIC b32 jk_platform_memory_commit(void *address, size_t size)
{
    return !mprotect(address, size, PROT_READ | PROT_WRITE);
}

JK_PUBLIC void *jk_platform_memory_alloc(size_t size)
{
    return mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
}

JK_PUBLIC void jk_platform_memory_free(void *address, size_t size)
{
    munmap(address, size);
}

JK_PUBLIC void jk_platform_console_utf8_enable(void) {}

typedef struct JkPlatformOsMetrics {
    b32 initialized;
} JkPlatformOsMetrics;

static JkPlatformOsMetrics jk_platform_data;

JK_PUBLIC void jk_platform_init(void)
{
    JK_ASSERT(!jk_platform_data.initialized);
    jk_platform_data.initialized = 1;
}

JK_PUBLIC uint64_t jk_platform_page_fault_count_get(void)
{
    JK_DEBUG_ASSERT(jk_platform_data.initialized);
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        return usage.ru_majflt + usage.ru_minflt;
    } else {
        return 0;
    }
}

JK_PUBLIC uint64_t jk_platform_os_timer_get(void)
{
    struct timeval value;
    gettimeofday(&value, 0);
    return jk_platform_os_timer_frequency() * (uint64_t)value.tv_sec + (uint64_t)value.tv_usec;
}

JK_PUBLIC uint64_t jk_platform_os_timer_frequency(void)
{
    return 1000000;
}

#endif

// ---- OS functions end -------------------------------------------------------

// ---- Compiler functions begin -----------------------------------------------

#ifdef _MSC_VER

#include <intrin.h>

JK_PUBLIC uint64_t jk_platform_cpu_timer_get(void)
{
    return __rdtsc();
}

#elif __TINYC__

JK_PUBLIC uint64_t jk_platform_cpu_timer_get(void)
{
    uint64_t edx;
    uint64_t eax;
    __asm__ volatile("rdtsc" : "=d"(edx), "=a"(eax));
    return (edx << 32) | eax;
}

#elif __arm64__

JK_PUBLIC uint64_t jk_platform_cpu_timer_get(void)
{
    uint64_t timebase;
    __asm__ volatile("mrs %0, CNTPCT_EL0" : "=r"(timebase));
    return timebase;
}

#else

#include <x86intrin.h>

JK_PUBLIC uint64_t jk_platform_cpu_timer_get(void)
{
    return __rdtsc();
}

#endif

// ---- Compiler functions end -------------------------------------------------

// ---- Arena begin ------------------------------------------------------------

JK_PUBLIC JkPlatformArenaInitResult jk_platform_arena_init(
        JkPlatformArena *arena, size_t virtual_size)
{
    size_t page_size = jk_platform_page_size();

    arena->virtual_size = virtual_size;
    arena->size = page_size;
    arena->pos = 0;

    arena->address = jk_platform_memory_reserve(virtual_size);
    if (!arena->address) {
        return JK_PLATFORM_ARENA_INIT_FAILURE;
    }
    if (!jk_platform_memory_commit(arena->address, page_size)) {
        return JK_PLATFORM_ARENA_INIT_FAILURE;
    }

    return JK_PLATFORM_ARENA_INIT_SUCCESS;
}

JK_PUBLIC void jk_platform_arena_terminate(JkPlatformArena *arena)
{
    jk_platform_memory_free(arena->address, arena->virtual_size);
}

JK_PUBLIC void *jk_platform_arena_push(JkPlatformArena *arena, size_t size)
{
    size_t new_pos = arena->pos + size;
    if (new_pos > arena->virtual_size) {
        return NULL;
    }
    if (new_pos > arena->size) {
        size_t expansion_size = jk_platform_page_size_round_up(new_pos - arena->size);
        if (!jk_platform_memory_commit(arena->address + arena->size, expansion_size)) {
            return NULL;
        }
        arena->size += expansion_size;
    }
    void *address = arena->address + arena->pos;
    arena->pos = new_pos;
    return address;
}

JK_PUBLIC void *jk_platform_arena_push_zero(JkPlatformArena *arena, size_t size)
{
    void *pointer = jk_platform_arena_push(arena, size);
    memset(pointer, 0, size);
    return pointer;
}

JK_PUBLIC JkPlatformArenaPopResult jk_platform_arena_pop(JkPlatformArena *arena, size_t size)
{
    if (size > arena->pos) {
        return JK_PLATFORM_ARENA_POP_TRIED_TO_POP_MORE_THAN_POS;
    }
    arena->pos -= size;
    return JK_PLATFORM_ARENA_POP_SUCCESS;
}

JK_PUBLIC void jk_platform_arena_clear(JkPlatformArena *arena)
{
    arena->pos = 0;
}

// ---- Arena end --------------------------------------------------------------

// ---- Profile begin ----------------------------------------------------------

typedef struct JkPlatformProfile {
    uint64_t start;

#if !JK_PLATFORM_PROFILE_DISABLE
    JkPlatformProfileEntry *current;
    uint64_t depth;
    size_t entry_count;
    JkPlatformProfileEntry *entries[1024];
#endif
} JkPlatformProfile;

static JkPlatformProfile jk_platform_profile;

JK_PUBLIC void jk_platform_profile_begin(void)
{
    jk_platform_profile.start = jk_platform_cpu_timer_get();
}

JK_PUBLIC void jk_platform_profile_end_and_print(void)
{
    uint64_t total = jk_platform_cpu_timer_get() - jk_platform_profile.start;
    uint64_t frequency = jk_platform_cpu_timer_frequency_estimate(100);
    printf("Total time: %.4fms (CPU freq %llu)\n",
            1000.0 * (double)total / (double)frequency,
            (long long)frequency);

#if !JK_PLATFORM_PROFILE_DISABLE
    for (size_t i = 0; i < jk_platform_profile.entry_count; i++) {
        JkPlatformProfileEntry *entry = jk_platform_profile.entries[i];

        JK_DEBUG_ASSERT(entry->active_count == 0
                && "jk_platform_profile_zone_begin was called without a matching "
                   "jk_platform_profile_zone_end");

        for (uint64_t j = 0; j < entry->depth; j++) {
            printf("\t");
        }
        printf("\t%s[%llu]: %llu (%.2f%%",
                entry->name,
                (long long)entry->hit_count,
                (long long)entry->elapsed_exclusive,
                (double)entry->elapsed_exclusive / (double)total * 100.0);
        if (entry->elapsed_inclusive != entry->elapsed_exclusive) {
            printf(", %.2f%% w/ children",
                    (double)entry->elapsed_inclusive / (double)total * 100.0);
        }
        printf(")");

        if (entry->byte_count) {
            double seconds = (double)entry->elapsed_inclusive / (double)frequency;
            printf(" ");
            jk_print_bytes_uint64(stdout, "%.3f", entry->byte_count);
            printf(" at ");
            jk_print_bytes_double(stdout, "%.3f", (double)entry->byte_count / seconds);
            printf("/s");
        }

        printf("\n");
    }
#endif
}

#if !JK_PLATFORM_PROFILE_DISABLE

JK_PUBLIC void jk_platform_profile_zone_begin(JkPlatformProfileTiming *timing,
        JkPlatformProfileEntry *entry,
        char *name,
        uint64_t byte_count)
{
    if (!entry->seen) {
        entry->seen = 1;
        entry->name = name;
        entry->byte_count += byte_count;
        entry->depth = jk_platform_profile.depth;
        jk_platform_profile.entries[jk_platform_profile.entry_count++] = entry;
        JK_DEBUG_ASSERT(
                jk_platform_profile.entry_count <= JK_ARRAY_COUNT(jk_platform_profile.entries));
    }

    timing->parent = jk_platform_profile.current;
    jk_platform_profile.current = entry;
    jk_platform_profile.depth++;

    timing->saved_elapsed_inclusive = entry->elapsed_inclusive;

#ifndef NDEBUG
    entry->active_count++;
    timing->entry = entry;
    timing->ended = 0;
#endif

    timing->start = jk_platform_cpu_timer_get();
    return;
}

JK_PUBLIC void jk_platform_profile_zone_end(JkPlatformProfileTiming *timing)
{
    uint64_t elapsed = jk_platform_cpu_timer_get() - timing->start;

#ifndef NDEBUG
    JK_ASSERT(!timing->ended
            && "jk_platform_profile_zone_end: Called multiple times for a single timing instance");
    timing->ended = 1;
    timing->entry->active_count--;
    JK_ASSERT(timing->entry->active_count >= 0
            && "jk_platform_profile_zone_end: Called more times than "
               "jk_platform_profile_zone_begin for some entry");
    JK_ASSERT(jk_platform_profile.current == timing->entry
            && "jk_platform_profile_zone_end: Must end all child timings before ending their "
               "parent");
#endif

    if (timing->parent) {
        timing->parent->elapsed_exclusive -= elapsed;
    }
    jk_platform_profile.current->elapsed_exclusive += elapsed;
    jk_platform_profile.current->elapsed_inclusive = timing->saved_elapsed_inclusive + elapsed;
    jk_platform_profile.current->hit_count++;

    jk_platform_profile.current = timing->parent;
    jk_platform_profile.depth--;
}

#endif

// ---- Profile end ------------------------------------------------------------

// ---- Repetition test begin --------------------------------------------------

JK_PUBLIC void jk_platform_repetition_test_run_wave(JkPlatformRepetitionTest *test,
        uint64_t target_byte_count,
        uint64_t frequency,
        uint64_t try_for_seconds)
{
    if (test->state == JK_REPETITION_TEST_ERROR) {
        return;
    }
    if (test->state == JK_REPETITION_TEST_UNINITIALIZED) {
        test->min.v[JK_REP_VALUE_CPU_TIMER] = UINT64_MAX;
    }
    test->state = JK_REPETITION_TEST_RUNNING;
    test->target_byte_count = target_byte_count;
    test->frequency = frequency;
    test->try_for_clocks = try_for_seconds * frequency;
    test->last_found_min_time = jk_platform_cpu_timer_get();
}

JK_PUBLIC void jk_platform_repetition_test_time_begin(JkPlatformRepetitionTest *test)
{
    test->block_open_count++;
    test->current.v[JK_REP_VALUE_PAGE_FAULT_COUNT] -= jk_platform_page_fault_count_get();
    test->current.v[JK_REP_VALUE_CPU_TIMER] -= jk_platform_cpu_timer_get();
}

JK_PUBLIC void jk_platform_repetition_test_time_end(JkPlatformRepetitionTest *test)
{
    test->current.v[JK_REP_VALUE_CPU_TIMER] += jk_platform_cpu_timer_get();
    test->current.v[JK_REP_VALUE_PAGE_FAULT_COUNT] += jk_platform_page_fault_count_get();
    test->block_close_count++;
}

JK_PUBLIC double jk_platform_repetition_test_bandwidth(
        JkPlatformRepValues values, uint64_t frequency)
{
    double seconds = (double)values.v[JK_REP_VALUE_CPU_TIMER] / (double)frequency;
    return (double)values.v[JK_REP_VALUE_BYTE_COUNT] / seconds;
}

static void jk_platform_repetition_test_print_values(
        char *name, JkPlatformRepValues values, uint64_t frequency)
{
    double test_count =
            values.v[JK_REP_VALUE_TEST_COUNT] ? (double)values.v[JK_REP_VALUE_TEST_COUNT] : 1.0;
    double v[JK_REP_VALUE_COUNT];

    for (int i = 0; i < JK_REP_VALUE_COUNT; i++) {
        v[i] = (double)values.v[i] / (double)test_count;
    }

    double seconds = v[JK_REP_VALUE_CPU_TIMER] / (double)frequency;
    printf("%s: %.0f (%.3f ms)", name, v[JK_REP_VALUE_CPU_TIMER], seconds * 1000.0);
    if (v[JK_REP_VALUE_BYTE_COUNT] > 0.0) {
        printf(" ");
        jk_print_bytes_double(stdout, "%.3f", (double)v[JK_REP_VALUE_BYTE_COUNT] / seconds);
        printf("/s");
    }
    if (v[JK_REP_VALUE_PAGE_FAULT_COUNT] > 0.0) {
        printf(" %.0f page faults (", v[JK_REP_VALUE_PAGE_FAULT_COUNT]);
        jk_print_bytes_double(
                stdout, "%.3f", v[JK_REP_VALUE_BYTE_COUNT] / v[JK_REP_VALUE_PAGE_FAULT_COUNT]);
        printf("/fault)");
    }
}

JK_PUBLIC b32 jk_platform_repetition_test_running(JkPlatformRepetitionTest *test)
{
    if (test->state != JK_REPETITION_TEST_RUNNING) {
        return 0;
    }
    if (test->block_open_count != test->block_close_count) {
        jk_platform_repetition_test_error(test,
                "JkPlatformRepetitionTest: jk_platform_repetition_test_time_begin calls not "
                "matched "
                "one-to-one with "
                "jk_platform_repetition_test_time_end calls\n");
        return 0;
    }

    uint64_t current_time = jk_platform_cpu_timer_get();
    if (test->block_open_count > 0) {
        if (test->current.v[JK_REP_VALUE_BYTE_COUNT] != test->target_byte_count) {
            jk_platform_repetition_test_error(test,
                    "JkPlatformRepetitionTest: Counted a different number of bytes than "
                    "target_byte_count\n");
            return 0;
        }

        test->total.v[JK_REP_VALUE_TEST_COUNT]++;
        for (int i = 0; i < JK_REP_VALUE_COUNT; i++) {
            test->total.v[i] += test->current.v[i];
        }
        if (test->current.v[JK_REP_VALUE_CPU_TIMER] < test->min.v[JK_REP_VALUE_CPU_TIMER]) {
            test->min = test->current;
            test->last_found_min_time = current_time;
            printf("\r                                                                             "
                   "          \r");
            jk_platform_repetition_test_print_values("Min", test->min, test->frequency);
        }
        if (test->current.v[JK_REP_VALUE_CPU_TIMER] > test->max.v[JK_REP_VALUE_CPU_TIMER]) {
            test->max = test->current;
        }
    }

    test->current = (JkPlatformRepValues){0};
    test->block_open_count = 0;
    test->block_close_count = 0;

    if (current_time - test->last_found_min_time > test->try_for_clocks) {
        test->state = JK_REPETITION_TEST_COMPLETE;

        // Print results
        if (test->total.v[JK_REP_VALUE_TEST_COUNT]) {
            printf("\r                                                                             "
                   "          \r");
            jk_platform_repetition_test_print_values("Min", test->min, test->frequency);
            printf("\n");
            jk_platform_repetition_test_print_values("Max", test->max, test->frequency);
            printf("\n");
            jk_platform_repetition_test_print_values("Avg", test->total, test->frequency);
            printf("\n");
        }
    }

    return test->state == JK_REPETITION_TEST_RUNNING;
}

JK_PUBLIC void jk_platform_repetition_test_count_bytes(
        JkPlatformRepetitionTest *test, uint64_t bytes)
{
    test->current.v[JK_REP_VALUE_BYTE_COUNT] += bytes;
}

JK_PUBLIC void jk_platform_repetition_test_error(JkPlatformRepetitionTest *test, char *message)
{
    test->state = JK_REPETITION_TEST_ERROR;
    fprintf(stderr, "%s\n", message);
}

// ---- Repetition test end ----------------------------------------------------

JK_PUBLIC size_t jk_platform_page_size_round_up(size_t n)
{
    size_t page_size = jk_platform_page_size();
    return (n + page_size - 1) & ~(page_size - 1);
}

JK_PUBLIC size_t jk_platform_page_size_round_down(size_t n)
{
    size_t page_size = jk_platform_page_size();
    return n & ~(page_size - 1);
}

JK_PUBLIC JkBuffer jk_platform_file_read_full(char *file_name, JkPlatformArena *storage)
{
    JK_PLATFORM_PROFILE_ZONE_TIME_BEGIN(jk_platform_file_read_full);

    FILE *file = fopen(file_name, "rb");
    if (!file) {
        JK_PLATFORM_PROFILE_ZONE_END(jk_platform_file_read_full);
        fprintf(stderr,
                "jk_platform_file_read_full: Failed to open file '%s': %s\n",
                file_name,
                strerror(errno));
        exit(1);
    }

    JkBuffer buffer = {.size = jk_platform_file_size(file_name)};
    buffer.data = jk_platform_arena_push(storage, buffer.size);
    if (!buffer.data) {
        JK_PLATFORM_PROFILE_ZONE_END(jk_platform_file_read_full);
        fprintf(stderr,
                "jk_platform_file_read_full: Failed to allocate memory for file '%s'\n",
                file_name);
        exit(1);
    }

    JK_PLATFORM_PROFILE_ZONE_BANDWIDTH_BEGIN(fread, buffer.size);
    if (fread(buffer.data, buffer.size, 1, file) != 1) {
        JK_PLATFORM_PROFILE_ZONE_END(fread);
        JK_PLATFORM_PROFILE_ZONE_END(jk_platform_file_read_full);
        fprintf(stderr, "jk_platform_file_read_full: fread failed\n");
        exit(1);
    }
    JK_PLATFORM_PROFILE_ZONE_END(fread);

    fclose(file);
    JK_PLATFORM_PROFILE_ZONE_END(jk_platform_file_read_full);
    return buffer;
}

JK_PUBLIC uint64_t jk_platform_cpu_timer_frequency_estimate(uint64_t milliseconds_to_wait)
{
    uint64_t os_freq = jk_platform_os_timer_frequency();
    uint64_t os_wait_time = os_freq * milliseconds_to_wait / 1000;

    uint64_t os_end = 0;
    uint64_t os_elapsed = 0;
    uint64_t cpu_start = jk_platform_cpu_timer_get();
    uint64_t os_start = jk_platform_os_timer_get();
    while (os_elapsed < os_wait_time) {
        os_end = jk_platform_os_timer_get();
        os_elapsed = os_end - os_start;
    }

    uint64_t cpu_end = jk_platform_cpu_timer_get();
    uint64_t cpu_elapsed = cpu_end - cpu_start;

    return os_freq * cpu_elapsed / os_elapsed;
}
