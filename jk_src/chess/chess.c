#include "chess.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

// #jk_build compiler_arguments /LD
// #jk_build linker_arguments /OUT:chess.dll /EXPORT:update /EXPORT:render
// #jk_build single_translation_unit

// #jk_build dependencies_begin
#include <jk_src/jk_lib/jk_lib.h>
// #jk_build dependencies_end

_Static_assert(sizeof(Color) == 4, "Color must be 4 bytes");

typedef enum PieceType {
    NONE,
    KING,
    QUEEN,
    ROOK,
    BISHOP,
    KNIGHT,
    PAWN,
} PieceType;

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

typedef struct Piece {
    PieceType type;
    Team team;
} Piece;

static b32 board_in_bounds(JkIntVector2 pos)
{
    return pos.x >= 0 && pos.x < 8 && pos.y >= 0 && pos.y < 8;
}

static uint8_t board_index_get(JkIntVector2 pos)
{
    JK_DEBUG_ASSERT(board_in_bounds(pos));
    return (uint8_t)(8 * pos.y + pos.x);
}

static uint8_t board_index_get_unbounded(JkIntVector2 pos)
{
    if (board_in_bounds(pos)) {
        return board_index_get(pos);
    } else {
        return UINT8_MAX;
    }
}

static Piece board_piece_get(Board board, JkIntVector2 pos)
{
    uint8_t piece_index = board_index_get(pos);
    uint8_t byte_index = piece_index / 2; // A piece is 4 bits, so there are 2 pieces per byte
    uint8_t is_higher_4_bits = piece_index % 2;
    uint8_t byte = board.bytes[byte_index];
    if (is_higher_4_bits) {
        byte >>= 4;
    }
    return (Piece){.type = byte & 0x7, .team = (byte >> 3) & 1}; // Mask to lower 4 bits
}

static void board_piece_set(Board *board, JkIntVector2 pos, Piece piece)
{
    uint8_t bits = (uint8_t)((piece.team << 3) | piece.type);
    uint8_t piece_index = board_index_get(pos);
    uint8_t byte_index = piece_index / 2; // A piece is 4 bits, so there are 2 pieces per byte
    uint8_t is_higher_4_bits = piece_index % 2;
    if (is_higher_4_bits) {
        board->bytes[byte_index] = (board->bytes[byte_index] & 0x0f) | (bits << 4);
    } else {
        board->bytes[byte_index] = (board->bytes[byte_index] & 0xf0) | bits;
    }
}

static JkIntVector2 all_directions[8] = {
    {0, 1},
    {0, -1},
    {1, 0},
    {1, 1},
    {1, -1},
    {-1, 0},
    {-1, 1},
    {-1, -1},
};

static JkIntVector2 diagonals[4] = {
    {1, 1},
    {1, -1},
    {-1, 1},
    {-1, -1},
};

static JkIntVector2 straights[4] = {
    {0, 1},
    {0, -1},
    {1, 0},
    {-1, 0},
};

static JkIntVector2 knight_moves[8] = {
    {2, 1},
    {2, -1},
    {-2, 1},
    {-2, -1},
    {1, 2},
    {1, -2},
    {-1, 2},
    {-1, -2},
};

// Usage: pawn_moves[team]
static JkIntVector2 pawn_moves[2] = {
    {0, 1},
    {0, -1},
};

// Usage: pawn_extended_moves[team]
static JkIntVector2 pawn_extended_moves[2] = {
    {0, 2},
    {0, -2},
};

// Usage: pawn_attacks[team][i]
static JkIntVector2 pawn_attacks[2][2] = {
    {
        {1, 1},
        {-1, 1},
    },
    {
        {1, -1},
        {-1, -1},
    },
};

static b32 square_available(Board board, Team team, JkIntVector2 square)
{
    if (board_in_bounds(square)) {
        Piece piece = board_piece_get(board, square);
        return piece.type == NONE || piece.team != team;
    } else {
        return 0;
    }
}

static void moves_append(Chess *chess, JkIntVector2 src, JkIntVector2 dest)
{
    chess->moves[chess->moves_count++] = (Move){
        .src = board_index_get(src),
        .dest = board_index_get(dest),
    };
}

