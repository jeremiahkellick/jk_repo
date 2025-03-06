#include "chess.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// #jk_build compiler_arguments /LD
// #jk_build linker_arguments /OUT:chess.dll /EXPORT:update /EXPORT:render
// #jk_build single_translation_unit

// #jk_build dependencies_begin
#include <jk_src/jk_lib/jk_lib.h>
// #jk_build dependencies_end

_Static_assert(sizeof(Color) == 4, "Color must be 4 bytes");

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

static Team board_current_team_get(Board board)
{
    return (board.flags >> BOARD_FLAG_INDEX_CURRENT_PLAYER) & 1;
}

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

static JkIntVector2 board_index_to_vector_2(uint8_t index)
{
    return (JkIntVector2){.x = index % 8, .y = index / 8};
}

static Piece board_piece_get_index(Board board, uint8_t index)
{
    uint8_t byte_index = index / 2; // A piece is 4 bits, so there are 2 pieces per byte
    uint8_t is_higher_4_bits = index % 2;
    uint8_t byte = board.bytes[byte_index];
    if (is_higher_4_bits) {
        byte >>= 4;
    }
    return (Piece){.type = byte & 0x7, .team = (byte >> 3) & 1};
}

static Piece board_piece_get(Board board, JkIntVector2 pos)
{
    return board_piece_get_index(board, board_index_get(pos));
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

static int32_t absolute_value(int32_t x)
{
    return x < 0 ? -x : x;
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

static PieceType promo_order[] = {
    QUEEN,
    KNIGHT,
    ROOK,
    BISHOP,
};

static b32 square_available(Board board, JkIntVector2 square)
{
    if (board_in_bounds(square)) {
        Piece piece = board_piece_get(board, square);
        return piece.type == NONE || piece.team != board_current_team_get(board);
    } else {
        return 0;
    }
}

static MovePacked move_pack(Move move)
{
    return (MovePacked){.bits = (uint16_t)move.piece.type | ((uint16_t)(move.piece.team & 1) << 3)
                | ((uint16_t)(move.src & 0x3f) << 4) | ((uint16_t)(move.dest & 0x3f) << 10)};
}

static Move move_unpack(MovePacked move_packed)
{
    uint16_t bits = move_packed.bits;
    return (Move){
        .piece =
                {
                    .type = bits & 0x7,
                    .team = (bits >> 3) & 1,
                },
        .src = (bits >> 4) & 0x3f,
        .dest = (bits >> 10) & 0x3f,
    };
}

static void moves_append(MoveArray *moves, JkIntVector2 src, JkIntVector2 dest, Piece piece)
{
    moves->data[moves->count++] = move_pack(
            (Move){.src = board_index_get(src), .dest = board_index_get(dest), .piece = piece});
}

static void moves_append_in_direction_until_stopped(
        MoveArray *moves, Board board, JkIntVector2 src, JkIntVector2 direction, Piece piece)
{
    JkIntVector2 dest = src;
    for (;;) {
        dest = jk_int_vector_2_add(dest, direction);
        if (board_in_bounds(dest)) {
            Piece dest_piece = board_piece_get(board, dest);
            if (dest_piece.type == NONE) {
                moves_append(moves, src, dest, piece);
            } else {
                if (dest_piece.team != board_current_team_get(board)) {
                    moves_append(moves, src, dest, piece);
                }
                return;
            }
        } else {
            return;
        }
    }
}

static uint64_t board_flag_king_moved_get(Team team)
{
    return 1llu << (1 + team);
}

static uint64_t board_flag_rook_moved_get(Team team, uint8_t is_right)
{
    return 1llu << (3 + ((team << 1) | is_right));
}

static uint64_t threats_in_direction_until_stopped(
        Board board, JkIntVector2 src, JkIntVector2 direction)
{
    uint64_t result = 0;
    JkIntVector2 dest = src;
    for (;;) {
        dest = jk_int_vector_2_add(dest, direction);
        if (board_in_bounds(dest)) {
            uint8_t index = board_index_get(dest);
            result |= 1llu << index;
            if (board_piece_get_index(board, index).type != NONE) {
                return result;
            }
        } else {
            return result;
        }
    }
}

static uint64_t board_threatened_squares_get(Board board, Team threatened_by)
{
    uint64_t result = 0;

    JkIntVector2 src;
    for (src.y = 0; src.y < 8; src.y++) {
        for (src.x = 0; src.x < 8; src.x++) {
            Piece piece = board_piece_get(board, src);
            if (piece.team == threatened_by) {
                switch (piece.type) {
                case NONE:
                case PIECE_TYPE_COUNT: {
                } break;

                case KING: {
                    for (uint64_t i = 0; i < JK_ARRAY_COUNT(all_directions); i++) {
                        JkIntVector2 dest = jk_int_vector_2_add(src, all_directions[i]);
                        if (board_in_bounds(dest)) {
                            result |= 1llu << board_index_get(dest);
                        }
                    }
                } break;

                case QUEEN: {
                    for (uint64_t i = 0; i < JK_ARRAY_COUNT(all_directions); i++) {
                        result |= threats_in_direction_until_stopped(board, src, all_directions[i]);
                    }
                } break;

                case ROOK: {
                    for (uint64_t i = 0; i < JK_ARRAY_COUNT(straights); i++) {
                        result |= threats_in_direction_until_stopped(board, src, straights[i]);
                    }
                } break;

                case BISHOP: {
                    for (uint64_t i = 0; i < JK_ARRAY_COUNT(diagonals); i++) {
                        result |= threats_in_direction_until_stopped(board, src, diagonals[i]);
                    }
                } break;

                case KNIGHT: {
                    for (uint64_t i = 0; i < JK_ARRAY_COUNT(knight_moves); i++) {
                        JkIntVector2 dest = jk_int_vector_2_add(src, knight_moves[i]);
                        if (board_in_bounds(dest)) {
                            result |= 1llu << board_index_get(dest);
                        }
                    }
                } break;

                case PAWN: {
                    // Attacks
                    for (uint8_t i = 0; i < 2; i++) {
                        JkIntVector2 dest =
                                jk_int_vector_2_add(src, pawn_attacks[threatened_by][i]);
                        if (board_in_bounds(dest)) {
                            result |= 1llu << board_index_get(dest);
                        }
                    }
                } break;
                }
            }
        }
    }

    return result;
}

static Board board_move_perform(Board board, MovePacked move_packed)
{
    Move move = move_unpack(move_packed);
    JkIntVector2 src = board_index_to_vector_2(move.src);
    JkIntVector2 dest = board_index_to_vector_2(move.dest);
    Team team = (board.flags >> BOARD_FLAG_INDEX_CURRENT_PLAYER) & 1;

    // En-passant handling
    if (move.piece.type == PAWN && src.x != dest.x
            && board_piece_get_index(board, move.dest).type == NONE) {
        int32_t y_delta = (board.flags & BOARD_FLAG_CURRENT_PLAYER) ? 1 : -1;
        board_piece_set(&board, jk_int_vector_2_add(dest, (JkIntVector2){0, y_delta}), (Piece){0});
    }

    // Castling handling
    int32_t delta_x = dest.x - src.x;
    if (move.piece.type == ROOK && src.y == (team ? 7 : 0) && (src.x == 0 || src.x == 7)) {
        board.flags |= board_flag_rook_moved_get(team, src.x == 7);
    }
    if (move.piece.type == KING) {
        board.flags |= board_flag_king_moved_get(team);

        if (absolute_value(delta_x) == 2) {
            int32_t rook_from_x, rook_to_x;
            if (delta_x > 0) {
                rook_from_x = 7;
                rook_to_x = 5;
            } else {
                rook_from_x = 0;
                rook_to_x = 3;
            }
            board_piece_set(&board, (JkIntVector2){rook_from_x, src.y}, (Piece){0});
            board_piece_set(&board,
                    (JkIntVector2){rook_to_x, src.y},
                    (Piece){.type = ROOK, .team = move.piece.team});
        }
    }

    board_piece_set(&board, src, (Piece){0});
    board_piece_set(&board, dest, move.piece);

    board.move_prev = move_packed;
    board.flags ^= BOARD_FLAG_CURRENT_PLAYER;

    return board;
}

static void moves_append_with_promo_potential(
        MoveArray *moves, JkIntVector2 src, JkIntVector2 dest, Piece piece)
{
    if (dest.y == 0 || dest.y == 7) { // Promotion
        Piece promo_piece = {.type = QUEEN, .team = piece.team};
        for (; promo_piece.type <= KNIGHT; promo_piece.type++) {
            moves_append(moves, src, dest, promo_piece);
        }
    } else { // Regular move
        moves_append(moves, src, dest, piece);
    }
}

// Returns the number of moves written to the buffer
static void moves_get(MoveArray *moves, Board board)
{
    moves->count = 0;

    Team current_team = board_current_team_get(board);
    JkIntVector2 src;
    for (src.y = 0; src.y < 8; src.y++) {
        for (src.x = 0; src.x < 8; src.x++) {
            Piece piece = board_piece_get(board, src);
            if (piece.team == current_team) {
                switch (piece.type) {
                case NONE:
                case PIECE_TYPE_COUNT: {
                } break;

                case KING: {
                    for (uint64_t i = 0; i < JK_ARRAY_COUNT(all_directions); i++) {
                        JkIntVector2 dest = jk_int_vector_2_add(src, all_directions[i]);
                        if (square_available(board, dest)) {
                            moves_append(moves, src, dest, piece);
                        }
                    }

                    if (!(board.flags & board_flag_king_moved_get(current_team))) {
                        uint64_t threatened_squares =
                                board_threatened_squares_get(board, !current_team);
                        for (uint8_t right = 0; right < 2; right++) {
                            if (!(board.flags & board_flag_rook_moved_get(current_team, right))) {
                                // Find out whether any of the castling spaces are threatened
                                uint8_t step = right ? 1 : UINT8_MAX;
                                uint8_t index = board_index_get(src);
                                uint64_t threat_check_mask = (1llu << index)
                                        | (1llu << (index + step))
                                        | (1llu << (index + step + step));
                                uint64_t threatened = threatened_squares & threat_check_mask;

                                // Find out whether any of the castling spaces are blocked
                                b32 blocked = 0;
                                int32_t x_step = right ? 1 : -1;
                                uint8_t empty_space_count = right ? 2 : 3;
                                JkIntVector2 pos = src;
                                for (uint8_t i = 0; i < empty_space_count; i++) {
                                    pos.x += x_step;
                                    if (board_piece_get(board, pos).type != NONE) {
                                        blocked = 1;
                                    }
                                }

                                if (!threatened && !blocked) {
                                    moves_append(moves,
                                            src,
                                            jk_int_vector_2_add(src, (JkIntVector2){2 * x_step, 0}),
                                            piece);
                                }
                            }
                        }
                    }
                } break;

                case QUEEN: {
                    for (uint64_t i = 0; i < JK_ARRAY_COUNT(all_directions); i++) {
                        moves_append_in_direction_until_stopped(
                                moves, board, src, all_directions[i], piece);
                    }
                } break;

                case ROOK: {
                    for (uint64_t i = 0; i < JK_ARRAY_COUNT(straights); i++) {
                        moves_append_in_direction_until_stopped(
                                moves, board, src, straights[i], piece);
                    }
                } break;

                case BISHOP: {
                    for (uint64_t i = 0; i < JK_ARRAY_COUNT(diagonals); i++) {
                        moves_append_in_direction_until_stopped(
                                moves, board, src, diagonals[i], piece);
                    }
                } break;

                case KNIGHT: {
                    for (uint64_t i = 0; i < JK_ARRAY_COUNT(knight_moves); i++) {
                        JkIntVector2 dest = jk_int_vector_2_add(src, knight_moves[i]);
                        if (square_available(board, dest)) {
                            moves_append(moves, src, dest, piece);
                        }
                    }
                } break;

                case PAWN: {
                    // Move
                    {
                        JkIntVector2 dest = jk_int_vector_2_add(src, pawn_moves[current_team]);
                        if (board_in_bounds(dest) && board_piece_get(board, dest).type == NONE) {
                            moves_append_with_promo_potential(moves, src, dest, piece);

                            // Extended move
                            if ((current_team == WHITE && src.y == 1)
                                    || (current_team == BLACK && src.y == 6)) {
                                JkIntVector2 extended_move =
                                        jk_int_vector_2_add(src, pawn_extended_moves[current_team]);
                                if (board_in_bounds(extended_move)
                                        && board_piece_get(board, extended_move).type == NONE) {
                                    moves_append(moves, src, extended_move, piece);
                                }
                            }
                        }
                    }

                    // Attacks
                    for (uint8_t i = 0; i < 2; i++) {
                        JkIntVector2 dest = jk_int_vector_2_add(src, pawn_attacks[current_team][i]);
                        if (board_in_bounds(dest)) {
                            Piece piece_at_dest = board_piece_get(board, dest);
                            if (piece_at_dest.type != NONE && piece_at_dest.team != current_team) {
                                moves_append_with_promo_potential(moves, src, dest, piece);
                            }
                        }
                    }
                } break;
                }
            }
        }
    }

    // En-passant
    Move move_prev = move_unpack(board.move_prev);
    if (absolute_value((int32_t)move_prev.src - (int32_t)move_prev.dest) == 16) {
        Piece move_prev_piece = board_piece_get_index(board, move_prev.dest);
        if (move_prev_piece.type == PAWN) {
            JkIntVector2 pos = board_index_to_vector_2(move_prev.dest);
            JkIntVector2 directions[] = {{-1, 0}, {1, 0}};
            for (uint8_t i = 0; i < JK_ARRAY_COUNT(directions); i++) {
                JkIntVector2 en_passant_src = jk_int_vector_2_add(pos, directions[i]);
                if (board_in_bounds(en_passant_src)) {
                    Piece piece = board_piece_get(board, en_passant_src);
                    if (piece.type == PAWN && piece.team == current_team) {
                        int32_t y_delta = move_prev.src < move_prev.dest ? -1 : 1;
                        JkIntVector2 en_passant_dest =
                                jk_int_vector_2_add(pos, (JkIntVector2){0, y_delta});
                        moves_append(moves, en_passant_src, en_passant_dest, piece);
                    }
                }
            }
        }
    }

    // Remove moves that leave king in check
    uint8_t move_index = 0;
    while (move_index < moves->count) {
        Board hypothetical = board_move_perform(board, moves->data[move_index]);

        uint8_t king_index = 0;
        for (uint8_t square_index = 0; square_index < 64; square_index++) {
            Piece piece = board_piece_get_index(hypothetical, square_index);
            if (piece.type == KING && piece.team == current_team) {
                king_index = square_index;
            }
        }

        uint64_t threatened = board_threatened_squares_get(hypothetical, !current_team);
        if (threatened & (1llu << king_index)) {
            moves->data[move_index] = moves->data[--moves->count];
        } else {
            move_index++;
        }
    }
}

#define MAX_DEPTH 3
static int32_t team_multiplier[2] = {1, -1};
static int32_t piece_value[PIECE_TYPE_COUNT] = {0, 0, 9, 5, 3, 3, 1};

static int32_t board_score(Board board)
{
    Team current_team = board_current_team_get(board);
    int32_t score = 0;
    uint8_t king_index = UINT8_MAX;
    for (uint8_t i = 0; i < 64; i++) {
        Piece piece = board_piece_get_index(board, i);
        score += team_multiplier[piece.team] * piece_value[piece.type];
        if (piece.type == KING && piece.team == current_team) {
            king_index = i;
        }
    }

    JK_ASSERT(king_index < 64);
    MoveArray moves = {0};
    moves_get(&moves, board);

    if (!moves.count) {
        uint64_t threatened = board_threatened_squares_get(board, !current_team);
        if ((threatened >> king_index) & 1) {
            score -= team_multiplier[current_team] * 1000;
        }
    }

    return score;
}

int32_t ai_score_get(Board board, int32_t depth)
{
    if (!(depth < MAX_DEPTH)) {
        return board_score(board);
    }

    MoveArray moves;
    moves_get(&moves, board);

    if (moves.count) {
        int32_t max_score = INT32_MIN;
        MovePacked best_move;
        for (int32_t i = 0; i < moves.count; i++) {
            int32_t score = team_multiplier[board_current_team_get(board)]
                    * ai_score_get(board_move_perform(board, moves.data[i]), depth + 1);
            if (max_score < score) {
                max_score = score;
                best_move = moves.data[i];
            }
        }

        return team_multiplier[board_current_team_get(board)] * max_score;
    } else {
        return board_score(board);
    }
}

MovePacked ai_move_get(Board board)
{
    MoveArray moves;
    moves_get(&moves, board);
    JK_ASSERT(moves.count);

    int32_t max_score = INT32_MIN;
    MovePacked best_move = {0};
    for (int32_t i = 0; i < moves.count; i++) {
        int32_t score = team_multiplier[board_current_team_get(board)]
                * ai_score_get(board_move_perform(board, moves.data[i]), 0);
        if (max_score < score) {
            max_score = score;
            best_move = moves.data[i];
        }
    }

    return best_move;
}

static void audio_write(Audio *audio)
{
    for (uint32_t sample_index = 0; sample_index < audio->sample_count; sample_index++) {
        for (int32_t channel_index = 0; channel_index < AUDIO_CHANNEL_COUNT; channel_index++) {
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

static b32 screen_in_bounds(Bitmap *bitmap, JkIntVector2 screen_pos)
{
    return screen_pos.x >= 0 && screen_pos.x < bitmap->width && screen_pos.y >= 0
            && screen_pos.y < bitmap->height;
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
    for (uint8_t i = 0; i < chess->moves.count; i++) {
        Move move = move_unpack(chess->moves.data[i]);
        if (move.src == src) {
            result |= 1llu << move.dest;
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
        chess->flags = FLAG_INITIALIZED;
        chess->selected_square = (JkIntVector2){-1, -1};
        chess->promo_square = (JkIntVector2){-1, -1};
        chess->result = 0;
        memcpy(&chess->board, &starting_state, sizeof(chess->board));
        moves_get(&chess->moves, chess->board);

        JK_DEBUG_ASSERT(board_flag_king_moved_get(WHITE) == BOARD_FLAG_WHITE_KING_MOVED);
        JK_DEBUG_ASSERT(board_flag_king_moved_get(BLACK) == BOARD_FLAG_BLACK_KING_MOVED);
        JK_DEBUG_ASSERT(board_flag_rook_moved_get(WHITE, 0) == BOARD_FLAG_A1_ROOK_MOVED);
        JK_DEBUG_ASSERT(board_flag_rook_moved_get(WHITE, 1) == BOARD_FLAG_H1_ROOK_MOVED);
        JK_DEBUG_ASSERT(board_flag_rook_moved_get(BLACK, 0) == BOARD_FLAG_A8_ROOK_MOVED);
        JK_DEBUG_ASSERT(board_flag_rook_moved_get(BLACK, 1) == BOARD_FLAG_H8_ROOK_MOVED);
    }

    if (button_pressed(chess, INPUT_FLAG_CANCEL)) {
        chess->selected_square = (JkIntVector2){-1, -1};
        chess->promo_square = (JkIntVector2){-1, -1};
    }

    // Start at an invalid value. If move.src remains at an invalid value, we should ignore it.
    // If move.src becomes valid, we should perform the move.
    Move move = {.src = UINT8_MAX};

    JkIntVector2 mouse_pos = screen_to_board_pos(&chess->bitmap, chess->input.mouse_pos);

    if (board_in_bounds(chess->promo_square)) {
        int32_t dist_from_promo_square = absolute_value(mouse_pos.y - chess->promo_square.y);
        if (button_pressed(chess, INPUT_FLAG_CONFIRM)) {
            if (mouse_pos.x == chess->promo_square.x
                    && dist_from_promo_square < JK_ARRAY_COUNT(promo_order)) {
                move.src = board_index_get(chess->selected_square);
                move.dest = board_index_get(chess->promo_square);
                move.piece.team = board_current_team_get(chess->board);
                move.piece.type = promo_order[dist_from_promo_square];
            } else {
                chess->selected_square = (JkIntVector2){-1, -1};
                chess->promo_square = (JkIntVector2){-1, -1};
            }
        }
    } else {
        uint64_t available_destinations =
                destinations_get_by_src(chess, board_index_get_unbounded(chess->selected_square));
        uint8_t mouse_index = board_index_get_unbounded(mouse_pos);
        b32 mouse_on_destination =
                mouse_index < 64 && (available_destinations & (1llu << mouse_index));
        uint8_t piece_drop_index = UINT8_MAX;

        if (button_pressed(chess, INPUT_FLAG_CONFIRM)) {
            if (mouse_on_destination) {
                piece_drop_index = mouse_index;
            } else {
                chess->selected_square = (JkIntVector2){-1, -1};
                for (uint8_t i = 0; i < chess->moves.count; i++) {
                    Move available_move = move_unpack(chess->moves.data[i]);
                    if (available_move.src == mouse_index) {
                        chess->flags |= FLAG_HOLDING_PIECE;
                        chess->selected_square = mouse_pos;
                    }
                }
            }
        }

        if (!(chess->input.flags & INPUT_FLAG_CONFIRM) && (chess->flags & FLAG_HOLDING_PIECE)) {
            chess->flags &= ~FLAG_HOLDING_PIECE;

            if (mouse_on_destination) {
                piece_drop_index = mouse_index;
            }
        }

        if (piece_drop_index < 64) {
            move.src = board_index_get(chess->selected_square);
            move.dest = (uint8_t)piece_drop_index;
            move.piece = board_piece_get_index(chess->board, move.src);
        }
    }

    if (move.src < 64) {
        JkIntVector2 dest = board_index_to_vector_2(move.dest);
        if (move.piece.type == PAWN && (dest.y == 0 || dest.y == 7)) { // Enter pawn promotion
            chess->promo_square = dest;
        } else { // Make a move
            chess->board = board_move_perform(chess->board, move_pack(move));
            // chess->board = board_move_perform(chess->board, ai_move_get(chess->board));

            chess->selected_square = (JkIntVector2){-1, -1};
            chess->promo_square = (JkIntVector2){-1, -1};
            Team current_team = board_current_team_get(chess->board);
            moves_get(&chess->moves, chess->board);

            if (!chess->moves.count) {
                chess->victor = !current_team;

                uint64_t threatened = board_threatened_squares_get(chess->board, chess->victor);
                uint8_t king_index = 0;
                for (uint8_t square_index = 0; square_index < 64; square_index++) {
                    Piece piece = board_piece_get_index(chess->board, square_index);
                    if (piece.type == KING && piece.team == current_team) {
                        king_index = square_index;
                        break;
                    }
                }

                chess->result =
                        (threatened >> king_index) & 1 ? RESULT_CHECKMATE : RESULT_STALEMATE;
            }
        }
    }

    audio_write(&chess->audio);

    chess->time++;

    chess->input_prev = chess->input;
}

Color color_background = {0x27, 0x20, 0x16};

// Color light_squares = {0xde, 0xe2, 0xde};
// Color dark_squares = {0x39, 0x41, 0x3a};

Color color_light_squares = {0xe9, 0xe2, 0xd7};
Color color_dark_squares = {0x50, 0x41, 0x2b};

// Blended halfway between the base square colors and #E26D5C

Color color_selection = {0x5c, 0x6d, 0xe2};

// Color white = {0x8e, 0x8e, 0x8e};
Color color_white_pieces = {0x82, 0x92, 0x85};
Color color_black_pieces = {0xff, 0x73, 0xa2};

static Color blend(Color a, Color b)
{
    return (Color){.r = a.r / 2 + b.r / 2, .g = a.g / 2 + b.g / 2, .b = a.b / 2 + b.b / 2};
}

static Color blend_alpha(Color foreground, Color background, uint8_t alpha)
{
    Color result = {0, 0, 0, 255};
    for (uint8_t i = 0; i < 3; i++) {
        result.v[i] = ((int32_t)foreground.v[i] * (int32_t)alpha
                              + background.v[i] * (255 - (int32_t)alpha))
                / 255;
    }
    return result;
}

static uint8_t atlas_piece_get_alpha(uint8_t *atlas, PieceType piece_type, JkIntVector2 pos)
{
    JK_DEBUG_ASSERT(
            pos.x >= 0 && pos.x < SQUARE_SIDE_LENGTH && pos.y >= 0 && pos.y < SQUARE_SIDE_LENGTH);

    int32_t y_offset = (piece_type - 1) * 112;
    return atlas[(pos.y + y_offset) * ATLAS_WIDTH + pos.x];
}

static void scale_alpha_map(uint8_t *dest,
        int32_t dest_width,
        int32_t dest_height,
        uint8_t *src,
        int32_t src_width,
        int32_t src_height)
{
    float scale_factor = (float)src_width / (float)dest_width;
    float scale_factor_squared = scale_factor * scale_factor;
    for (int32_t dy = 0; dy < dest_height; dy++) {
        for (int32_t dx = 0; dx < dest_width; dx++) {
            float y1 = (float)dy * scale_factor;
            float x1 = (float)dx * scale_factor;
            float y2 = y1 + scale_factor;
            float x2 = x1 + scale_factor;
            float min_y = floorf(y1);
            float min_x = floorf(x1);
            float max_y = ceilf(y1 + scale_factor);
            float max_x = ceilf(x1 + scale_factor);
            float alpha = 0.0;
            for (int32_t sy = (int32_t)min_y; sy < (int32_t)max_y; sy++) {
                float height;
                if (sy == (int32_t)min_y) {
                    height = 1.0f - (y1 - min_y);
                } else if (sy == (int32_t)max_y - 1) {
                    height = 1.0f - (max_y - y2);
                } else {
                    height = 1.0f;
                }
                for (int32_t sx = (int32_t)min_x; sx < (int32_t)max_x; sx++) {
                    float width;
                    if (sx == min_x) {
                        width = 1.0f - (x1 - min_x);
                    } else if (sx == (int32_t)max_x - 1) {
                        width = 1.0f - (max_x - x2);
                    } else {
                        width = 1.0f;
                    }

                    float multiplier = (width * height) / scale_factor_squared;
                    alpha += multiplier * (float)src[sy * src_width + sx];
                }
            }
            dest[dy * dest_width + dx] = (uint8_t)alpha;
        }
    }
}

static int32_t minimum(int32_t a, int32_t b)
{
    return a < b ? a : b;
}

RENDER_FUNCTION(render)
{
    static char string_buf[1024];

    int32_t scaled_atlas_height = minimum(chess->bitmap.width, chess->bitmap.height);
    int32_t scaled_atlas_width = (scaled_atlas_height * 5) / 6;

    uint64_t time_before_scale = chess->cpu_timer_get();
    scale_alpha_map(chess->scaled_atlas,
            scaled_atlas_width,
            scaled_atlas_height,
            chess->atlas,
            ATLAS_WIDTH,
            ATLAS_HEIGHT);
    snprintf(string_buf,
            JK_ARRAY_COUNT(string_buf),
            "%.0fms\n",
            (double)(chess->cpu_timer_get() - time_before_scale) * 1000.0
                    / (double)chess->cpu_timer_frequency);
    chess->debug_print(string_buf);

    JkIntVector2 pos;
    for (pos.y = 0; pos.y < chess->bitmap.height; pos.y++) {
        for (pos.x = 0; pos.x < chess->bitmap.width; pos.x++) {
            Color color;
            if (pos.x < scaled_atlas_width && pos.y < scaled_atlas_height) {
                uint8_t alpha = chess->scaled_atlas[pos.y * scaled_atlas_width + pos.x];
                color = (Color){alpha, alpha, alpha, alpha};
            } else {
                color = (Color){0};
            }
            chess->bitmap.memory[pos.y * chess->bitmap.width + pos.x] = color;
        }
    }

    /*
    // Figure out which squares should be highlighted
    uint8_t selected_index = board_index_get_unbounded(chess->selected_square);
    uint64_t destinations = destinations_get_by_src(chess, selected_index);
    uint8_t mouse_index =
            board_index_get_unbounded(screen_to_board_pos(&chess->bitmap, chess->input.mouse_pos));
    b32 promoting = board_in_bounds(chess->promo_square);

    // uint64_t threatened = board_threatened_squares_get(
    //         chess->board, !((chess->board.flags >> BOARD_FLAG_INDEX_CURRENT_PLAYER) & 1));

    JkIntVector2 pos;
    for (pos.y = 0; pos.y < chess->bitmap.height; pos.y++) {
        for (pos.x = 0; pos.x < chess->bitmap.width; pos.x++) {
            JkIntVector2 board_pos_pixels = screen_to_board_pos_pixels(&chess->bitmap, pos);
            Color color;
            if (board_pos_pixels_in_bounds(board_pos_pixels)) {
                JkIntVector2 square_pos =
                        jk_int_vector_2_remainder(SQUARE_SIDE_LENGTH, board_pos_pixels);

                JkIntVector2 board_pos = board_pos_pixels_to_squares(board_pos_pixels);
                uint8_t index = board_index_get(board_pos);
                b32 light = (board_pos_pixels.x / SQUARE_SIDE_LENGTH) % 2
                        == (board_pos_pixels.y / SQUARE_SIDE_LENGTH) % 2;
                Color square_color = light ? color_light_squares : color_dark_squares;
                Piece piece = board_piece_get(chess->board, board_pos);

                color = square_color;
                if (promoting) {
                    int32_t dist_from_promo_square =
                            absolute_value(board_pos.y - chess->promo_square.y);
                    if (board_pos.x == chess->promo_square.x
                            && dist_from_promo_square < JK_ARRAY_COUNT(promo_order)) {
                        color = color_background;
                        piece.team = board_current_team_get(chess->board);
                        piece.type = promo_order[dist_from_promo_square];
                    }
                } else {
                    if (index == selected_index) {
                        color = blend(color_selection, square_color);
                    } else if (destinations & (1llu << index)) {
                        color = blend(color_selection, square_color);
                        if (index == mouse_index) {
                            int32_t x_dist_from_edge = square_pos.x < SQUARE_SIDE_LENGTH / 2
                                    ? square_pos.x
                                    : SQUARE_SIDE_LENGTH - 1 - square_pos.x;
                            int32_t y_dist_from_edge = square_pos.y < SQUARE_SIDE_LENGTH / 2
                                    ? square_pos.y
                                    : SQUARE_SIDE_LENGTH - 1 - square_pos.y;
                            if ((x_dist_from_edge >= 4 && y_dist_from_edge >= 4)
                                    && (x_dist_from_edge < 8 || y_dist_from_edge < 8)) {
                                color = square_color;
                            }
                        }
                    }
                }

                if (piece.type != NONE) {
                    Color color_piece = piece.team ? color_black_pieces : color_white_pieces;
                    uint8_t alpha = atlas_piece_get_alpha(chess->atlas, piece.type, square_pos);
                    if (index == selected_index
                            && ((chess->flags & FLAG_HOLDING_PIECE) || promoting)) {
                        alpha /= 2;
                    }
                    color = blend_alpha(color_piece, color, alpha);
                }
            } else {
                color = color_background;
            }
            chess->bitmap.memory[pos.y * chess->bitmap.width + pos.x] = color;
        }
    }

    if ((chess->flags & FLAG_HOLDING_PIECE) && selected_index < 64) {
        Piece piece = board_piece_get_index(chess->board, selected_index);
        if (piece.type != NONE) {
            JkIntVector2 held_piece_offset = jk_int_vector_2_sub(chess->input.mouse_pos,
                    (JkIntVector2){SQUARE_SIDE_LENGTH / 2, SQUARE_SIDE_LENGTH / 2});
            for (pos.y = 0; pos.y < SQUARE_SIDE_LENGTH; pos.y++) {
                for (pos.x = 0; pos.x < SQUARE_SIDE_LENGTH; pos.x++) {
                    JkIntVector2 screen_pos = jk_int_vector_2_add(pos, held_piece_offset);
                    if (screen_in_bounds(&chess->bitmap, screen_pos)) {
                        int32_t index = screen_pos.y * chess->bitmap.width + screen_pos.x;
                        Color color_piece = piece.team ? color_black_pieces : color_white_pieces;
                        Color color_bg = chess->bitmap.memory[index];
                        uint8_t alpha = atlas_piece_get_alpha(chess->atlas, piece.type, pos);
                        chess->bitmap.memory[index] = blend_alpha(color_piece, color_bg, alpha);
                    }
                }
            }
        }
    }

    if (chess->result) {
        for (pos.y = 0; pos.y < SQUARE_SIDE_LENGTH * 2; pos.y++) {
            for (pos.x = 0; pos.x < SQUARE_SIDE_LENGTH * 4; pos.x++) {
                int32_t result_offset = 0;
                if (chess->result == RESULT_CHECKMATE) {
                    result_offset = (chess->board.flags & BOARD_FLAG_CURRENT_PLAYER ? 1 : 2)
                            * SQUARE_SIDE_LENGTH * 2;
                }
                JkIntVector2 screen_pos = jk_int_vector_2_add(
                        jk_int_vector_2_add(screen_board_origin_get(&chess->bitmap), pos),
                        (JkIntVector2){SQUARE_SIDE_LENGTH * 2, SQUARE_SIDE_LENGTH * 3});
                if (screen_in_bounds(&chess->bitmap, screen_pos)) {
                    uint8_t alpha = chess->atlas[(pos.y + result_offset) * ATLAS_WIDTH + pos.x
                            + SQUARE_SIDE_LENGTH];
                    chess->bitmap.memory[screen_pos.y * chess->bitmap.width + screen_pos.x] =
                            blend_alpha((Color){255, 255, 255}, color_background, alpha);
                }
            }
        }
    }
    */
}
