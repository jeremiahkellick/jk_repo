#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// #jk_build dependencies_begin
#include <jk_src/jk_lib/arena/arena.h>
#include <jk_src/jk_lib/buffer/buffer.h>
#include <jk_src/jk_lib/profile/profile.h>
#include <jk_src/jk_lib/string/utf8.h>
#include <jk_src/jk_lib/utils.h>
// #jk_build dependencies_end

#include "json.h"

static char *jk_json_type_strings[JK_JSON_TYPE_COUNT] = {
    "INVALID",
    "OBJECT",
    "ARRAY",
    "STRING",
    "NUMBER",
    "true",
    "false",
    "null",
};

char *jk_json_token_strings[JK_JSON_TOKEN_TYPE_COUNT] = {
    "INVALID",
    "VALUE",
    ",",
    ":",
    "{",
    "}",
    "[",
    "]",
    "EOF",
};

JK_PUBLIC bool jk_json_is_whitespace(int byte)
{
    return byte == ' ' || byte == '\n' || byte == '\r' || byte == '\t';
}

JK_PUBLIC void jk_json_print_token(FILE *file, JkJsonToken *token, JkArena *storage)
{
    if (token->type == JK_JSON_TOKEN_VALUE) {
        jk_json_print(file, token->value, 0, storage);
    } else {
        fprintf(file, "%s", jk_json_token_strings[token->type]);
    }
}

JK_PUBLIC void jk_json_print(FILE *file, JkJson *json, int indent_level, JkArena *storage)
{
    if (json->type == JK_JSON_ARRAY || json->type == JK_JSON_OBJECT) {
        bool is_object = json->type == JK_JSON_OBJECT;
        if (!json->first_child) {
            fprintf(file, is_object ? "{}" : "[]");
            return;
        }
        fprintf(file, is_object ? "{\n" : "[\n");
        for (JkJson *child = json->first_child; child != NULL; child = child->sibling) {
            for (int i = 0; i < indent_level + 1; i++) {
                fprintf(file, "\t");
            }
            if (is_object) {
                fprintf(file, "\"%.*s\": ", (int)child->name.size, (char const *)child->name.data);
            }
            jk_json_print(file, child, indent_level + 1, storage);
            fprintf(file, "%s", child->sibling ? ",\n" : "\n");
        }
        for (int i = 0; i < indent_level; i++) {
            fprintf(file, "\t");
        }
        fprintf(file, is_object ? "}" : "]");
        return;
    } else if (json->type == JK_JSON_STRING) {
        JkBuffer string = jk_json_parse_string(json->value, storage);
        fprintf(file, "\"%.*s\"", (int)string.size, (char const *)string.data);
        return;
    } else if (json->type == JK_JSON_NUMBER) {
        fprintf(file, "%f", jk_json_parse_number(json->value));
        return;
    }
    fprintf(file, "%s", jk_json_type_strings[json->type]);
}

typedef struct JkJsonExactMatch {
    char *string;
    long length;
    JkJsonType type;
} JkJsonExactMatch;

static JkJsonExactMatch jk_json_exact_matches[] = {
    {
        .string = "true",
        .length = sizeof("true") - 1,
        .type = JK_JSON_TRUE,
    },
    {
        .string = "false",
        .length = sizeof("false") - 1,
        .type = JK_JSON_FALSE,
    },
    {
        .string = "null",
        .length = sizeof("null") - 1,
        .type = JK_JSON_NULL,
    },
};

