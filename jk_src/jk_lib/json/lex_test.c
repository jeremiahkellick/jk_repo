#include <stdio.h>
#include <stdlib.h>

#include <jk_gen/single_translation_unit.h>

// #jk_build dependencies_begin
#include <jk_src/jk_lib/json/json.h>
// #jk_build dependencies_end

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

static void jk_json_print_c(FILE *file, int c)
{
    if (c == EOF) {
        fprintf(file, "end of file");
    } else {
        fprintf(file, "character '%c'", c);
    }
}

int main(int argc, char **argv)
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    FILE *file = fopen("./lex_test.json", "rb");

    JkArena arena;
    jk_arena_init(&arena, (size_t)1 << 36);

    JkJsonToken token;
    do {
        JkJsonLexErrorData error_data;
        JkJsonLexStatus lex_status = jk_json_lex(
                &arena, stream_read_file, stream_seek_relative_file, file, &token, &error_data);
        if (lex_status == JK_JSON_LEX_SUCCESS) {
            jk_json_print_token(stdout, &token);
            printf(" ");
        } else {
            printf("\n");
            fprintf(stderr, "%s: Unexpected ", argv[0]);
            jk_json_print_c(stderr, error_data.c);
            if (lex_status == JK_JSON_LEX_UNEXPECTED_CHARACTER_IN_STRING) {
                fprintf(stderr,
                        ": Invalid character in string. You may have missed a closing double "
                        "quote.");
            } else if (lex_status == JK_JSON_LEX_INVALID_ESCAPE_CHARACTER) {
                fprintf(stderr, ": '\\' must be followed by a valid escape character\n");
            } else if (lex_status == JK_JSON_LEX_INVALID_UNICODE_ESCAPE) {
                fprintf(stderr, ": '\\u' escape must be followed by 4 hexadecimal digits\n");
            } else if (lex_status == JK_JSON_LEX_CHARACTER_NOT_FOLLOWED_BY_DIGIT) {
                fprintf(stderr,
                        ": Expected '%c' to be followed by a digit",
                        error_data.c_to_be_followed_by_digit);
            }
            fprintf(stderr, "\n");
            exit(1);
        }
    } while (token.type != JK_JSON_TOKEN_EOF);

    return 0;
}
