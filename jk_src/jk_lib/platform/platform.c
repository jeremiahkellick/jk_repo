#include <errno.h>
#include <math.h>
#include <stdarg.h>
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
    HANDLE process;
} JkPlatformData;

static JkPlatformData jk_platform_globals;

static void jk_platform_globals_init(void)
{
    jk_platform_globals.initialized = 1;
    jk_platform_globals.process = OpenProcess(PROCESS_QUERY_INFORMATION, 0, GetCurrentProcessId());
}

#define JK_PLATFORMS_ENSURE_GLOBALS_INIT()      \
    do {                                        \
        if (!jk_platform_globals.initialized) { \
            jk_platform_globals_init();         \
        }                                       \
    } while (0)

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

static PROCESS_MEMORY_COUNTERS jk_process_memory_info_get(void)
{
    JK_PLATFORMS_ENSURE_GLOBALS_INIT();

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
                    jk_platform_globals.process, &memory_counters, sizeof(memory_counters))) {
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

JK_PUBLIC void jk_platform_set_working_directory_to_executable_directory(void)
{
    // Load executable file name into buffer
    char buffer[MAX_PATH];
    DWORD file_name_length = GetModuleFileNameA(0, buffer, MAX_PATH);
    if (file_name_length <= 0) {
        OutputDebugStringA("Failed to find the path of this executable\n");
    }

    // Truncate file name at last component to convert it the containing directory name
    uint64_t last_slash = 0;
    for (uint64_t i = 0; buffer[i]; i++) {
        if (buffer[i] == '/' || buffer[i] == '\\') {
            last_slash = i;
        }
    }
    buffer[last_slash + 1] = '\0';

    if (!SetCurrentDirectoryA(buffer)) {
        OutputDebugStringA("Failed to set the working directory\n");
    }
}

static void jk_windows_print_last_error(void)
{
    DWORD error_code = GetLastError();
    if (error_code == 0) {
        fprintf(stderr, "Unknown error\n");
    } else {
        char message_buf[MAX_PATH] = {'\0'};
        FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL,
                error_code,
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                message_buf,
                MAX_PATH - 1,
                NULL);
        fprintf(stderr, "%s", message_buf);
    }
}

JK_PUBLIC int jk_platform_exec(JkBufferArray command)
{
    static char command_buffer[4096];

    if (!command.count) {
        fprintf(stderr, "jk_platform_exec: Received an empty command\n");
        return 1;
    }

    STARTUPINFO si = {.cb = sizeof(si)};
    PROCESS_INFORMATION pi = {0};
    uint64_t string_i = 0;
    for (uint64_t args_i = 0; args_i < command.count; args_i++) {
        string_i += snprintf(&command_buffer[string_i],
                JK_ARRAY_COUNT(command_buffer) - string_i,
                jk_string_contains_whitespace(command.items[args_i]) ? "%s\"%.*s\"" : "%s%.*s",
                args_i == 0 ? "" : " ",
                (int)command.items[args_i].size,
                command.items[args_i].data);
        if (string_i >= JK_ARRAY_COUNT(command_buffer)) {
            fprintf(stderr, "jk_platform_exec: Insufficient buffer size\n");
            return 1;
        }
    }

    printf("%s\n", command_buffer);

    if (!CreateProcessA(NULL, command_buffer, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        fprintf(stderr,
                "jk_platform_exec: Could not run '%.*s': ",
                (int)command.items[0].size,
                command.items[0].data);
        jk_windows_print_last_error();
        return 1;
    }
    CloseHandle(pi.hThread);
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit_status;
    if (!GetExitCodeProcess(pi.hProcess, &exit_status)) {
        fprintf(stderr,
                "jk_platform_exec: Could not get exit status of '%.*s': ",
                (int)command.items[0].size,
                command.items[0].data);
        jk_windows_print_last_error();
        exit_status = 1;
    }
    CloseHandle(pi.hProcess);
    return (int)exit_status;
}

JK_PUBLIC void jk_platform_sleep(uint64_t milliseconds)
{
    Sleep(milliseconds);
}

JK_PUBLIC b32 jk_platform_ensure_directory_exists(char *directory_path)
{
    char buffer[MAX_PATH];

    size_t length = strlen(directory_path);
    size_t i = 0;
    if (directory_path[i] == '/') {
        i++; // Skip leading slash which indicates an absolute path
    }
    while (i < length) {
        while (i < length && directory_path[i] != '/') {
            i++;
        }
        memcpy(buffer, directory_path, i);
        buffer[i] = '\0';

        if (!CreateDirectoryA(buffer, 0) && GetLastError() != ERROR_ALREADY_EXISTS) {
            return 0;
        }

        i++;
    }

    return 1;
}

#else

#include <limits.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#if __APPLE__
#include <mach-o/dyld.h>
#include <mach/mach_time.h>
#endif

#if __linux__
#include <time.h>
#endif

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

JK_PUBLIC uint64_t jk_platform_page_fault_count_get(void)
{
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        return usage.ru_majflt + usage.ru_minflt;
    } else {
        return 0;
    }
}

