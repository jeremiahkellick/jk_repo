#ifndef JK_JSON_H
#define JK_JSON_H

#include <jk_src/jk_lib/jk_lib.h>

typedef enum JkJsonType {
    JK_JSON_INVALID,
    JK_JSON_OBJECT,
    JK_JSON_ARRAY,
    JK_JSON_STRING,
    JK_JSON_NUMBER,
    JK_JSON_TRUE,
    JK_JSON_FALSE,
    JK_JSON_NULL,
    JK_JSON_TYPE_COUNT,
} JkJsonType;

typedef struct JkJson {
    JkJsonType type;
    JkBuffer name;
    int64_t child_count;
    struct JkJson *first_child;
    struct JkJson *sibling;
    JkBuffer value;
} JkJson;

typedef enum JkJsonTokenType {
    JK_JSON_TOKEN_INVALID,
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

JK_GLOBAL_DECLARE JkBuffer jk_json_token_strings[JK_JSON_TOKEN_TYPE_COUNT];

JK_PUBLIC b32 jk_json_is_whitespace(int64_t byte);

JK_PUBLIC JkBuffer jk_json_token_to_string(JkArena *arena, JkJsonToken *token);

JK_PUBLIC JkBuffer jk_json_to_string(JkArena *arena, JkJson *json, int64_t indent_level);

JK_PUBLIC JkJsonToken jk_json_lex(JkBuffer text, int64_t *pos, JkArena *storage);

JK_PUBLIC JkJson *jk_json_parse(JkBuffer text, JkArena *storage);

JK_PUBLIC JkBuffer jk_json_parse_string(JkBuffer json_string_value, JkArena *storage);

JK_PUBLIC JkJson *jk_json_member_get(JkJson *object, char *name);

#endif
