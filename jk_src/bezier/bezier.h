#ifndef BEZIER_H
#define BEZIER_H

#include <jk_src/chess/chess.h>
#include <jk_src/jk_lib/jk_lib.h>
#include <jk_src/jk_shapes/jk_shapes.h>
#include <stdint.h>

#define FRAME_RATE 60

#define CLEAR_COLOR_B 0x27
#define CLEAR_COLOR_G 0x20
#define CLEAR_COLOR_R 0x16

typedef struct Bezier {
    uint64_t time;
    int32_t draw_square_side_length;
    JkColor *draw_buffer;
    int64_t cpu_timer_frequency;
    uint64_t (*cpu_timer_get)(void);
    void (*debug_print)(char *);
    uint8_t memory[512ll * 1024 * 1024];
} Bezier;

typedef void BezierRenderFunction(JkContext *context, ChessAssets *assets, Bezier *bezier);
BezierRenderFunction bezier_render;

#endif
