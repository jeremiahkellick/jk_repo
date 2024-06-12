#include <assert.h>

#include "platform.h"

// OS functions
#ifdef _WIN32

#include <stdio.h>
#include <sys/stat.h>
#include <windows.h>

#include <psapi.h>

typedef struct JkPlatformData {
    bool initialized;
    uint64_t large_page_size;
    HANDLE process;
} JkPlatformData;

static JkPlatformData jk_platform_data;

JK_PUBLIC void jk_platform_init(void)
{
    assert(!jk_platform_data.initialized);
    jk_platform_data.initialized = true;
    jk_platform_data.large_page_size = jk_platform_large_pages_try_enable();
    jk_platform_data.process = OpenProcess(PROCESS_QUERY_INFORMATION, false, GetCurrentProcessId());
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
    static bool has_been_called = false;
    assert(!has_been_called);
    has_been_called = true;

    uint64_t result = 0;

    HANDLE process_token;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &process_token)) {
        TOKEN_PRIVILEGES privileges = {0};
        privileges.PrivilegeCount = 1;
        privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        if (LookupPrivilegeValue(0, SE_LOCK_MEMORY_NAME, &privileges.Privileges[0].Luid)) {
            AdjustTokenPrivileges(process_token, false, &privileges, 0, 0, 0);
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

JK_PUBLIC bool jk_platform_memory_commit(void *address, size_t size)
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

JK_PUBLIC uint64_t jk_platform_page_fault_count_get(void)
{
    assert(jk_platform_data.initialized);
    PROCESS_MEMORY_COUNTERS memory_counters = {.cb = sizeof(memory_counters)};
    GetProcessMemoryInfo(jk_platform_data.process, &memory_counters, sizeof(memory_counters));
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

JK_PUBLIC bool jk_platform_memory_commit(void *address, size_t size)
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
    bool initialized;
} JkPlatformOsMetrics;

static JkPlatformOsMetrics jk_platform_data;

JK_PUBLIC void jk_platform_init(void)
{
    assert(!jk_platform_data.initialized);
    jk_platform_data.initialized = true;
}

JK_PUBLIC uint64_t jk_platform_page_fault_count_get(void)
{
    assert(jk_platform_data.initialized);
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

// Compiler functions
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
