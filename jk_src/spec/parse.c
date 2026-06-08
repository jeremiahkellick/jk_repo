#ifndef SPEC_PARSE_H
#define SPEC_PARSE_H

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>

// #jk_build single_translation_unit

// #jk_build dependencies_begin
#include <jk_src/spec/lf.h>
// #jk_build dependencies_end

#define SPEC_CHAR_EOF -1

typedef enum SpecTokenType {
    SPEC_TOKEN_INVALID,
    SPEC_TOKEN_STRUCT,
    SPEC_TOKEN_ENUM,
    SPEC_TOKEN_FLAGS,
    SPEC_TOKEN_INTEGER_TYPE,
    SPEC_TOKEN_IDENTIFIER,
    SPEC_TOKEN_NUMBER,
    SPEC_TOKEN_STRING,
    SPEC_TOKEN_SYMBOL,
    SPEC_TOKEN_EOF,
} SpecTokenType;

typedef struct SpecToken {
    SpecTokenType type;
    str text;
    s8 bit_count;
} SpecToken;

typedef struct SpecFile {
    str path;
    str contents;
} SpecFile;

typedef struct SpecParser {
    bool err;
    s64 err_position;
    SpecFile file;
    s64 cursor;
} SpecParser;

static s32 spec_char_get(SpecParser *p) {
    return (0 <= p->cursor && p->cursor < p->file.contents.len)
            ? p->file.contents.str[p->cursor]
            : SPEC_CHAR_EOF;
}

static s32 spec_char_next(SpecParser *p) {
    s32 c = spec_char_get(p);
    p->cursor++;
    return c;
}

static bool spec_char_is_space(s32 c) {
    return c == ' ' || ('\t' <= c && c <= '\r');
}

static bool spec_char_is_digit(s32 c) {
    return '0' <= c && c <= '9';
}

static bool spec_char_is_alpha(s32 c) {
    return ('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z');
}

static bool spec_err(SpecParser *p) {
    return 0 <= p->err_position;
}

s32 spec_symbols[] = {
    '(',
    ')',
    '*',
    '+',
    ',',
    ';',
    '@',
    '[',
    ']',
    '{',
    '}',
};
static bool spec_char_is_symbol(s32 c) {
    for (s32 i = 0; i < ARRAY_LEN(spec_symbols); i++) {
        if (c == spec_symbols[i]) {
            return true;
        }
    }
    return false;
}

