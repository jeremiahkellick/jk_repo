#define _DEFAULT_SOURCE

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

typedef uint32_t b32;

#define GIGABYTE (1llu << 30)

#define JK_ARRAY_COUNT(array) (sizeof(array) / sizeof((array)[0]))

#define JK_MIN(a, b) ((a) < (b) ? (a) : (b))

// ---- Buffer begin -----------------------------------------------------------

typedef struct JkBuffer {
    uint64_t size;
    uint8_t *data;
} JkBuffer;

typedef struct JkBufferArray {
    uint64_t count;
    JkBuffer *items;
} JkBufferArray;

#define JK_STRING(string_literal) \
    ((JkBuffer){sizeof(string_literal) - 1, (uint8_t *)string_literal})

#define JKS JK_STRING

#define JK_STRING_INITIALIZER(string_literal)                 \
    {                                                         \
        sizeof(string_literal) - 1, (uint8_t *)string_literal \
    }

#define JKSI JK_STRING_INITIALIZER

static JkBuffer jk_buffer_from_null_terminated(char *string)
{
    if (string) {
        return (JkBuffer){.size = strlen(string), .data = (uint8_t *)string};
    } else {
        return (JkBuffer){0};
    }
}

static int jk_buffer_character_get(JkBuffer buffer, uint64_t pos)
{
    return pos < buffer.size ? buffer.data[pos] : EOF;
}

static int jk_buffer_character_next(JkBuffer buffer, uint64_t *pos)
{
    int c = jk_buffer_character_get(buffer, *pos);
    (*pos)++;
    return c;
}

static int jk_buffer_compare(JkBuffer a, JkBuffer b)
{
    for (uint64_t pos = 0; 1; pos++) {
        int a_char = jk_buffer_character_get(a, pos);
        int b_char = jk_buffer_character_get(b, pos);
        if (a_char < b_char) {
            return -1;
        } else if (a_char > b_char) {
            return 1;
        } else if (a_char == EOF && b_char == EOF) {
            return 0;
        }
    }
}

static b32 jk_char_is_whitespace(uint8_t c)
{
    return c == ' ' || ('\t' <= c && c <= '\r');
}

static b32 jk_string_contains_whitespace(JkBuffer string)
{
    for (uint64_t i = 0; i < string.size; i++) {
        if (jk_char_is_whitespace(string.data[i])) {
            return 1;
        }
    }
    return 0;
}

// Returns the index where the search_string appears in the text if found, or -1 if not found
static int64_t jk_string_find(JkBuffer text, JkBuffer search_string)
{
    for (int64_t i = 0; i <= (int64_t)text.size - (int64_t)search_string.size; i++) {
        b32 match = 1;
        for (int64_t j = 0; j < (int64_t)search_string.size; j++) {
            if (text.data[i + j] != search_string.data[j]) {
                match = 0;
                break;
            }
        }
        if (match) {
            return i;
        }
    }

    return -1;
}

// ---- Buffer end -------------------------------------------------------------

// ---- Arena begin ------------------------------------------------------------

typedef struct JkArenaRoot {
    JkBuffer memory;
} JkArenaRoot;

typedef struct JkArena {
    uint64_t base;
    uint64_t pos;
    JkArenaRoot *root;
    b32 (*grow)(struct JkArena *arena, uint64_t new_size);
} JkArena;

static b32 jk_arena_valid(JkArena *arena)
{
    return !!arena->root;
}

static void *jk_arena_push(JkArena *arena, uint64_t size)
{
    uint64_t new_pos = arena->pos + size;
    if (arena->root->memory.size < new_pos) {
        if (!arena->grow(arena, new_pos)) {
            return NULL;
        }
    }
    void *address = arena->root->memory.data + arena->pos;
    arena->pos = new_pos;
    return address;
}

static void *jk_arena_push_zero(JkArena *arena, uint64_t size)
{
    void *address = jk_arena_push(arena, size);
    memset(address, 0, size);
    return address;
}

static void jk_arena_pop(JkArena *arena, uint64_t size)
{
    arena->pos -= size;
}

static JkArena jk_arena_child_get(JkArena *parent)
{
    return (JkArena){
        .base = parent->pos,
        .pos = parent->pos,
        .root = parent->root,
        .grow = parent->grow,
    };
}

static void *jk_arena_pointer_current(JkArena *arena)
{
    return arena->root->memory.data + arena->pos;
}

// ---- Arena end --------------------------------------------------------------

static JkBuffer jk_buffer_copy(JkArena *arena, JkBuffer buffer)
{
    JkBuffer result = {.size = buffer.size, .data = jk_arena_push(arena, buffer.size)};
    memcpy(result.data, buffer.data, buffer.size);
    return result;
}

static char *jk_buffer_to_null_terminated(JkArena *arena, JkBuffer buffer)
{
    char *result = jk_arena_push(arena, buffer.size + 1);
    result[buffer.size] = '\0';
    memcpy(result, buffer.data, buffer.size);
    return result;
}

#ifdef _WIN32

#include <direct.h>
#include <windows.h>

