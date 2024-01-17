#define _DEFAULT_SOURCE

#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#ifdef _WIN32

#include <windows.h>

#ifndef PATH_MAX
#define PATH_MAX MAX_PATH
#endif

#define realpath(N, R) _fullpath((R), (N), PATH_MAX)

#else

#include <unistd.h>

#endif

#define BUF_SIZE 4096
#define MAX_SEARCH_LEVELS 20

static char *program_name = NULL;

#define MAX_ARGS 63

typedef struct Command {
    int args_count;
    char *args[MAX_ARGS + 1];
} Command;

void command_append_array(Command *c, int args_count, char **args)
{
    if (c->args_count + args_count > MAX_ARGS) {
        fprintf(stderr, "%s: MAX_ARGS (%d) exceeded", program_name, MAX_ARGS);
        exit(1);
    }
    for (int i = 0; i < args_count; i++) {
        c->args[c->args_count++] = args[i];
    }
}

/**
 * Append arguments onto a command
 *
 * @param c Command to append arguments onto
 * @param ... Arguments to append, any number of char * null terminated strings
 */
#define command_append(c, ...) \
    command_append_array(      \
            c, (sizeof((char *[]){__VA_ARGS__}) / sizeof(char *)), ((char *[]){__VA_ARGS__}))

int command_run(Command *c)
{
#ifdef _WIN32
    STARTUPINFO si = {.cb = sizeof(si)};
    PROCESS_INFORMATION pi = {0};
    char command_string[BUF_SIZE];
    int string_i = 0;
    for (int args_i = 0; args_i < c->args_count; args_i++) {
        string_i += snprintf(&command_string[string_i],
                BUF_SIZE - string_i,
                "%s%s",
                args_i == 0 ? "" : " ",
                c->args[args_i]);
        if (string_i >= BUF_SIZE) {
            fprintf(stderr, "%s: Insufficient BUF_SIZE\n", program_name);
            exit(1);
        }
    }

    printf("%s\n", command_string);
    if (!CreateProcess(NULL, command_string, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
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

    // Wait until child process exits.
    WaitForSingleObject(pi.hProcess, INFINITE);

    // Close process and thread handles.
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
#else
    // Print command
    for (int i = 0; i < c->args_count; i++) {
        if (i != 0) {
            printf(" ");
        }
        printf("%s", c->args[i]);
    }
    printf("\n");

    // Run command
    int command_pid;
    if ((command_pid = fork())) {
        if (command_pid == -1) {
            perror(program_name);
            exit(1);
        }
        int status;
        waitpid(command_pid, &status, 0);
        return status;
    } else {
        int result = execvp(c->args[0], c->args);
        if (result == -1) {
            perror(program_name);
        } else {
            fprintf(stderr, "%s: execve returned unexpectedly\n", program_name);
        }
        exit(1);
    }
#endif
}

int main(int argc, char **argv)
{
    program_name = argv[0];

    if (argc != 2) {
        fprintf(stderr, "Usage: %s [program main source file]\n", program_name);
        exit(1);
    }

    char source_file_path[PATH_MAX] = {0};
    char basename[PATH_MAX] = {0};
    char root_path[PATH_MAX] = {0};
    char build_path[PATH_MAX] = {0};
    {
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
        if (root_path_length > PATH_MAX - 6) {
            fprintf(stderr, "%s: PATH_MAX exceeded\n", program_name);
            exit(1);
        }
        strcpy(build_path, root_path);
        strcat(build_path, "build");
    }

    chdir(build_path);

    Command c = {0};

#ifdef _WIN32
    command_append(&c, "cl", source_file_path);

    // MSVC compiler options
    command_append(&c, "/W4");
    command_append(&c, "/D");
    command_append(&c, "_CRT_SECURE_NO_WARNINGS");
    command_append(&c, "/Zi");
    command_append(&c, "/std:c++20");
    command_append(&c, "/EHsc");
    command_append(&c, "/I", root_path);

    // MSVC linker options
    // command_append(&c, "/link");
#else
    command_append(&c, "gcc", source_file_path);
    command_append(&c, "-o", basename);
    command_append(&c, "-std=c99");
    command_append(&c, "-pedantic");
    command_append(&c, "-g");
    command_append(&c, "-pipe");
    command_append(&c, "-Wall");
    command_append(&c, "-Wextra");
    command_append(&c, "-fstack-protector");
    command_append(&c, "-Werror=vla");
    command_append(&c, "-Wno-pointer-arith");
    command_append(&c, "-lm");
    command_append(&c, "-I", root_path);
#endif

    command_run(&c);

    return 0;
}
