#ifndef CHESS_H
#define CHESS_H

#include <jk_src/jk_lib/jk_lib.h>
#include <jk_src/jk_shapes/jk_shapes.h>
#include <stdint.h>

#define SAMPLES_PER_SECOND 48000

#define SAMPLES_PER_FRAME (SAMPLES_PER_SECOND / FRAME_RATE)

typedef enum InputId {
    INPUT_CONFIRM,
    INPUT_CANCEL,
    INPUT_RESET,
} InputId;

typedef struct Input {
    int64_t flags;
    JkIntVector2 mouse_pos;
} Input;

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
    SOUND_CHECK,
    SOUND_DRAW,
    SOUND_WIN,
    SOUND_LOSE,

    SOUND_COUNT,
} SoundIndex;

typedef struct Bitmap {
    JkColor *memory;
    int32_t width;
    int32_t height;
} Bitmap;

typedef enum Team {
    WHITE,
    BLACK,

    TEAM_COUNT,
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

#define CHARACTER_SHAPE_OFFSET (PIECE_TYPE_COUNT - 32)

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

typedef enum MoveFlag {
    MOVE_FLAG_AGE_BIT_1,
    MOVE_FLAG_AGE_BIT_2,
    MOVE_FLAG_CHECK_EVALUATED,
    MOVE_FLAG_IN_CHECK,
} MoveFlag;

#define MOVE_FLAGS_AGE_MASK 0x3

typedef enum NodeAge {
    NODE_AGE_CHILD = 0,
    NODE_AGE_ADULT = 1,
    NODE_AGE_ELDER = 2,
} NodeAge;

typedef struct MoveNode {
#if JK_BUILD_MODE != JK_RELEASE
    struct MoveNode *parent;
#endif
    struct MoveNode *next_sibling;
    struct MoveNode *first_child;
    struct MoveNode *search_candidate;

    MovePacked move;
    uint8_t flags;
    uint8_t line_depth;
    int32_t score;
} MoveNode;

typedef enum BoardFlag {
    BOARD_FLAG_CURRENT_PLAYER,
    BOARD_FLAG_WHITE_QUEEN_SIDE_CASTLING_RIGHTS,
    BOARD_FLAG_WHITE_KING_SIDE_CASTLING_RIGHTS,
    BOARD_FLAG_BLACK_QUEEN_SIDE_CASTLING_RIGHTS,
    BOARD_FLAG_BLACK_KING_SIDE_CASTLING_RIGHTS,
} BoardFlag;

// We can represent a square with 4 bits where the three lower order bits are the piece and the high
// order bit is the team. Then a board is 4 bits * 64 squares / 8 bits per byte = 32 bytes
typedef struct Board {
    uint8_t bytes[32];
    MovePacked move_prev;
    uint64_t flags;
} Board;

typedef enum ChessFlag {
    CHESS_FLAG_INITIALIZED,
    CHESS_FLAG_HOLDING_PIECE,
    CHESS_FLAG_WANTS_AI_MOVE,
} ChessFlag;

#define DRAW_BUFFER_SIDE_LENGTH 4096ll
#define DRAW_BUFFER_SIZE (sizeof(JkColor) * DRAW_BUFFER_SIDE_LENGTH * DRAW_BUFFER_SIDE_LENGTH)

#define CLEAR_COLOR_B 0x27
#define CLEAR_COLOR_G 0x20
#define CLEAR_COLOR_R 0x16

typedef enum PlayerType {
    PLAYER_HUMAN,
    PLAYER_AI,

    PLAYER_TYPE_COUNT,
} PlayerType;

typedef enum Result {
    RESULT_NONE,
    RESULT_STALEMATE,
    RESULT_CHECKMATE,
    RESULT_TIME,
} Result;

typedef enum Screen {
    SCREEN_GAME,
    SCREEN_MENU,

    SCREEN_COUNT,
} Screen;

typedef enum Timer {
    TIMER_1_MIN,
    TIMER_3_MIN,
    TIMER_10_MIN,
    TIMER_30_MIN,

    TIMER_COUNT,
} Timer;

typedef enum ButtonId {
    BUTTON_MENU_OPEN,
    BUTTON_MENU_CLOSE,
    BUTTON_WHITE,
    BUTTON_BLACK,
    BUTTON_RANDOM,
    BUTTON_START_GAME,
    BUTTON_YOU,
    BUTTON_AI,
    BUTTON_1_MIN,
    BUTTON_3_MIN,
    BUTTON_10_MIN,
    BUTTON_30_MIN,

    BUTTON_COUNT,
} ButtonId;

typedef struct Button {
    ButtonId id;
    JkIntRect rect;
} Button;

typedef struct ChessAssets {
    float font_ascent;
    float font_descent;
    JkShape shapes[PIECE_TYPE_COUNT + 95];
    JkSpan sounds[SOUND_COUNT];
} ChessAssets;

typedef enum TeamChoice {
    TEAM_CHOICE_WHITE,
    TEAM_CHOICE_BLACK,
    TEAM_CHOICE_RANDOM,

    TEAM_CHOICE_COUNT,
} TeamChoice;

typedef struct Settings {
    TeamChoice team_choice;
    PlayerType opponent_type;
    Timer timer;
} Settings;

typedef struct AiRequest {
    Board board;
    b32 wants_ai_move;
} AiRequest;

typedef struct AiResponse {
    Board board;
    Move move;
} AiResponse;

typedef struct AiTarget {
    Team assisting_team;
    int32_t score;
} AiTarget;

typedef struct Ai {
    AiResponse response;
    JkArena *arena;
    JkRandomGeneratorU64 generator;
    MoveNode *root;

    uint64_t time;
    uint64_t time_frequency;
    uint64_t time_started;
    uint64_t time_limit;
} Ai;

typedef struct AudioState {
    SoundIndex sound;
    uint64_t started_time;
} AudioState;

typedef struct RenderState {
    int32_t square_side_length;
    b32 holding_piece;
    Board board;
    Team perspective;
    JkIntVector2 mouse_pos;
    uint8_t selected_index;
    JkIntVector2 promo_square;
    Result result;
    int64_t time_player_seconds[2];
    Screen screen;
    Settings settings;
    uint64_t animation_time;
} RenderState;

typedef struct Chess {
    // Platform read-write, game read-only
    Input input;
    int32_t square_side_length;
    JkColor *draw_buffer;
    JkBuffer render_memory;
    AiResponse ai_response;
    uint64_t time;
    uint64_t audio_time;
    uint64_t os_time;
    uint64_t os_timer_frequency;

    // Game read-write, platform read-only
    uint64_t flags;
    uint64_t turn_index;
    Board board;
    PlayerType player_types[2];
    Team perspective;
    JkIntVector2 selected_square;
    JkIntVector2 promo_square;
    Input input_prev;
    MoveArray moves;
    Result result;
    PieceType piece_prev_type;
    uint64_t os_time_prev;
    uint64_t os_time_move_prev;
    int64_t os_time_player[TEAM_COUNT];
    uint64_t os_time_turn_start;
    Screen screen;
    Button buttons[BUTTON_COUNT];
    Settings settings;
    AudioState audio_state;
    JkRandomGeneratorU64 generator;
    RenderState render_state_prev;
} Chess;

typedef void AiInitFunction(
        JkArena *arena, Ai *ai, Board board, uint64_t time, uint64_t time_frequency);
AiInitFunction ai_init;

typedef b32 AiRunningFunction(Ai *ai);
AiRunningFunction ai_running;

typedef void AudioFunction(ChessAssets *assets,
        AudioState state,
        uint64_t time,
        uint64_t sample_count,
        AudioSample *sample_buffer);
AudioFunction audio;

typedef b32 BoardEqualFunction(Board *a, Board *b);
BoardEqualFunction board_equal;

typedef void UpdateFunction(ChessAssets *assets, Chess *chess);
UpdateFunction update;

typedef void RenderFunction(ChessAssets *assets, Chess *chess);
RenderFunction render;

typedef void PrintSetFunction(void (*print)(JkBuffer string));
PrintSetFunction print_set;

#endif
