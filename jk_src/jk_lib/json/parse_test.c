#include <stdio.h>
#include <stdlib.h>

#include <jk_src/jk_lib/json/json.c>

#ifdef _WIN32
#include <windows.h>
#endif

static size_t stream_read_file(FILE *file, size_t byte_count, void *buffer)
{
    return fread(buffer, 1, byte_count, file);
}

static int stream_seek_relative_file(FILE *file, long offset)
{
    return fseek(file, offset, SEEK_CUR);
}

int main(int argc, char **argv)
{
    char *program_name = argv[0];

#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    FILE *file = fopen("./parse_test.json", "rb");

    JkArena arena;
    jk_arena_init(&arena, (size_t)1 << 36);

    JkJsonParseError error;
    JkJson *json = jk_json_parse(&arena, stream_read_file, stream_seek_relative_file, file, &error);
    if (json) {
        jk_json_print(stdout, json);
    } else {
        switch (error.type) {
        case JK_JSON_PARSE_UNEXPECTED_TOKEN: {
            fprintf(stderr, "%s: Unexpected token '", program_name);
            jk_json_print_token(stderr, &error.token);
            fprintf(stderr, "'\n");
            exit(1);
        } break;
        case JK_JSON_PARSE_LEX_ERROR: {
            fprintf(stderr, "%s: Lex error\n", program_name);
            exit(1);
        } break;
        case JK_JSON_PARSE_ERROR_TYPE_COUNT: {
            fprintf(stderr, "%s: Invalid parse error type\n", program_name);
            exit(1);
        } break;
        }
    }

    return 0;
}
