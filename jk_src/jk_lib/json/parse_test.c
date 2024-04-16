#include <assert.h>
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
    jk_arena_init(&storage, (size_t)1 << 35);

    JkBuffer text = jk_file_read_full("./parse_test.json", &storage);

    JkJson *json = jk_json_parse(text, &storage);
    if (json) {
        jk_json_print(stdout, json, 0, &storage);
        printf("\n");
        assert(json->type == JK_JSON_OBJECT);
        JkJson *smokes = jk_json_member_get(json, "smokes");
        assert(smokes && smokes->type == JK_JSON_FALSE);
        assert(jk_json_member_get(json, "this_member_does_not_exist") == NULL);
    } else {
        fprintf(stderr, "%s: Invalid JSON\n", argv[0]);
        exit(1);
    }

    return 0;
}
