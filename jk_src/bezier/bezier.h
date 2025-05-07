#ifndef BEZIER_H
#define BEZIER_H

#include <jk_src/jk_lib/jk_lib.h>
#include <jk_src/stb/stb_truetype.h>
#include <stdint.h>

#define FRAME_RATE 60

typedef enum PenCommandType {
    PEN_COMMAND_MOVE,
    PEN_COMMAND_LINE,
    PEN_COMMAND_CURVE,
} PenCommandType;

typedef struct PenCommand {
    PenCommandType type;
    JkVector2 coords[3];
} PenCommand;

typedef struct PenCommandArray {
    uint64_t count;
    PenCommand *items;
} PenCommandArray;

typedef struct Character {
    int32_t x0, y0, x1, y1;
    PenCommandArray shape;
} Character;

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

typedef struct Bezier {
    uint64_t time;
    int32_t draw_square_side_length;
    Color *draw_buffer;
    uint64_t cpu_timer_frequency;
    uint64_t (*cpu_timer_get)(void);
    void (*debug_print)(char *);
    uint8_t memory[512llu * 1024 * 1024];
    uint8_t scratch_memory[512llu * 1024 * 1024];
    PenCommandArray shape;
    JkBuffer ttf_file;
    stbtt_fontinfo font;
    b32 initialized;
    int32_t font_y0_min;
    int32_t font_y1_max;
    Character character;
    PenCommand *shape_memory;
} Bezier;

typedef void RenderFunction(Bezier *chess);
RenderFunction render;

#endif