#if __APPLE__

JK_PUBLIC uint64_t jk_platform_os_timer_get(void)
{
    return mach_absolute_time();
}

JK_PUBLIC uint64_t jk_platform_os_timer_frequency(void)
{
    mach_timebase_info_data_t timebase_info;
    JK_ASSERT(mach_timebase_info(&timebase_info) == KERN_SUCCESS);
    return (1000000000llu * timebase_info.denom) / timebase_info.numer;
}

#endif

#if __linux

JK_PUBLIC uint64_t jk_platform_os_timer_get(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000llu + (uint64_t)ts.tv_nsec;
}

JK_PUBLIC uint64_t jk_platform_os_timer_frequency(void)
{
    return 1000000000llu;
}

#endif

JK_PUBLIC void jk_platform_set_working_directory_to_executable_directory(void)
{
#if __APPLE__
    char path[PATH_MAX];
    uint32_t bufsize = PATH_MAX;
    JK_ASSERT(_NSGetExecutablePath(path, &bufsize) == 0);
    // Truncate path at last slash
    for (uint32_t i = bufsize - 1; 0 <= i; i--) {
        if (path[i] == '/') {
            path[i] = '\0';
            break;
        }
    }
#endif

    JK_ASSERT(chdir(path) == 0);
}

JK_PUBLIC b32 jk_platform_ensure_directory_exists(char *directory_path)
{
    char buffer[PATH_MAX];

    uint64_t length = strlen(directory_path);
    uint64_t i = 0;
    if (directory_path[i] == '/') {
        i++; // Skip leading slash which indicates an absolute path
    }
    while (i < length) {
        while (i < length && directory_path[i] != '/') {
            i++;
        }
        memcpy(buffer, directory_path, i);
        buffer[i] = '\0';

        if (mkdir(buffer, 0755) == -1 && errno != EEXIST) {
            fprintf(stderr,
                    "jk_platform_ensure_directory_exists: Failed to create \"%s\": %s\n",
                    buffer,
                    strerror(errno));
            return 0;
        }

        i++;
    }

    return 1;
}

#endif

// ---- OS functions end -------------------------------------------------------

// ---- ISA functions begin ----------------------------------------------------

#if __TINYC__

JK_PUBLIC uint64_t jk_platform_cpu_timer_get(void)
{
    uint64_t edx;
    uint64_t eax;
    __asm__ volatile("rdtsc" : "=d"(edx), "=a"(eax));
    return (edx << 32) | eax;
}

#else

#if defined(__x86_64__) || defined(_M_X64)

#ifdef _MSC_VER
#include <intrin.h>
#else
#include <x86intrin.h>
#endif

JK_PUBLIC uint64_t jk_platform_cpu_timer_get(void)
{
    return __rdtsc();
}

JK_PUBLIC double jk_platform_fma_64(double a, double b, double c)
{
    return _mm_cvtsd_f64(_mm_fmadd_sd(_mm_set_sd(a), _mm_set_sd(b), _mm_set_sd(c)));
}

#elif __arm64__

JK_PUBLIC uint64_t jk_platform_cpu_timer_get(void)
{
    uint64_t timebase;
    __asm__ volatile("mrs %0, CNTPCT_EL0" : "=r"(timebase));
    return timebase;
}

#else

_STATIC_ASSERT(0 && "Unknown ISA");

#endif

#endif

// ---- ISA functions end ------------------------------------------------------

// ---- Virtual arena begin ------------------------------------------------------------

