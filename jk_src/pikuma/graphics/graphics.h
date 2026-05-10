#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <jk_src/jk_lib/jk_lib.h>
#include <jk_src/jk_shapes/jk_shapes.h>

#define SDF_SPREAD 8.0f

#define FPS 60
#define DELTA_TIME (1.0f / FPS)

#define SAMPLE_COUNT 4
#define LANE_COUNT 8
#define TILE_SIDE_LENGTH 32

#define TEXTURE_POW_2 8
#define TEXTURE_SIDE_LENGTH (1 << TEXTURE_POW_2)
#define TEXTURE_PIXEL_COUNT (TEXTURE_SIDE_LENGTH * TEXTURE_SIDE_LENGTH)
#define TEXTURE_MASK (TEXTURE_SIDE_LENGTH - 1)

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
    ENV_FLAG_DEBUG_DISPLAY,
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

typedef struct Texture {
    JkColor bg;
    JkColor colors[4];
    JkColor data[TEXTURE_PIXEL_COUNT];
} Texture;

typedef struct TextureArray {
    int64_t count;
    Texture *e;
} TextureArray;

typedef enum ObjectFlag {
    OBJ_COLLIDE,
    OBJ_FLAT,
    OBJ_WALKABLE,
    OBJ_FLAG_COUNT,
} ObjectFlag;

typedef struct ObjectId {
    int64_t i;
} ObjectId;

typedef struct Object {
    uint32_t flags;
    ObjectId parent;
    JkTransform transform;
    JkSpan vertices; // JkVec3Array
    JkSpan faces; // FaceArray
    int32_t texture_id;
    float repeat_size;
} Object;

typedef struct ObjectArray {
    int64_t count;
    Object *e;
} ObjectArray;

typedef struct Assets {
    JkSpan texcoords; // JkVec2Array
    JkSpan objects; // ObjectArray
    JkSpan textures; // TextureArray

    float font_ascent;
    float font_descent;
    float font_monospace_advance_width;
    JkShape shapes[95];
} Assets;

#define ASCII_TO_SHAPE_OFFSET (1 - 32)

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
    int64_t frame_id;
    float camera_yaw;
    float camera_pitch;
    JkVec3 player_position;
    int64_t test_frames_remaining;
} State;

typedef struct Recording {
    int64_t count;
    State initial;
    Input inputs[32 * 1024];
} Recording;

typedef struct Environment {
    // Platform layer should set these pointers
    Assets *assets;
    int64_t (*estimate_cpu_frequency)(int64_t);

    // Platform layer should ensure these point to buffers of the given size
    JkColor *draw_buffer; // DRAW_BUFFER_SIZE
    float *z_buffer; // Z_BUFFER_SIZE
    Recording *recording; // sizeof(Recording)

    // Platform layer should set ENV_FLAG_RUNNING if initialization was successful
    uint64_t flags;

    RecordState record_state;
    int64_t playback_cursor;

    Input input;
    State state;

    // The platform layer can set this at any time to stop the app from running at a convenient
    // point. The platform layer should not touch ENV_FLAG_RUNNING directly beyond initial setup.
    _Alignas(64) b32 volatile shutdown_requested;
} Environment;

typedef void RenderFunction(JkContext *context, Environment *env);
RenderFunction render;

#endif
