#ifndef BEZIER_H
#define BEZIER_H

#include <jk_src/jk_lib/jk_lib.h>
#include <jk_src/jk_shapes/jk_shapes.h>
#include <jk_src/stb/stb_truetype.h>
#include <stdint.h>

#define FRAME_RATE 60

#define CLEAR_COLOR_B 0x27
#define CLEAR_COLOR_G 0x20
#define CLEAR_COLOR_R 0x16

#define DRAW_BUFFER_SIDE_LENGTH 3072

typedef struct Bezier {
    uint64_t time;
    int32_t draw_square_side_length;
    JkColor *draw_buffer;
    uint64_t cpu_timer_frequency;
    uint64_t (*cpu_timer_get)(void);
    void (*debug_print)(char *);
    uint8_t memory[512llu * 1024 * 1024];
    uint8_t scratch_memory[512llu * 1024 * 1024];
    JkShapesPenCommandArray pawn_commands;
    JkBuffer ttf_file;
    stbtt_fontinfo font;
    b32 initialized;
    int32_t font_y0_min;
    int32_t font_y1_max;
    JkArena arena;
    void *arena_saved_pointer;
    float prev_scale;
    JkShape shapes[96];
} Bezier;

typedef void RenderFunction(Bezier *chess);
RenderFunction render;

#endif