static b32 jk_platform_arena_virtual_grow(JkArena *arena, uint64_t new_size)
{
    JkPlatformArenaVirtualRoot *root = (JkPlatformArenaVirtualRoot *)arena->root;
    new_size = jk_platform_page_size_round_up(new_size);
    if (root->virtual_size < new_size) {
        return 0;
    } else {
        uint64_t expansion_size = new_size - root->generic.memory.size;
        if (jk_platform_memory_commit(
                    root->generic.memory.data + root->generic.memory.size, expansion_size)) {
            root->generic.memory.size = new_size;
            return 1;
        } else {
            return 0;
        }
    }
}

JK_PUBLIC JkArena jk_platform_arena_virtual_init(
        JkPlatformArenaVirtualRoot *root, uint64_t virtual_size)
{
    uint64_t page_size = jk_platform_page_size();

    root->virtual_size = virtual_size;
    root->generic.memory.size = page_size;
    root->generic.memory.data = jk_platform_memory_reserve(virtual_size);
    if (!root->generic.memory.data) {
        return (JkArena){0};
    }
    if (!jk_platform_memory_commit(root->generic.memory.data, page_size)) {
        return (JkArena){0};
    }

    return (JkArena){
        .root = &root->generic,
        .grow = jk_platform_arena_virtual_grow,
    };
}

JK_PUBLIC void jk_platform_arena_virtual_release(JkPlatformArenaVirtualRoot *root)
{
    jk_platform_memory_free(root->generic.memory.data, root->generic.memory.size);
}

// ---- Virtual arena end --------------------------------------------------------------

// ---- Profile begin ----------------------------------------------------------

typedef struct JkPlatformProfile {
    uint64_t start;

    uint64_t frame_count;
    uint64_t frame_elapsed[JK_PLATFORM_PROFILE_FRAME_TYPE_COUNT];

#if !JK_PLATFORM_PROFILE_DISABLE
    size_t zone_count;
    JkPlatformProfileZone *zones[1024];
    JkPlatformProfileZone *zone_current;
    uint64_t zone_depth;
#endif
} JkPlatformProfile;

static JkPlatformProfile jk_platform_profile;

JK_PUBLIC void jk_platform_profile_frame_begin(void)
{
    if (!jk_platform_profile.frame_count) {
        jk_platform_profile.frame_elapsed[JK_PLATFORM_PROFILE_FRAME_MIN] = UINT64_MAX;
    }
    jk_platform_profile.frame_count++;
    jk_platform_profile.start = jk_platform_cpu_timer_get();
}

JK_PUBLIC void jk_platform_profile_frame_end(void)
{
    uint64_t elapsed = jk_platform_cpu_timer_get() - jk_platform_profile.start;

    jk_platform_profile.frame_elapsed[JK_PLATFORM_PROFILE_FRAME_TOTAL] += elapsed;

    b32 is_min = elapsed < jk_platform_profile.frame_elapsed[JK_PLATFORM_PROFILE_FRAME_MIN];
    if (is_min) {
        jk_platform_profile.frame_elapsed[JK_PLATFORM_PROFILE_FRAME_MIN] = elapsed;
    }

    b32 is_max = jk_platform_profile.frame_elapsed[JK_PLATFORM_PROFILE_FRAME_MAX] < elapsed;
    if (is_max) {
        jk_platform_profile.frame_elapsed[JK_PLATFORM_PROFILE_FRAME_MAX] = elapsed;
    }

#if !JK_PLATFORM_PROFILE_DISABLE
    for (uint64_t i = 0; i < jk_platform_profile.zone_count; i++) {
        JkPlatformProfileZone *zone = jk_platform_profile.zones[i];

        JkPlatformProfileZoneFrame current = zone->frames[JK_PLATFORM_PROFILE_FRAME_CURRENT];
        zone->frames[JK_PLATFORM_PROFILE_FRAME_CURRENT] = (JkPlatformProfileZoneFrame){0};

        for (JkPlatformProfileMetric metric = 0; metric < JK_PLATFORM_PROFILE_METRIC_COUNT;
                metric++) {
            zone->frames[JK_PLATFORM_PROFILE_FRAME_TOTAL].a[metric] += current.a[metric];
        }
        if (is_min) {
            zone->frames[JK_PLATFORM_PROFILE_FRAME_MIN] = current;
        }
        if (is_max) {
            zone->frames[JK_PLATFORM_PROFILE_FRAME_MAX] = current;
        }
    }
#endif
}

