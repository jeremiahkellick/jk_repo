#include <stdio.h>
#include <stdlib.h>

#include <jk_gen/single_translation_unit.h>

// #jk_build dependencies_begin
#include <jk_src/jk_lib/json/json.h>
// #jk_build dependencies_end

#ifdef _WIN32
#include <windows.h>
#endif

int main(int argc, char **argv)
{
    (void)argc;

#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    JkArena storage;
    jk_arena_init(&storage, (size_t)1 << 36);

    JkBuffer text = jk_file_read_full("./lex_test.json", &storage);
    JkBufferPointer text_pointer = {.buffer = text, .index = 0};

    JkJsonToken token;
    do {
        token = jk_json_lex(&text_pointer, &storage);
        if (token.type == JK_JSON_INVALID) {
            fprintf(stderr, "%s: Invalid JSON\n", argv[0]);
            exit(1);
        }
        jk_json_print_token(stdout, &token, &storage);
        printf(" ");
    } while (token.type != JK_JSON_TOKEN_EOF);

    printf("\n");

    return 0;
}
