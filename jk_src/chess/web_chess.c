#include <stddef.h>

// clang-format off

// #jk_build single_translation_unit
// #jk_build compiler_arguments -o chess.wasm --target=wasm32 --no-standard-libraries
// #jk_build linker_arguments -Wl,--no-entry,--export=init,--export=tick

// clang-format on

// #jk_build dependencies_begin
#include <jk_src\chess\chess.h>
// #jk_build dependencies_end

#include <jk_gen/chess/assets.c>

extern uint8_t __heap_base[];

__attribute__((no_builtin("memset"))) void *memset(void *address, int value, size_t size)
{
    uint8_t *bytes = address;
    for (uint64_t i = 0; i < size; i++) {
        bytes[i] = value;
    }
    return address;
}

__attribute__((no_builtin("memcpy"))) void *memcpy(void *dest, void const *src, size_t size)
{
    uint8_t *dest_bytes = dest;
    uint8_t const *src_bytes = src;
    for (uint64_t i = 0; i < size; i++) {
        dest_bytes[i] = src_bytes[i];
    }
    return dest;
}

#define PAGE_SIZE (64 * JK_KILOBYTE)

static ChessAssets *g_assets = (ChessAssets *)chess_assets_byte_array;
static Chess g_chess;

void debug_print(char *string) {}

uint8_t *init(void)
{
    g_chess.render_memory.size = 2 * JK_MEGABYTE;
    int64_t required_memory = DRAW_BUFFER_SIZE + g_chess.render_memory.size;
    int64_t current_memory =
            (int64_t)__builtin_wasm_memory_size(0) * PAGE_SIZE - (int64_t)__heap_base;
    int64_t delta = required_memory - current_memory;
    if (0 < delta) {
        if (__builtin_wasm_memory_grow(0, (delta + PAGE_SIZE - 1) / PAGE_SIZE) == UINT32_MAX) {
            return 0;
        }
    }

    g_chess.draw_buffer = (JkColor *)__heap_base;
    g_chess.render_memory.data = __heap_base + DRAW_BUFFER_SIZE;

    // TODO: implement properly
    g_chess.os_timer_frequency = 1;
    g_chess.debug_print = debug_print;

    for (int32_t y = 0; y < DRAW_BUFFER_SIDE_LENGTH; y++) {
        for (int32_t x = 0; x < DRAW_BUFFER_SIDE_LENGTH; x++) {
            g_chess.draw_buffer[y * DRAW_BUFFER_SIDE_LENGTH + x] = (JkColor){
                .r = x * 255 / DRAW_BUFFER_SIDE_LENGTH,
                .g = 0,
                .b = y * 255 / DRAW_BUFFER_SIDE_LENGTH,
                .a = 255,
            };
        }
    }

    return __heap_base;
}

void tick(int32_t square_side_length)
{
    g_chess.square_side_length = square_side_length;
    update(g_assets, &g_chess);
    render(g_assets, &g_chess);
}
