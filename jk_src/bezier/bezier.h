#ifndef BEZIER_H
#define BEZIER_H

#include <stdint.h>

#define FRAME_RATE 60

typedef struct Color {
    union {
        struct {
            uint8_t b;
            uint8_t g;
            uint8_t r;
            uint8_t a;
        };
        uint8_t v[4];
    };
} Color;

typedef struct Bitmap {
    Color *memory;
    int32_t width;
    int32_t height;
} Bitmap;

#define CLEAR_COLOR_B 0x27
#define CLEAR_COLOR_G 0x20
#define CLEAR_COLOR_R 0x16

#define DRAW_BUFFER_SIDE_LENGTH 3072
#define DRAW_BUFFER_WIDTH DRAW_BUFFER_SIDE_LENGTH
#define DRAW_BUFFER_HEIGHT DRAW_BUFFER_SIDE_LENGTH

typedef struct Bezier {
    uint64_t time;
    int32_t draw_square_side_length;
    Color *draw_buffer;
    uint64_t cpu_timer_frequency;
    uint64_t (*cpu_timer_get)(void);
    void (*debug_print)(char *);
    uint8_t memory[1024 * 1024];
} Bezier;

typedef void RenderFunction(Bezier *chess);
RenderFunction render;

#endif
