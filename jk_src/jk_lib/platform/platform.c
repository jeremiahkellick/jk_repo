#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// #jk_build dependencies_begin
#include <jk_src/jk_lib/jk_lib.h>
// #jk_build dependencies_end

#include "platform.h"

static int32_t jk_platform_init_common(int32_t argc, char **argv)
{
    jk_platform_thread_init();
    return jk_platform_entry_point(argc, argv);
}

// ---- OS functions begin -----------------------------------------------------

#ifdef _WIN32

#include <windows.h>

#define INITGUID // Causes definition of SystemTraceControlGuid in evntrace.h
#include <dbghelp.h>
#include <sys/stat.h>

typedef BOOL JkPlatformWindowsDpiFunction(DPI_AWARENESS_CONTEXT value);

#if JK_PLATFORM_DESKTOP_APP

JK_GLOBAL_DEFINE HINSTANCE jk_platform_hinstance;

int32_t WinMain(HINSTANCE instance, HINSTANCE prev_instance, LPSTR command_line, int show_code)
{
    jk_platform_hinstance = instance;
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    jk_platform_set_working_directory_to_executable_directory();
    return jk_platform_init_common(__argc, __argv);
}

JK_PUBLIC void jk_platform_print(JkBuffer string)
{
    if (0 < string.size) {
        JkArena scratch = jk_arena_scratch_get();
        OutputDebugStringA(jk_buffer_to_null_terminated(&scratch, string));
    }
}

#else

int32_t main(int32_t argc, char **argv)
{
    SetConsoleOutputCP(CP_UTF8);
    return jk_platform_init_common(argc, argv);
}

JK_PUBLIC void jk_platform_print(JkBuffer string)
{
    if (0 < string.size) {
        fwrite(string.data, 1, string.size, stdout);
    }
}

#endif

JK_PUBLIC int64_t jk_platform_file_size(char *file_name)
{
    struct __stat64 info = {0};
    if (_stat64(file_name, &info)) {
        fprintf(stderr, "jk_platform_file_size: stat returned an error\n");
        return 0;
    }
    return (int64_t)info.st_size;
}

JK_PUBLIC int64_t jk_platform_page_size(void)
{
    return 4096;
}

JK_PUBLIC JkBuffer jk_platform_memory_alloc(JkAllocType type, int64_t size)
{
    static DWORD flAllocationType[JK_ALLOC_TYPE_COUNT] = {
        /* JK_ALLOC_RESERVE = */ MEM_RESERVE,
        /* JK_ALLOC_COMMIT = */ MEM_COMMIT | MEM_RESERVE,
    };
    static DWORD flProtect[JK_ALLOC_TYPE_COUNT] = {
        /* JK_ALLOC_RESERVE = */ PAGE_NOACCESS,
        /* JK_ALLOC_COMMIT = */ PAGE_READWRITE,
    };

    JkBuffer result = {.size = size};
    if (0 < size) {
        result.data = VirtualAlloc(NULL, size, flAllocationType[type], flProtect[type]);
        if (!result.data) {
            jk_log(JK_LOG_ERROR, JKS("Failed to allocate memory"));
            result.size = -1;
        }
    }
    return result;
}

JK_PUBLIC b32 jk_platform_memory_commit(void *address, int64_t size)
{
    if (0 < size) {
        if (VirtualAlloc(address, size, MEM_COMMIT, PAGE_READWRITE)) {
            return 1;
        } else {
            jk_log(JK_LOG_ERROR, JKS("Failed to commit memory"));
            return 0;
        }
    } else {
        return 1;
    }
}

