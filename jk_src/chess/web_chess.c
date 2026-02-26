#include <stddef.h>

// clang-format off

// #jk_build isa wasm
// #jk_build single_translation_unit
// #jk_build export init_main tick get_sound get_started_time_0 get_started_time_1 get_ai_request get_ai_response_ai_thread get_ai_response_main_thread init_audio fill_audio_buffer ai_alloc_memory ai_begin_request ai_tick web_is_draggable

// clang-format on

// #jk_build dependencies_begin
#include <jk_src/chess/chess.h>
// #jk_build dependencies_end

#include <jk_gen/chess/assets.c>

#define WEB_AUDIO_BUFFER_SIZE (JK_SIZEOF(AudioSample) * SAMPLES_PER_SECOND / 5)
#define AI_MEMORY_SIZE (1 * JK_GIGABYTE)

extern uint8_t __heap_base[];

#define PAGE_SIZE (64 * JK_KILOBYTE)

static ChessAssets *g_assets = (ChessAssets *)chess_assets_byte_array;
static Chess g_chess;

static Ai g_ai;
static int32_t g_ai_request_id;
static AiRequest g_ai_request;
static JkArenaRoot g_ai_arena_root;
static JkArena g_ai_arena;

typedef union FloatConvert {
    float f32s[2];
    double f64;
} FloatConvert;

static FloatConvert g_started_time;

// ---- Imported functions begin -----------------------------------------------

void console_log(int32_t size, uint8_t *data);

double performance_now(void);

// ---- Imported functions end -------------------------------------------------

static void debug_print(JkBuffer string)
{
    if (0 < string.size) {
        console_log(string.size, string.data);
    }
}

JK_PUBLIC uint64_t jk_cpu_timer_get(void)
{
    return performance_now() * 10.0;
}

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
    JkBuffer log_memory = {.size = 1 * JK_MEGABYTE};
    if (ensure_memory(DRAW_BUFFER_SIZE + g_chess.render_memory.size + log_memory.size)) {
        g_chess.draw_buffer = (JkColor *)__heap_base;
        g_chess.render_memory.data = __heap_base + DRAW_BUFFER_SIZE;
        g_chess.os_timer_frequency = 1000;

        // Setup context
        log_memory.data = __heap_base + DRAW_BUFFER_SIZE + g_chess.render_memory.size;
        static JkContext c;
        c.log = jk_log_init(debug_print, log_memory);
        jk_context = &c;

        return __heap_base;
    } else {
        return 0;
    }
}

// Return whether or not the ai_request changed
b32 tick(int32_t square_side_length,
        int32_t mouse_x,
        int32_t mouse_y,
        b32 mouse_down,
        double os_time,
        double audio_time)
{
    g_chess.square_side_length = square_side_length;
    g_chess.input.mouse_pos.x = mouse_x;
    g_chess.input.mouse_pos.y = mouse_y;
    JK_FLAG_SET(g_chess.input.flags, INPUT_CONFIRM, mouse_down);
    g_chess.os_time = os_time;
    g_chess.audio_time = audio_time;

    update(jk_context, g_assets, &g_chess);
    render(g_assets, &g_chess);

    g_started_time.f64 = (double)g_chess.audio_state.started_time;
    if (!g_ai_request.wants_ai_move != !JK_FLAG_GET(g_chess.flags, CHESS_FLAG_WANTS_AI_MOVE)
            || !board_equal(&g_ai_request.board, &g_chess.board)) {
        g_ai_request.board = g_chess.board;
        g_ai_request.wants_ai_move = JK_FLAG_GET(g_chess.flags, CHESS_FLAG_WANTS_AI_MOVE);
        return 1;
    } else {
        return 0;
    }
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

AiRequest *get_ai_request(void)
{
    return &g_ai_request;
}

AiResponse *get_ai_response_ai_thread(void)
{
    return &g_ai.response;
}

AiResponse *get_ai_response_main_thread(void)
{
    return &g_chess.ai_response;
}

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

b32 ai_alloc_memory(void)
{
    static JkContext c;
    static JkArenaRoot scratch_arena_roots[JK_ARRAY_COUNT(c.scratch_arenas)];

    int64_t scratch_arena_size = 16 * JK_KILOBYTE;
    JkBuffer log_memory = {.size = 1 * JK_MEGABYTE};
    if (ensure_memory(AI_MEMORY_SIZE + JK_ARRAY_COUNT(c.scratch_arenas) * scratch_arena_size
                + log_memory.size)) {
        for (int64_t i = 0; i < JK_ARRAY_COUNT(c.scratch_arenas); i++) {
            JkBuffer memory = {
                .size = scratch_arena_size,
                .data = __heap_base + AI_MEMORY_SIZE + i * scratch_arena_size,
            };
            c.scratch_arenas[i] = jk_arena_fixed_init(scratch_arena_roots + i, memory);
        }
        log_memory.data = __heap_base + AI_MEMORY_SIZE
                + JK_ARRAY_COUNT(c.scratch_arenas) * scratch_arena_size;
        c.log = jk_log_init(debug_print, log_memory);
        jk_context = &c;
        return 1;
    } else {
        return 0;
    }
}

b32 ai_begin_request(double os_time)
{
    if (g_ai_request.wants_ai_move) {
        g_ai_arena = jk_arena_fixed_init(
                &g_ai_arena_root, (JkBuffer){.size = AI_MEMORY_SIZE, .data = __heap_base});
        ai_init(&g_ai_arena, &g_ai, g_ai_request.board, os_time, 1000);
        return 1;
    } else {
        return 0;
    }
}

b32 ai_tick(double os_time)
{
    g_ai.time = os_time;
    return ai_running(jk_context, &g_ai);
}

b32 web_is_draggable(int32_t x, int32_t y)
{
    return is_draggable(&g_chess, (JkIntVec2){x, y});
}
