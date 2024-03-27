#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// #jk_build dependencies_begin
#include <jk_src/jk_lib/arena/arena.h>
#include <jk_src/jk_lib/profile/profile.h>
#include <jk_src/jk_lib/quicksort/quicksort.h>
#include <jk_src/jk_lib/string/utf8.h>
#include <jk_src/jk_lib/utils.h>
// #jk_build dependencies_end

#include "json.h"

#define JK_JSON_CMP_STRING_LENGTH 6

char *jk_json_name = "<global jk_json_name should be overwritten with argv[0]>";

char *jk_json_type_strings[JK_JSON_TYPE_COUNT] = {
    "COLLECTION",
    "STRING",
    "NUMBER",
    "true",
    "false",
    "null",
};

char *jk_json_token_strings[JK_JSON_TOKEN_TYPE_COUNT] = {
    "VALUE",
    ",",
    ":",
    "{",
    "}",
    "[",
    "]",
    "EOF",
};

bool jk_json_is_whitespace(int byte)
{
    return byte == ' ' || byte == '\n' || byte == '\r' || byte == '\t';
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

static int jk_json_label_compare(void *a, void *b)
{
    JkJson *a_json = *(JkJson **)a;
    JkJson *b_json = *(JkJson **)b;
    assert(a_json->label);
    assert(b_json->label);
    int result = strcmp(a_json->label, b_json->label);
    return result;
}

static void jk_json_label_quicksort(JkJsonCollection *collection)
{
    JkJson *tmp;
    jk_quicksort(collection->elements, collection->count, sizeof(tmp), &tmp, jk_json_label_compare);
}

static JkJson *jk_json_label_search(JkJson **elements, size_t count, char *label)
{
    if (count <= 0) {
        return NULL;
    }
    size_t i = count / 2;
    assert(elements[i]->label);
    int comparison = strcmp(label, elements[i]->label);
    if (comparison < 0) {
        return jk_json_label_search(elements, i, label);
    } else if (comparison > 0) {
        return jk_json_label_search(&elements[i + 1], count - (i + 1), label);
    } else {
        return elements[i];
    }
}

void jk_json_print_token(FILE *file, JkJsonToken *token)
{
    if (token->type == JK_JSON_TOKEN_VALUE) {
        jk_json_print(file, token->value, 0);
    } else {
        fprintf(file, "%s", jk_json_token_strings[token->type]);
    }
}

void jk_json_print(FILE *file, JkJson *json, int indent_level)
{
    if (json->type == JK_JSON_COLLECTION) {
        JkJsonCollection *collection = &json->u.collection;
        bool is_object = collection->type == JK_JSON_COLLECTION_OBJECT;
        if (collection->count == 0) {
            fprintf(file, is_object ? "{}" : "[]");
            return;
        }
        fprintf(file, is_object ? "{\n" : "[\n");
        for (size_t i = 0; i < collection->count; i++) {
            for (int j = 0; j < indent_level + 1; j++) {
                fprintf(file, "\t");
            }
            if (is_object) {
                fprintf(file, "\"%s\": ", collection->elements[i]->label);
            }
            jk_json_print(file, collection->elements[i], indent_level + 1);
            fprintf(file, "%s", i != collection->count - 1 ? ",\n" : "\n");
        }
        for (int i = 0; i < indent_level; i++) {
            fprintf(file, "\t");
        }
        fprintf(file, is_object ? "}" : "]");
        return;
    } else if (json->type == JK_JSON_STRING) {
        fprintf(file, "\"%s\"", json->u.string);
        return;
    } else if (json->type == JK_JSON_NUMBER) {
        fprintf(file, "%f", json->u.number);
        return;
    }
    fprintf(file, "%s", jk_json_type_strings[json->type]);
}

JkJsonLexStatus jk_json_lex(JkArena *storage,
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
        char *c = jk_arena_push(storage, 1);
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
                    JkUtf8Codepoint utf8 = {0};
                    jk_utf8_codepoint_encode(unicode32, &utf8);
                    *c = utf8.b[0];
                    for (int i = 1; i < 4 && jk_utf8_byte_is_continuation(utf8.b[i]); i++) {
                        c = jk_arena_push(storage, 1);
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
            c = jk_arena_push(storage, 1);
        }
        *c = '\0';
        token->type = JK_JSON_TOKEN_VALUE;
        token->value = jk_arena_push(storage, sizeof(*token->value));
        token->value->type = JK_JSON_STRING;
        token->value->label = NULL;
        token->value->u.string = string;
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
        token->value = jk_arena_push(storage, sizeof(*token->value));
        token->value->type = JK_JSON_NUMBER;
        token->value->label = NULL;
        token->value->u.number = 0.0;

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
            token->value->u.number = (token->value->u.number * 10.0) + (e->c - '0');
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
                token->value->u.number += (e->c - '0') * multiplier;
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
        token->value->u.number =
                sign * token->value->u.number * pow(10.0, exponent_sign * exponent);
    } break;
    case 't':
    case 'f':
    case 'n': {
        memset(cmp_buffer, 0, sizeof(cmp_buffer));
        cmp_buffer[0] = (char)e->c;
        long read_count = (long)stream_read(stream, JK_JSON_CMP_STRING_LENGTH - 1, cmp_buffer + 1);
        for (size_t i = 0; i < JK_ARRAY_COUNT(jk_json_exact_matches); i++) {
            JkJsonExactMatch *match = &jk_json_exact_matches[i];
            if (strncmp(cmp_buffer, match->string, match->length) == 0
                    && !isalnum(cmp_buffer[match->length])) {
                stream_seek_relative(
                        stream, -jk_min(JK_JSON_CMP_STRING_LENGTH - match->length, read_count));
                token->type = JK_JSON_TOKEN_VALUE;
                token->value = jk_arena_push(storage, sizeof(*token->value));
                token->value->label = NULL;
                token->value->type = match->type;
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

#define JK_NOTHING

#define JK_JSON_LEX_NEXT_TOKEN(cleanup)                 \
    do {                                                \
        data->lex_status = jk_json_lex(storage,         \
                stream_read,                            \
                stream_seek_relative,                   \
                stream,                                 \
                &data->token,                           \
                &data->lex_error_data);                 \
        if (data->lex_status != JK_JSON_LEX_SUCCESS) {  \
            cleanup;                                    \
            data->error_type = JK_JSON_PARSE_LEX_ERROR; \
            return NULL;                                \
        }                                               \
    } while (0)

static JkJson *jk_json_parse_with_token(JkArena *storage,
        JkArena *tmp_storage,
        size_t (*stream_read)(void *stream, size_t byte_count, void *buffer),
        int (*stream_seek_relative)(void *stream, long offset),
        void *stream,
        JkJsonParseData *data)
{
    JkJson *result;
    JK_PROFILE_TIME_BEGIN(parse_json);

    JkJsonParseData *d = data;
    if (d->token.type == JK_JSON_TOKEN_VALUE) {
        result = d->token.value;
        goto cleanup;
    } else if (d->token.type == JK_JSON_TOKEN_OPEN_BRACE
            || d->token.type == JK_JSON_TOKEN_OPEN_BRACKET) {
        bool is_object = d->token.type == JK_JSON_TOKEN_OPEN_BRACE;

        JK_JSON_LEX_NEXT_TOKEN(JK_NOTHING);

        JkJson *json = jk_arena_push(storage, sizeof(*json));
        json->type = JK_JSON_COLLECTION;
        json->label = NULL;
        JkJsonCollection *collection = &json->u.collection;
        collection->type = is_object ? JK_JSON_COLLECTION_OBJECT : JK_JSON_COLLECTION_ARRAY;
        collection->count = 0;

        if ((is_object && d->token.type == JK_JSON_TOKEN_CLOSE_BRACE)
                || (!is_object && d->token.type == JK_JSON_TOKEN_CLOSE_BRACKET)) {
            collection->elements = NULL;
        } else {
            size_t base_pos = tmp_storage->pos;
            JkJson **tmp_elements = (JkJson **)&tmp_storage->address[base_pos];
            do {
                if (collection->count > 0) {
                    JK_JSON_LEX_NEXT_TOKEN(tmp_storage->pos = base_pos);
                }

                char *label = NULL;
                if (is_object) {
                    if (!(d->token.type == JK_JSON_TOKEN_VALUE
                                && d->token.value->type == JK_JSON_STRING)) {
                        data->error_type = JK_JSON_PARSE_UNEXPECTED_TOKEN;
                        result = NULL;
                        goto cleanup;
                    }

                    label = d->token.value->u.string;

                    JK_JSON_LEX_NEXT_TOKEN(tmp_storage->pos = base_pos);
                    if (d->token.type != JK_JSON_TOKEN_COLON) {
                        data->error_type = JK_JSON_PARSE_UNEXPECTED_TOKEN;
                        result = NULL;
                        goto cleanup;
                    }

                    JK_JSON_LEX_NEXT_TOKEN(tmp_storage->pos = base_pos);
                }

                jk_arena_push(tmp_storage, sizeof(*tmp_elements));

                tmp_elements[collection->count] = jk_json_parse_with_token(
                        storage, tmp_storage, stream_read, stream_seek_relative, stream, data);
                if (tmp_elements[collection->count] == NULL) {
                    result = NULL;
                    goto cleanup;
                }
                tmp_elements[collection->count]->label = label;

                collection->count++;
                JK_JSON_LEX_NEXT_TOKEN(tmp_storage->pos = base_pos);
            } while (d->token.type == JK_JSON_TOKEN_COMMA);

            if ((is_object && d->token.type != JK_JSON_TOKEN_CLOSE_BRACE)
                    || (!is_object && d->token.type != JK_JSON_TOKEN_CLOSE_BRACKET)) {
                data->error_type = JK_JSON_PARSE_UNEXPECTED_TOKEN;
                result = NULL;
                goto cleanup;
            }

            collection->elements =
                    jk_arena_push(storage, sizeof(collection->elements[0]) * collection->count);
            memcpy(collection->elements,
                    tmp_elements,
                    sizeof(collection->elements[0]) * collection->count);
            if (is_object) {
                jk_json_label_quicksort(collection);
            }

            tmp_storage->pos = base_pos;
        }

        result = json;
        goto cleanup;
    } else {
        d->error_type = JK_JSON_PARSE_UNEXPECTED_TOKEN;
        result = NULL;
        goto cleanup;
    }

cleanup:
    JK_PROFILE_TIME_END(parse_json);
    return result;
}

JkJson *jk_json_parse(JkArena *storage,
        JkArena *tmp_storage,
        size_t (*stream_read)(void *stream, size_t byte_count, void *buffer),
        int (*stream_seek_relative)(void *stream, long offset),
        void *stream,
        JkJsonParseData *data)
{
    JK_JSON_LEX_NEXT_TOKEN(JK_NOTHING);
    return jk_json_parse_with_token(
            storage, tmp_storage, stream_read, stream_seek_relative, stream, data);
}

#undef JK_JSON_LEX_NEXT_TOKEN

#undef JK_NOTHING

JkJson *jk_json_member_get(JkJsonCollection *object, char *label)
{
    assert(object->type == JK_JSON_COLLECTION_OBJECT);
    return jk_json_label_search(object->elements, object->count, label);
}