JK_PUBLIC void jk_platform_memory_free(JkBuffer memory)
{
    if (0 < memory.size) {
        // TODO: Consider how to deal with different freeing behavior between Windows and Unix
        VirtualFree(memory.data, 0, MEM_RELEASE);
    }
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
    static uint8_t initialized;
    static HANDLE process;
    static HINSTANCE library;
    static GetProcessMemoryInfoPointer GetProcessMemoryInfo;
    if (!initialized) {
        initialized = TRUE;
        process = OpenProcess(PROCESS_QUERY_INFORMATION, 0, GetCurrentProcessId());
        library = LoadLibraryA("psapi.dll");
        if (library) {
            GetProcessMemoryInfo =
                    (GetProcessMemoryInfoPointer)GetProcAddress(library, "GetProcessMemoryInfo");
        } else {
            // TODO: log error
        }
    }

    PROCESS_MEMORY_COUNTERS memory_counters = {.cb = JK_SIZEOF(memory_counters)};
    if (GetProcessMemoryInfo) {
        if (!GetProcessMemoryInfo(process, &memory_counters, JK_SIZEOF(memory_counters))) {
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
    return memory_counters.PageFaultCount;
}

JK_PUBLIC uint64_t jk_platform_os_timer_get(void)
{
    LARGE_INTEGER value;
    QueryPerformanceCounter(&value);
    return value.QuadPart;
}

JK_PUBLIC int64_t jk_platform_os_timer_frequency(void)
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
    int64_t last_slash = 0;
    for (int64_t i = 0; buffer[i]; i++) {
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

    STARTUPINFO si = {.cb = JK_SIZEOF(si)};
    PROCESS_INFORMATION pi = {0};
    int64_t string_i = 0;
    for (int64_t args_i = 0; args_i < command.count; args_i++) {
        string_i += snprintf(&command_buffer[string_i],
                JK_ARRAY_COUNT(command_buffer) - string_i,
                jk_string_contains_whitespace(command.e[args_i]) ? "%s\"%.*s\"" : "%s%.*s",
                args_i == 0 ? "" : " ",
                (int)command.e[args_i].size,
                command.e[args_i].data);
        if (string_i >= JK_ARRAY_COUNT(command_buffer)) {
            fprintf(stderr, "jk_platform_exec: Insufficient buffer size\n");
            return 1;
        }
    }

    printf("%s\n", command_buffer);

    if (!CreateProcessA(NULL, command_buffer, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        fprintf(stderr,
                "jk_platform_exec: Could not run '%.*s': ",
                (int)command.e[0].size,
                command.e[0].data);
        jk_windows_print_last_error();
        return 1;
    }
    CloseHandle(pi.hThread);
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit_status;
    if (!GetExitCodeProcess(pi.hProcess, &exit_status)) {
        fprintf(stderr,
                "jk_platform_exec: Could not get exit status of '%.*s': ",
                (int)command.e[0].size,
                command.e[0].data);
        jk_windows_print_last_error();
        exit_status = 1;
    }
    CloseHandle(pi.hProcess);
    return (int)exit_status;
}

JK_PUBLIC void jk_platform_sleep(int64_t milliseconds)
{
    Sleep(milliseconds);
}

JK_PUBLIC b32 jk_platform_ensure_directory_exists(char *directory_path)
{
    char buffer[MAX_PATH];

    int64_t length = strlen(directory_path);
    int64_t i = 0;
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

JK_PUBLIC b32 jk_platform_create_directory(JkBuffer path)
{
    char buffer[MAX_PATH];

    int64_t i = 0;
    if (path.data[i] == '/') {
        i++; // Skip leading slash which indicates an absolute path
    }
    while (i < path.size) {
        while (i < path.size && path.data[i] != '/') {
            i++;
        }
        memcpy(buffer, path.data, i);
        buffer[i] = '\0';

        if (!CreateDirectoryA(buffer, 0) && GetLastError() != ERROR_ALREADY_EXISTS) {
            return 0;
        }

        i++;
    }

    return 1;
}

JK_PUBLIC JK_NOINLINE JkBuffer jk_platform_stack_trace(
        JkBuffer buffer, int64_t skip, int64_t indent)
{
    static SRWLOCK lock = SRWLOCK_INIT;
    static uint8_t module_buffer[1024];

    AcquireSRWLockExclusive(&lock);

    JkBuffer result = {.size = 0, .data = buffer.data};

    HANDLE process = GetCurrentProcess();
    HANDLE thread = GetCurrentThread();

    if (SymInitialize(process, NULL, TRUE)) {
        SymSetOptions(SYMOPT_LOAD_LINES);

        CONTEXT context = {0};
        context.ContextFlags = CONTEXT_FULL;
        RtlCaptureContext(&context);

        STACKFRAME frame = {0};
        frame.AddrPC.Offset = context.Rip;
        frame.AddrPC.Mode = AddrModeFlat;
        frame.AddrFrame.Offset = context.Rbp;
        frame.AddrFrame.Mode = AddrModeFlat;
        frame.AddrStack.Offset = context.Rsp;
        frame.AddrStack.Mode = AddrModeFlat;

        for (int64_t frame_num = 0; result.size < buffer.size
                && StackWalk(IMAGE_FILE_MACHINE_AMD64,
                        process,
                        thread,
                        &frame,
                        &context,
                        NULL,
                        SymFunctionTableAccess,
                        SymGetModuleBase,
                        NULL);
                frame_num++) {
            if (frame_num < skip) {
                continue;
            }
            uint64_t offset = frame.AddrPC.Offset;

            JkBuffer module_name = {0};
            uint64_t module_base = SymGetModuleBase(process, offset);
            if (module_base) {
                uint64_t length = GetModuleFileNameA((HINSTANCE)module_base,
                        (char *)module_buffer,
                        JK_ARRAY_COUNT(module_buffer));
                if (length) {
                    module_name = jk_path_basename(
                            (JkBuffer){.size = (int64_t)length, .data = module_buffer});
                }
            }

            JkBuffer function_name = JKSI("Unknown function");
            char symbolBuffer[sizeof(IMAGEHLP_SYMBOL) + 255];
            PIMAGEHLP_SYMBOL symbol = (PIMAGEHLP_SYMBOL)symbolBuffer;
            symbol->SizeOfStruct = sizeof(IMAGEHLP_SYMBOL) + 255;
            symbol->MaxNameLength = 254;
            DWORD64 not_used_0;
            if (SymGetSymFromAddr(process, offset, &not_used_0, symbol)) {
                function_name.size = 0;
                while (symbol->Name[function_name.size]
                        && function_name.size < symbol->MaxNameLength) {
                    function_name.size++;
                }
                function_name.data = (uint8_t *)symbol->Name;
            }

            IMAGEHLP_LINE line_data;
            line_data.SizeOfStruct = sizeof(IMAGEHLP_LINE);

            JkBuffer file_name = {0};
            uint64_t line = 0;
            DWORD not_used_1;
            if (SymGetLineFromAddr(process, offset, &not_used_1, &line_data)) {
                while (line_data.FileName[file_name.size] && file_name.size < 1024) {
                    file_name.size++;
                }
                file_name.data = (uint8_t *)line_data.FileName;
                line = line_data.LineNumber;
            }

            // Indent
            for (int64_t i = 0; i < indent && result.size < buffer.size; i++, result.size++) {
                result.data[result.size] = ' ';
            }

            result.size += snprintf((char *)(result.data + result.size),
                    buffer.size - result.size,
                    "0x%012llx in %.*s",
                    offset,
                    (int)function_name.size,
                    function_name.data);
            if (file_name.size) {
                result.size += snprintf((char *)(result.data + result.size),
                        buffer.size - result.size,
                        " at %.*s:%lld\n",
                        (int)file_name.size,
                        file_name.data,
                        line);
            } else {
                result.size += snprintf((char *)(result.data + result.size),
                        buffer.size - result.size,
                        " from %.*s\n",
                        (int)module_name.size,
                        module_name.data);
            }

            if (jk_buffer_compare(function_name, JKS("main")) == 0
                    || jk_buffer_compare(function_name, JKS("WinMain")) == 0) {
                break;
            }
        }
        SymCleanup(process);
    }

    ReleaseSRWLockExclusive(&lock);

    return result;
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

int32_t main(int32_t argc, char **argv)
{
    return jk_platform_init_common(argc, argv);
}

JK_PUBLIC int64_t jk_platform_file_size(char *file_name)
{
    struct stat stat_struct = {0};
    if (stat(file_name, &stat_struct)) {
        fprintf(stderr, "jk_platform_file_size: stat returned an error\n");
        return 0;
    }
    return (int64_t)stat_struct.st_size;
}

JK_PUBLIC int64_t jk_platform_page_size(void)
{
    static int64_t page_size = 0;
    if (page_size == 0) {
        page_size = getpagesize();
    }
    return page_size;
}

JK_PUBLIC JkBuffer jk_platform_memory_alloc(JkAllocType type, int64_t size)
{
    static int prot[JK_ALLOC_TYPE_COUNT] = {
        /* JK_ALLOC_RESERVE = */ PROT_NONE,
        /* JK_ALLOC_COMMIT = */ PROT_READ | PROT_WRITE,
    };

    JkBuffer result = {.size = size};
    if (0 < size) {
        result.data = mmap(NULL, size, prot[type], MAP_PRIVATE | MAP_ANON, -1, 0);
        if (!result.data) {
            jk_log(JK_LOG_ERROR, JKS("Failed to allocate memory"));
            result.size = -1;
        }
    }
    return result;
}

JK_PUBLIC b32 jk_platform_memory_commit(void *address, int64_t size)
{
    if (0 < size) {
        if (mprotect(address, size, PROT_READ | PROT_WRITE)) {
            jk_log(JK_LOG_ERROR, JKS("Failed to commit memory"));
            return 0;
        } else {
            return 1;
        }
    } else {
        return 1;
    }
}

JK_PUBLIC void jk_platform_memory_free(JkBuffer memory)
{
    if (0 < memory.size) {
        // TODO: Consider how to deal with different freeing behavior between Windows and Unix
        munmap(memory.data, memory.size);
    }
}

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

JK_PUBLIC int64_t jk_platform_os_timer_frequency(void)
{
    mach_timebase_info_data_t timebase_info;
    JK_ASSERT(mach_timebase_info(&timebase_info) == KERN_SUCCESS);
    return (1000000000ll * timebase_info.denom) / timebase_info.numer;
}

#endif

#if __linux

JK_PUBLIC uint64_t jk_platform_os_timer_get(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ll + (uint64_t)ts.tv_nsec;
}

JK_PUBLIC int64_t jk_platform_os_timer_frequency(void)
{
    return 1000000000ll;
}

#endif

JK_PUBLIC void jk_platform_set_working_directory_to_executable_directory(void)
{
#if __APPLE__
    char path[PATH_MAX];
    uint32_t bufsize = PATH_MAX;
    JK_ASSERT(_NSGetExecutablePath(path, &bufsize) == 0);

    // Truncate path at last slash
    int64_t length = 0;
    for (int64_t i = 0; i < bufsize; i++) {
        if (path[i] == '\0') {
            break;
        }
        length++;
    }
    for (int64_t i = length - 1; 0 <= i; i--) {
        if (path[i] == '/') {
            path[i] = '\0';
            break;
        }
    }

    if (chdir(path) != 0) {
        fprintf(stderr, "chdir(\"%s\"): %s\n", path, strerror(errno));
        JK_ASSERT(0);
    }
#elif __linux__
    char path[PATH_MAX];
    int64_t len = readlink("/proc/self/exe", path, PATH_MAX - 1);
    JK_ASSERT(len != -1);

    // Truncate path at last slash
    for (int64_t i = len - 1; 0 <= i; i--) {
        if (path[i] == '/') {
            path[i] = '\0';
            break;
        }
    }

    if (chdir(path) != 0) {
        fprintf(stderr, "chdir(\"%s\"): %s\n", path, strerror(errno));
        JK_ASSERT(0);
    }
#else
    JK_ASSERT(0 && "Not implemented");
#endif
}

JK_PUBLIC b32 jk_platform_ensure_directory_exists(char *directory_path)
{
    char buffer[PATH_MAX];

    int64_t length = jk_strlen(directory_path);
    int64_t i = 0;
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

JK_PUBLIC b32 jk_platform_create_directory(JkBuffer path)
{
    char buffer[PATH_MAX];

    int64_t i = 0;
    if (path.data[i] == '/') {
        i++; // Skip leading slash which indicates an absolute path
    }
    while (i < path.size) {
        while (i < path.size && path.data[i] != '/') {
            i++;
        }
        memcpy(buffer, path.data, i);
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

JK_PUBLIC JkBuffer jk_platform_stack_trace(JkBuffer buffer, int64_t skip, int64_t indent)
{
    return (JkBuffer){0};
}

JK_PUBLIC void jk_platform_print(JkBuffer string)
{
    if (0 < string.size) {
        fwrite(string.data, 1, string.size, stdout);
    }
}

#endif

// ---- OS functions end -------------------------------------------------------

// ---- ISA functions begin ----------------------------------------------------

#if defined(__x86_64__) || defined(_M_X64)

#ifdef _MSC_VER
#include <intrin.h>
#else
#include <x86intrin.h>
#endif

JK_PUBLIC double jk_platform_fma_64(double a, double b, double c)
{
    return _mm_cvtsd_f64(_mm_fmadd_sd(_mm_set_sd(a), _mm_set_sd(b), _mm_set_sd(c)));
}

#elif __arm64__

#endif

// ---- ISA functions end ------------------------------------------------------

// ---- Virtual arena begin ------------------------------------------------------------

static b32 jk_platform_arena_virtual_grow(JkArena *arena, int64_t new_size)
{
    JkPlatformArenaVirtualRoot *root = (JkPlatformArenaVirtualRoot *)arena->root;
    new_size = jk_platform_page_size_round_up(new_size);
    if (!(root->generic.memory.size <= new_size && new_size <= root->virtual_size)) {
        return 0;
    } else {
        int64_t expansion_size = new_size - root->generic.memory.size;
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
        JkPlatformArenaVirtualRoot *root, int64_t virtual_size)
{
    JkArena result = {0};
    jk_memset(root, 0, sizeof(*root));

    JkBuffer reserved = jk_platform_memory_alloc(JK_ALLOC_RESERVE, virtual_size);
    if (reserved.size == virtual_size) {
        int64_t page_size = jk_platform_page_size();
        if (jk_platform_memory_commit(reserved.data, page_size)) {
            root->generic.memory.size = page_size;
            root->generic.memory.data = reserved.data;
            root->generic.grow = jk_platform_arena_virtual_grow;
            root->virtual_size = virtual_size;
            result.root = &root->generic;
        } else {
            jk_platform_memory_free(reserved);
        }
    }

    return result;
}

JK_PUBLIC void jk_platform_arena_virtual_release(JkPlatformArenaVirtualRoot *root)
{
    jk_platform_memory_free(
            (JkBuffer){.size = root->virtual_size, .data = root->generic.memory.data});
}

// ---- Virtual arena end --------------------------------------------------------------

// ---- Repetition test begin --------------------------------------------------

JK_PUBLIC void jk_platform_repetition_test_run_wave(JkPlatformRepetitionTest *test,
        int64_t target_byte_count,
        int64_t frequency,
        int64_t try_for_seconds)
{
    if (test->state == JK_REPETITION_TEST_ERROR) {
        return;
    }
    if (test->state == JK_REPETITION_TEST_UNINITIALIZED) {
        test->min.v[JK_PLATFORM_REPETITION_TEST_VALUE_CPU_TIME] = INT64_MAX;
        test->max.v[JK_PLATFORM_REPETITION_TEST_VALUE_CPU_TIME] = INT64_MIN;
    }
    test->state = JK_REPETITION_TEST_RUNNING;
    test->target_byte_count = target_byte_count;
    test->frequency = frequency;
    test->try_for_clocks = try_for_seconds * frequency;
    test->last_found_min_time = jk_cpu_timer_get();
}

JK_PUBLIC void jk_platform_repetition_test_time_begin(JkPlatformRepetitionTest *test)
{
    test->block_open_count++;
    test->current.page_fault_count -= jk_platform_page_fault_count_get();
    test->current.cpu_time -= jk_cpu_timer_get();
}

JK_PUBLIC void jk_platform_repetition_test_time_end(JkPlatformRepetitionTest *test)
{
    test->current.cpu_time += jk_cpu_timer_get();
    test->current.page_fault_count += jk_platform_page_fault_count_get();
    test->block_close_count++;
}

JK_PUBLIC double jk_platform_repetition_test_bandwidth(
        JkPlatformRepetitionTestSample sample, int64_t frequency)
{
    double seconds =
            (double)sample.v[JK_PLATFORM_REPETITION_TEST_VALUE_CPU_TIME] / (double)frequency;
    return (double)sample.v[JK_PLATFORM_REPETITION_TEST_VALUE_BYTE_COUNT] / seconds;
}

static void jk_platform_repetition_test_print_sample(JkPlatformRepetitionTest *test,
        char *name,
        JkPlatformRepetitionTestSampleType type,
        int64_t frequency,
        JkPlatformRepetitionTest *baseline)
{
    JkPlatformRepetitionTestSample *sample = test->samples + type;

    int64_t count = sample->count ? sample->count : 1;

    int64_t cpu_time = sample->cpu_time / count;
    double seconds = cpu_time / (double)frequency;
    printf("%s: %lld (%.2f ms", name, (long long)cpu_time, seconds * 1000.0);
    if (baseline) {
        if (test == baseline) {
            printf(", baseline");
        } else {
            JkPlatformRepetitionTestSample *baseline_sample = baseline->samples + type;
            int64_t baseline_count = baseline_sample->count ? baseline_sample->count : 1;
            int64_t baseline_cpu_time = baseline_sample->cpu_time / baseline_count;
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

    uint64_t current_time = jk_cpu_timer_get();
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

    if ((int64_t)(current_time - test->last_found_min_time) > test->try_for_clocks) {
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
        JkPlatformRepetitionTest *test, int64_t bytes)
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
        int64_t option_count,
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
                    for (int64_t j = 0; !matched && j < option_count; j++) {
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
                    for (int64_t j = 0; !matched && j < option_count; j++) {
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

    int64_t pos = 0;
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

JK_PUBLIC void jk_platform_thread_init(void)
{
    static JK_THREAD_LOCAL JkContext context;
    static JK_THREAD_LOCAL JkPlatformArenaVirtualRoot
            scratch_arena_roots[JK_ARRAY_COUNT(context.scratch_arenas)];
    for (int64_t i = 0; i < JK_ARRAY_COUNT(context.scratch_arenas); i++) {
        context.scratch_arenas[i] =
                jk_platform_arena_virtual_init(scratch_arena_roots + i, 8 * JK_GIGABYTE);
    }
    JkBuffer log_memory = jk_platform_memory_alloc(JK_ALLOC_COMMIT, 16 * JK_MEGABYTE);
    context.log = jk_log_init(jk_platform_print, log_memory);
    jk_context = &context;
}

JK_PUBLIC int64_t jk_platform_page_size_round_up(int64_t n)
{
    int64_t page_size = jk_platform_page_size();
    return (n + page_size - 1) & ~(page_size - 1);
}

JK_PUBLIC int64_t jk_platform_page_size_round_down(int64_t n)
{
    int64_t page_size = jk_platform_page_size();
    return n & ~(page_size - 1);
}

JK_PUBLIC JkBuffer jk_platform_file_read_full(JkArena *arena, char *file_name)
{
    JK_PROFILE_ZONE_TIME_BEGIN(jk_platform_file_read_full);

    FILE *file = fopen(file_name, "rb");
    if (!file) {
        JK_PROFILE_ZONE_END(jk_platform_file_read_full);
        fprintf(stderr,
                "jk_platform_file_read_full: Failed to open file '%s': %s\n",
                file_name,
                strerror(errno));
        exit(1);
    }

    JkBuffer buffer = {.size = jk_platform_file_size(file_name)};
    buffer.data = jk_arena_push(arena, buffer.size);
    if (!buffer.data) {
        JK_PROFILE_ZONE_END(jk_platform_file_read_full);
        fprintf(stderr,
                "jk_platform_file_read_full: Failed to allocate memory for file '%s'\n",
                file_name);
        exit(1);
    }

    JK_PROFILE_ZONE_BANDWIDTH_BEGIN(fread, buffer.size);
    if (fread(buffer.data, buffer.size, 1, file) != 1) {
        JK_PROFILE_ZONE_END(fread);
        JK_PROFILE_ZONE_END(jk_platform_file_read_full);
        fprintf(stderr, "jk_platform_file_read_full: fread failed\n");
        exit(1);
    }
    JK_PROFILE_ZONE_END(fread);

    fclose(file);
    JK_PROFILE_ZONE_END(jk_platform_file_read_full);
    return buffer;
}

JK_PUBLIC JkBufferArray jk_platform_file_read_lines(JkArena *arena, char *file_name)
{
    JkBuffer file = jk_platform_file_read_full(arena, file_name);
    JkBufferArray lines = {.e = jk_arena_pointer_current(arena)};

    int64_t start = 0;
    int64_t i = 0;
    for (; i < file.size; i++) {
        if (file.data[i] == '\n') {
            JkBuffer *line = jk_arena_push(arena, JK_SIZEOF(*line));
            if (!line) {
                goto end;
            }
            line->data = file.data + start;
            line->size = i - start;
            start = i + 1;
        }
    }
    if (start < i) {
        JkBuffer *line = jk_arena_push(arena, JK_SIZEOF(*line));
        if (!line) {
            goto end;
        }
        line->data = file.data + start;
        line->size = i - start;
    }

end:
    lines.count = (JkBuffer *)jk_arena_pointer_current(arena) - lines.e;
    return lines;
}

JK_PUBLIC b32 jk_platform_write_as_c_byte_array(
        JkBuffer buffer, JkBuffer file_path, JkBuffer array_name)
{
    if (!jk_platform_create_directory(jk_path_directory(file_path))) {
        jk_log(JK_LOG_ERROR, JKS("Failed to create directory\n"));
        return 0;
    }

    JkArena arena = jk_arena_scratch_get();

    char *file_path_nt = jk_buffer_to_null_terminated(&arena, file_path);
    char *array_name_nt = jk_buffer_to_null_terminated(&arena, array_name);
    if (!file_path_nt || !array_name_nt) {
        jk_log(JK_LOG_ERROR, JKS("String memory limit exceeded\n"));
        return 0;
    }

    FILE *file = fopen(file_path_nt, "wb");
    if (!file) {
        JK_LOGF(JK_LOG_ERROR,
                jkfn("Failed to open file '"),
                jkfs(file_path),
                jkfn("': "),
                jkfn(strerror(errno)),
                jkf_nl);
        return 0;
    }

    fprintf(file, "JK_PUBLIC char %s[%lld] = {\n", array_name_nt, (long long)buffer.size);
    int64_t byte_index = 0;
    while (byte_index < buffer.size) {
        fprintf(file, "   ");
        for (int64_t i = 0; i < 16 && byte_index < buffer.size; i++) {
            fprintf(file, " 0x%02x,", (int32_t)buffer.data[byte_index++]);
        }
        fprintf(file, "\n");
    }
    fprintf(file, "};\n");

    fclose(file);

    return 1;
}

JK_PUBLIC int64_t jk_platform_cpu_timer_frequency_estimate(int64_t milliseconds_to_wait)
{
    int64_t os_freq = jk_platform_os_timer_frequency();
    int64_t os_wait_time = os_freq * milliseconds_to_wait / 1000;

    uint64_t os_end = 0;
    int64_t os_elapsed = 0;
    uint64_t cpu_start = jk_cpu_timer_get();
    uint64_t os_start = jk_platform_os_timer_get();
    while (os_elapsed < os_wait_time) {
        os_end = jk_platform_os_timer_get();
        os_elapsed = os_end - os_start;
    }

    uint64_t cpu_end = jk_cpu_timer_get();
    int64_t cpu_elapsed = cpu_end - cpu_start;

    return os_freq * cpu_elapsed / os_elapsed;
}

JK_PUBLIC void jk_platform_profile_end_and_print(void)
{
    jk_profile_frame_end();

    int64_t frequency = jk_platform_cpu_timer_frequency_estimate(100);
    uint8_t print_bytes[4096];
    JkBuffer print_memory = JK_BUFFER_INIT_FROM_BYTE_ARRAY(print_bytes);
    JkArenaRoot arena_root;
    JkArena arena = jk_arena_fixed_init(&arena_root, print_memory);
    jk_log(JK_LOG_INFO, jk_profile_report(&arena, frequency));
}

JK_PUBLIC void jk_platform_print_bytes_int64(FILE *file, char *format, int64_t byte_count)
{
    int64_t abs = JK_ABS(byte_count);
    if (abs < 1024) {
        fprintf(file, "%lld bytes", (long long)byte_count);
    } else if (abs < 1024 * 1024) {
        fprintf(file, format, (double)byte_count / 1024.0);
        fprintf(file, " KiB");
    } else if (abs < 1024 * 1024 * 1024) {
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
