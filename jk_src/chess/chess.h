#ifndef CHESS_H
#define CHESS_H

#include <jk_src/jk_lib/jk_lib.h>
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
    INPUT_RESET,
} InputId;

#define INPUT_FLAG_UP (1 << INPUT_UP)
#define INPUT_FLAG_DOWN (1 << INPUT_DOWN)
#define INPUT_FLAG_LEFT (1 << INPUT_LEFT)
#define INPUT_FLAG_RIGHT (1 << INPUT_RIGHT)
#define INPUT_FLAG_CONFIRM (1 << INPUT_CONFIRM)
#define INPUT_FLAG_CANCEL (1 << INPUT_CANCEL)
#define INPUT_FLAG_RESET (1 << INPUT_RESET)

typedef struct Input {
    int64_t flags;
    JkIntVector2 mouse_pos;
} Input;

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

#define SQUARE_SIDE_LENGTH 112
#define BOARD_SIDE_LENGTH (SQUARE_SIDE_LENGTH * 8)

typedef struct Bitmap {
    Color *memory;
    int32_t width;
    int32_t height;
} Bitmap;

typedef struct Move {
    uint8_t src;
    uint8_t dest;
} Move;

typedef struct MoveArray {
    uint8_t count;
    Move data[UINT8_MAX];
} MoveArray;

typedef enum BoardFlagIndex {
    BOARD_FLAG_INDEX_CURRENT_PLAYER,
    BOARD_FLAG_INDEX_WHITE_KING_MOVED,
    BOARD_FLAG_INDEX_BLACK_KING_MOVED,
    BOARD_FLAG_INDEX_A1_ROOK_MOVED,
    BOARD_FLAG_INDEX_H1_ROOK_MOVED,
    BOARD_FLAG_INDEX_A8_ROOK_MOVED,
    BOARD_FLAG_INDEX_H8_ROOK_MOVED,
} BoardFlagIndex;

#define BOARD_FLAG_CURRENT_PLAYER (1llu << BOARD_FLAG_INDEX_CURRENT_PLAYER)
#define BOARD_FLAG_WHITE_KING_MOVED (1llu << BOARD_FLAG_INDEX_WHITE_KING_MOVED)
#define BOARD_FLAG_BLACK_KING_MOVED (1llu << BOARD_FLAG_INDEX_BLACK_KING_MOVED)
#define BOARD_FLAG_A1_ROOK_MOVED (1llu << BOARD_FLAG_INDEX_A1_ROOK_MOVED)
#define BOARD_FLAG_H1_ROOK_MOVED (1llu << BOARD_FLAG_INDEX_H1_ROOK_MOVED)
#define BOARD_FLAG_A8_ROOK_MOVED (1llu << BOARD_FLAG_INDEX_A8_ROOK_MOVED)
#define BOARD_FLAG_H8_ROOK_MOVED (1llu << BOARD_FLAG_INDEX_H8_ROOK_MOVED)

// We can represent a square with 4 bits where the three lower order bits are the piece and the high
// order bit is the team. Then a board is 4 bits * 64 squares / 8 bits per byte = 32 bytes
typedef struct Board {
    uint8_t bytes[32];
    Move move_prev;
    uint64_t flags;
} Board;

typedef enum FlagIndex {
    FLAG_INDEX_INITIALIZED,
    FLAG_INDEX_HOLDING_PIECE,
} FlagIndex;

#define FLAG_INITIALIZED (1llu << FLAG_INDEX_INITIALIZED)
#define FLAG_HOLDING_PIECE (1llu << FLAG_INDEX_HOLDING_PIECE)

#define ATLAS_WIDTH (SQUARE_SIDE_LENGTH * 5llu)
#define ATLAS_HEIGHT (SQUARE_SIDE_LENGTH * 6llu)

typedef enum Result {
    RESULT_NONE,
    RESULT_STALEMATE,
    RESULT_CHECKMATE,
} Result;

typedef enum Team {
    WHITE,
    BLACK,
} Team;

typedef struct Chess {
    uint64_t flags;
    JkIntVector2 selected_square;
    Input input;
    Input input_prev;
    Audio audio;
    Bitmap bitmap;
    uint64_t time;
    Board board;
    uint8_t atlas[ATLAS_WIDTH * ATLAS_HEIGHT];
    MoveArray moves;
    Result result;
    Team victor;
    JkBuffer move_pool_memory;
    void (*debug_print)(char *string);
    uint64_t (*cpu_time)(void);
    uint64_t cpu_frequency;
} Chess;

#define UPDATE_FUNCTION(name) void name(Chess *chess)
typedef UPDATE_FUNCTION(UpdateFunction);
UpdateFunction update;

#define RENDER_FUNCTION(name) void name(Chess *chess)
typedef RENDER_FUNCTION(RenderFunction);
RenderFunction render;

#endif