static SpecToken spec_token_next(SpecParser *p) {
    if (spec_err(p)) {
        return STRUCT(SpecToken){0};
    }

    SpecToken t = {0};

    // Skip whitespace and comments
    for (;;) {
        while (spec_char_is_space(spec_char_get(p))) {
            p->cursor++;
        }
        s32 cursor_reset = p->cursor;
        if (spec_char_next(p) == '/') {
            s32 second_char = spec_char_next(p);
            if (second_char == '/') {
                s32 c;
                do {
                    c = spec_char_next(p);
                } while (c != SPEC_CHAR_EOF && c != '\n');
            } else if (second_char == '*') {
                s32 c;
                do {
                    c = spec_char_next(p);
                } while (!(c == SPEC_CHAR_EOF || (c == '*' && spec_char_get(p) == '/')));
                p->cursor++;
            }
        }
        if (p->cursor <= cursor_reset + 2) { // No comment
            p->cursor = cursor_reset;
        }
    }

    s64 start_position = p->cursor;
    s32 first_char = spec_char_get(p);

    if (first_char == SPEC_CHAR_EOF) {
        t.type = SPEC_TOKEN_EOF;
    } else if (spec_char_is_symbol(first_char)) {
        t.type = SPEC_TOKEN_SYMBOL;
        p->cursor++;
    } else if (first_char == '"' || first_char == '\'') {
        p->cursor++;
        s32 c;
        while ((c = spec_char_get(p)) != SPEC_CHAR_EOF && c != first_char) {
            p->cursor++;
        }
        if (c == SPEC_CHAR_EOF) {
            fprintf(stderr, "error: Unclosed string\n");
            p->err_position = start_position;
        } else {
            t.type = SPEC_TOKEN_STRING;
            start_position++;
        }
    } else {
        s32 c;
        while ((c = spec_char_get(p)) != SPEC_CHAR_EOF && !spec_char_is_space(c)
                && !spec_char_is_symbol(c)) {
            p->cursor++;
        }
    }

    t.text.len = p->cursor - start_position;
    t.text.str = p->file.contents.str + start_position;

    if (t.type == SPEC_TOKEN_INVALID) {
        // Let's see if we have a keyword
        bool int_type_char = first_char == 's' || first_char == 'u';
        if (str_eql(t.text, "enum")) {
            t.type = SPEC_TOKEN_ENUM;
        } else if (str_eql(t.text, "flags")) {
            t.type = SPEC_TOKEN_FLAGS;
        } else if (str_eql(t.text, "struct")) {
            t.type = SPEC_TOKEN_STRUCT;
        } else if (int_type_char && t.text.len == 2 && '1' <= t.text.str[1]
                && t.text.str[1] <= '9') {
            t.type = SPEC_TOKEN_INTEGER_TYPE;
            t.bit_count = t.text.str[1] - '0';
        } else if (int_type_char && t.text.len == 3 && spec_char_is_digit(t.text.str[1])
                && spec_char_is_digit(t.text.str[2])) {
            t.bit_count = ((t.text.str[1] - '0') * 10) + (t.text.str[2] - '0');
            if (10 <= t.bit_count && t.bit_count <= 32) {
                t.type = SPEC_TOKEN_INTEGER_TYPE;
            }
        }
    }

    if (t.type == SPEC_TOKEN_INVALID) {
        // At this point it's either an identifier, number, or invalid text
        if (spec_char_is_alpha(first_char) || first_char == '_') {
            // Either an identifier or invalid
            bool valid = true;
            for (s64 i = 1; valid && i < t.text.len; i++) {
                s32 c = t.text.str[i];
                if (!(spec_char_is_alpha(c) || spec_char_is_digit(c) || c == '_')) {
                    valid = false;
                }
            }
            if (valid) {
                t.type = SPEC_TOKEN_IDENTIFIER;
            }
        } else if (spec_char_is_digit(first_char) || first_char == '.') {
            // Either a number or invalid
            bool valid = true;
            bool decimal = false;
            for (s64 i = 0; valid && i < t.text.len; i++) {
                s32 c = t.text.str[i];
                if (c == '.') {
                    if (decimal) {
                        valid = false; // Can't have multiple decimal points
                    }
                    decimal = true;
                } else if (!spec_char_is_digit(c)) {
                    valid = false;
                }
            }
            if (valid) {
                t.type = SPEC_TOKEN_NUMBER;
            }
        }
    }

    return t;
}

static str spec_file_read(Arena *arena, str path) {
    str result = {0};

    bool err = false;
    bool pushed = false;

    FILE *file = 0;
    ARENA_TEMP(*arena) {
        char *path_nt = str_cstring(arena, path);
        file = fopen(path_nt, "rb");
        err = err || file == 0;
    }
    uptr reset_pos = arena->pos;
    if (!err) {
        err = err || fseek(file, 0, SEEK_END) == -1;
        result.len = ftell(file);
        err = err || result.len == -1;
        err = err || fseek(file, 0, SEEK_SET) == -1;

        if (!err) {
            result.str = Arena_bytes(arena, result.len);
            if (result.str) {
                pushed = true;
                u64 read = fread(result.str, result.len, 1, file);
                err = err || read != result.len;
            }
        }

        fclose(file);
    }

    if (err) {
        fprintf(stderr,
                "error: Failed to read file '%.*s': %s",
                (s32)path.len,
                path.str,
                strerror(errno));
        Arena_reset(arena, reset_pos);
        return STRUCT(str){0};
    } else {
        return result;
    }
}

s32 main(s32 argc, char **argv) {}

#endif
