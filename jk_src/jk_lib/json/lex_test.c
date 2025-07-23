#include <stdio.h>
#include <stdlib.h>

// #jk_build single_translation_unit

// #jk_build dependencies_begin
#include <jk_src/jk_lib/jk_lib.h>
#include <jk_src/jk_lib/json/json.h>
#include <jk_src/jk_lib/platform/platform.h>
// #jk_build dependencies_end

int main(int argc, char **argv)
{
    (void)argc;

    jk_platform_set_working_directory_to_executable_directory();
    jk_platform_console_utf8_enable();

    JkPlatformArenaVirtualRoot arena_root;
    JkArena storage = jk_platform_arena_virtual_init(&arena_root, (size_t)1 << 36);

    JkBuffer text = jk_platform_file_read_full(&storage, "../jk_src/jk_lib/json/lex_test.json");
    uint64_t pos = 0;

    JkJsonToken token;
    do {
        token = jk_json_lex(text, &pos, &storage);
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
