#define _DEFAULT_SOURCE

#include <ctype.h>
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
static size_t root_path_length = 0;
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

/** Replace all instances of \ with / in the given null terminated string */
static void path_to_forward_slashes(char *path)
{
    for (int i = 0; path[i] != '\0'; i++) {
        if (path[i] == '\\') {
            path[i] = '/';
        }
    }
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
    root_path_length = jk_src - source_file_path;
    strncpy(root_path, source_file_path, root_path_length);

    // Append "build" to the repository root path to make the build path
    if (root_path_length > PATH_MAX - 7) {
        fprintf(stderr, "%s: PATH_MAX exceeded\n", program_name);
        exit(1);
    }
    strcpy(build_path, root_path);
    strcat(build_path, "build/");

    path_to_forward_slashes(source_file_path);
    path_to_forward_slashes(root_path);
    path_to_forward_slashes(build_path);
}

bool is_space_exclude_newlines(int c)
{
    // We don't consider carraige returns and newlines because none of the things we're parsing are
    // allowed to span multiple lines.
    switch (c) {
    case ' ':
    case '\t':
    case '\v':
    case '\f':
        return true;
    default:
        return false;
    }
}

int next_nonspace(FILE *file)
{
    int c;
    do {
        c = getc(file);
    } while (is_space_exclude_newlines(c));
    return c;
}

char const *const jk_build_string = "jk_build";
char const *const jk_src_string = "jk_src/";

void find_dependencies(StringArray *source_file_paths)
{
    static char buf[BUF_SIZE] = {'\0'};

    bool dependencies_adjacent = false;

    for (int path_index = 0; path_index < source_file_paths->count; path_index++) {
        char *path = source_file_paths->items[path_index];
        FILE *file = fopen(path, "r");
        if (file == NULL) {
            fprintf(stderr, "%s: Failed to open \"%s\"\n", program_name, path);
            perror(program_name);
            exit(1);
        }

        int pound;
        while ((pound = getc(file)) != EOF) {
            if (pound != '#') {
                goto reset_parse;
            }

            // Read control into buf
            int control_char;
            size_t control_length = 0;
            while (!isspace(control_char = getc(file))) {
                if (control_char == EOF) {
                    goto end_of_file;
                }
                buf[control_length++] = (char)control_char;
                if (control_length > 32) {
                    goto reset_parse;
                }
            }
            buf[control_length] = 0;

            int c0 = control_char;
            while (is_space_exclude_newlines(c0)) {
                c0 = getc(file);
            }

            if (strcmp(buf, "jk_build") == 0) {
                if (c0 == EOF || c0 == '\r' || c0 == '\n') {
                    fprintf(stderr,
                            "%s: Found #jk_build control comment with no command\n",
                            program_name);
                    fprintf(stderr, "%s: Usage: // #jk_build [command]\n", program_name);
                    exit(1);
                }

                // Read command into buf
                buf[0] = (char)c0;
                int c;
                size_t command_length = 1;
                while (!isspace(c = getc(file)) && c != EOF) {
                    buf[command_length++] = (char)c;
                    if (command_length > 32) {
                        fprintf(stderr,
                                "%s: #jk_build control comment with unknown command \"%s...\"\n",
                                program_name,
                                buf);
                        exit(1);
                    }
                }
                buf[command_length] = '\0';

                if (strcmp(buf, "dependencies_adjacent") == 0) {
                    if (dependencies_adjacent) {
                        fprintf(stderr,
                                "%s: Double '#jk_build dependencies_adjacent' control comments "
                                "with no "
                                "'#jk_build end' in between\n",
                                program_name);
                        exit(1);
                    }
                    dependencies_adjacent = true;
                } else if (strcmp(buf, "end") == 0) {
                    if (dependencies_adjacent) {
                        dependencies_adjacent = false;
                    } else {
                        fprintf(stderr,
                                "%s: Encountered '#jk_build end' control comment when there was no "
                                "condition to end\n",
                                program_name);
                        exit(1);
                    }
                } else {
                    fprintf(stderr,
                            "%s: #jk_build control comment with unknown command \"%s\"\n",
                            program_name,
                            buf);
                    exit(1);
                }
            } else if (dependencies_adjacent && strcmp(buf, "include") == 0) {
                if (c0 == EOF) {
                    goto end_of_file;
                } else if (c0 != '<') {
                    goto reset_parse;
                }

                // Write path to buf
                int c;
                size_t path_length = 0;
                while ((c = getc(file)) != '>') {
                    if (c == EOF || c == '\r' || c == '\n') {
                        goto reset_parse;
                    }
                    buf[path_length++] = (char)c;
                    if (path_length > BUF_SIZE - 1) {
                        fprintf(stderr, "%s: Include path too big for BUF_SIZE\n", program_name);
                        exit(1);
                    }
                }

                buf[path_length] = '\0';
                if (buf[path_length - 2] != '.' || buf[path_length - 1] != 'h') {
                    fprintf(stderr,
                            "%s: Warning: Tried to use dependencies_adjacent on an include path "
                            "that did not end in '.h', ignoring <%s>\n",
                            program_name,
                            buf);
                    goto reset_parse;
                }
                buf[path_length - 1] = 'c';

                size_t jk_src_string_length = strlen(jk_src_string);
                if (path_length < jk_src_string_length
                        || !(memcmp(buf, jk_src_string, jk_src_string_length) == 0)) {
                    fprintf(stderr,
                            "%s: Warning: Tried to use dependencies_adjacent on an include path "
                            "that did not start with 'jk_src/', ignoring <%s>\n",
                            program_name,
                            buf);
                    goto reset_parse;
                }

                path_to_forward_slashes(buf);
                // Check if already in source_file_paths
                for (int i = 0; i < source_file_paths->count; i++) {
                    if (strcmp(buf, source_file_paths->items[i] + root_path_length) == 0) {
                        goto reset_parse;
                    }
                }

                // Allocate new buffer and write abolute path of dependency to it
                char *dependency_path = malloc(root_path_length + path_length + 1);
                if (dependency_path == NULL) {
                    fprintf(stderr, "%s: Out of memory\n", program_name);
                    exit(1);
                }
                memcpy(dependency_path, root_path, root_path_length);
                memcpy(dependency_path + root_path_length, buf, path_length);
                dependency_path[root_path_length + path_length] = '\0';

                array_append(source_file_paths, dependency_path);
            }
        reset_parse:
            continue;
        }

    end_of_file:
        fclose(file);
        if (dependencies_adjacent) {
            fprintf(stderr,
                    "%s: '#jk_build dependencies_adjacent' condition started but never ended. Use "
                    "'#jk_build end' to mark where to stop.\n",
                    program_name);
            exit(1);
        }
    }
}

#undef MATCH

int main(int argc, char **argv)
{
    populate_globals(argc, argv);

    chdir(build_path);

    StringArray source_file_paths = {0};
    array_append(&source_file_paths, source_file_path);
    find_dependencies(&source_file_paths);

    StringArray command = {0};

#ifdef _WIN32
    // MSVC compiler options
    array_append(&command, "cl");
    array_append(&command, "/W4");
    array_append(&command, "/D", "_CRT_SECURE_NO_WARNINGS");
    array_append(&command, "/Zi");
    array_append(&command, "/std:c++20");
    array_append(&command, "/EHsc");
    array_append(&command, "/O2");
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

    array_concat(&command, source_file_paths.count, source_file_paths.items);

    return command_run(&command);
}
