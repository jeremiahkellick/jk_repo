#include <stdio.h>

// #jk_build single_translation_unit

// #jk_build dependencies_begin
#include <jk_src/jk_lib/platform/platform.h>
// #jk_build dependencies_end

int32_t jk_platform_entry_point(int32_t argc, char **argv)
{
    JkPlatformArenaVirtualRoot arena_root;
    JkArena arena = jk_platform_arena_virtual_init(&arena_root, JK_GIGABYTE);

    for (int64_t n = 4; n <= 9; n++) {
        char *file_name = jk_buffer_to_null_terminated(
                &arena, JK_FORMAT(&arena, jkfn("coefficients_"), jkfi(n)));
        JkBuffer file = jk_platform_file_read_full(&arena, file_name);
        float *coeffs = (float *)file.data;
        printf("{");
        for (int64_t i = 0; i <= n; i++) {
            if (i) {
                printf(", ");
            }
            printf("%.6af", coeffs[i]);
        }
        printf("},\n");
    }

    return 0;
}
