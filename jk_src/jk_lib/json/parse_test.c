#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <jk_gen/single_translation_unit.h>

// #jk_build dependencies_begin
#include <jk_src/jk_lib/json/json.h>
// #jk_build dependencies_end

#ifdef _WIN32
#include <windows.h>
#endif

static size_t stream_read_file(void *file, size_t byte_count, void *buffer)
{
    FILE *file_internal = file;
    return fread(buffer, 1, byte_count, file_internal);
}

static int stream_seek_relative_file(void *file, long offset)
{
    FILE *file_internal = file;
    return fseek(file_internal, offset, SEEK_CUR);
}

int main(int argc, char **argv)
{
    (void)argc;

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
        printf("\n");
        assert(json->type == JK_JSON_COLLECTION);
        JkJson *smokes = jk_json_member_get(&json->u.collection, "smokes");
        assert(smokes && smokes->type == JK_JSON_FALSE);
        assert(jk_json_member_get(&json->u.collection, "this_member_does_not_exist") == NULL);
    } else {
        switch (error.error_type) {
        case JK_JSON_PARSE_UNEXPECTED_TOKEN: {
            fprintf(stderr, "%s: Unexpected token '", argv[0]);
            jk_json_print_token(stderr, &error.token);
            fprintf(stderr, "'\n");
            exit(1);
        } break;
        case JK_JSON_PARSE_LEX_ERROR: {
            fprintf(stderr, "%s: Lex error\n", argv[0]);
            exit(1);
        } break;
        case JK_JSON_PARSE_ERROR_TYPE_COUNT: {
            fprintf(stderr, "%s: Invalid parse error type\n", argv[0]);
            exit(1);
        } break;
        }
    }

    return 0;
}
