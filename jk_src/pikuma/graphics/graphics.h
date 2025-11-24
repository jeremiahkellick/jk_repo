#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <jk_src/jk_lib/jk_lib.h>

#define DRAW_BUFFER_SIDE_LENGTH 4096
#define DRAW_BUFFER_SIZE (sizeof(JkColor) * DRAW_BUFFER_SIDE_LENGTH * DRAW_BUFFER_SIDE_LENGTH)

#define CLEAR_COLOR_R 0x16
#define CLEAR_COLOR_G 0x20
#define CLEAR_COLOR_B 0x27

typedef enum InputId {
    INPUT_CONFIRM,
    INPUT_RESET,
} InputId;

typedef struct Input {
    uint64_t flags;
    JkIntVector2 mouse_pos;
} Input;

typedef enum Flag {
    FLAG_INITIALIZED,
} Flag;

typedef struct Face {
    int32_t v[3];
} Face;

typedef struct FaceArray {
    uint64_t count;
    Face *items;
} FaceArray;

typedef struct Assets {
    JkSpan vertices;
    JkSpan faces;
} Assets;

typedef struct State {
    JkColor *draw_buffer;
    JkBuffer memory;
    uint64_t os_timer_frequency;

    uint64_t flags;
    JkIntVector2 dimensions;
    uint64_t os_time;
    Input input;
} State;

typedef void RenderFunction(Assets *assets, State *state);
RenderFunction render;

#endif
