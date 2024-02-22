#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define UTF_8_CONT_PREFIX_MASK 0xf000
#define UTF_8_CONT_PREFIX_VALUE 0x8000

#define ARRAY_COUNT(array) (sizeof(array) / sizeof((array)[0]))

char *program_name = "<global program_name should be overwritten with argv[0]>";

typedef enum JkJsonTokenType {
    JK_JSON_TOKEN_EOF,
    JK_JSON_TOKEN_STRING,
    JK_JSON_TOKEN_NUMBER,
    JK_JSON_TOKEN_TRUE,
    JK_JSON_TOKEN_FALSE,
    JK_JSON_TOKEN_NULL,
    JK_JSON_TOKEN_COMMA,
    JK_JSON_TOKEN_COLON,
    JK_JSON_TOKEN_OPEN_BRACE,
    JK_JSON_TOKEN_CLOSE_BRACE,
    JK_JSON_TOKEN_OPEN_BRACKET,
    JK_JSON_TOKEN_CLOSE_BRACKET,
    JK_JSON_TOKEN_TYPE_COUNT,
} JkJsonTokenType;

static char *jk_json_token_strings[JK_JSON_TOKEN_TYPE_COUNT] = {
    "EOF",
    "STRING",
    "NUMBER",
    "TRUE",
    "FALSE",
    "NULL",
    ",",
    ":",
    "{",
    "}",
    "[",
    "]",
};

typedef struct JkJsonToken {
    JkJsonTokenType type;
    union {
        char *string;
        double number;
    } value;
} JkJsonToken;

static void jk_json_print_token(FILE *file, JkJsonToken *token)
{
    if (token->type == JK_JSON_TOKEN_NUMBER) {
        fprintf(file, "%f", token->value.number);
        return;
    }
    fprintf(file, "%s", jk_json_token_strings[token->type]);
}

static bool jk_json_is_whitespace(int byte)
{
    return byte == ' ' || byte == '\n' || byte == '\r' || byte == '\t';
}

static bool jk_utf8_is_cont(int byte)
{
    return (byte & UTF_8_CONT_PREFIX_MASK) == UTF_8_CONT_PREFIX_VALUE;
}

#define JK_JSON_CMP_STRING_LENGTH 6

typedef struct JkJsonExactMatch {
    char *string;
    long length;
    JkJsonTokenType token_type;
} JkJsonExactMatch;