#ifndef PATH_MAX
#define PATH_MAX MAX_PATH
#endif

#define chdir _chdir
#define mkdir(path, mode) _mkdir(path)
#define realpath(N, R) _fullpath(R, N, PATH_MAX)

static size_t jk_platform_file_size(char *file_name)
{
    struct __stat64 info = {0};
    if (_stat64(file_name, &info)) {
        fprintf(stderr, "jk_platform_file_size: stat returned an error\n");
        return 0;
    }
    return (size_t)info.st_size;
}

static size_t jk_platform_page_size(void)
{
    return 4096;
}

static void *jk_platform_memory_reserve(size_t size)
{
    return VirtualAlloc(NULL, size, MEM_RESERVE, PAGE_NOACCESS);
}

static b32 jk_platform_memory_commit(void *address, size_t size)
{
    return VirtualAlloc(address, size, MEM_COMMIT, PAGE_READWRITE) != NULL;
}

static void jk_platform_memory_free(void *address, size_t size)
{
    // TODO: Consider how to deal with different freeing behavior between Windows and Unix
    VirtualFree(address, 0, MEM_RELEASE);
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

static int jk_platform_exec(JkBufferArray command)
{
    static char command_buffer[4096];

    if (!command.count) {
        fprintf(stderr, "jk_platform_exec: Received an empty command\n");
        return 1;
    }

    STARTUPINFO si = {.cb = sizeof(si)};
    PROCESS_INFORMATION pi = {0};
    int string_i = 0;
    for (int args_i = 0; args_i < command.count; args_i++) {
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

static JkBuffer jk_platform_file_read_full(JkArena *arena, char *file_name)
{
    FILE *file = fopen(file_name, "rb");
    if (!file) {
        fprintf(stderr,
                "jk_platform_file_read_full: Failed to open file '%s': %s\n",
                file_name,
                strerror(errno));
        exit(1);
    }

    JkBuffer buffer = {.size = jk_platform_file_size(file_name)};
    buffer.data = jk_arena_push(arena, buffer.size);
    if (!buffer.data) {
        fprintf(stderr,
                "jk_platform_file_read_full: Failed to allocate memory for file '%s'\n",
                file_name);
        exit(1);
    }

    if (fread(buffer.data, buffer.size, 1, file) != 1) {
        fprintf(stderr, "jk_platform_file_read_full: fread failed\n");
        exit(1);
    }

    fclose(file);
    return buffer;
}

#else // If not Windows, assume Unix

#include <sys/wait.h>
#include <unistd.h>

static size_t jk_platform_page_size(void)
{
    static size_t page_size = 0;
    if (page_size == 0) {
        page_size = getpagesize();
    }
    return page_size;
}

static void *jk_platform_memory_reserve(size_t size)
{
    return mmap(NULL, size, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0);
}

static b32 jk_platform_memory_commit(void *address, size_t size)
{
    return !mprotect(address, size, PROT_READ | PROT_WRITE);
}

static void jk_platform_memory_free(void *address, size_t size)
{
    munmap(address, size);
}

static int command_run(StringArray *command)
{
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
}

#endif

static size_t jk_platform_page_size_round_up(size_t n)
{
    size_t page_size = jk_platform_page_size();
    return (n + page_size - 1) & ~(page_size - 1);
}

// ---- Virtual arena begin ------------------------------------------------------------

typedef struct JkPlatformArenaVirtualRoot {
    JkArenaRoot generic;
    uint64_t virtual_size;
} JkPlatformArenaVirtualRoot;

static JkArena jk_platform_arena_virtual_init(
        JkPlatformArenaVirtualRoot *root, uint64_t virtual_size);

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

static JkArena jk_platform_arena_virtual_init(
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

// ---- Virtual arena end --------------------------------------------------------------

typedef struct StringNode {
    JkBuffer string;
    struct StringNode *previous;
} StringNode;

typedef struct StringArrayBuilder {
    JkArena *arena;
    StringNode tail;
    uint64_t count;
} StringArrayBuilder;

static void string_array_builder_init(StringArrayBuilder *b, JkArena *arena)
{
    b->arena = arena;
    b->tail = (StringNode){0};
    b->count = 0;
}

static void string_array_builder_push(StringArrayBuilder *b, JkBuffer string)
{
    StringNode *node = jk_arena_push(b->arena, sizeof(*node));
    node->string = string;
    node->previous = b->tail.previous;
    b->tail.previous = node;
    b->count++;
}

static void string_array_builder_push_multiple(StringArrayBuilder *b, JkBufferArray strings)
{
    for (uint64_t i = 0; i < strings.count; i++) {
        string_array_builder_push(b, strings.items[i]);
    }
}

static void string_array_builder_push_null_terminated(StringArrayBuilder *b, char *string)
{
    string_array_builder_push(b, jk_buffer_from_null_terminated(string));
}

static void string_array_builder_push_null_terminated_multiple(
        StringArrayBuilder *b, uint64_t count, char **strings)
{
    for (uint64_t i = 0; i < count; i++) {
        string_array_builder_push_null_terminated(b, strings[i]);
    }
}

static void string_array_builder_concat(StringArrayBuilder *dest, StringArrayBuilder *src)
{
    StringNode *head = &src->tail;
    while (head->previous) {
        head = head->previous;
    }
    head->previous = dest->tail.previous;
    dest->tail.previous = src->tail.previous;
    dest->count += src->count;
}

static JkBufferArray string_array_builder_build(StringArrayBuilder *b)
{
    JkBufferArray result = {
        .count = b->count,
        .items = jk_arena_push(b->arena, b->count * sizeof(result.items[0])),
    };
    int64_t i = result.count;
    for (StringNode *node = b->tail.previous; node; node = node->previous) {
        result.items[--i] = node->string;
    }
    return result;
}

static b32 string_array_builder_contains(StringArrayBuilder *b, JkBuffer search_string)
{
    for (StringNode *node = b->tail.previous; node; node = node->previous) {
        if (jk_buffer_compare(node->string, search_string) == 0) {
            return 1;
        }
    }
    return 0;
}

static JkBuffer string_array_builder_at_index(StringArrayBuilder *b, uint64_t index)
{
    if (b->count <= index) {
        return (JkBuffer){0};
    }

    uint64_t step_count = b->count - index;
    StringNode *node = b->tail.previous;
    while (--step_count) {
        node = node->previous;
    }
    return node->string;
}

#define append(string_array_builder, ...)                                    \
    string_array_builder_push_null_terminated_multiple(string_array_builder, \
            sizeof((char *[]){__VA_ARGS__}) / sizeof(char *),                \
            (char *[]){__VA_ARGS__})

static JkBuffer concat_strings(JkArena *arena, JkBuffer a, JkBuffer b)
{
    JkBuffer result;
    result.size = a.size + b.size;
    result.data = jk_arena_push(arena, result.size);
    memcpy(result.data, a.data, a.size);
    memcpy(result.data + a.size, b.data, b.size);
    return result;
}

static void back_slashes_to_forward_slashes(JkBuffer string)
{
    for (uint64_t i = 0; i < string.size; i++) {
        if (string.data[i] == '\\') {
            string.data[i] = '/';
        }
    }
}

typedef enum Compiler {
    COMPILER_NONE,
    COMPILER_GCC,
    COMPILER_MSVC,
    COMPILER_TCC,
    COMPILER_CLANG,
} Compiler;

typedef enum JkBuildMode {
    JK_BUILD_DEBUG,
    JK_BUILD_OPTIMIZED,
    JK_BUILD_RELEASE,
} JkBuildMode;

typedef struct Paths {
    JkBuffer source_file;
    JkBuffer basename;
    JkBuffer repo_root;
    JkBuffer build;
} Paths;

/** argv[0] */
static char *program_name = NULL;

/** Populates globals source_file_path, basename, root_path, and build_path */
static Paths paths_get(JkArena *arena, char *source_file_arg)
{
    Paths paths;

    // Get absolute path of the source file
    paths.source_file = jk_buffer_from_null_terminated(
            realpath(source_file_arg, jk_arena_push(arena, PATH_MAX)));
    if (!paths.source_file.size) {
        fprintf(stderr, "%s: Invalid source file path '%s'\n", program_name, source_file_arg);
        exit(1);
    }
    back_slashes_to_forward_slashes(paths.source_file);

    // Find basename
    uint64_t last_component = 0;
    uint64_t last_dot = 0;
    for (uint64_t i = 0; i < paths.source_file.size; i++) {
        switch (paths.source_file.data[i]) {
        case '/':
            last_component = i + 1;
            break;
        case '.':
            last_dot = i;
            break;
        default:
            break;
        }
    }
    if (last_component == paths.source_file.size) {
        fprintf(stderr, "%s: Invalid source file path '%s'\n", program_name, source_file_arg);
        exit(1);
    }
    if (last_dot == 0 || last_dot <= last_component) {
        last_dot = paths.source_file.size;
    }
    paths.basename.size = last_dot - last_component;
    paths.basename.data = paths.source_file.data + last_component;

    // Find repository root path
    int64_t jk_src_offset = jk_string_find(paths.source_file, JKS("jk_src"));
    if (jk_src_offset == -1) {
        fprintf(stderr,
                "%s: File '%s' not located under the jk_src directory\n",
                program_name,
                source_file_arg);
        exit(1);
    }
    paths.repo_root.size = (uint64_t)jk_src_offset;
    paths.repo_root.data = paths.source_file.data;

    paths.build = concat_strings(arena, paths.repo_root, JKS("build"));

    return paths;
}

static b32 is_space_exclude_newlines(int c)
{
    // We don't consider carraige returns and newlines because none of the things we're parsing are
    // allowed to span multiple lines.
    switch (c) {
    case ' ':
    case '\t':
    case '\v':
    case '\f':
        return 1;
    default:
        return 0;
    }
}

static int next_nonspace(JkBuffer file, uint64_t *cursor)
{
    int c;
    do {
        c = jk_buffer_character_next(file, cursor);
    } while (is_space_exclude_newlines(c));
    return c;
}

static b32 compare_case_insensitive(char *a, char *b)
{
    if (!(a && b)) {
        return 0;
    }
    int i;
    for (i = 0; a[i] != '\0' && b[i] != '\0'; i++) {
        if (tolower(a[i]) != tolower(b[i])) {
            return 0;
        }
    }
    return a[i] == '\0' && b[i] == '\0';
}

static void ensure_directory_exists(JkBuffer directory_path)
{
    char buffer[PATH_MAX];

    if (!directory_path.size) {
        fprintf(stderr, "%s: Attempted to create directory with empty name\n", program_name);
        exit(1);
    }

    if (PATH_MAX <= directory_path.size) {
        fprintf(stderr, "%s: Attempted to create path with too long a name\n", program_name);
        exit(1);
    }

    uint64_t i = 0;
    if (directory_path.data[i] == '/') {
        i++; // Skip leading slash which indicates an absolute path
    }
    while (i < directory_path.size) {
        while (i < directory_path.size && directory_path.data[i] != '/') {
            i++;
        }
        memcpy(buffer, directory_path.data, i);
        buffer[i] = '\0';

        if (mkdir(buffer, 0755) == -1 && errno != EEXIST) {
            fprintf(stderr,
                    "%s: Failed to create \"%s\": %s\n",
                    program_name,
                    buffer,
                    strerror(errno));
            exit(1);
        }

        i++;
    }
}

#define JK_SRC_STRING_LITERAL "jk_src/"
#define JK_GEN_STRING_LITERAL "jk_gen/"

static JkBuffer jk_build_string = JKSI("jk_build");
static JkBuffer jk_src_string = JKSI(JK_SRC_STRING_LITERAL);
static JkBuffer jk_gen_string = JKSI(JK_GEN_STRING_LITERAL);

static b32 parse_files(JkArena *storage,
        JkArena *scratch_arena,
        Paths paths,
        StringArrayBuilder *dependencies,
        StringArrayBuilder *nasm_files,
        StringArrayBuilder *compiler_arguments,
        StringArrayBuilder *linker_arguments)
{
    b32 single_translation_unit = 0;

    uint64_t path_index = 0;
    JkBuffer path = paths.source_file;
    do {
        JkArena file_arena = jk_arena_child_get(scratch_arena);
        JkBuffer file = jk_platform_file_read_full(
                &file_arena, jk_buffer_to_null_terminated(&file_arena, path));

        uint64_t pos = 0;
        b32 dependencies_open = 0;

        int char1;
        while ((char1 = jk_buffer_character_next(file, &pos)) != EOF) {
            int pound;
            if (char1 == '/') { // Parse control comment
                if (jk_buffer_character_next(file, &pos) != '/') {
                    goto reset_parse;
                }
                pound = next_nonspace(file, &pos);
            } else {
                pound = char1;
            }

            if (pound != '#') {
                goto reset_parse;
            }

            // Read control string
            JkBuffer control;
            {
                uint64_t start = pos;
                int c;
                while (!isspace(c = jk_buffer_character_get(file, pos))) {
                    if (c == EOF) {
                        goto end_of_file;
                    }
                    pos++;
                };
                control.size = pos - start;
                control.data = file.data + start;
            }

            int c0;
            while (is_space_exclude_newlines(c0 = jk_buffer_character_get(file, pos))) {
                pos++;
            }

            if (char1 == '/' && jk_buffer_compare(control, jk_build_string) == 0) {
                if (c0 == EOF || c0 == '\r' || c0 == '\n') {
                    fprintf(stderr,
                            "%s: Found #jk_build control comment with no command\n",
                            program_name);
                    fprintf(stderr, "%s: Usage: // %s\n", "#jk_build [command]", program_name);
                    exit(1);
                }

                // Read command
                JkBuffer command;
                {
                    uint64_t start = pos;
                    int c;
                    while (!isspace(c = jk_buffer_character_get(file, pos)) && c != EOF) {
                        pos++;
                    }
                    command.size = pos - start;
                    command.data = file.data + start;
                }

                b32 cmd_compiler_arguments =
                        jk_buffer_compare(command, JKS("compiler_arguments")) == 0;
                b32 cmd_linker_arguments = jk_buffer_compare(command, JKS("linker_arguments")) == 0;

                if (cmd_compiler_arguments || cmd_linker_arguments) {
                    if (path_index == 0) {
                        for (;;) {
                            int a0;
                            while (is_space_exclude_newlines(
                                    a0 = jk_buffer_character_get(file, pos))) {
                                pos++;
                            }
                            if (a0 == EOF) {
                                goto end_of_file;
                            }
                            if (a0 == '\r' || a0 == '\n') {
                                break;
                            }

                            // Read argument
                            JkBuffer argument;
                            {
                                uint64_t start = pos;
                                int c;
                                while (!isspace(c = jk_buffer_character_get(file, pos))
                                        && c != EOF) {
                                    pos++;
                                }
                                argument.size = pos - start;
                                argument.data = file.data + start;
                            }
                            string_array_builder_push(
                                    cmd_compiler_arguments ? compiler_arguments : linker_arguments,
                                    jk_buffer_copy(storage, argument));
                        }
                    }
                } else if (jk_buffer_compare(command, JKS("single_translation_unit")) == 0) {
                    if (path_index == 0) {
                        single_translation_unit = 1;
                    }
                } else if (jk_buffer_compare(command, JKS("dependencies_begin")) == 0) {
                    if (dependencies_open) {
                        fprintf(stderr,
                                "%s: Double '#jk_build dependencies_begin' control comments with "
                                "no '#jk_build dependencies_end' in between\n",
                                program_name);
                        exit(1);
                    }
                    dependencies_open = 1;
                } else if (jk_buffer_compare(command, JKS("dependencies_end")) == 0) {
                    if (dependencies_open) {
                        dependencies_open = 0;
                    } else {
                        fprintf(stderr,
                                "%s: Encountered '#jk_build dependencies_end' control comment when "
                                "there was no condition to end\n",
                                program_name);
                        exit(1);
                    }
                } else if (jk_buffer_compare(command, JKS("nasm")) == 0) {
                    {
                        int c;
                        while (is_space_exclude_newlines(c = jk_buffer_character_get(file, pos))) {
                            pos++;
                        }
                        if (c == '\n' || c == EOF) {
                            fprintf(stderr,
                                    "%s: '#jk_build nasm' expects a file path\n",
                                    program_name);
                            exit(1);
                        }
                    }

                    // Read nasm file path
                    JkBuffer nasm_file_path;
                    {
                        uint64_t start = pos;
                        int c;
                        while ((c = jk_buffer_character_get(file, pos)) != '\n' && c != '\r'
                                && c != EOF) {
                            pos++;
                        }
                        nasm_file_path.size = pos - start;
                        nasm_file_path.data = file.data + start;
                    }
                    back_slashes_to_forward_slashes(nasm_file_path);

                    string_array_builder_push(
                            nasm_files, concat_strings(storage, paths.repo_root, nasm_file_path));
                } else {
                    fprintf(stderr,
                            "%s: #jk_build control comment with unknown command \"%.*s\"\n",
                            program_name,
                            (int)command.size,
                            command.data);
                    exit(1);
                }
            } else if (char1 != '/' && jk_buffer_compare(control, JKS("include")) == 0) {
                if (c0 == EOF) {
                    goto end_of_file;
                } else if (c0 != '<') {
                    goto reset_parse;
                }
                pos++;

                // Read path
                JkBuffer include_path;
                {
                    uint64_t start = pos;
                    int c;
                    while ((c = jk_buffer_character_get(file, pos)) != '>') {
                        if (c == EOF) {
                            goto end_of_file;
                        }
                        if (c == '\r' || c == '\n') {
                            goto reset_parse;
                        }
                        pos++;
                    }
                    include_path.size = pos - start;
                    include_path.data = file.data + start;
                }
                back_slashes_to_forward_slashes(include_path);

                if (dependencies_open) {
                    JkBuffer prefix = {
                        .size = JK_MIN(include_path.size, jk_src_string.size),
                        .data = include_path.data,
                    };
                    if (jk_buffer_compare(prefix, jk_src_string) != 0
                            && jk_buffer_compare(prefix, jk_gen_string) != 0) {
                        fprintf(stderr,
                                "%s: Warning: Tried to use dependencies_begin on an include path "
                                "that did not start with 'jk_src/' or 'jk_gen/', ignoring <%.*s>\n",
                                program_name,
                                (int)include_path.size,
                                include_path.data);
                        goto reset_parse;
                    }

                    if (include_path.data[include_path.size - 2] != '.'
                            || include_path.data[include_path.size - 1] != 'h') {
                        fprintf(stderr,
                                "%s: Warning: Tried to use dependencies_begin on an include path "
                                "that did not end in '.h', ignoring <%.*s>\n",
                                program_name,
                                (int)include_path.size,
                                include_path.data);
                        goto reset_parse;
                    }

                    // Modify the extension from .h to .c
                    include_path.data[include_path.size - 1] = 'c';

                    // Check if already in dependencies
                    if (string_array_builder_contains(dependencies, include_path)) {
                        goto reset_parse;
                    }

                    string_array_builder_push(dependencies, jk_buffer_copy(storage, include_path));
                }
            }

        reset_parse:
            continue;
        }

    end_of_file:
        if (dependencies_open) {
            fprintf(stderr,
                    "%s: '#jk_build dependencies_begin' condition started but never ended. Use "
                    "'#jk_build dependencies_end' to mark where to stop.\n",
                    program_name);
            exit(1);
        }

        path = concat_strings(storage,
                paths.repo_root,
                string_array_builder_at_index(dependencies, path_index++));
    } while (path_index <= dependencies->count);

    return single_translation_unit;
}

int main(int argc, char **argv)
{
    program_name = argv[0];

    // Parse arguments
    Compiler compiler = COMPILER_NONE;
    char *source_file_arg = NULL;
    b32 optimize = 0;
    b32 no_profile = 0;
    {
        b32 help = 0;
        b32 usage_error = 0;
        b32 options_ended = 0;
        int non_option_arguments = 0;
        for (int i = 1; i < argc; i++) {
            char *compiler_string = NULL;
            b32 expect_compiler_string = 0;

            if (argv[i][0] == '-' && argv[i][1] != '\0' && !options_ended) { // Option argument
                if (argv[i][1] == '-') {
                    if (argv[i][2] == '\0') { // -- encountered
                        options_ended = 1;
                    } else { // Double hyphen option
                        char *name = &argv[i][2];
                        int end = 0;
                        while (name[end] != '=' && name[end] != '\0') {
                            end++;
                        }
                        if (strncmp(name, "compiler", end) == 0) {
                            expect_compiler_string = 1;

                            if (name[end] == '=') {
                                if (name[end + 1] != '\0') {
                                    compiler_string = &name[end + 1];
                                }
                            } else {
                                compiler_string = argv[++i];
                            }
                        } else if (strcmp(argv[i], "--help") == 0) {
                            help = 1;
                        } else if (strcmp(argv[i], "--optimize") == 0) {
                            optimize = 1;
                        } else if (strcmp(argv[i], "--no-profile") == 0) {
                            no_profile = 1;
                        } else {
                            fprintf(stderr, "%s: Invalid option '%s'\n", program_name, argv[i]);
                            usage_error = 1;
                        }
                    }
                } else { // Single-hypen option(s)
                    b32 has_argument = 0;
                    for (char *c = &argv[i][1]; *c != '\0' && !has_argument; c++) {
                        switch (*c) {
                        case 'c': {
                            has_argument = 1;
                            expect_compiler_string = 1;
                            compiler_string = ++c;
                            if (compiler_string[0] == '\0') {
                                compiler_string = argv[++i];
                            }
                        } break;

                        case 'O': {
                            optimize = 1;
                        } break;

                        default:
                            fprintf(stderr,
                                    "%s: Invalid option '%c' in '%s'\n",
                                    program_name,
                                    *c,
                                    argv[i]);
                            usage_error = 1;
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
                    } else if (compare_case_insensitive(compiler_string, "clang")) {
                        compiler = COMPILER_CLANG;
                    } else {
                        fprintf(stderr,
                                "%s: Option '-c, --compiler' given invalid argument '%s'\n",
                                argv[0],
                                compiler_string);
                        usage_error = 1;
                    }
                } else {
                    fprintf(stderr,
                            "%s: Option '-c, --compiler' missing required argument\n",
                            argv[0]);
                    usage_error = 1;
                }
            }
        }
        if (!help && non_option_arguments != 1) {
            fprintf(stderr,
                    "%s: Expected 1 non-option argument, got %d\n",
                    program_name,
                    non_option_arguments);
            usage_error = 1;
        }
        if (help || usage_error) {
            printf("NAME\n"
                   "\tjk_build - builds programs in jk_repo\n\n"
                   "SYNOPSIS\n"
                   "\tjk_build [-c gcc|msvc|tcc] [--no-profile] [-l|-O] FILE\n\n"
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
                   "\t\ttotal timing. Equivalent to\n"
                   "\t\t#define JK_PLATFORM_PROFILE_DISABLE 1\n\n"
                   "\t-O, --optimize\n"
                   "\t\tPrioritize the speed of the resulting executable over its\n"
                   "\t\tdebuggability and compilation speed.\n");
            exit(usage_error);
        }

        if (compiler == COMPILER_NONE) {
#ifdef _WIN32
            compiler = COMPILER_MSVC; // Default to MSVC on Windows
#elif __linux__
            compiler = COMPILER_GCC; // Default to GCC on Linux
#else
            compiler = COMPILER_CLANG; // Default to clang on macOS
#endif
        }
    }

    JkPlatformArenaVirtualRoot storage_root;
    JkArena storage = jk_platform_arena_virtual_init(&storage_root, 1 * GIGABYTE);

    JkPlatformArenaVirtualRoot scratch_arena_root;
    JkArena scratch_arena = jk_platform_arena_virtual_init(&scratch_arena_root, 1 * GIGABYTE);

    StringArrayBuilder dependencies;
    string_array_builder_init(&dependencies, &storage);
    StringArrayBuilder nasm_files;
    string_array_builder_init(&nasm_files, &storage);
    StringArrayBuilder compiler_arguments;
    string_array_builder_init(&compiler_arguments, &storage);
    StringArrayBuilder linker_arguments;
    string_array_builder_init(&linker_arguments, &storage);

    Paths paths = paths_get(&storage, source_file_arg);

    ensure_directory_exists(paths.build);
    {
        JkArena tmp_arena = jk_arena_child_get(&scratch_arena);
        if (chdir(jk_buffer_to_null_terminated(&tmp_arena, paths.build)) == -1) {
            fprintf(stderr,
                    "%s: Failed to change working directory to \"%.*s\": %s\n",
                    program_name,
                    (int)paths.build.size,
                    paths.build.data,
                    strerror(errno));
            exit(1);
        }
    }

    b32 is_objective_c_file = paths.source_file.data[paths.source_file.size - 1] == 'm';

    b32 single_translation_unit = parse_files(&storage,
            &scratch_arena,
            paths,
            &dependencies,
            &nasm_files,
            &compiler_arguments,
            &linker_arguments);

    JkBufferArray nasm_files_array = string_array_builder_build(&nasm_files);
    for (int i = 0; i < nasm_files_array.count; i++) {
        char output_path_buffer[16];
        JkBuffer output_path_relative = {
            .size = snprintf(output_path_buffer, sizeof(output_path_buffer), "/nasm%d.o", i),
            .data = (uint8_t *)output_path_buffer,
        };
        JkBuffer nasm_output_path = concat_strings(&storage, paths.build, output_path_relative);

        StringArrayBuilder nasm_command;
        string_array_builder_init(&nasm_command, &storage);

        append(&nasm_command, "nasm");
        append(&nasm_command, "-o");
        string_array_builder_push(&nasm_command, nasm_output_path);

        if (compiler == COMPILER_MSVC) {
            append(&nasm_command, "-f", "win64");
        } else {
            append(&nasm_command, "-f", "elf64");
        }

        string_array_builder_push(&nasm_command, nasm_files_array.items[i]);

        jk_platform_exec(string_array_builder_build(&nasm_command));

        if (compiler == COMPILER_MSVC) {
            StringArrayBuilder lib_command;
            string_array_builder_init(&lib_command, &storage);
            append(&lib_command, "lib", "/nologo");
            string_array_builder_push(&lib_command, nasm_output_path);
            jk_platform_exec(string_array_builder_build(&lib_command));
        }
    }

    // Compile command
    StringArrayBuilder command;
    string_array_builder_init(&command, &storage);

    switch (compiler) { // Compiler options
    case COMPILER_MSVC: {
        append(&command, "cl");

        append(&command, "/W4");
        append(&command, "/w44062");
        append(&command, "/wd4100");
        append(&command, "/wd4200");
        append(&command, "/wd4244");
        append(&command, "/wd4305");
        append(&command, "/wd4324");
        append(&command, "/wd4706");
        append(&command, "/nologo");
        append(&command, "/Gm-");
        append(&command, "/GR-");
        append(&command, "/D", "_CRT_SECURE_NO_WARNINGS");
        append(&command, "/Zi");
        append(&command, "/std:c11");
        append(&command, "/EHa-");
        if (optimize) {
            append(&command, "/MT");
            append(&command, "/O2");
            append(&command, "/GL");
        } else {
            append(&command, "/MTd");
            append(&command, "/Od");
        }
        if (!single_translation_unit) {
            append(&command, "/D", "JK_PUBLIC=");
        }
        if (no_profile) {
            append(&command, "/D", "JK_PLATFORM_PROFILE_DISABLE");
        }
        append(&command, "/I");
        string_array_builder_push(&command, paths.repo_root);
    } break;

    case COMPILER_GCC: {
        append(&command, "gcc");

        append(&command, "-o");
        string_array_builder_push(&command, paths.basename);

        append(&command, "-std=c11");
        append(&command, "-pedantic");
        append(&command, "-mfma");
        append(&command, "-g");
        append(&command, "-Wall");
        append(&command, "-Wextra");
        append(&command, "-fstack-protector");
        append(&command, "-fzero-init-padding-bits=all");
        append(&command, "-Werror=vla");
        append(&command, "-Wno-missing-braces");
        if (single_translation_unit) {
            append(&command, "-Wno-unused-function");
        } else {
            append(&command, "-D", "JK_PUBLIC=");
        }
        if (optimize) {
            append(&command, "-O3");
            append(&command, "-flto");
            append(&command, "-fuse-linker-plugin");
        } else {
            append(&command, "-Og");
        }
        if (no_profile) {
            append(&command, "-D", "JK_PLATFORM_PROFILE_DISABLE");
        }

        append(&command, "-I");
        string_array_builder_push(&command, paths.repo_root);
    } break;

    case COMPILER_TCC: {
        append(&command, "tcc");

#ifdef _WIN32
        JkBuffer basename = concat_strings(&storage, paths.basename, JKS(".exe"));
#else
        JkBuffer basename = paths.basename;
#endif
        append(&command, "-o");
        string_array_builder_push(&command, basename);

        append(&command, "-std=c11");
        append(&command, "-Wall");
        append(&command, "-g");

        if (!single_translation_unit) {
            append(&command, "-D", "JK_PUBLIC=");
        }
        if (no_profile) {
            append(&command, "-D", "JK_PLATFORM_PROFILE_DISABLE");
        }

        append(&command, "-I");
        string_array_builder_push(&command, paths.repo_root);
    } break;

    case COMPILER_CLANG: {
        append(&command, "clang");

#ifdef _WIN32
        JkBuffer basename = concat_strings(&storage, paths.basename, JKS(".exe"));
#else
        JkBuffer basename = paths.basename;
#endif
        append(&command, "-o");
        string_array_builder_push(&command, basename);

        append(&command, "-mfma");
        append(&command,
                "-Wall",
                "-Wextra",
                "-Wpedantic",
                "-Wno-missing-braces",
                "-Wno-missing-field-initializers");
        append(&command, "-g");
        if (!is_objective_c_file) {
            append(&command, "-std=c11");
        }
        if (single_translation_unit) {
            append(&command, "-Wno-unused-function");
        } else {
            append(&command, "-D", "JK_PUBLIC=");
        }
        if (optimize) {
            append(&command, "-O3");
            append(&command, "-flto");
        } else {
            append(&command, "-Og");
        }
        if (no_profile) {
            append(&command, "-D", "JK_PLATFORM_PROFILE_DISABLE");
        }

        append(&command, "-I");
        string_array_builder_push(&command, paths.repo_root);
    } break;

    case COMPILER_NONE: {
        fprintf(stderr, "%s: compiler should never be COMPILER_NONE by this point\n", argv[0]);
        exit(1);
    } break;
    }

    string_array_builder_concat(&command, &compiler_arguments);

    if (single_translation_unit) {
        // Get path for the single translation unit root source file
        JkBuffer stu_file_path;
        {
            stu_file_path = jk_buffer_copy(&storage, paths.source_file);
            _Static_assert(sizeof(JK_GEN_STRING_LITERAL) == sizeof(JK_SRC_STRING_LITERAL),
                    "required for this simple find and replace to work");
            uint64_t jk_src_index = jk_string_find(stu_file_path, jk_src_string);
            memcpy(stu_file_path.data + jk_src_index,
                    JK_GEN_STRING_LITERAL,
                    sizeof(JK_GEN_STRING_LITERAL) - 1);

            stu_file_path.size--;

            stu_file_path = concat_strings(
                    &storage, stu_file_path, is_objective_c_file ? JKS("stu.m") : JKS("stu.c"));
        }

        // Get the directory
        JkBuffer stu_directory;
        {
            int64_t last_slash = stu_file_path.size - 1;
            while (0 <= last_slash && stu_file_path.data[last_slash] != '/') {
                last_slash--;
            }
            stu_directory.size = last_slash;
            stu_directory.data = stu_file_path.data;
        }

        ensure_directory_exists(stu_directory);

        FILE *stu_file = fopen(jk_buffer_to_null_terminated(&storage, stu_file_path), "wb");
        if (stu_file == NULL) {
            fprintf(stderr,
                    "%s: Failed to open '%.*s': %s\n",
                    program_name,
                    (int)stu_file_path.size,
                    stu_file_path.data,
                    strerror(errno));
            exit(1);
        }

        fprintf(stu_file, "#define JK_PUBLIC static\n");
        JkBuffer source_file_path_relative = {
            .size = paths.source_file.size - paths.repo_root.size,
            .data = paths.source_file.data + paths.repo_root.size,
        };
        fprintf(stu_file,
                "#include <%.*s>\n",
                (int)source_file_path_relative.size,
                source_file_path_relative.data);
        JkBufferArray dependencies_array = string_array_builder_build(&dependencies);
        for (int i = 0; i < dependencies_array.count; i++) {
            fprintf(stu_file,
                    "#include <%.*s>\n",
                    (int)dependencies_array.items[i].size,
                    dependencies_array.items[i].data);
        }

        fclose(stu_file);

        string_array_builder_push(&command, stu_file_path);
    } else {
        string_array_builder_push(&command, paths.source_file);

        // Prepend repo_root absolute path to all of the dependencies' paths
        for (StringNode *node = dependencies.tail.previous; node; node = node->previous) {
            node->string = concat_strings(&storage, paths.repo_root, node->string);
        }

        string_array_builder_concat(&command, &dependencies);
    }

    switch (compiler) { // Linker options
    case COMPILER_MSVC: {
        append(&command, "/link");

        JkBuffer out_option = concat_strings(
                &storage, concat_strings(&storage, JKS("/OUT:"), paths.basename), JKS(".exe"));
        string_array_builder_push(&command, out_option);

        append(&command, "/INCREMENTAL:NO");

        JkBuffer lib_path = concat_strings(
                &storage, concat_strings(&storage, JKS("/LIBPATH:\""), paths.repo_root), JKS("\""));
        string_array_builder_push(&command, lib_path);

        append(&command, "Advapi32.lib");
    } break;

    case COMPILER_GCC: {
        append(&command, "-lm");
    } break;

    case COMPILER_TCC: {
#ifndef _WIN32
        append(&command, "-lm");
#endif
    } break;

    case COMPILER_CLANG: {
    } break;

    case COMPILER_NONE: {
        fprintf(stderr, "%s: compiler should never be COMPILER_NONE by this point\n", argv[0]);
        exit(1);
    } break;
    }

    string_array_builder_concat(&command, &linker_arguments);

    for (int i = 0; i < nasm_files.count; i++) {
        char file_name[32];
        snprintf(file_name, 32, "nasm%d.%s", i, compiler == COMPILER_MSVC ? "lib" : "o");
        append(&command, file_name);
    }

    return jk_platform_exec(string_array_builder_build(&command));
}
