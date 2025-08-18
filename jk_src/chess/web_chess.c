#include <stddef.h>

// clang-format off

// #jk_build single_translation_unit
// #jk_build compiler_arguments -o chess.wasm --target=wasm32 --no-standard-libraries
// #jk_build linker_arguments -Wl,--no-entry,--export=init_main,--export=tick,--export=get_sound,--export=get_started_time_0,--export=get_started_time_1,--export=init_audio,--export=fill_audio_buffer

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

typedef union FloatConvert {
    float f32s[2];
    double f64;
} FloatConvert;

static FloatConvert g_started_time;

void debug_print(char *string) {}

static b32 ensure_memory(int64_t required_memory)
{
    int64_t current_memory =
            (int64_t)__builtin_wasm_memory_size(0) * PAGE_SIZE - (int64_t)__heap_base;
    int64_t delta = required_memory - current_memory;
    if (0 < delta) {
        if (__builtin_wasm_memory_grow(0, (delta + PAGE_SIZE - 1) / PAGE_SIZE) == UINT32_MAX) {
            return 0;
        }
    }

    return 1;
}

uint8_t *init_main(void)
{
    g_chess.render_memory.size = 2 * JK_MEGABYTE;
    if (ensure_memory(DRAW_BUFFER_SIZE + g_chess.render_memory.size)) {
        g_chess.draw_buffer = (JkColor *)__heap_base;
        g_chess.render_memory.data = __heap_base + DRAW_BUFFER_SIZE;
        g_chess.os_timer_frequency = 1000;
        g_chess.debug_print = debug_print;
        return __heap_base;
    } else {
        return 0;
    }
}

void tick(int32_t square_side_length,
        int32_t mouse_x,
        int32_t mouse_y,
        b32 mouse_down,
        double os_time,
        double audio_time)
{
    g_chess.square_side_length = square_side_length;
    g_chess.input.mouse_pos.x = mouse_x;
    g_chess.input.mouse_pos.y = mouse_y;
    g_chess.input.flags = JK_FLAG_SET(g_chess.input.flags, INPUT_CONFIRM, mouse_down);
    g_chess.os_time = os_time;
    g_chess.audio_time = audio_time;
    update(g_assets, &g_chess);
    render(g_assets, &g_chess);
    g_started_time.f64 = (double)g_chess.audio_state.started_time;
}

SoundIndex get_sound(void)
{
    return g_chess.audio_state.sound;
}

float get_started_time_0(void)
{
    return g_started_time.f32s[0];
}

float get_started_time_1(void)
{
    return g_started_time.f32s[1];
}

#define WEB_AUDIO_BUFFER_SIZE (sizeof(AudioSample) * SAMPLES_PER_SECOND / 5)

static AudioSample *audio_buffer;

AudioSample *init_audio(void)
{
    if (ensure_memory(WEB_AUDIO_BUFFER_SIZE)) {
        audio_buffer = (AudioSample *)__heap_base;
        return audio_buffer;
    } else {
        return 0;
    }
}

void fill_audio_buffer(SoundIndex sound,
        float started_time_0,
        float started_time_1,
        double time,
        double sample_count)
{
    FloatConvert started_time = {.f32s = {started_time_0, started_time_1}};
    audio(g_assets, (AudioState){sound, started_time.f64}, time, sample_count, audio_buffer);
}
