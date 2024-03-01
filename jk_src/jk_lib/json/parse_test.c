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
    jk_json_name = argv[0];

#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    FILE *file = fopen("./parse_test.json", "rb");

    JkArena storage;
    JkArena tmp_storage;
    jk_arena_init(&storage, (size_t)1 << 35);
    jk_arena_init(&tmp_storage, (size_t)1 << 35);

    JkJsonParseData error;
    JkJson *json = jk_json_parse(
            &storage, &tmp_storage, stream_read_file, stream_seek_relative_file, file, &error);
    if (json) {
        jk_json_print(stdout, json, 0);
    } else {
        switch (error.error_type) {
        case JK_JSON_PARSE_UNEXPECTED_TOKEN: {
            fprintf(stderr, "%s: Unexpected token '", jk_json_name);
            jk_json_print_token(stderr, &error.token);
            fprintf(stderr, "'\n");
            exit(1);
        } break;
        case JK_JSON_PARSE_LEX_ERROR: {
            fprintf(stderr, "%s: Lex error\n", jk_json_name);
            exit(1);
        } break;
        case JK_JSON_PARSE_ERROR_TYPE_COUNT: {
            fprintf(stderr, "%s: Invalid parse error type\n", jk_json_name);
            exit(1);
        } break;
        }
    }

    return 0;
}