static void jk_platform_profile_frame_print(void (*print)(void *data, char *format, ...),
        void *data,
        char *name,
        JkPlatformProfileFrameType frame_index,
        double frequency,
        uint64_t frame_count)
{
    uint64_t total = jk_platform_profile.frame_elapsed[frame_index];
    print(data, "\n%s: %.4fms\n", name, 1000.0 * (double)total / (frequency * (double)frame_count));

#if !JK_PLATFORM_PROFILE_DISABLE
    for (size_t i = 0; i < jk_platform_profile.zone_count; i++) {
        JkPlatformProfileZone *zone = jk_platform_profile.zones[i];
        JkPlatformProfileZoneFrame *frame = zone->frames + frame_index;

        JK_DEBUG_ASSERT(zone->active_count == 0
                && "jk_platform_profile_zone_begin was called without a matching "
                   "jk_platform_profile_zone_end");

        for (uint64_t j = 0; j < frame->depth; j++) {
            print(data, "\t");
        }
        print(data,
                "\t%s[%llu]: %llu (%.2f%%",
                zone->name,
                (long long)(frame->hit_count / frame_count),
                (long long)(frame->elapsed_exclusive / frame_count),
                (double)frame->elapsed_exclusive / (double)total * 100.0);
        if (frame->elapsed_inclusive != frame->elapsed_exclusive) {
            print(data,
                    ", %.2f%% w/ children",
                    (double)frame->elapsed_inclusive / (double)total * 100.0);
        }
        print(data, ")");

        if (frame->byte_count) {
            double seconds = (double)frame->elapsed_inclusive / frequency;
            print(data, " ");
            jk_platform_print_bytes_uint64(stdout, "%.3f", frame->byte_count / frame_count);
            print(data, " at ");
            jk_platform_print_bytes_double(stdout, "%.3f", (double)frame->byte_count / seconds);
            print(data, "/s");
        }

        print(data, "\n");
    }
#endif
}

JK_PUBLIC void jk_platform_profile_print_custom(
        void (*print)(void *data, char *format, ...), void *data)
{
    uint64_t frequency = jk_platform_cpu_timer_frequency_estimate(100);
    print(data, "CPU frequency: %llu\n", frequency);

    if (jk_platform_profile.frame_count == 0) {
        print(data, "\nNo profile data was captured.\n");
    } else if (jk_platform_profile.frame_count == 1) {
        jk_platform_profile_frame_print(
                print, data, "Total time", JK_PLATFORM_PROFILE_FRAME_MIN, (double)frequency, 1);
    } else {
        jk_platform_profile_frame_print(
                print, data, "Min", JK_PLATFORM_PROFILE_FRAME_MIN, (double)frequency, 1);
        jk_platform_profile_frame_print(
                print, data, "Max", JK_PLATFORM_PROFILE_FRAME_MAX, (double)frequency, 1);
        jk_platform_profile_frame_print(print,
                data,
                "Average",
                JK_PLATFORM_PROFILE_FRAME_TOTAL,
                (double)frequency,
                jk_platform_profile.frame_count);
    }
}

JK_PUBLIC void jk_platform_profile_frame_end_and_print_custom(
        void (*print)(void *data, char *format, ...), void *data)
{
    jk_platform_profile_frame_end();
    jk_platform_profile_print_custom(print, data);
}

