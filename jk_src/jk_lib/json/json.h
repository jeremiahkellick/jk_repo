#ifndef JK_JSON_H
#define JK_JSON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include <jk_src/jk_lib/arena/arena.h>

typedef enum JkJsonType {
    JK_JSON_OBJECT,
    JK_JSON_ARRAY,
    JK_JSON_STRING,
    JK_JSON_NUMBER,
    JK_JSON_TRUE,
    JK_JSON_FALSE,
    JK_JSON_NULL,
    JK_JSON_TYPE_COUNT,
} JkJsonType;

typedef struct JkJson JkJson;

typedef struct JkJsonMember {
    char *name;
    JkJson *value;
} JkJsonMember;

typedef struct JkJsonObject {
    size_t member_count;
    JkJsonMember *members;
} JkJsonObject;

typedef struct JkJsonArray {
    size_t length;
    JkJson **elements;
} JkJsonArray;

struct JkJson {
    JkJsonType type;
    union {
        JkJsonArray array;
        JkJsonObject object;
        char *string;
        double number;
    } u;
};

extern char *jk_json_value_strings[JK_JSON_TYPE_COUNT];

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
    JkJson *value;
} JkJsonToken;

extern char *jk_json_token_strings[JK_JSON_TOKEN_TYPE_COUNT];

bool jk_json_is_whitespace(int byte);

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

void jk_json_print_token(FILE *file, JkJsonToken *token);

void jk_json_print(FILE *file, JkJson *json, int indent_level);

JkJsonLexStatus jk_json_lex(JkArena *storage,
        size_t (*stream_read)(void *stream, size_t byte_count, void *buffer),
        int (*stream_seek_relative)(void *stream, long offset),
        void *stream,
        JkJsonToken *token,
        JkJsonLexErrorData *error_data);

typedef enum JkJsonParseErrorType {
    JK_JSON_PARSE_UNEXPECTED_TOKEN,
    JK_JSON_PARSE_LEX_ERROR,
    JK_JSON_PARSE_ERROR_TYPE_COUNT,
} JkJsonParseErrorType;

typedef struct JkJsonParseData {
    JkJsonParseErrorType error_type;
    JkJsonToken token;
    JkJsonLexStatus lex_status;
    JkJsonLexErrorData lex_error_data;
} JkJsonParseData;

JkJson *jk_json_parse(JkArena *storage,
        JkArena *tmp_storage,
        size_t (*stream_read)(void *stream, size_t byte_count, void *buffer),
        int (*stream_seek_relative)(void *stream, long offset),
        void *stream,
        JkJsonParseData *data);

#endif
