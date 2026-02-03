#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

// #jk_build single_translation_unit

// #jk_build dependencies_begin
#include <jk_src/jk_lib/jk_lib.h>
#include <jk_src/jk_lib/json/json.h>
#include <jk_src/jk_lib/platform/platform.h>
// #jk_build dependencies_end

int32_t jk_platform_entry_point(int32_t argc, char **argv)
{
    jk_platform_set_working_directory_to_executable_directory();

    JkPlatformArenaVirtualRoot arena_root;
    JkArena storage = jk_platform_arena_virtual_init(&arena_root, (int64_t)1 << 35);

    JkBuffer text = jk_platform_file_read_full(&storage, "../jk_src/jk_lib/json/parse_test.json");

    JkJson *json = jk_json_parse(text, &storage);
    if (json) {
        jk_platform_print(jk_json_to_string(&storage, json, 0));
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