static void jk_platform_profile_printf(void *data, char *format, ...)
{
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

JK_PUBLIC void jk_platform_profile_print(void)
{
    jk_platform_profile_print_custom(jk_platform_profile_printf, 0);
}

JK_PUBLIC void jk_platform_profile_frame_end_and_print(void)
{
    jk_platform_profile_frame_end_and_print_custom(jk_platform_profile_printf, 0);
}

#if !JK_PLATFORM_PROFILE_DISABLE

JK_PUBLIC void jk_platform_profile_zone_begin(JkPlatformProfileTiming *timing,
        JkPlatformProfileZone *zone,
        char *name,
        uint64_t byte_count)
{
    JkPlatformProfileZoneFrame *frame = zone->frames + JK_PLATFORM_PROFILE_FRAME_CURRENT;

    if (!zone->seen) {
        zone->seen = 1;
        zone->name = name;
        frame->byte_count += byte_count;
        frame->depth = jk_platform_profile.zone_depth;
        jk_platform_profile.zones[jk_platform_profile.zone_count++] = zone;
        JK_DEBUG_ASSERT(
                jk_platform_profile.zone_count <= JK_ARRAY_COUNT(jk_platform_profile.zones));
    }

    timing->parent = jk_platform_profile.zone_current;
    jk_platform_profile.zone_current = zone;
    jk_platform_profile.zone_depth++;

    timing->saved_elapsed_inclusive = frame->elapsed_inclusive;

#if JK_BUILD_MODE != JK_RELEASE
    zone->active_count++;
    timing->zone = zone;
    timing->ended = 0;
#endif

    timing->start = jk_platform_cpu_timer_get();
    return;
}

JK_PUBLIC void jk_platform_profile_zone_end(JkPlatformProfileTiming *timing)
{
    uint64_t elapsed = jk_platform_cpu_timer_get() - timing->start;
    JkPlatformProfileZoneFrame *parent_frame =
            timing->parent->frames + JK_PLATFORM_PROFILE_FRAME_CURRENT;
    JkPlatformProfileZoneFrame *current_frame =
            jk_platform_profile.zone_current->frames + JK_PLATFORM_PROFILE_FRAME_CURRENT;

#if JK_BUILD_MODE != JK_RELEASE
    JK_ASSERT(!timing->ended
            && "jk_platform_profile_zone_end: Called multiple times for a single timing instance");
    timing->ended = 1;
    timing->zone->active_count--;
    JK_ASSERT(timing->zone->active_count >= 0
            && "jk_platform_profile_zone_end: Called more times than "
               "jk_platform_profile_zone_begin for some zone");
    JK_ASSERT(jk_platform_profile.zone_current == timing->zone
            && "jk_platform_profile_zone_end: Must end all child timings before ending their "
               "parent");
#endif

    if (timing->parent) {
        parent_frame->elapsed_exclusive -= elapsed;
    }
    current_frame->elapsed_exclusive += elapsed;
    current_frame->elapsed_inclusive = timing->saved_elapsed_inclusive + elapsed;
    current_frame->hit_count++;

    jk_platform_profile.zone_current = timing->parent;
    jk_platform_profile.zone_depth--;
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
        test->min.v[JK_PLATFORM_REPETITION_TEST_VALUE_CPU_TIME] = UINT64_MAX;
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
    test->current.v[JK_PLATFORM_REPETITION_TEST_VALUE_PAGE_FAULT_COUNT] -=
            jk_platform_page_fault_count_get();
    test->current.v[JK_PLATFORM_REPETITION_TEST_VALUE_CPU_TIME] -= jk_platform_cpu_timer_get();
}

JK_PUBLIC void jk_platform_repetition_test_time_end(JkPlatformRepetitionTest *test)
{
    test->current.v[JK_PLATFORM_REPETITION_TEST_VALUE_CPU_TIME] += jk_platform_cpu_timer_get();
    test->current.v[JK_PLATFORM_REPETITION_TEST_VALUE_PAGE_FAULT_COUNT] +=
            jk_platform_page_fault_count_get();
    test->block_close_count++;
}

JK_PUBLIC double jk_platform_repetition_test_bandwidth(
        JkPlatformRepetitionTestSample sample, uint64_t frequency)
{
    double seconds =
            (double)sample.v[JK_PLATFORM_REPETITION_TEST_VALUE_CPU_TIME] / (double)frequency;
    return (double)sample.v[JK_PLATFORM_REPETITION_TEST_VALUE_BYTE_COUNT] / seconds;
}

static void jk_platform_repetition_test_print_sample(JkPlatformRepetitionTest *test,
        char *name,
        JkPlatformRepetitionTestSampleType type,
        uint64_t frequency,
        JkPlatformRepetitionTest *baseline)
{
    JkPlatformRepetitionTestSample *sample = test->samples + type;

    uint64_t count = sample->count ? sample->count : 1;

    uint64_t cpu_time = sample->cpu_time / count;
    double seconds = cpu_time / (double)frequency;
    printf("%s: %llu (%.2f ms", name, (long long)cpu_time, seconds * 1000.0);
    if (baseline) {
        if (test == baseline) {
            printf(", baseline");
        } else {
            JkPlatformRepetitionTestSample *baseline_sample = baseline->samples + type;
            uint64_t baseline_count = baseline_sample->count ? baseline_sample->count : 1;
            uint64_t baseline_cpu_time = baseline_sample->cpu_time / baseline_count;
            if (cpu_time <= baseline_cpu_time) {
                printf(", %.2fx faster", (double)baseline_cpu_time / (double)cpu_time);
            } else {
                printf(", %.2fx slower", (double)cpu_time / (double)baseline_cpu_time);
            }
        }
    }
    printf(")");
    if (sample->byte_count) {
        printf(" ");
        jk_platform_print_bytes_double(
                stdout, "%.2f", ((double)sample->byte_count / (double)count) / seconds);
        printf("/s");
    }
    if (sample->page_fault_count) {
        printf(" %.2f page faults (", (double)sample->page_fault_count / (double)count);
        jk_platform_print_bytes_double(
                stdout, "%.2f", (double)sample->byte_count / (double)sample->page_fault_count);
        printf("/fault)");
    }
}

JK_PUBLIC b32 jk_platform_repetition_test_running_baseline(
        JkPlatformRepetitionTest *test, JkPlatformRepetitionTest *baseline)
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
        if (test->current.v[JK_PLATFORM_REPETITION_TEST_VALUE_BYTE_COUNT]
                != test->target_byte_count) {
            jk_platform_repetition_test_error(test,
                    "JkPlatformRepetitionTest: Counted a different number of bytes than "
                    "target_byte_count\n");
            return 0;
        }

        test->total.v[JK_PLATFORM_REPETITION_TEST_VALUE_COUNT]++;
        for (int i = 0; i < JK_PLATFORM_REPETITION_TEST_VALUE_TYPE_COUNT; i++) {
            test->total.v[i] += test->current.v[i];
        }
        if (test->current.v[JK_PLATFORM_REPETITION_TEST_VALUE_CPU_TIME]
                < test->min.v[JK_PLATFORM_REPETITION_TEST_VALUE_CPU_TIME]) {
            test->min = test->current;
            test->last_found_min_time = current_time;
            printf("\r                                                                             "
                   "          \r");
            jk_platform_repetition_test_print_sample(
                    test, "Min", JK_PLATFORM_REPETITION_TEST_SAMPLE_MIN, test->frequency, baseline);
        }
        if (test->current.v[JK_PLATFORM_REPETITION_TEST_VALUE_CPU_TIME]
                > test->max.v[JK_PLATFORM_REPETITION_TEST_VALUE_CPU_TIME]) {
            test->max = test->current;
        }
    }

    test->current = (JkPlatformRepetitionTestSample){0};
    test->block_open_count = 0;
    test->block_close_count = 0;

    if (current_time - test->last_found_min_time > test->try_for_clocks) {
        test->state = JK_REPETITION_TEST_COMPLETE;

        // Print results
        if (test->total.v[JK_PLATFORM_REPETITION_TEST_VALUE_COUNT]) {
            printf("\r                                                                             "
                   "          \r");
            jk_platform_repetition_test_print_sample(
                    test, "Min", JK_PLATFORM_REPETITION_TEST_SAMPLE_MIN, test->frequency, baseline);
            printf("\n");
            jk_platform_repetition_test_print_sample(
                    test, "Max", JK_PLATFORM_REPETITION_TEST_SAMPLE_MAX, test->frequency, baseline);
            printf("\n");
            jk_platform_repetition_test_print_sample(test,
                    "Avg",
                    JK_PLATFORM_REPETITION_TEST_SAMPLE_TOTAL,
                    test->frequency,
                    baseline);
            printf("\n");
        }
    }

    return test->state == JK_REPETITION_TEST_RUNNING;
}

