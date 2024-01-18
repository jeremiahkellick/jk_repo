#define _DEFAULT_SOURCE

#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef _WIN32

#include <direct.h>
#include <windows.h>

#ifndef PATH_MAX
#define PATH_MAX MAX_PATH
#endif

#define chdir _chdir
#define realpath(N, R) _fullpath((R), (N), PATH_MAX)

#else // If not Windows, assume Unix

#include <sys/wait.h>
#include <unistd.h>

#endif

#define BUF_SIZE 4096
#define ARRAY_MAX 255

/** argv[0] */
static char *program_name = NULL;
/** Abolsute path of source file passed in as argv[1] */
static char source_file_path[PATH_MAX] = {0};
/** Basename of source file without extension */
static char basename[PATH_MAX] = {0};
/** jk_repo/ */
static char root_path[PATH_MAX] = {0};
/** jk_repo/build/ */
static char build_path[PATH_MAX] = {0};

#ifdef _WIN32
static void windows_print_last_error_and_exit(void)
{
    fprintf(stderr, "%s: ", program_name);
    DWORD error_code = GetLastError();
    if (error_code == 0) {
        fprintf(stderr, "Unknown error\n");
    } else {
        char message_buf[BUF_SIZE] = {'\0'};
        FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL,
                error_code,
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                message_buf,
                BUF_SIZE - 1,
                NULL);
        fprintf(stderr, "%s", message_buf);
    }
    exit(1);
}
#endif

typedef struct StringArray {
    int count;
    char *items[ARRAY_MAX + 1];
} StringArray;

static void array_concat(StringArray *array, int count, char **items)
{
    if (array->count + count > ARRAY_MAX) {
        fprintf(stderr, "%s: ARRAY_MAX (%d) exceeded\n", program_name, ARRAY_MAX);
        exit(1);
    }
    for (int i = 0; i < count; i++) {
        array->items[array->count++] = items[i];
    }
}

/**
 * Append strings onto a StringArray
 *
 * @param array Array to append onto
 * @param ... Any number of 'char *'s to append
 */
#define array_append(array, ...) \
    array_concat(                \
            array, (sizeof((char *[]){__VA_ARGS__}) / sizeof(char *)), ((char *[]){__VA_ARGS__}))

static int command_run(StringArray *command)
{
#ifdef _WIN32
    STARTUPINFO si = {.cb = sizeof(si)};
    PROCESS_INFORMATION pi = {0};
    char command_string[BUF_SIZE];
    int string_i = 0;
    for (int args_i = 0; args_i < command->count; args_i++) {
        string_i += snprintf(&command_string[string_i],
                BUF_SIZE - string_i,
                "%s%s",
                args_i == 0 ? "" : " ",
                command->items[args_i]);
        if (string_i >= BUF_SIZE) {
            fprintf(stderr, "%s: Insufficient BUF_SIZE\n", program_name);
            exit(1);
        }
    }

    printf("%s\n", command_string);

    if (!CreateProcessA(NULL, command_string, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        windows_print_last_error_and_exit();
    }
    CloseHandle(pi.hThread);
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit_status;
    if (!GetExitCodeProcess(pi.hProcess, &exit_status)) {
        windows_print_last_error_and_exit();
    }
    CloseHandle(pi.hProcess);
    return (int)exit_status;
#else
    // Print command
    for (int i = 0; i < command->count; i++) {
        if (i != 0) {
            printf(" ");
        }
        printf("%s", command->items[i]);
    }
    printf("\n");

    // Run command
    int command_pid;
    if ((command_pid = fork())) {
        if (command_pid == -1) {
            perror(program_name);
            exit(1);
        }
        int wstatus;
        waitpid(command_pid, &wstatus, 0);
        if (WIFEXITED(wstatus)) {
            return WEXITSTATUS(wstatus);
        } else {
            fprintf(stderr, "%s: Compiler exited abnormally\n", program_name);
            exit(1);
        }
    } else {
        execvp(command->items[0], command->items);
        perror(program_name);
        exit(1);
    }
#endif
}

/** Populates globals program_name, source_file_path, basename, root_path, and build_path */
static void populate_globals(int argc, char **argv)
{
    program_name = argv[0];

    if (argc != 2) {
        fprintf(stderr, "Usage: %s [program main source file]\n", program_name);
        exit(1);
    }

    // Get absolute path of the source file
    char *realpath_result = realpath(argv[1], source_file_path);
    if (realpath_result == NULL) {
        fprintf(stderr, "%s: Invalid source file path\n", program_name);
        exit(1);
    }

    // Find basename
    size_t length = strlen(source_file_path);
    size_t last_component = 0;
    size_t last_dot = 0;
    for (size_t i = 0; i < length; i++) {
        switch (source_file_path[i]) {
        case '/':
        case '\\':
            last_component = i + 1;
            break;
        case '.':
            last_dot = i;
            break;
        default:
            break;
        }
    }
    if (last_component == length) {
        fprintf(stderr, "%s: Invalid source file path\n", program_name);
        exit(1);
    }
    if (last_dot == 0 || last_dot <= last_component) {
        last_dot = length;
    }
    strncpy(basename, &source_file_path[last_component], last_dot - last_component);

    // Find repository root path
    char *jk_src = strstr(source_file_path, "jk_src");
    if (jk_src == NULL) {
        fprintf(stderr, "%s: File not located under the jk_src directory\n", program_name);
        exit(1);
    }
    size_t root_path_length = jk_src - source_file_path;
    strncpy(root_path, source_file_path, root_path_length);

    // Append "build" to the repository root path to make the build path
    if (root_path_length > PATH_MAX - 7) {
        fprintf(stderr, "%s: PATH_MAX exceeded\n", program_name);
        exit(1);
    }
    strcpy(build_path, root_path);
    strcat(build_path, "build/");
}

int main(int argc, char **argv)
{
    populate_globals(argc, argv);

    chdir(build_path);

    StringArray command = {0};

#ifdef _WIN32
    // MSVC compiler options
    array_append(&command, "cl");
    array_append(&command, "/W4");
    array_append(&command, "/D", "_CRT_SECURE_NO_WARNINGS");
    array_append(&command, "/Zi");
    array_append(&command, "/std:c++20");
    array_append(&command, "/EHsc");
    array_append(&command, "/I", root_path);
#else
    // GCC compiler options
    array_append(&command, "gcc");
    array_append(&command, "-o", basename);
    array_append(&command, "-std=c99");
    array_append(&command, "-pedantic");
    array_append(&command, "-g");
    array_append(&command, "-pipe");
    array_append(&command, "-Wall");
    array_append(&command, "-Wextra");
    array_append(&command, "-fstack-protector");
    array_append(&command, "-Werror=vla");
    array_append(&command, "-Wno-pointer-arith");
    array_append(&command, "-I", root_path);
    // GCC linker options
    array_append(&command, "-lm");
#endif

    array_append(&command, source_file_path);

    return command_run(&command);
}
