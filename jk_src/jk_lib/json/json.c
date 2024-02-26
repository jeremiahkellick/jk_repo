#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <jk_src/jk_lib/arena/arena.c>
#include <jk_src/jk_lib/string/utf8.c>

#ifdef _WIN32
#include <windows.h>
#endif

#define JK_JSON_CMP_STRING_LENGTH 6

#define ARRAY_COUNT(array) (sizeof(array) / sizeof((array)[0]))

char *program_name = "<global program_name should be overwritten with argv[0]>";

typedef enum JkJsonValueType {
    JK_JSON_VALUE_OBJECT,
    JK_JSON_VALUE_ARRAY,
    JK_JSON_VALUE_STRING,
    JK_JSON_VALUE_NUMBER,
    JK_JSON_VALUE_TRUE,
    JK_JSON_VALUE_FALSE,
    JK_JSON_VALUE_NULL,
    JK_JSON_VALUE_TYPE_COUNT,
} JkJsonValueType;

static char *jk_json_value_strings[JK_JSON_VALUE_TYPE_COUNT] = {
    "OBJECT",
    "ARRAY",
    "STRING",
    "NUMBER",
    "TRUE",
    "FALSE",
    "NULL",
};

typedef struct JkJsonValue {
    JkJsonValueType type;
    union {
        char *string;
        double number;
    } u;
} JkJsonValue;

typedef enum JkJsonTokenType {
    JK_JSON_TOKEN_VALUE,
    JK_JSON_TOKEN_COMMA,
    JK_JSON_TOKEN_COLON,
    JK_JSON_TOKEN_OPEN_BRACE,
    JK_JSON_TOKEN_CLOSE_BRACE,
    JK_JSON_TOKEN_OPEN_BRACKET,
    JK_JSON_TOKEN_CLOSE_BRACKET,
    JK_JSON_TOKEN_EOF,
    JK_JSON_TOKEN_TYPE_COUNT,
} JkJsonTokenType;

typedef struct JkJsonToken {
    JkJsonTokenType type;
    JkJsonValue value;
} JkJsonToken;

static char *jk_json_token_strings[JK_JSON_TOKEN_TYPE_COUNT] = {
    "VALUE",
    ",",
    ":",
    "{",
    "}",
    "[",
    "]",
    "EOF",
};

static void jk_json_print_token(FILE *file, JkJsonToken *token)
{
    if (token->type == JK_JSON_TOKEN_VALUE) {
        if (token->value.type == JK_JSON_VALUE_STRING) {
            fprintf(file, "\"%s\"", token->value.u.string);
            return;
        }
        if (token->value.type == JK_JSON_VALUE_NUMBER) {
            fprintf(file, "%f", token->value.u.number);
            return;
        }
        fprintf(file, "%s", jk_json_value_strings[token->value.type]);
    } else {
        fprintf(file, "%s", jk_json_token_strings[token->type]);
    }
}

static bool jk_json_is_whitespace(int byte)
{
    return byte == ' ' || byte == '\n' || byte == '\r' || byte == '\t';
}

typedef struct JkJsonExactMatch {
    char *string;
    long length;
    JkJsonValueType value_type;
} JkJsonExactMatch;

static JkJsonExactMatch jk_json_exact_matches[] = {
    {
        .string = "true",
        .length = sizeof("true") - 1,
        .value_type = JK_JSON_VALUE_TRUE,
    },
    {
        .string = "false",
        .length = sizeof("false") - 1,
        .value_type = JK_JSON_VALUE_FALSE,
    },
    {
        .string = "null",
        .length = sizeof("null") - 1,
        .value_type = JK_JSON_VALUE_NULL,
    },
};

static long jk_min(long a, long b)
{
    return a < b ? a : b;
}

static int jk_json_getc(
        size_t (*stream_read)(void *stream, size_t byte_count, void *buffer), void *stream)
{
    char c;
    return stream_read(stream, 1, &c) ? (int)c : EOF;
}

static void jk_json_print_c(FILE *file, int c)
{
    if (c == EOF) {
        fprintf(file, "end of file");
    } else {
        fprintf(file, "character '%c'", c);
    }
}