JK_PUBLIC b32 jk_platform_repetition_test_running(JkPlatformRepetitionTest *test)
{
    return jk_platform_repetition_test_running_baseline(test, 0);
}

JK_PUBLIC void jk_platform_repetition_test_count_bytes(
        JkPlatformRepetitionTest *test, uint64_t bytes)
{
    test->current.v[JK_PLATFORM_REPETITION_TEST_VALUE_BYTE_COUNT] += bytes;
}

JK_PUBLIC void jk_platform_repetition_test_error(JkPlatformRepetitionTest *test, char *message)
{
    test->state = JK_REPETITION_TEST_ERROR;
    fprintf(stderr, "%s\n", message);
}

// ---- Repetition test end ----------------------------------------------------

// ---- Command line arguments parsing begin -----------------------------------

static void jk_argv_swap_to_front(char **argv, char **arg)
{
    for (; arg > argv; arg--) {
        char *tmp = *arg;
        *arg = *(arg - 1);
        *(arg - 1) = tmp;
    }
}

JK_PUBLIC void jk_options_parse(int argc,
        char **argv,
        JkOption *options_in,
        JkOptionResult *options_out,
        size_t option_count,
        JkOptionsParseResult *result)
{
    b32 options_ended = 0;
    result->operands = &argv[argc];
    result->operand_count = 0;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] != '\0' && !options_ended) { // Option argument
            b32 i_plus_one_is_arg = 0;
            if (argv[i][1] == '-') {
                if (argv[i][2] == '\0') { // -- encountered
                    options_ended = 1;
                } else { // Double hyphen option
                    char *name = &argv[i][2];
                    int end = 0;
                    while (name[end] != '=' && name[end] != '\0') {
                        end++;
                    }
                    b32 matched = 0;
                    for (size_t j = 0; !matched && j < option_count; j++) {
                        if (options_in[j].long_name
                                && strncmp(name, options_in[j].long_name, end) == 0) {
                            matched = 1;
                            options_out[j].present = 1;

                            if (options_in[j].arg_name) {
                                if (name[end] == '=') {
                                    if (name[end + 1] != '\0') {
                                        options_out[j].arg = &name[end + 1];
                                    }
                                } else {
                                    i_plus_one_is_arg = 1;
                                    options_out[j].arg = argv[i + 1];
                                }
                            } else {
                                if (name[end] == '=') {
                                    fprintf(stderr,
                                            "%s: Error in '%s': Option does not accept an "
                                            "argument\n",
                                            argv[0],
                                            argv[i]);
                                    result->usage_error = 1;
                                }
                            }
                        }
                    }
                    if (!matched) {
                        fprintf(stderr, "%s: Invalid option '%s'\n", argv[0], argv[i]);
                        result->usage_error = 1;
                    }
                }
            } else { // Single-hypen option(s)
                b32 has_argument = 0;
                for (char *c = &argv[i][1]; *c != '\0' && !has_argument; c++) {
                    b32 matched = 0;
                    for (size_t j = 0; !matched && j < option_count; j++) {
                        if (*c == options_in[j].flag) {
                            matched = 1;
                            options_out[j].present = 1;
                            has_argument = options_in[j].arg_name != NULL;

                            if (has_argument) {
                                options_out[j].arg = ++c;
                                if (options_out[j].arg[0] == '\0') {
                                    i_plus_one_is_arg = 1;
                                    options_out[j].arg = argv[i + 1];
                                }
                            }
                        }
                    }
                    if (!matched) {
                        fprintf(stderr, "%s: Invalid option '%c' in '%s'\n", argv[0], *c, argv[i]);
                        result->usage_error = 1;
                        break;
                    }
                }
            }
            if (&argv[i] > result->operands) {
                jk_argv_swap_to_front(result->operands, &argv[i]);
                result->operands++;
            }
            if (i_plus_one_is_arg) {
                if (argv[i + 1]) {
                    i++;
                    if (&argv[i] > result->operands) {
                        jk_argv_swap_to_front(result->operands, &argv[i]);
                        result->operands++;
                    }
                } else {
                    fprintf(stderr,
                            "%s: Option '%s' missing required argument\n",
                            argv[0],
                            argv[i - 1]);
                    result->usage_error = 1;
                }
            }
        } else { // Regular argument
            result->operand_count++;
            if (&argv[i] < result->operands) {
                result->operands = &argv[i];
            }
        }
    }
}