JK_PUBLIC JkJsonToken jk_json_lex(JkBufferPointer *text_pointer, JkArena *storage)
{
    JkBufferPointer *t = text_pointer;
    int c;
    JkJsonToken token = {0};

    while (jk_json_is_whitespace((c = jk_buffer_character_peek(t)))) {
        t->index++;
    }

    switch (c) {
    case '"': {
        t->index++;
        size_t start = t->index;

        while ((c = jk_buffer_character_peek(t)) != '"') {
            t->index++;
        }

        size_t end = t->index;
        t->index++;

        token.type = JK_JSON_TOKEN_VALUE;
        token.value = jk_arena_push_zero(storage, sizeof(*token.value));
        token.value->type = JK_JSON_STRING;
        token.value->value = (JkBuffer){.size = end - start, .data = t->buffer.data + start};
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
        size_t start = t->index;
        t->index++;

        // Advance past integer
        while (isdigit(jk_buffer_character_peek(t))) {
            t->index++;
        }

        // Advance past fraction if there is one
        if (jk_buffer_character_peek(t) == '.') {
            t->index++;
            while (isdigit(jk_buffer_character_peek(t))) {
                t->index++;
            }
        }

        // Advance past exponent if there is one
        int peek = jk_buffer_character_peek(t);
        if (peek == 'e' || peek == 'E') {
            t->index++;
            peek = jk_buffer_character_peek(t);
            if (isdigit(peek) || peek == '-' || peek == '+') {
                t->index++;
                while (isdigit(jk_buffer_character_peek(t))) {
                    t->index++;
                }
            }
        }

        size_t end = t->index;

        token.type = JK_JSON_TOKEN_VALUE;
        token.value = jk_arena_push_zero(storage, sizeof(*token.value));
        token.value->type = JK_JSON_NUMBER;
        token.value->value = (JkBuffer){.size = end - start, .data = t->buffer.data + start};
    } break;
    case 't':
    case 'f':
    case 'n': {
        for (size_t i = 0; i < JK_ARRAY_COUNT(jk_json_exact_matches); i++) {
            JkJsonExactMatch *match = &jk_json_exact_matches[i];
            if (strncmp((char const *)t->buffer.data + t->index, match->string, match->length)
                    == 0) {
                t->index += match->length;
                token.type = JK_JSON_TOKEN_VALUE;
                token.value = jk_arena_push_zero(storage, sizeof(*token.value));
                token.value->type = match->type;
            }
        }
    } break;
    case ',': {
        t->index++;
        token.type = JK_JSON_TOKEN_COMMA;
    } break;
    case ':': {
        t->index++;
        token.type = JK_JSON_TOKEN_COLON;
    } break;
    case '{': {
        t->index++;
        token.type = JK_JSON_TOKEN_OPEN_BRACE;
    } break;
    case '}': {
        t->index++;
        token.type = JK_JSON_TOKEN_CLOSE_BRACE;
    } break;
    case '[': {
        t->index++;
        token.type = JK_JSON_TOKEN_OPEN_BRACKET;
    } break;
    case ']': {
        t->index++;
        token.type = JK_JSON_TOKEN_CLOSE_BRACKET;
    } break;
    case EOF: {
        token.type = JK_JSON_TOKEN_EOF;
    } break;
    default: {
        token.type = JK_JSON_TOKEN_INVALID;
    } break;
    }

    return token;
}

#define JK_JSON_LEX_NEXT_TOKEN                      \
    do {                                            \
        token = jk_json_lex(text_pointer, storage); \
        if (token.type == JK_JSON_TOKEN_INVALID) {  \
            return NULL;                            \
        }                                           \
    } while (0)

static JkJson *jk_json_parse_with_token(
        JkBufferPointer *text_pointer, JkJsonToken token, JkArena *storage)
{
    if (token.type == JK_JSON_TOKEN_VALUE) {
        return token.value;
    } else if (token.type == JK_JSON_TOKEN_OPEN_BRACE || token.type == JK_JSON_TOKEN_OPEN_BRACKET) {
        bool is_object = token.type == JK_JSON_TOKEN_OPEN_BRACE;
        JkJson *json = jk_arena_push_zero(storage, sizeof(*json));
        json->type = is_object ? JK_JSON_OBJECT : JK_JSON_ARRAY;

        JK_JSON_LEX_NEXT_TOKEN;

        if ((is_object && token.type == JK_JSON_TOKEN_CLOSE_BRACE)
                || (!is_object && token.type == JK_JSON_TOKEN_CLOSE_BRACKET)) {
            return json;
        } else {
            bool first = true;
            JkJson *child = NULL;
            JkJson *prev_child = NULL;
            do {
                if (first) {
                    first = false;
                } else {
                    JK_JSON_LEX_NEXT_TOKEN;
                }

                JkBuffer name = {0};
                if (is_object) {
                    if (!(token.type == JK_JSON_TOKEN_VALUE
                                && token.value->type == JK_JSON_STRING)) {
                        return NULL;
                    }

                    name = token.value->value;

                    JK_JSON_LEX_NEXT_TOKEN;
                    if (token.type != JK_JSON_TOKEN_COLON) {
                        return NULL;
                    }

                    JK_JSON_LEX_NEXT_TOKEN;
                }

                child = jk_json_parse_with_token(text_pointer, token, storage);
                if (child == NULL) {
                    return NULL;
                }
                child->name = name;

                if (prev_child) {
                    prev_child->sibling = child;
                } else {
                    json->first_child = child;
                }
                prev_child = child;

                JK_JSON_LEX_NEXT_TOKEN;
            } while (token.type == JK_JSON_TOKEN_COMMA);

            if ((is_object && token.type != JK_JSON_TOKEN_CLOSE_BRACE)
                    || (!is_object && token.type != JK_JSON_TOKEN_CLOSE_BRACKET)) {
                return NULL;
            }
        }

        return json;
    } else {
        return NULL;
    }
}