// Returns the number of moves written to the buffer
static void moves_get(Chess *chess, Team current_team, Move *moves_buffer)
{
    chess->moves_count = 0;

    JkIntVector2 src;
    for (src.y = 0; src.y < 8; src.y++) {
        for (src.x = 0; src.x < 8; src.x++) {
            Piece piece = board_piece_get(chess->board, src);
            if (piece.team == current_team) {
                switch (piece.type) {
                case NONE: {
                } break;

                case KING: {
                    for (int32_t i = 0; i < JK_ARRAY_COUNT(all_directions); i++) {
                        JkIntVector2 dest = jk_int_vector_2_add(src, all_directions[i]);
                        if (square_available(chess->board, current_team, dest)) {
                            moves_append(chess, src, dest);
                        }
                    }
                } break;

                case QUEEN: {
                } break;

                case ROOK: {
                } break;

                case BISHOP: {
                } break;

                case KNIGHT: {
                } break;

                case PAWN: {
                    // Basic move
                    {
                        JkIntVector2 dest = jk_int_vector_2_add(src, pawn_moves[current_team]);
                        if (board_in_bounds(dest)
                                && board_piece_get(chess->board, dest).type == NONE) {
                            moves_append(chess, src, dest);
                        }
                    }

                    // Extended move
                    if ((current_team == WHITE && src.y == 1)
                            || (current_team == BLACK && src.y == 6)) {
                        JkIntVector2 dest =
                                jk_int_vector_2_add(src, pawn_extended_moves[current_team]);
                        if (board_in_bounds(dest)
                                && board_piece_get(chess->board, dest).type == NONE) {
                            moves_append(chess, src, dest);
                        }
                    }

                    // Attacks
                    for (uint8_t i = 0; i < 2; i++) {
                        JkIntVector2 dest = jk_int_vector_2_add(src, pawn_attacks[current_team][i]);
                        if (board_in_bounds(dest)) {
                            Piece piece_at_dest = board_piece_get(chess->board, dest);
                            if (piece_at_dest.type != NONE && piece_at_dest.team != current_team) {
                                moves_append(chess, src, dest);
                            }
                        }
                    }

                    // TODO: en-passant
                } break;
                }
            }
        }
    }
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

static JkIntVector2 screen_board_origin_get(Bitmap *bitmap)
{
    JkIntVector2 result = {0};

    int32_t x = (bitmap->width - BOARD_SIDE_LENGTH) / 2;
    int32_t y = (bitmap->height - BOARD_SIDE_LENGTH) / 2;
    if (x >= 0) {
        result.x = x;
    }
    if (y >= 0) {
        result.y = y;
    }

    return result;
}

static JkIntVector2 screen_to_board_pos_pixels(Bitmap *bitmap, JkIntVector2 screen_pos)
{
    return jk_int_vector_2_sub(screen_pos, screen_board_origin_get(bitmap));
}

static b32 board_pos_pixels_in_bounds(JkIntVector2 board_pos_pixels)
{
    return board_pos_pixels.x >= 0 && board_pos_pixels.x < BOARD_SIDE_LENGTH
            && board_pos_pixels.y >= 0 && board_pos_pixels.y < BOARD_SIDE_LENGTH;
}

static JkIntVector2 board_pos_pixels_to_squares(JkIntVector2 board_pos_pixels)
{
    if (board_pos_pixels_in_bounds(board_pos_pixels)) {
        return (JkIntVector2){
            .x = board_pos_pixels.x / SQUARE_SIDE_LENGTH,
            .y = 7 - board_pos_pixels.y / SQUARE_SIDE_LENGTH,
        };
    } else {
        return (JkIntVector2){-1, -1};
    }
}

static JkIntVector2 screen_to_board_pos(Bitmap *bitmap, JkIntVector2 screen_pos)
{
    return board_pos_pixels_to_squares(screen_to_board_pos_pixels(bitmap, screen_pos));
}

static b32 button_pressed(Chess *chess, uint64_t flag)
{
    return (chess->input.flags & flag) && !(chess->input_prev.flags & flag);
}

static uint64_t destinations_get_by_src(Chess *chess, uint8_t src)
{
    uint64_t result = 0;
    for (uint8_t i = 0; i < chess->moves_count; i++) {
        if (chess->moves[i].src == src) {
            result |= 1llu << chess->moves[i].dest;
        }
    }
    return result;
}

UPDATE_FUNCTION(update)
{
    // Debug reset
    if (button_pressed(chess, INPUT_FLAG_RESET)) {
        chess->flags &= ~FLAG_INITIALIZED;
    }

    if (!(chess->flags & FLAG_INITIALIZED)) {
        chess->flags |= FLAG_INITIALIZED;

        chess->flags &= ~FLAG_CURRENT_PLAYER;
        chess->selected_square = (JkIntVector2){-1, -1};
        memcpy(&chess->board, &starting_state, sizeof(chess->board));
        moves_get(chess, WHITE, chess->moves);
    }

    uint64_t available_destinations =
            destinations_get_by_src(chess, board_index_get_unbounded(chess->selected_square));

    if (button_pressed(chess, INPUT_FLAG_CONFIRM)) {
        JkIntVector2 clicked_pos = screen_to_board_pos(&chess->bitmap, chess->input.mouse_pos);
        if (board_in_bounds(clicked_pos)) {
            uint8_t clicked_index = board_index_get(clicked_pos);
            if (available_destinations & (1llu << clicked_index)) {
                Piece piece = board_piece_get(chess->board, chess->selected_square);
                board_piece_set(&chess->board, chess->selected_square, (Piece){0});
                board_piece_set(&chess->board, clicked_pos, piece);
                chess->selected_square = (JkIntVector2){-1, -1};
                chess->flags ^= FLAG_CURRENT_PLAYER;
                moves_get(chess, (chess->flags >> FLAG_INDEX_CURRENT_PLAYER) & 1, chess->moves);
            } else {
                for (uint8_t i = 0; i < chess->moves_count; i++) {
                    if (chess->moves[i].src == board_index_get(clicked_pos)) {
                        chess->selected_square = clicked_pos;
                    }
                }
            }
        }
    }
    if (button_pressed(chess, INPUT_FLAG_CANCEL)) {
        chess->selected_square = (JkIntVector2){-1, -1};
    }

    audio_write(&chess->audio, (chess->input.flags & INPUT_FLAG_UP) ? 2 : 1);

    chess->time++;

    chess->input_prev = chess->input;
}

Color background = {0x24, 0x29, 0x25};

// Color light_squares = {0xde, 0xe2, 0xde};
// Color dark_squares = {0x39, 0x41, 0x3a};

Color light_squares = {0xe9, 0xe2, 0xd7};
Color dark_squares = {0x50, 0x41, 0x2b};

// Blended halfway between the base square colors and #E26D5C
Color light_squares_selected = {0xa3, 0xa8, 0xdd};
Color dark_squares_selected = {0x56, 0x57, 0x87};

// Color light_squares = {0xdf, 0xdd, 0xec};
// Color dark_squares = {0x38, 0x33, 0x5b};

// Color white = {0x8e, 0x8e, 0x8e};
Color white = {0x82, 0x92, 0x85};
Color black = {0xff, 0x73, 0xa2};

RENDER_FUNCTION(render)
{
    // Figure out which squares should be highlighted
    uint64_t highlighted = 0;
    uint8_t selected_index = board_index_get_unbounded(chess->selected_square);
    if (selected_index < 64) {
        highlighted |= 1llu << selected_index;
    }
    highlighted |= destinations_get_by_src(chess, selected_index);

    JkIntVector2 pos;
    for (pos.y = 0; pos.y < chess->bitmap.height; pos.y++) {
        for (pos.x = 0; pos.x < chess->bitmap.width; pos.x++) {
            JkIntVector2 board_pos_pixels = screen_to_board_pos_pixels(&chess->bitmap, pos);
            Color color;
            if (board_pos_pixels_in_bounds(board_pos_pixels)) {
                JkIntVector2 board_pos = board_pos_pixels_to_squares(board_pos_pixels);
                uint8_t index = board_index_get(board_pos);
                b32 light = (board_pos_pixels.x / SQUARE_SIDE_LENGTH) % 2
                        == (board_pos_pixels.y / SQUARE_SIDE_LENGTH) % 2;
                if (highlighted & (1llu << index)) {
                    color = light ? light_squares_selected : dark_squares_selected;
                } else {
                    color = light ? light_squares : dark_squares;
                }

                JkIntVector2 square_pos =
                        jk_int_vector_2_remainder(SQUARE_SIDE_LENGTH, board_pos_pixels);
                Piece piece = board_piece_get(chess->board, board_pos);
                if (piece.type != NONE) {
                    int32_t tilemap_y_offset = (piece.type - 1) * 112;
                    int32_t pixel_index =
                            SQUARE_SIDE_LENGTH * (square_pos.y + tilemap_y_offset) + square_pos.x;
                    int32_t byte_index = pixel_index / 8;
                    uint8_t bit_index = pixel_index % 8;
                    if ((chess->tilemap[byte_index] >> bit_index) & 1) {
                        color = piece.team ? black : white;
                    }
                }
            } else {
                color = background;
            }
            chess->bitmap.memory[pos.y * chess->bitmap.width + pos.x] = color;
        }
    }
}
