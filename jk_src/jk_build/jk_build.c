#define _DEFAULT_SOURCE

#include <ctype.h>
#include <errno.h>
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
#define mkdir(path, mode) _mkdir(path)
#define realpath(N, R) _fullpath(R, N, PATH_MAX)

#else // If not Windows, assume Unix

#include <sys/wait.h>
#include <unistd.h>

#endif

#define ARRAY_MAX 255

typedef enum Compiler {
    COMPILER_NONE,
    COMPILER_GCC,
    COMPILER_MSVC,
    COMPILER_TCC,
} Compiler;

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
    DWORD error_code = GetLastError();
    if (error_code == 0) {
        fprintf(stderr, "Unknown error\n");
    } else {
        char message_buf[PATH_MAX] = {'\0'};
        FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL,
                error_code,
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                message_buf,
                PATH_MAX - 1,
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

#ifdef _WIN32
static bool contains_whitespace(char *string)
{
    for (char *c = string; *c != '\0'; c++) {
        if (isspace(*c)) {
            return true;
        }
    }
    return false;
}
#endif

static int command_run(StringArray *command)
{
#ifdef _WIN32

#define BUFFER_SIZE 4096

    STARTUPINFO si = {.cb = sizeof(si)};
    PROCESS_INFORMATION pi = {0};
    char command_string[BUFFER_SIZE];
    int string_i = 0;
    for (int args_i = 0; args_i < command->count; args_i++) {
        string_i += snprintf(&command_string[string_i],
                BUFFER_SIZE - string_i,
                contains_whitespace(command->items[args_i]) ? "%s\"%s\"" : "%s%s",
                args_i == 0 ? "" : " ",
                command->items[args_i]);
        if (string_i >= BUFFER_SIZE) {
            fprintf(stderr, "%s: Insufficient BUFFER_SIZE\n", program_name);
            exit(1);
        }
    }

    printf("%s\n", command_string);

    if (!CreateProcessA(NULL, command_string, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        fprintf(stderr, "%s: Could not run '%s': ", program_name, command->items[0]);
        windows_print_last_error_and_exit();
    }
    CloseHandle(pi.hThread);
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit_status;
    if (!GetExitCodeProcess(pi.hProcess, &exit_status)) {
        fprintf(stderr, "%s: Could not get exit status of '%s': ", program_name, command->items[0]);
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
            fprintf(stderr, "%s: Could not fork: %s\n", program_name, strerror(errno));
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
        fprintf(stderr,
                "%s: Could not run '%s': %s\n",
                program_name,
                command->items[0],
                strerror(errno));
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

/** Populates globals source_file_path, basename, root_path, and build_path */
static void populate_paths(char *source_file_arg)
{
    // Get absolute path of the source file
    char *realpath_result = realpath(source_file_arg, source_file_path);
    if (realpath_result == NULL) {
        fprintf(stderr, "%s: Invalid source file path '%s'\n", program_name, source_file_arg);
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
        fprintf(stderr, "%s: Invalid source file path '%s'\n", program_name, source_file_arg);
        exit(1);
    }
    if (last_dot == 0 || last_dot <= last_component) {
        last_dot = length;
    }
    strncpy(basename, &source_file_path[last_component], last_dot - last_component);

    // Find repository root path
    char *jk_src = strstr(source_file_path, "jk_src");
    if (jk_src == NULL) {
        fprintf(stderr,
                "%s: File '%s' not located under the jk_src directory\n",
                program_name,
                source_file_arg);
        exit(1);
    }
    root_path_length = jk_src - source_file_path;
    strncpy(root_path, source_file_path, root_path_length);

    // Append "build" to the repository root path to make the build path
    if (root_path_length > PATH_MAX - 32) {
        fprintf(stderr, "%s: Maximum repository root path length exceeded\n", program_name);
        exit(1);
    }
    strcpy(build_path, root_path);
    strcat(build_path, "build");

    path_to_forward_slashes(source_file_path);
    path_to_forward_slashes(root_path);
    path_to_forward_slashes(build_path);
}

static bool is_space_exclude_newlines(int c)
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

static int next_nonspace(FILE *file)
{
    int c;
    do {
        c = getc(file);
    } while (is_space_exclude_newlines(c));
    return c;
}

static bool compare_case_insensitive(char *a, char *b)
{
    if (!(a && b)) {
        return false;
    }
    int i;
    for (i = 0; a[i] != '\0' && b[i] != '\0'; i++) {
        if (tolower(a[i]) != tolower(b[i])) {
            return false;
        }
    }
    return a[i] == '\0' && b[i] == '\0';
}

static void ensure_directory_exists(char *directory_path)
{
    if (mkdir(directory_path, 0775) == -1 && errno != EEXIST) {
        fprintf(stderr,
                "%s: Failed to create \"%s\": %s\n",
                program_name,
                directory_path,
                strerror(errno));
        exit(1);
    }
}

#define JK_GEN_STRING_LITERAL "jk_gen/"

static char const jk_build_string[] = "jk_build";
static char const jk_src_string[] = "jk_src/";
static char const jk_gen_string[] = JK_GEN_STRING_LITERAL;
static char const jk_gen_stu_string[] = JK_GEN_STRING_LITERAL "single_translation_unit.h";

static bool parse_files(char *root_file_path,
        StringArray *dependencies,
        StringArray *nasm_files,
        StringArray *compiler_arguments,
        StringArray *linker_arguments)
{
    static char buf[PATH_MAX] = {'\0'};

    bool dependencies_open = false;
    bool single_translation_unit = false;

    int path_index = 0;
    char *path = root_file_path;
    do {
        FILE *file = fopen(path, "r");
        if (file == NULL) {
            fprintf(stderr, "%s: Failed to open '%s': %s\n", program_name, path, strerror(errno));
            exit(1);
        }

        int char1;
        while ((char1 = getc(file)) != EOF) {
            int pound;
            if (char1 == '/') { // Parse control comment
                if (getc(file) != '/') {
                    goto reset_parse;
                }
                pound = next_nonspace(file);
            } else {
                pound = char1;
            }

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

            if (char1 == '/' && strcmp(buf, jk_build_string) == 0) {
                if (c0 == EOF || c0 == '\r' || c0 == '\n') {
                    fprintf(stderr,
                            "%s: Found #jk_build control comment with no command\n",
                            program_name);
                    fprintf(stderr, "%s: Usage: // %s\n", "#jk_build [command]", program_name);
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

                bool cmd_compiler_arguments = strcmp(buf, "compiler_arguments") == 0;
                bool cmd_linker_arguments = strcmp(buf, "linker_arguments") == 0;

                if (cmd_compiler_arguments || cmd_linker_arguments) {
                    // Read rest of line into buf
                    size_t pos = 0;
                    while ((c = getc(file)) != '\n' && c != EOF) {
                        buf[pos++] = (char)c;
                        if (pos > PATH_MAX - 1) {
                            fprintf(stderr,
                                    "%s: '#jk_build nasm': Path exceeded PATH_MAX\n",
                                    program_name);
                            exit(1);
                        }
                    }
                    buf[pos++] = '\0';

                    char *memory = malloc(sizeof(char) * pos);
                    if (memory == NULL) {
                        fprintf(stderr, "%s: Out of memory\n", program_name);
                        exit(1);
                    }
                    memcpy(memory, buf, pos);

                    bool is_start_of_flag = true;
                    for (size_t i = 0; i < pos; i++) {
                        if (isspace(memory[i])) {
                            is_start_of_flag = true;
                            memory[i] = '\0';
                        } else if (is_start_of_flag) {
                            is_start_of_flag = false;
                            array_append(
                                    cmd_compiler_arguments ? compiler_arguments : linker_arguments, &memory[i]);
                        }
                    }
                } else if (strcmp(buf, "dependencies_begin") == 0) {
                    if (dependencies_open) {
                        fprintf(stderr,
                                "%s: Double '#jk_build dependencies_begin' control comments with "
                                "no '#jk_build dependencies_end' in between\n",
                                program_name);
                        exit(1);
                    }
                    dependencies_open = true;
                } else if (strcmp(buf, "dependencies_end") == 0) {
                    if (dependencies_open) {
                        dependencies_open = false;
                    } else {
                        fprintf(stderr,
                                "%s: Encountered '#jk_build dependencies_end' control comment when "
                                "there was no condition to end\n",
                                program_name);
                        exit(1);
                    }
                } else if (strcmp(buf, "nasm") == 0) {
                    if (c == '\n' || c == EOF) {
                        fprintf(stderr, "%s: '#jk_build nasm' expects a file path\n", program_name);
                        exit(1);
                    }
                    // Read nasm file path into buf
                    size_t nasm_path_length = 0;
                    while ((c = getc(file)) != '\n' && c != EOF) {
                        buf[nasm_path_length++] = (char)c;
                        if (nasm_path_length > PATH_MAX - 1) {
                            fprintf(stderr,
                                    "%s: '#jk_build nasm': Path exceeded PATH_MAX\n",
                                    program_name);
                            exit(1);
                        }
                    }
                    buf[nasm_path_length] = '\0';
                    // Allocate new buffer and write the abolute path of the nasm file to it
                    char *nasm_path = malloc(root_path_length + nasm_path_length + 1);
                    if (nasm_path == NULL) {
                        fprintf(stderr, "%s: Out of memory\n", program_name);
                        exit(1);
                    }
                    memcpy(nasm_path, root_path, root_path_length);
                    memcpy(nasm_path + root_path_length, buf, nasm_path_length);
                    nasm_path[root_path_length + nasm_path_length] = '\0';

                    array_append(nasm_files, nasm_path);
                } else {
                    fprintf(stderr,
                            "%s: #jk_build control comment with unknown command \"%s\"\n",
                            program_name,
                            buf);
                    exit(1);
                }
            } else if (char1 != '/' && strcmp(buf, "include") == 0) {
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
                    if (path_length > PATH_MAX - 1) {
                        fprintf(stderr, "%s: Include path too big for PATH_MAX\n", program_name);
                        exit(1);
                    }
                }
                buf[path_length] = '\0';
                path_to_forward_slashes(buf);

                if (dependencies_open) {
                    if (path_length < sizeof(jk_src_string) - 1
                            || !(memcmp(buf, jk_src_string, sizeof(jk_src_string) - 1) == 0)) {
                        fprintf(stderr,
                                "%s: Warning: Tried to use dependencies_begin on an include path "
                                "that did not start with 'jk_src/', ignoring <%s>\n",
                                program_name,
                                buf);
                        goto reset_parse;
                    }
                } else {
                    if (path_length == sizeof(jk_gen_stu_string) - 1
                            && memcmp(buf, jk_gen_stu_string, sizeof(jk_gen_stu_string) - 1) == 0) {
                        single_translation_unit = true;
                    }
                    goto reset_parse;
                }

                if (buf[path_length - 2] != '.' || buf[path_length - 1] != 'h') {
                    fprintf(stderr,
                            "%s: Warning: Tried to use dependencies_begin on an include path that "
                            "did not end in '.h', ignoring <%s>\n",
                            program_name,
                            buf);
                    goto reset_parse;
                }
                buf[path_length - 1] = 'c';

                // Check if already in dependencies
                for (int i = 0; i < dependencies->count; i++) {
                    if (strcmp(buf, dependencies->items[i] + root_path_length) == 0) {
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

                array_append(dependencies, dependency_path);
            }

        reset_parse:
            continue;
        }

    end_of_file:
        fclose(file);
        if (dependencies_open) {
            fprintf(stderr,
                    "%s: '#jk_build dependencies_begin' condition started but never ended. Use "
                    "'#jk_build dependencies_end' to mark where to stop.\n",
                    program_name);
            exit(1);
        }

        path = dependencies->items[path_index++];
    } while (path_index <= dependencies->count);

    return single_translation_unit;
}

int main(int argc, char **argv)
{
    program_name = argv[0];

    // Parse arguments
    Compiler compiler = COMPILER_NONE;
    char *source_file_arg = NULL;
    bool optimize = false;
    bool no_profile = false;
    {
        bool help = false;
        bool usage_error = false;
        bool options_ended = false;
        int non_option_arguments = 0;
        for (int i = 1; i < argc; i++) {
            char *compiler_string = NULL;
            bool expect_compiler_string = false;

            if (argv[i][0] == '-' && argv[i][1] != '\0' && !options_ended) { // Option argument
                if (argv[i][1] == '-') {
                    if (argv[i][2] == '\0') { // -- encountered
                        options_ended = true;
                    } else { // Double hyphen option
                        char *name = &argv[i][2];
                        int end = 0;
                        while (name[end] != '=' && name[end] != '\0') {
                            end++;
                        }
                        if (strncmp(name, "compiler", end) == 0) {
                            expect_compiler_string = true;

                            if (name[end] == '=') {
                                if (name[end + 1] != '\0') {
                                    compiler_string = &name[end + 1];
                                }
                            } else {
                                compiler_string = argv[++i];
                            }
                        } else if (strcmp(argv[i], "--help") == 0) {
                            help = true;
                        } else if (strcmp(argv[i], "--optimize") == 0) {
                            optimize = true;
                        } else if (strcmp(argv[i], "--no-profile") == 0) {
                            no_profile = true;
                        } else {
                            fprintf(stderr, "%s: Invalid option '%s'\n", program_name, argv[i]);
                            usage_error = true;
                        }
                    }
                } else { // Single-hypen option(s)
                    bool has_argument = false;
                    for (char *c = &argv[i][1]; *c != '\0' && !has_argument; c++) {
                        switch (*c) {
                        case 'c': {
                            has_argument = true;
                            expect_compiler_string = true;
                            compiler_string = ++c;
                            if (compiler_string[0] == '\0') {
                                compiler_string = argv[++i];
                            }
                        } break;

                        case 'O': {
                            optimize = true;
                        } break;

                        default:
                            fprintf(stderr,
                                    "%s: Invalid option '%c' in '%s'\n",
                                    program_name,
                                    *c,
                                    argv[i]);
                            usage_error = true;
                            break;
                        }
                    }
                }
            } else { // Regular argument
                non_option_arguments++;
                source_file_arg = argv[i];
            }

            if (expect_compiler_string) {
                if (compiler_string) {
                    if (compare_case_insensitive(compiler_string, "gcc")) {
                        compiler = COMPILER_GCC;
                    } else if (compare_case_insensitive(compiler_string, "msvc")) {
                        compiler = COMPILER_MSVC;
                    } else if (compare_case_insensitive(compiler_string, "tcc")) {
                        compiler = COMPILER_TCC;
                    } else {
                        fprintf(stderr,
                                "%s: Option '-c, --compiler' given invalid argument '%s'\n",
                                argv[0],
                                compiler_string);
                        usage_error = true;
                    }
                } else {
                    fprintf(stderr,
                            "%s: Option '-c, --compiler' missing required argument\n",
                            argv[0]);
                    usage_error = true;
                }
            }
        }
        if (!help && non_option_arguments != 1) {
            fprintf(stderr,
                    "%s: Expected 1 non-option argument, got %d\n",
                    program_name,
                    non_option_arguments);
            usage_error = true;
        }
        if (help || usage_error) {
            printf("NAME\n"
                   "\tjk_build - builds programs in jk_repo\n\n"
                   "SYNOPSIS\n"
                   "\tjk_build [-c gcc|msvc|tcc] [--no-profile] [-O] FILE\n\n"
                   "DESCRIPTION\n"
                   "\tjk_build can be used to compile any program in jk_repo. FILE can be any\n"
                   "\t'.c' or '.cpp' file that defines an entry point function. Dependencies,\n"
                   "\tif any, do not need to be included in the command line arguments. They\n"
                   "\twill be found by jk_build and included when it invokes a compiler.\n\n"
                   "OPTIONS\n"
                   "\t-c COMPILER, --compiler=COMPILER\n"
                   "\t\tChoose which compiler to use. COMPILER can be gcc, msvc, or tcc.\n\n"
                   "\t--help\tDisplay this help text and exit.\n\n"
                   "\t--no-profile\n"
                   "\t\tExclude profiler timings from the compilation, except for the\n"
                   "\t\ttotal timing. Equivalent to \"#define JK_PROFILE_DISABLE 1\".\n\n"
                   "\t-O, --optimize\n"
                   "\t\tPrioritize the speed of the resulting executable over its\n"
                   "\t\tdebuggability and compilation speed.\n");
            exit(usage_error);
        }

        if (compiler == COMPILER_NONE) {
#ifdef _WIN32
            compiler = COMPILER_MSVC; // Default to MSVC on Windows
#else
            compiler = COMPILER_GCC; // Default to GCC on non-Windows platforms
#endif
        }
    }

    populate_paths(source_file_arg);

    ensure_directory_exists(build_path);
    if (chdir(build_path) == -1) {
        fprintf(stderr,
                "%s: Failed to change working directory to \"%s\": %s\n",
                program_name,
                build_path,
                strerror(errno));
        exit(1);
    }

    StringArray dependencies = {0};
    StringArray nasm_files = {0};
    StringArray compiler_arguments = {0};
    StringArray linker_arguments = {0};
    bool single_translation_unit = parse_files(
            source_file_path, &dependencies, &nasm_files, &compiler_arguments, &linker_arguments);

    for (int i = 0; i < nasm_files.count; i++) {
        char nasm_output_path[PATH_MAX];
        snprintf(nasm_output_path, PATH_MAX, "%s/nasm%d.o", build_path, i);

        StringArray nasm_command = {0};

        array_append(&nasm_command, "nasm");
        array_append(&nasm_command, "-o", nasm_output_path);

        if (compiler == COMPILER_MSVC) {
            array_append(&nasm_command, "-f", "win64");
        } else {
            array_append(&nasm_command, "-f", "elf64");
        }

        array_append(&nasm_command, nasm_files.items[i]);

        command_run(&nasm_command);

        if (compiler == COMPILER_MSVC) {
            StringArray lib_command = {0};
            array_append(&lib_command, "lib", "/nologo", nasm_output_path);
            command_run(&lib_command);
        }
    }

    // Compile command
    StringArray command = {0};

    switch (compiler) { // Compiler options
    case COMPILER_MSVC: {
        array_append(&command, "cl");
        array_append(&command, "/W4");
        array_append(&command, "/w44062");
        array_append(&command, "/wd4100");
        array_append(&command, "/wd4706");
        array_append(&command, "/nologo");
        array_append(&command, "/Gm-");
        array_append(&command, "/GR-");
        array_append(&command, "/D", "_CRT_SECURE_NO_WARNINGS");
        array_append(&command, "/Zi");
        array_append(&command, "/std:c++20");
        array_append(&command, "/EHa-");
        if (optimize) {
            array_append(&command, "/O2");
            array_append(&command, "/GL");
            array_append(&command, "/D", "NDEBUG");
        } else {
            array_append(&command, "/Od");
        }
        if (!single_translation_unit) {
            array_append(&command, "/D", "JK_PUBLIC=");
        }
        if (no_profile) {
            array_append(&command, "/D", "JK_PROFILE_DISABLE");
        }
        array_append(&command, "/I", root_path);
    } break;

    case COMPILER_GCC: {
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
        array_append(&command, "-Wno-missing-braces");
        if (single_translation_unit) {
            array_append(&command, "-Wno-unused-function");
        } else {
            array_append(&command, "-D", "JK_PUBLIC=");
        }
        if (optimize) {
            array_append(&command, "-O3");
            array_append(&command, "-flto");
            array_append(&command, "-fuse-linker-plugin");
            array_append(&command, "-D", "NDEBUG");
        } else {
            array_append(&command, "-Og");
        }
        if (no_profile) {
            array_append(&command, "-D", "JK_PROFILE_DISABLE");
        }
        array_append(&command, "-D", "_DEFAULT_SOURCE=");
        array_append(&command, "-I", root_path);
    } break;

    case COMPILER_TCC: {
        array_append(&command, "tcc");
#ifdef _WIN32
        strcat(basename, ".exe");
#endif
        array_append(&command, "-o", basename);
        array_append(&command, "-std=c99");
        array_append(&command, "-Wall");
        array_append(&command, "-g");
        if (!single_translation_unit) {
            array_append(&command, "-D", "JK_PUBLIC=");
        }
        if (optimize) {
            array_append(&command, "-D", "NDEBUG");
        }
        if (no_profile) {
            array_append(&command, "-D", "JK_PROFILE_DISABLE");
        }
        array_append(&command, "-D", "_DEFAULT_SOURCE=");
        array_append(&command, "-I", root_path);
    } break;

    case COMPILER_NONE: {
        fprintf(stderr, "%s: compiler should never be COMPILER_NONE by this point\n", argv[0]);
        exit(1);
    } break;
    }

    array_concat(&command, compiler_arguments.count, compiler_arguments.items);

    array_append(&command, source_file_path);

    if (single_translation_unit) {
        char buffer[PATH_MAX];
        strcpy(buffer, root_path);
        strcat(buffer, jk_gen_string);
        ensure_directory_exists(buffer);
        strcat(buffer, &jk_gen_stu_string[sizeof(jk_gen_string) - 1]);

        FILE *stu_file = fopen(buffer, "wb");
        if (stu_file == NULL) {
            fprintf(stderr, "%s: Failed to open '%s': %s\n", program_name, buffer, strerror(errno));
            exit(1);
        }

        fprintf(stu_file, "#define JK_PUBLIC static\n");
        for (int i = 0; i < dependencies.count; i++) {
            fprintf(stu_file, "#include <%s>\n", dependencies.items[i] + root_path_length);
        }

        fclose(stu_file);
    } else {
        array_concat(&command, dependencies.count, dependencies.items);
    }

    switch (compiler) { // Linker options
    case COMPILER_MSVC: {
        array_append(&command, "/link");
        array_append(&command, "/INCREMENTAL:NO");

        char libpath[PATH_MAX];
        snprintf(libpath, PATH_MAX, "/LIBPATH:\"%s\"", root_path);
        array_append(&command, libpath);

        array_append(&command, "Advapi32.lib");
    } break;

    case COMPILER_GCC: {
        array_append(&command, "-lm");
    } break;

    case COMPILER_TCC: {
#ifndef _WIN32
        array_append(&command, "-lm");
#endif
    } break;

    case COMPILER_NONE: {
        fprintf(stderr, "%s: compiler should never be COMPILER_NONE by this point\n", argv[0]);
        exit(1);
    } break;
    }

    array_concat(&command, linker_arguments.count, linker_arguments.items);

    for (int i = 0; i < nasm_files.count; i++) {
        char file_name[32];
        snprintf(file_name, 32, "nasm%d.%s", i, compiler == COMPILER_MSVC ? "lib" : "o");
        array_append(&command, file_name);
    }

    return command_run(&command);
}