JK_PUBLIC JkJson *jk_json_parse(JkBuffer text, JkArena *storage)
{
    JkJsonToken token;
    JkBufferPointer text_pointer_alloc = {.buffer = text, .index = 0};
    JkBufferPointer *text_pointer = &text_pointer_alloc;
    JK_JSON_LEX_NEXT_TOKEN;
    return jk_json_parse_with_token(text_pointer, token, storage);
}

#undef JK_JSON_LEX_NEXT_TOKEN

JK_PUBLIC JkBuffer jk_json_parse_string(JkBuffer json_string_value, JkArena *storage)
{
    int c;
    JkBufferPointer pointer = {.buffer = json_string_value, .index = 0};
    JkBuffer string = {0};
    char *storage_pointer = jk_arena_push(storage, 1);
    char *start = storage_pointer;

    while ((c = jk_buffer_character_next(&pointer)) != EOF) {
        if (c < 0x20) {
            return string;
        }
        if (c == '\\') {
            c = jk_buffer_character_next(&pointer);
            switch (c) {
            case '"':
            case '\\':
            case '/': {
                *storage_pointer = (char)c;
            } break;

            case 'b': {
                *storage_pointer = '\b';
            } break;
            case 'f': {
                *storage_pointer = '\f';
            } break;
            case 'n': {
                *storage_pointer = '\n';
            } break;
            case 'r': {
                *storage_pointer = '\r';
            } break;
            case 't': {
                *storage_pointer = '\t';
            } break;

            case 'u': {
                uint32_t unicode32 = 0;
                for (int i = 0; i < 4; i++) {
                    int digit_value;
                    c = jk_buffer_character_next(&pointer);
                    int lowered = tolower(c);
                    if (lowered >= 'a' && lowered <= 'f') {
                        digit_value = 10 + (lowered - 'a');
                    } else if (c >= '0' && c <= '9') {
                        digit_value = c - '0';
                    } else {
                        return string;
                    }
                    unicode32 = unicode32 * 0x10 + digit_value;
                }
                JkUtf8Codepoint utf8 = {0};
                jk_utf8_codepoint_encode(unicode32, &utf8);
                *storage_pointer = utf8.b[0];
                for (int i = 1; i < 4 && jk_utf8_byte_is_continuation(utf8.b[i]); i++) {
                    storage_pointer = jk_arena_push(storage, 1);
                    *storage_pointer = utf8.b[i];
                }
            } break;

            default: {
                return string;
            } break;
            }
        } else {
            *storage_pointer = (char)c;
        }
        storage_pointer = jk_arena_push(storage, 1);
    }

    *storage_pointer = '\0';
    string.data = (uint8_t *)start;
    string.size = storage_pointer - start;
    return string;
}

JK_PUBLIC double jk_json_parse_number(JkBuffer json_number_value)
{
    double significand_sign = 1.0;
    double significand = 0.0;
    double exponent_sign = 1.0;
    double exponent = 0.0;

    JkBufferPointer pointer = {.buffer = json_number_value, .index = 0};
    int c = jk_buffer_character_next(&pointer);

    if (c == '-') {
        significand_sign = -1.0;

        c = jk_buffer_character_next(&pointer);

        if (!isdigit(c)) {
            return NAN;
        }
    }

    // Parse integer
    do {
        significand = (significand * 10.0) + (c - '0');
    } while (isdigit((c = jk_buffer_character_next(&pointer))));

    // Parse fraction if there is one
    if (c == '.') {
        c = jk_buffer_character_next(&pointer);

        if (!isdigit(c)) {
            return NAN;
        }

        double multiplier = 0.1;
        do {
            significand += (c - '0') * multiplier;
            multiplier /= 10.0;
        } while (isdigit((c = jk_buffer_character_next(&pointer))));
    }

    // Parse exponent if there is one
    if (c == 'e' || c == 'E') {
        c = jk_buffer_character_next(&pointer);

        if ((c == '-' || c == '+')) {
            if (c == '-') {
                exponent_sign = -1.0;
            }
            c = jk_buffer_character_next(&pointer);
        }

        if (!isdigit(c)) {
            return NAN;
        }

        do {
            exponent = (exponent * 10.0) + (c - '0');
        } while (isdigit((c = jk_buffer_character_next(&pointer))));
    }

    return significand_sign * significand * pow(10.0, exponent_sign * exponent);
}

JK_PUBLIC JkJson *jk_json_member_get(JkJson *object, char *name)
{
    assert(object->type == JK_JSON_OBJECT);
    JK_PROFILE_ZONE_TIME_BEGIN(jk_json_member_get);

    for (JkJson *child = object->first_child; child != NULL; child = child->sibling) {
        if (strncmp(name, (char const *)child->name.data, child->name.size) == 0) {
            JK_PROFILE_ZONE_END(jk_json_member_get);
            return child;
        }
    }

    JK_PROFILE_ZONE_END(jk_json_member_get);
    return NULL;
}
