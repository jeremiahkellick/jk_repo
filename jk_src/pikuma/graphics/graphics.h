#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <jk_src/jk_lib/jk_lib.h>

#define FPS 60
#define DELTA_TIME (1.0f / FPS)

#define SAMPLE_COUNT 4
#define LANE_COUNT 8
#define TILE_SIDE_LENGTH 64

#define DRAW_BUFFER_SIDE_LENGTH 4096ll
#define PIXEL_COUNT (DRAW_BUFFER_SIDE_LENGTH * DRAW_BUFFER_SIDE_LENGTH)
#define DRAW_BUFFER_SIZE (SAMPLE_COUNT * PIXEL_COUNT * JK_SIZEOF(JkColor))
#define Z_BUFFER_SIZE (SAMPLE_COUNT * PIXEL_COUNT * JK_SIZEOF(float))

#define CLEAR_COLOR_R 0x00
#define CLEAR_COLOR_G 0x00
#define CLEAR_COLOR_B 0x00

typedef enum Flag {
    FLAG_INITIALIZED,
} Flag;

typedef enum EnvironmentFlag {
    ENV_FLAG_RUNNING,
} EnvironmentFlag;

typedef enum RecordState {
    RECORD_STATE_NONE,
    RECORD_STATE_RECORDING,
    RECORD_STATE_PLAYBACK,
    RECORD_STATE_COUNT,
} RecordState;

typedef struct Face {
    int32_t v[3]; // vertex indexes
    int32_t t[3]; // texcoord indexes
} Face;

typedef struct FaceArray {
    int64_t count;
    Face *e;
} FaceArray;

typedef struct Bitmap {
    JkIntVec2 dimensions;
    JkColor *memory;
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
    Object *e;
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

typedef struct Input {
    JkIntVec2 dimensions;
    JkKeyboard keyboard;
    JkMouse mouse;
} Input;

typedef struct State {
    uint64_t flags;
    float camera_yaw;
    float camera_pitch;
    JkVec2 camera_position;
    int64_t test_frames_remaining;
} State;

typedef struct Recording {
    int64_t count;
    State initial;
    Input inputs[32 * 1024];
} Recording;

typedef struct Environment {
    uint64_t flags;
    RecordState record_state;
    int64_t playback_cursor;
    JkColor *draw_buffer;
    float *z_buffer;
    Recording *recording;
    int64_t (*estimate_cpu_frequency)(int64_t);

    _Alignas(64) b32 volatile should_run;
} Environment;

typedef void RenderFunction(
        JkContext *context, Assets *assets, Environment *env, State *state, Input *input);
RenderFunction render;

#endif