static JkJsonExactMatch jk_json_exact_matches[] = {
    {
        .string = "true",
        .length = sizeof("true") - 1,
        .token_type = JK_JSON_TOKEN_TRUE,
    },
    {
        .string = "false",
        .length = sizeof("false") - 1,
        .token_type = JK_JSON_TOKEN_FALSE,
    },
    {
        .string = "null",
        .length = sizeof("null") - 1,
        .token_type = JK_JSON_TOKEN_NULL,
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

typedef enum JkJsonLexResultType {
    JK_JSON_LEX_SUCCESS,
    JK_JSON_LEX_UNEXPECTED_CHARACTER,
    JK_JSON_LEX_CHARACTER_NOT_FOLLOWED_BY_DIGIT,
    JK_JSON_LEX_RESULT_TYPE_COUNT,
} JkJsonLexResultType;

typedef struct JkJsonLexResult {
    JkJsonLexResultType type;
    int c;
    int c_to_be_followed_by_digit;
} JkJsonLexResult;

JkJsonLexResult jk_json_lex(size_t (*stream_read)(void *stream, size_t byte_count, void *buffer),
        int (*stream_seek_relative)(void *stream, long offset),
        void *stream,
        JkJsonToken *token)
{
    JkJsonLexResult r = {.type = JK_JSON_LEX_SUCCESS};
    char cmp_buffer[JK_JSON_CMP_STRING_LENGTH + 1] = {'\0'};

    do {
        r.c = jk_json_getc(stream_read, stream);
    } while (jk_json_is_whitespace(r.c));

    switch (r.c) {
    case '"': {
        printf("\n");
        fprintf(stderr, "%s: string not implemented\n", program_name);
        exit(1);
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

        token->type = JK_JSON_TOKEN_NUMBER;
        token->value.number = 0.0;

        if (r.c == '-') {
            sign = -1.0;

            r.c_to_be_followed_by_digit = r.c;
            r.c = jk_json_getc(stream_read, stream);

            if (!isdigit(r.c)) {
                r.type = JK_JSON_LEX_CHARACTER_NOT_FOLLOWED_BY_DIGIT;
                return r;
            }
        }

        // Parse integer
        do {
            token->value.number = (token->value.number * 10.0) + (r.c - '0');
        } while (isdigit((r.c = jk_json_getc(stream_read, stream))));

        // Parse fraction if there is one
        if (r.c == '.') {
            r.c_to_be_followed_by_digit = r.c;
            r.c = jk_json_getc(stream_read, stream);

            if (!isdigit(r.c)) {
                r.type = JK_JSON_LEX_CHARACTER_NOT_FOLLOWED_BY_DIGIT;
                return r;
            }

            double multiplier = 0.1;
            do {
                token->value.number += (r.c - '0') * multiplier;
                multiplier /= 10.0;
            } while (isdigit((r.c = jk_json_getc(stream_read, stream))));
        }

        // Parse exponent if there is one
        if (r.c == 'e' || r.c == 'E') {
            r.c_to_be_followed_by_digit = r.c;
            r.c = jk_json_getc(stream_read, stream);

            if ((r.c == '-' || r.c == '+')) {
                if (r.c == '-') {
                    exponent_sign = -1.0;
                }
                r.c_to_be_followed_by_digit = r.c;
                r.c = jk_json_getc(stream_read, stream);
            }

            if (!isdigit(r.c)) {
                r.type = JK_JSON_LEX_CHARACTER_NOT_FOLLOWED_BY_DIGIT;
                return r;
            }

            do {
                exponent = (exponent * 10.0) + (r.c - '0');
            } while (isdigit((r.c = jk_json_getc(stream_read, stream))));
        }

        // Number should not be directly followed by more alphanumeric characters
        if (isalnum(r.c)) {
            r.type = JK_JSON_LEX_UNEXPECTED_CHARACTER;
            return r;
        }

        if (r.c != EOF) {
            stream_seek_relative(stream, -1);
        }
        token->value.number = sign * token->value.number * pow(10.0, exponent_sign * exponent);
    } break;
    case 't':
    case 'f':
    case 'n': {
        memset(cmp_buffer, 0, sizeof(cmp_buffer));
        cmp_buffer[0] = (char)r.c;
        long read_count = (long)stream_read(stream, JK_JSON_CMP_STRING_LENGTH - 1, cmp_buffer + 1);
        for (int i = 0; i < ARRAY_COUNT(jk_json_exact_matches); i++) {
            JkJsonExactMatch *match = &jk_json_exact_matches[i];
            if (strncmp(cmp_buffer, match->string, match->length) == 0) {
                // If the next character is still a word character, the word we're matching with is
                // longer than the keyword, so not actually a match.
                if (isalnum(cmp_buffer[match->length])) {
                    stream_seek_relative(stream,
                            -jk_min(JK_JSON_CMP_STRING_LENGTH - match->length - 1, read_count));
                    r.type = JK_JSON_LEX_UNEXPECTED_CHARACTER;
                    r.c = cmp_buffer[match->length];
                    return r;
                }

                stream_seek_relative(
                        stream, -jk_min(JK_JSON_CMP_STRING_LENGTH - match->length, read_count));
                token->type = match->token_type;
                return r;
            }
        }
        stream_seek_relative(stream, -read_count);
        r.type = JK_JSON_LEX_UNEXPECTED_CHARACTER;
        return r;
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
        r.type = JK_JSON_LEX_UNEXPECTED_CHARACTER;
    } break;
    }

    return r;
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

    FILE *file = fopen("./json.json", "rb");

    JkJsonToken token;
    do {
        JkJsonLexResult result =
                jk_json_lex(stream_read_file, stream_seek_relative_file, file, &token);
        if (result.type == JK_JSON_LEX_SUCCESS) {
            jk_json_print_token(stdout, &token);
            printf(" ");
        } else {
            printf("\n");
            fprintf(stderr, "%s: Unexpected ", program_name);
            jk_json_print_c(stderr, result.c);
            if (result.type == JK_JSON_LEX_CHARACTER_NOT_FOLLOWED_BY_DIGIT) {
                fprintf(stderr,
                        ": Expected '%c' to be followed by a digit",
                        result.c_to_be_followed_by_digit);
            }
            fprintf(stderr, "\n");
            exit(1);
        }
    } while (token.type != JK_JSON_TOKEN_EOF);

    return 0;
}
