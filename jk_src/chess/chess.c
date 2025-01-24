#include "chess.h"

#include <math.h>
#include <string.h>

// #jk_build compiler_arguments /LD
// #jk_build linker_arguments /OUT:chess.dll /EXPORT:update /EXPORT:render
// #jk_build single_translation_unit

// #jk_build dependencies_begin
#include <jk_src/jk_lib/jk_lib.h>
// #jk_build dependencies_end

_Static_assert(sizeof(Color) == 4, "Color must be 4 bytes");

typedef enum Piece {
    NONE,
    KING,
    QUEEN,
    ROOK,
    BISHOP,
    KNIGHT,
    PAWN,
} Piece;

typedef enum Team {
    WHITE,
    BLACK,
} Team;

typedef enum Column {
    A,
    B,
    C,
    D,
    E,
    F,
    G,
    H,
} Column;

// clang-format off
// Byte array encoding the chess starting positions
Board starting_state = {
    0x53, 0x24, 0x41, 0x35,
    0x66, 0x66, 0x66, 0x66,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0xEE, 0xEE, 0xEE, 0xEE,
    0xDB, 0xAC, 0xC9, 0xBD,
};
// clang-format on

// We'll use this struct to pass around individual 4-bit square values. The higher 4 bits should
// always be zero.
typedef struct Square {
    uint8_t bits;
} Square;

static uint8_t square_index_get(int32_t col, int32_t row)
{
    JK_DEBUG_ASSERT(col >= 0 && col < 8);
    JK_DEBUG_ASSERT(row >= 0 && row < 8);
    return (uint8_t)(8 * row + col);
}

static Square square_get(Board board, int32_t col, int32_t row)
{
    JK_DEBUG_ASSERT(col >= 0 && col < 8);
    JK_DEBUG_ASSERT(row >= 0 && row < 8);
    uint8_t square_index = square_index_get(col, row);
    uint8_t byte_index = square_index / 2; // A square is 4 bits, so there are 2 squares per byte
    uint8_t is_higher_4_bits = square_index % 2;
    uint8_t byte = board.bytes[byte_index];
    if (is_higher_4_bits) {
        byte >>= 4;
    }
    return (Square){byte & 0xf}; // Mask to lower 4 bits
}

static void square_set(Board board, uint8_t col, uint8_t row, Square square)
{
    uint8_t square_index = square_index_get(col, row);
    uint8_t byte_index = square_index / 2; // A square is 4 bits, so there are 2 squares per byte
    uint8_t is_higher_4_bits = square_index % 2;
    if (is_higher_4_bits) {
        board.bytes[byte_index] = (board.bytes[byte_index] & 0x0f) | (square.bits << 4);
    } else {
        board.bytes[byte_index] = (board.bytes[byte_index] & 0xf0) | square.bits;
    }
}

static Piece square_piece_get(Square square)
{
    return square.bits & 0x7; // Mask to the lower 3 bits
}

static Team square_team_get(Square square)
{
    return (square.bits >> 3) & 1;
}

static void audio_write(Audio *audio, int32_t pitch_multiplier)
{
    for (uint32_t sample_index = 0; sample_index < audio->sample_count; sample_index++) {
        for (int channel_index = 0; channel_index < AUDIO_CHANNEL_COUNT; channel_index++) {
            audio->sample_buffer[sample_index].channels[channel_index] = 0;
        }
        audio->audio_time++;
    }
}

UPDATE_FUNCTION(update)
{
    if (!(chess->flags & FLAG_INITIALIZED)) {
        chess->flags |= FLAG_INITIALIZED;
        memcpy(&chess->board, &starting_state, sizeof(chess->board));
    }

    audio_write(&chess->audio, (chess->input.flags & INPUT_FLAG_UP) ? 2 : 1);

    chess->time++;
}

Color background = {0x24, 0x29, 0x25};

// Color light_squares = {0xde, 0xe2, 0xde};
// Color dark_squares = {0x39, 0x41, 0x3a};

Color light_squares = {0xe9, 0xe2, 0xd7};
Color dark_squares = {0x50, 0x41, 0x2b};

//Color light_squares = {0xdf, 0xdd, 0xec};
//Color dark_squares = {0x38, 0x33, 0x5b};

// Color white = {0x8e, 0x8e, 0x8e};
Color white = {0x82, 0x92, 0x85};
Color black = {0xff, 0x73, 0xa2};

RENDER_FUNCTION(render)
{
    int32_t board_size = SQUARE_SIZE * 8;
    int32_t x_offset = (chess->bitmap.width - board_size) / 2;
    if (x_offset < 0) {
        x_offset = 0;
    }
    int32_t y_offset = (chess->bitmap.height - board_size) / 2;
    if (y_offset < 0) {
        y_offset = 0;
    }

    for (int32_t y = 0; y < chess->bitmap.height; y++) {
        for (int32_t x = 0; x < chess->bitmap.width; x++) {
            int32_t board_x = x - x_offset;
            int32_t board_y = y - y_offset;
            Color color;
            if (board_x >= 0 && board_x < board_size && board_y >= 0 && board_y < board_size) {
                if ((board_x / SQUARE_SIZE) % 2 == (board_y / SQUARE_SIZE) % 2) {
                    color = light_squares;
                } else {
                    color = dark_squares;
                }
                int32_t square_x = board_x % SQUARE_SIZE;
                int32_t square_y = board_y % SQUARE_SIZE;
                Square square =
                        square_get(chess->board, board_x / SQUARE_SIZE, 7 - board_y / SQUARE_SIZE);
                Piece piece = square_piece_get(square);
                if (piece != NONE) {
                    int32_t tilemap_y_offset = (piece - 1) * 112;
                    int32_t pixel_index = SQUARE_SIZE * (square_y + tilemap_y_offset) + square_x;
                    int32_t byte_index = pixel_index / 8;
                    uint8_t bit_index = pixel_index % 8;
                    if ((chess->tilemap[byte_index] >> bit_index) & 1) {
                        color = square_team_get(square) ? black : white;
                    }
                }
            } else {
                color = background;
            }
            chess->bitmap.memory[y * chess->bitmap.width + x] = color;
        }
    }
}
