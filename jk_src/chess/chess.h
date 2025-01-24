#ifndef CHESS_H
#define CHESS_H

#include <stdint.h>

#define SAMPLES_PER_SECOND 48000
#define FRAME_RATE 60

#define SAMPLES_PER_FRAME (SAMPLES_PER_SECOND / FRAME_RATE)

typedef enum InputId {
    INPUT_UP,
    INPUT_DOWN,
    INPUT_LEFT,
    INPUT_RIGHT,
    INPUT_CONFIRM,
    INPUT_CANCEL,
} InputId;

#define INPUT_FLAG_UP (1 << INPUT_UP)
#define INPUT_FLAG_DOWN (1 << INPUT_DOWN)
#define INPUT_FLAG_LEFT (1 << INPUT_LEFT)
#define INPUT_FLAG_RIGHT (1 << INPUT_RIGHT)
#define INPUT_FLAG_CONFIRM (1 << INPUT_CONFIRM)
#define INPUT_FLAG_CANCEL (1 << INPUT_CANCEL)

typedef struct Input {
    int64_t flags;
    int32_t mouse_x;
    int32_t mouse_y;
} Input;

typedef struct Color {
    uint8_t b;
    uint8_t g;
    uint8_t r;
    uint8_t padding[1];
} Color;

typedef enum AudioChannel {
    AUDIO_CHANNEL_LEFT,
    AUDIO_CHANNEL_RIGHT,
    AUDIO_CHANNEL_COUNT,
} AudioChannel;

typedef struct AudioSample {
    int16_t channels[AUDIO_CHANNEL_COUNT];
} AudioSample;

typedef struct Audio {
    uint32_t sample_count;
    AudioSample *sample_buffer;
    uint32_t audio_time;
    double sin_t;
} Audio;

#define SQUARE_SIZE 112

typedef struct Bitmap {
    Color *memory;
    int32_t width;
    int32_t height;
} Bitmap;

// We can represent a square with 4 bits where the three lower order bits are the piece and the high
// order bit is the team. Then a board is 4 bits * 64 squares / 8 bits per byte = 32 bytes
typedef struct Board {
    uint8_t bytes[32];
} Board;

#define FLAG_INITIALIZED (1 << 0)

typedef struct Chess {
    uint64_t flags;
    Input input;
    Audio audio;
    Bitmap bitmap;
    int64_t time;
    Board board;
    uint8_t tilemap[SQUARE_SIZE * SQUARE_SIZE * 6];
} Chess;

#define UPDATE_FUNCTION(name) void name(Chess *chess)
typedef UPDATE_FUNCTION(UpdateFunction);
UpdateFunction update;

#define RENDER_FUNCTION(name) void name(Chess *chess)
typedef RENDER_FUNCTION(RenderFunction);
RenderFunction render;

#endif