JK_PUBLIC void jk_options_print_help(FILE *file, JkOption *options, int option_count)
{
    fprintf(file, "OPTIONS\n");
    for (int i = 0; i < option_count; i++) {
        if (i != 0) {
            fprintf(file, "\n");
        }
        printf("\t");
        if (options[i].flag) {
            fprintf(file,
                    "-%c%s%s",
                    options[i].flag,
                    options[i].arg_name ? " " : "",
                    options[i].arg_name ? options[i].arg_name : "");
        }
        if (options[i].long_name) {
            fprintf(file,
                    "%s--%s%s%s",
                    options[i].flag ? ", " : "",
                    options[i].long_name,
                    options[i].arg_name ? "=" : "",
                    options[i].arg_name ? options[i].arg_name : "");
        }
        fprintf(file, "%s", options[i].description);
    }
}

JK_PUBLIC double jk_parse_double(JkBuffer number_string)
{
    double significand_sign = 1.0;
    double significand = 0.0;
    double exponent_sign = 1.0;
    double exponent = 0.0;

    uint64_t pos = 0;
    int c = jk_buffer_character_next(number_string, &pos);

    if (c == '-') {
        significand_sign = -1.0;

        c = jk_buffer_character_next(number_string, &pos);

        if (!jk_char_is_digit(c)) {
            return NAN;
        }
    }

    // Parse integer
    do {
        significand = (significand * 10.0) + (c - '0');
    } while (jk_char_is_digit((c = jk_buffer_character_next(number_string, &pos))));

    // Parse fraction if there is one
    if (c == '.') {
        c = jk_buffer_character_next(number_string, &pos);

        if (!jk_char_is_digit(c)) {
            return NAN;
        }

        double multiplier = 0.1;
        do {
            significand += (c - '0') * multiplier;
            multiplier /= 10.0;
        } while (jk_char_is_digit((c = jk_buffer_character_next(number_string, &pos))));
    }

    // Parse exponent if there is one
    if (c == 'e' || c == 'E') {
        c = jk_buffer_character_next(number_string, &pos);

        if ((c == '-' || c == '+')) {
            if (c == '-') {
                exponent_sign = -1.0;
            }
            c = jk_buffer_character_next(number_string, &pos);
        }

        if (!jk_char_is_digit(c)) {
            return NAN;
        }

        do {
            exponent = (exponent * 10.0) + (c - '0');
        } while (jk_char_is_digit((c = jk_buffer_character_next(number_string, &pos))));
    }

    return significand_sign * significand * pow(10.0, exponent_sign * exponent);
}

