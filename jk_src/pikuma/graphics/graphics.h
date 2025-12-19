#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <jk_src/jk_lib/jk_lib.h>

#define DRAW_BUFFER_SIDE_LENGTH 4096
#define DRAW_BUFFER_SIZE (JK_SIZEOF(JkColor) * DRAW_BUFFER_SIDE_LENGTH * DRAW_BUFFER_SIDE_LENGTH)

#define CLEAR_COLOR_R 0x16
#define CLEAR_COLOR_G 0x20
#define CLEAR_COLOR_B 0x27

typedef enum InputId {
    INPUT_CONFIRM,
    INPUT_RESET,
} InputId;

typedef struct Input {
    uint64_t flags;
    JkIntVec2 mouse_pos;
} Input;

typedef enum Flag {
    FLAG_INITIALIZED,
} Flag;

typedef struct Face {
    int32_t v[3]; // vertex indexes
    int32_t t[3]; // texcoord indexes
} Face;

typedef struct FaceArray {
    int64_t count;
    Face *items;
} FaceArray;

typedef struct Bitmap {
    JkIntVec2 dimensions;
    JkColor3 *memory;
} Bitmap;

typedef struct BitmapSpan {
    JkIntVec2 dimensions;
    int64_t offset;
} BitmapSpan;

typedef struct ObjectId {
    int64_t i;
} ObjectId;

typedef struct Object {
    ObjectId parent;
    JkTransform transform;
    JkSpan faces; // FaceArray
    BitmapSpan texture;
} Object;

typedef struct ObjectArray {
    int64_t count;
    Object *items;
} ObjectArray;

typedef struct Assets {
    JkSpan vertices; // JkVec3Array
    JkSpan texcoords; // JkVec2Array
    JkSpan objects; // ObjectArray
} Assets;

typedef struct State {
    JkColor *draw_buffer;
    JkBuffer memory;
    int64_t os_timer_frequency;
    void (*print)(JkBuffer string);

    uint64_t flags;
    JkIntVec2 dimensions;
    uint64_t os_time;
    Input input;
} State;

typedef void RenderFunction(Assets *assets, State *state);
RenderFunction render;

#endif
