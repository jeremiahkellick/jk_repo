#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <jk_src/jk_lib/jk_lib.h>

#define FPS 60
#define DELTA_TIME (1.0f / FPS)

#define DRAW_BUFFER_SIDE_LENGTH 4096ll
#define PIXEL_COUNT (2 * DRAW_BUFFER_SIDE_LENGTH * DRAW_BUFFER_SIDE_LENGTH + 1)
#define DRAW_BUFFER_SIZE (JK_SIZEOF(JkColor) * PIXEL_COUNT)
#define NEXT_BUFFER_SIZE (JK_SIZEOF(PixelIndex) * PIXEL_COUNT)
#define Z_BUFFER_SIZE (JK_SIZEOF(float) * PIXEL_COUNT)

#define CLEAR_COLOR_R 0x16
#define CLEAR_COLOR_G 0x20
#define CLEAR_COLOR_B 0x27

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
    float repeat_size;
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

typedef struct PixelIndex {
    int32_t i;
} PixelIndex;

typedef struct Pixel {
    JkColor *color;
    float *z;
    PixelIndex *next;
} Pixel;

typedef struct State {
    JkColor *draw_buffer;
    float *z_buffer;
    PixelIndex *next_buffer;
    JkBuffer memory;
    int64_t os_timer_frequency;
    void (*print)(JkBuffer string);

    JkKeyboard keyboard;
    JkIntVec2 mouse_pos;
    JkVec2 mouse_delta;

    uint64_t flags;
    float camera_yaw;
    float camera_pitch;
    int64_t pixel_count;
    JkIntVec2 dimensions;
    uint64_t os_time;
} State;

typedef void RenderFunction(Assets *assets, State *state);
RenderFunction render;

#endif
