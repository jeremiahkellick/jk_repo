#ifndef CHESS_H
#define CHESS_H

#include <jk_src/jk_lib/jk_lib.h>
#include <jk_src/jk_shapes/jk_shapes.h>
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

typedef enum SoundIndex {
    SOUND_NONE,
    SOUND_MOVE,
    SOUND_CAPTURE,
    SOUND_COUNT,
} SoundIndex;

typedef struct Audio {
    // Platform read-write, game read-only
    int64_t sample_count;
    AudioSample *sample_buffer;

    // Platform doesn't access, game read-write
    int64_t time;
    SoundIndex sound;
    int64_t sound_started_time;
} Audio;

typedef struct Bitmap {
    Color *memory;
    int32_t width;
    int32_t height;
} Bitmap;

typedef enum Team {
    WHITE,
    BLACK,
} Team;

typedef enum PieceType {
    NONE,
    KING,
    QUEEN,
    ROOK,
    BISHOP,
    KNIGHT,
    PAWN,
    PIECE_TYPE_COUNT,
} PieceType;

#define CHARACTER_SHAPE_OFFSET (32 - PIECE_TYPE_COUNT)

typedef struct Piece {
    PieceType type;
    Team team;
} Piece;

typedef struct MovePacked {
    uint16_t bits;
} MovePacked;

typedef struct Move {
    uint8_t src;
    uint8_t dest;
    Piece piece;
} Move;

typedef struct MoveArray {
    uint8_t count;
    MovePacked data[UINT8_MAX];
} MoveArray;

typedef enum BoardFlagIndex {
    BOARD_FLAG_INDEX_CURRENT_PLAYER,
    BOARD_FLAG_INDEX_WHITE_QUEEN_SIDE_CASTLING_RIGHTS,
    BOARD_FLAG_INDEX_WHITE_KING_SIDE_CASTLING_RIGHTS,
    BOARD_FLAG_INDEX_BLACK_QUEEN_SIDE_CASTLING_RIGHTS,
    BOARD_FLAG_INDEX_BLACK_KING_SIDE_CASTLING_RIGHTS,
} BoardFlagIndex;

#define BOARD_FLAG_CURRENT_PLAYER (1llu << BOARD_FLAG_INDEX_CURRENT_PLAYER)
#define BOARD_FLAG_WHITE_QUEEN_SIDE_CASTLING_RIGHTS \
    (1llu << BOARD_FLAG_INDEX_WHITE_QUEEN_SIDE_CASTLING_RIGHTS)
#define BOARD_FLAG_WHITE_KING_SIDE_CASTLING_RIGHTS \
    (1llu << BOARD_FLAG_INDEX_WHITE_KING_SIDE_CASTLING_RIGHTS)
#define BOARD_FLAG_BLACK_QUEEN_SIDE_CASTLING_RIGHTS \
    (1llu << BOARD_FLAG_INDEX_BLACK_QUEEN_SIDE_CASTLING_RIGHTS)
#define BOARD_FLAG_BLACK_KING_SIDE_CASTLING_RIGHTS \
    (1llu << BOARD_FLAG_INDEX_BLACK_KING_SIDE_CASTLING_RIGHTS)

// We can represent a square with 4 bits where the three lower order bits are the piece and the high
// order bit is the team. Then a board is 4 bits * 64 squares / 8 bits per byte = 32 bytes
typedef struct Board {
    uint8_t bytes[32];
    MovePacked move_prev;
    uint64_t flags;
} Board;

typedef enum FlagIndex {
    FLAG_INDEX_INITIALIZED,
    FLAG_INDEX_HOLDING_PIECE,
    FLAG_INDEX_REQUEST_AI_MOVE,
} FlagIndex;

#define FLAG_INITIALIZED (1llu << FLAG_INDEX_INITIALIZED)
#define FLAG_HOLDING_PIECE (1llu << FLAG_INDEX_HOLDING_PIECE)
#define FLAG_REQUEST_AI_MOVE (1llu << FLAG_INDEX_REQUEST_AI_MOVE)

#define ATLAS_SQUARE_SIDE_LENGTH 384
#define ATLAS_WIDTH (ATLAS_SQUARE_SIDE_LENGTH * 5)
#define ATLAS_HEIGHT (ATLAS_SQUARE_SIDE_LENGTH * 6)

#define DRAW_BUFFER_WIDTH (ATLAS_SQUARE_SIDE_LENGTH * 8)
#define DRAW_BUFFER_HEIGHT (ATLAS_SQUARE_SIDE_LENGTH * 8)

#define CLEAR_COLOR_B 0x27
#define CLEAR_COLOR_G 0x20
#define CLEAR_COLOR_R 0x16

typedef enum PlayerType {
    PLAYER_HUMAN,
    PLAYER_AI,
} PlayerType;

typedef enum Result {
    RESULT_NONE,
    RESULT_STALEMATE,
    RESULT_CHECKMATE,
} Result;

typedef struct ChessAssets {
    int32_t font_y0_min;
    int32_t font_y1_max;
    JkShape shapes[PIECE_TYPE_COUNT + 95];
    JkSpan sounds[SOUND_COUNT];
} ChessAssets;

typedef struct Chess {
    uint64_t flags;
    PlayerType player_types[2];
    JkIntVector2 selected_square;
    JkIntVector2 promo_square;
    Input input;
    Input input_prev;
    Audio audio;
    int32_t square_side_length;
    int32_t square_side_length_prev;
    Color *draw_buffer;
    uint64_t time;
    Board board;
    uint8_t atlas[ATLAS_WIDTH * ATLAS_HEIGHT];
    uint8_t scaled_atlas[ATLAS_WIDTH * ATLAS_HEIGHT];
    JkBuffer ai_memory;
    MoveArray moves;
    Move ai_move;
    Result result;
    Team victor;
    uint64_t cpu_timer_frequency;
    uint64_t (*cpu_timer_get)(void);
    void (*debug_print)(char *);
} Chess;

typedef Move AiMoveGetFunction(Chess *chess);
AiMoveGetFunction ai_move_get;

typedef void UpdateFunction(ChessAssets *assets, Chess *chess);
UpdateFunction update;

typedef void RenderFunction(ChessAssets *assets, Chess *chess);
RenderFunction render;

#endif
