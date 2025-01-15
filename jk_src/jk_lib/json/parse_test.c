#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <jk_gen/jk_lib/json/parse_test.stu.h>

// #jk_build dependencies_begin
#include <jk_src/jk_lib/jk_lib.h>
#include <jk_src/jk_lib/json/json.h>
#include <jk_src/jk_lib/platform/platform.h>
// #jk_build dependencies_end

int main(int argc, char **argv)
{
    (void)argc;

    jk_platform_console_utf8_enable();

    JkPlatformArena storage;
    jk_platform_arena_init(&storage, (size_t)1 << 35);

    JkBuffer text = jk_platform_file_read_full("./parse_test.json", &storage);

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