// ---- Command line arguments parsing end -------------------------------------

// ---- File formats begin -----------------------------------------------------

JK_PUBLIC b32 jk_riff_chunk_valid(JkRiffChunkMain *chunk_main, JkRiffChunk *chunk)
{
    return ((uint8_t *)chunk - (uint8_t *)&chunk_main->form_type) < chunk_main->size;
}

JK_PUBLIC JkRiffChunk *jk_riff_chunk_next(JkRiffChunk *chunk)
{
    return (JkRiffChunk *)(chunk->data + ((chunk->size + 1) & ~1));
}

// ---- File formats end -------------------------------------------------------

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

JK_PUBLIC JkBuffer jk_platform_file_read_full(JkArena *arena, char *file_name)
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
    buffer.data = jk_arena_push(arena, buffer.size);
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

JK_PUBLIC JkBufferArray jk_platform_file_read_lines(JkArena *arena, char *file_name)
{
    JkBuffer file = jk_platform_file_read_full(arena, file_name);
    JkBufferArray lines = {.items = jk_arena_pointer_current(arena)};

    uint64_t start = 0;
    uint64_t i = 0;
    for (; i < file.size; i++) {
        if (file.data[i] == '\n') {
            JkBuffer *line = jk_arena_push(arena, sizeof(*line));
            if (!line) {
                goto end;
            }
            line->data = file.data + start;
            line->size = i - start;
            start = i + 1;
        }
    }
    if (start < i) {
        JkBuffer *line = jk_arena_push(arena, sizeof(*line));
        if (!line) {
            goto end;
        }
        line->data = file.data + start;
        line->size = i - start;
    }

end:
    lines.count = (JkBuffer *)jk_arena_pointer_current(arena) - lines.items;
    return lines;
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

JK_PUBLIC void jk_platform_print_bytes_uint64(FILE *file, char *format, uint64_t byte_count)
{
    if (byte_count < 1024) {
        fprintf(file, "%llu bytes", (long long)byte_count);
    } else if (byte_count < 1024 * 1024) {
        fprintf(file, format, (double)byte_count / 1024.0);
        fprintf(file, " KiB");
    } else if (byte_count < 1024 * 1024 * 1024) {
        fprintf(file, format, (double)byte_count / (1024.0 * 1024.0));
        fprintf(file, " MiB");
    } else {
        fprintf(file, format, (double)byte_count / (1024.0 * 1024.0 * 1024.0));
        fprintf(file, " GiB");
    }
}

JK_PUBLIC void jk_platform_print_bytes_double(FILE *file, char *format, double byte_count)
{
    if (byte_count < 1024.0) {
        fprintf(file, "%.0f bytes", byte_count);
    } else if (byte_count < 1024.0 * 1024.0) {
        fprintf(file, format, byte_count / 1024.0);
        fprintf(file, " KiB");
    } else if (byte_count < 1024.0 * 1024.0 * 1024.0) {
        fprintf(file, format, byte_count / (1024.0 * 1024.0));
        fprintf(file, " MiB");
    } else {
        fprintf(file, format, byte_count / (1024.0 * 1024.0 * 1024.0));
        fprintf(file, " GiB");
    }
}
