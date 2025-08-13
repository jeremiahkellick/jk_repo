#include <stddef.h>

// clang-format off

// #jk_build single_translation_unit
// #jk_build compiler_arguments -o chess.wasm --target=wasm32 --no-standard-libraries
// #jk_build linker_arguments -Wl,--no-entry,--export=update,--export=audio,--export=board_equal,--export=render,--export=ai_init,--export=ai_running

// clang-format on

// #jk_build dependencies_begin
#include <jk_src\chess\chess.h>
// #jk_build dependencies_end

void *memset(void *s, int c, size_t n)
{
    return jk_memset(s, c, n);
}

void *memcpy(void *dest, void const *src, size_t n)
{
    return jk_memcpy(dest, (void *)src, n);
}
