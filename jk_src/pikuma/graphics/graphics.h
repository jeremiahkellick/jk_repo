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

typedef struct State {
    JkColor *draw_buffer;
    JkBuffer memory;
    uint64_t os_timer_frequency;

    JkIntVector2 dimensions;
    uint64_t os_time;
    Input input;
} State;

typedef void RenderFunction(State *state);
RenderFunction render;

#endif