typedef enum JkJsonLexStatus {
    JK_JSON_LEX_SUCCESS,
    JK_JSON_LEX_UNEXPECTED_CHARACTER,
    JK_JSON_LEX_UNEXPECTED_CHARACTER_IN_STRING,
    JK_JSON_LEX_INVALID_ESCAPE_CHARACTER,
    JK_JSON_LEX_INVALID_UNICODE_ESCAPE,
    JK_JSON_LEX_CHARACTER_NOT_FOLLOWED_BY_DIGIT,
    JK_JSON_LEX_RESULT_TYPE_COUNT,
} JkJsonLexStatus;

typedef struct JkJsonLexErrorData {
    int c;
    int c_to_be_followed_by_digit;
} JkJsonLexErrorData;

JkJsonLexStatus jk_json_lex(JkArena *arena,
        size_t (*stream_read)(void *stream, size_t byte_count, void *buffer),
        int (*stream_seek_relative)(void *stream, long offset),
        void *stream,
        JkJsonToken *token,
        JkJsonLexErrorData *error_data)
{
    JkJsonLexErrorData *e = error_data;
    char cmp_buffer[JK_JSON_CMP_STRING_LENGTH + 1] = {'\0'};

    do {
        e->c = jk_json_getc(stream_read, stream);
    } while (jk_json_is_whitespace(e->c));

    switch (e->c) {
    case '"': {
        char *c = jk_arena_push(arena, 1);
        char *string = c;
        while ((e->c = jk_json_getc(stream_read, stream)) != '"') {
            if (e->c < 0x20 || e->c == EOF) {
                return JK_JSON_LEX_UNEXPECTED_CHARACTER_IN_STRING;
            }
            if (e->c == '\\') {
                e->c = jk_json_getc(stream_read, stream);
                switch (e->c) {
                case '"':
                case '\\':
                case '/': {
                    *c = (char)e->c;
                } break;

                case 'b': {
                    *c = '\b';
                } break;
                case 'f': {
                    *c = '\f';
                } break;
                case 'n': {
                    *c = '\n';
                } break;
                case 'r': {
                    *c = '\r';
                } break;
                case 't': {
                    *c = '\t';
                } break;

                case 'u': {
                    uint32_t unicode32 = 0;
                    for (int i = 0; i < 4; i++) {
                        int digit_value;
                        e->c = jk_json_getc(stream_read, stream);
                        int lowered = tolower(e->c);
                        if (lowered >= 'a' && lowered <= 'f') {
                            digit_value = 10 + (lowered - 'a');
                        } else if (e->c >= '0' && e->c <= '9') {
                            digit_value = e->c - '0';
                        } else {
                            return JK_JSON_LEX_INVALID_UNICODE_ESCAPE;
                        }
                        unicode32 = unicode32 * 0x10 + digit_value;
                    }
                    JkUtf8Codepoint utf8;
                    jk_utf8_codepoint_encode(unicode32, &utf8);
                    *c = utf8.b[0];
                    for (int i = 1; i < 4 && jk_utf8_byte_is_continuation(utf8.b[i]); i++) {
                        c = jk_arena_push(arena, 1);
                        *c = utf8.b[i];
                    }
                } break;

                default: {
                    return JK_JSON_LEX_INVALID_ESCAPE_CHARACTER;
                } break;
                }
            } else {
                *c = (char)e->c;
            }
            c = jk_arena_push(arena, 1);
        }
        *c = '\0';
        token->type = JK_JSON_TOKEN_VALUE;
        token->value.type = JK_JSON_VALUE_STRING;
        token->value.u.string = string;
    } break;
    case '-':
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9': {
        double sign = 1.0;
        double exponent = 0.0;
        double exponent_sign = 1.0;

        token->type = JK_JSON_TOKEN_VALUE;
        token->value.type = JK_JSON_VALUE_NUMBER;
        token->value.u.number = 0.0;

        if (e->c == '-') {
            sign = -1.0;

            e->c_to_be_followed_by_digit = e->c;
            e->c = jk_json_getc(stream_read, stream);

            if (!isdigit(e->c)) {
                return JK_JSON_LEX_CHARACTER_NOT_FOLLOWED_BY_DIGIT;
            }
        }

        // Parse integer
        do {
            token->value.u.number = (token->value.u.number * 10.0) + (e->c - '0');
        } while (isdigit((e->c = jk_json_getc(stream_read, stream))));

        // Parse fraction if there is one
        if (e->c == '.') {
            e->c_to_be_followed_by_digit = e->c;
            e->c = jk_json_getc(stream_read, stream);

            if (!isdigit(e->c)) {
                return JK_JSON_LEX_CHARACTER_NOT_FOLLOWED_BY_DIGIT;
            }

            double multiplier = 0.1;
            do {
                token->value.u.number += (e->c - '0') * multiplier;
                multiplier /= 10.0;
            } while (isdigit((e->c = jk_json_getc(stream_read, stream))));
        }

        // Parse exponent if there is one
        if (e->c == 'e' || e->c == 'E') {
            e->c_to_be_followed_by_digit = e->c;
            e->c = jk_json_getc(stream_read, stream);

            if ((e->c == '-' || e->c == '+')) {
                if (e->c == '-') {
                    exponent_sign = -1.0;
                }
                e->c_to_be_followed_by_digit = e->c;
                e->c = jk_json_getc(stream_read, stream);
            }

            if (!isdigit(e->c)) {
                return JK_JSON_LEX_CHARACTER_NOT_FOLLOWED_BY_DIGIT;
            }

            do {
                exponent = (exponent * 10.0) + (e->c - '0');
            } while (isdigit((e->c = jk_json_getc(stream_read, stream))));
        }

        // Number should not be directly followed by more alphanumeric characters
        if (isalnum(e->c)) {
            return JK_JSON_LEX_UNEXPECTED_CHARACTER;
        }

        if (e->c != EOF) {
            stream_seek_relative(stream, -1);
        }
        token->value.u.number = sign * token->value.u.number * pow(10.0, exponent_sign * exponent);
    } break;
    case 't':
    case 'f':
    case 'n': {
        memset(cmp_buffer, 0, sizeof(cmp_buffer));
        cmp_buffer[0] = (char)e->c;
        long read_count = (long)stream_read(stream, JK_JSON_CMP_STRING_LENGTH - 1, cmp_buffer + 1);
        for (int i = 0; i < ARRAY_COUNT(jk_json_exact_matches); i++) {
            JkJsonExactMatch *match = &jk_json_exact_matches[i];
            if (strncmp(cmp_buffer, match->string, match->length) == 0
                    && !isalnum(cmp_buffer[match->length])) {
                stream_seek_relative(
                        stream, -jk_min(JK_JSON_CMP_STRING_LENGTH - match->length, read_count));
                token->type = JK_JSON_TOKEN_VALUE;
                token->value.type = match->value_type;
                return JK_JSON_LEX_SUCCESS;
            }
        }
        stream_seek_relative(stream, -read_count);
        return JK_JSON_LEX_UNEXPECTED_CHARACTER;
    } break;
    case ',': {
        token->type = JK_JSON_TOKEN_COMMA;
    } break;
    case ':': {
        token->type = JK_JSON_TOKEN_COLON;
    } break;
    case '{': {
        token->type = JK_JSON_TOKEN_OPEN_BRACE;
    } break;
    case '}': {
        token->type = JK_JSON_TOKEN_CLOSE_BRACE;
    } break;
    case '[': {
        token->type = JK_JSON_TOKEN_OPEN_BRACKET;
    } break;
    case ']': {
        token->type = JK_JSON_TOKEN_CLOSE_BRACKET;
    } break;
    case EOF: {
        token->type = JK_JSON_TOKEN_EOF;
    } break;
    default: {
        return JK_JSON_LEX_UNEXPECTED_CHARACTER;
    } break;
    }

    return JK_JSON_LEX_SUCCESS;
}

size_t stream_read_file(FILE *file, size_t byte_count, void *buffer)
{
    return fread(buffer, 1, byte_count, file);
}

int stream_seek_relative_file(FILE *file, long offset)
{
    return fseek(file, offset, SEEK_CUR);
}

int main(int argc, char **argv)
{
    program_name = argv[0];

#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    FILE *file = fopen("./json.json", "rb");

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
            fprintf(stderr, "%s: Unexpected ", program_name);
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
