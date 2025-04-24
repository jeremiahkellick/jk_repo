#include "chess.h"

#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// #jk_build compiler_arguments /LD
// #jk_build linker_arguments /OUT:chess.dll /EXPORT:update /EXPORT:render /EXPORT:ai_move_get
// #jk_build single_translation_unit

// #jk_build dependencies_begin
#include <jk_src/jk_lib/platform/platform.h>
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

static Board starting_state = {
    // clang-format off
    // Byte array encoding the chess starting positions
    .bytes = {
        0x53, 0x24, 0x41, 0x35,
        0x66, 0x66, 0x66, 0x66,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0xEE, 0xEE, 0xEE, 0xEE,
        0xDB, 0xAC, 0xC9, 0xBD,
    },
    // clang-format on
};

static Board puzzle_state = {
    // clang-format off
    // Byte array encoding the chess starting positions
    .bytes = {
        0x00, 0x00, 0x00, 0x30,
        0x66, 0x16, 0x00, 0x00,
        0x00, 0x05, 0x00, 0x00,
        0x00, 0x50, 0x06, 0x00,
        0x00, 0x00, 0x00, 0xE6,
        0x00, 0xE0, 0x00, 0x00,
        0xEE, 0xAE, 0x2C, 0x00,
        0x00, 0x90, 0x00, 0x0B,
    },
    // clang-format on
};

static Board bug_state = {
    // clang-format off
    // Byte array encoding the chess starting positions
    .bytes = {
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0xD0, 0xB0, 0x00,
        0x00, 0x00, 0x00, 0x16,
        0x60, 0x00, 0x00, 0x00,
        0xE0, 0x00, 0xE0, 0x00,
        0x00, 0x00, 0x09, 0xE0,
        0x00, 0x00, 0x00, 0x00,
    },
    // clang-format on
    .flags = 0x1e,
};

static char *puzzle_fen = "8/1bp1rkp1/p4p2/1p1p1QPR/3q3P/8/5PK1/8 b - - 0 1";

static char *wtf_fen = "rnb1kbnr/pppp1ppp/5q2/4p3/3PP3/5N2/PPP2PPP/RNBQKB1R b - - 0 1";

static char *wtf2_fen = "1nb1k1nr/r2p1qp1/1pp3B1/3pP1Qp/p4p2/2N2N2/PPP2PPP/R4RK1 b k - 1 1";

static char *wtf5_fen = "r1b1k1nr/1p4pp/p3p3/5p2/5B2/nP4P1/PbP2P1P/1R1R2K1 b kq - 4 3";

static char *wtf9_fen = "r2k1b2/p3r3/npp2p2/1P1p4/P4Bb1/1BP3P1/3RNP1P/4R1K1 b - - 0 2";

static char debug_print_buffer[4096];

static int debug_printf(void (*debug_print)(char *), char *format, ...)
{
    va_list args;
    va_start(args, format);
    int result = vsnprintf(debug_print_buffer, JK_ARRAY_COUNT(debug_print_buffer), format, args);
    va_end(args);
    debug_print(debug_print_buffer);
    return result;
}

static Chess debug_chess;

static Color debug_draw_buffer[DRAW_BUFFER_WIDTH * DRAW_BUFFER_HEIGHT];

static void debug_render(Board board)
{
    debug_chess.board = board;
    render(&debug_chess);
}

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

static Board parse_fen(JkBuffer fen)
{
    // We'll start with no castling rights. (Flag set mean disallowed, clear means allowed).
    Board board = {.flags = BOARD_FLAG_WHITE_QUEEN_SIDE_CASTLING_RIGHTS
                | BOARD_FLAG_WHITE_KING_SIDE_CASTLING_RIGHTS
                | BOARD_FLAG_BLACK_QUEEN_SIDE_CASTLING_RIGHTS
                | BOARD_FLAG_BLACK_KING_SIDE_CASTLING_RIGHTS};
    uint64_t i = 0;
    JkIntVector2 pos = {0, 7};

    while (isspace(fen.data[i])) {
        i++;
    }

    // Parse piece positions
    for (; i < fen.size && !isspace(fen.data[i]); i++) {
        uint8_t c = fen.data[i];
        Team team = WHITE;
        if (c >= 'a') {
            team = BLACK;
            c -= 'a' - 'A';
        }

        if (c == 'K') {
            board_piece_set(&board, pos, (Piece){KING, team});
            pos.x++;
        } else if (c == 'Q') {
            board_piece_set(&board, pos, (Piece){QUEEN, team});
            pos.x++;
        } else if (c == 'R') {
            board_piece_set(&board, pos, (Piece){ROOK, team});
            pos.x++;
        } else if (c == 'B') {
            board_piece_set(&board, pos, (Piece){BISHOP, team});
            pos.x++;
        } else if (c == 'N') {
            board_piece_set(&board, pos, (Piece){KNIGHT, team});
            pos.x++;
        } else if (c == 'P') {
            board_piece_set(&board, pos, (Piece){PAWN, team});
            pos.x++;
        } else if (c >= '1' && c <= '8') {
            pos.x += c - '0';
        }

        if (!(pos.x < 8)) {
            pos.y--;
            pos.x = pos.x % 8;
        }
    }

    while (isspace(fen.data[i])) {
        i++;
    }

    // Parse current player
    if (tolower(fen.data[i++]) == 'b') {
        board.flags |= BOARD_FLAG_CURRENT_PLAYER;
    }

    while (isspace(fen.data[i])) {
        i++;
    }

    // Parse castling rights. Flag set means disallowed.
    uint8_t c;
    while (!isspace(c = fen.data[i++])) {
        if (c == 'Q') {
            board.flags &= ~BOARD_FLAG_WHITE_QUEEN_SIDE_CASTLING_RIGHTS;
        } else if (c == 'K') {
            board.flags &= ~BOARD_FLAG_WHITE_KING_SIDE_CASTLING_RIGHTS;
        } else if (c == 'q') {
            board.flags &= ~BOARD_FLAG_BLACK_QUEEN_SIDE_CASTLING_RIGHTS;
        } else if (c == 'k') {
            board.flags &= ~BOARD_FLAG_BLACK_KING_SIDE_CASTLING_RIGHTS;
        }
    }

    while (isspace(fen.data[i])) {
        i++;
    }

    uint8_t column_char = fen.data[i++];
    uint8_t row_char = fen.data[i++];
    JkIntVector2 en_passant_pos = {-1, -1};
    if (column_char >= 'a' && column_char <= 'h') {
        en_passant_pos.x = column_char - 'a';
    }
    if (row_char >= '1' && row_char <= '8') {
        en_passant_pos.y = row_char - '1';
    }
    if (board_in_bounds(en_passant_pos)) {
        Move move_prev = {.piece = {.type = PAWN, .team = !board_current_team_get(board)}};
        JkIntVector2 forward = {0, move_prev.piece.team == WHITE ? 1 : -1};
        move_prev.src = board_index_get(jk_int_vector_2_sub(en_passant_pos, forward));
        move_prev.dest = board_index_get(jk_int_vector_2_add(en_passant_pos, forward));
        board.move_prev = move_pack(move_prev);
    }

    return board;
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

static uint8_t board_castling_rights_get(Board board, Team team)
{
    return (board.flags >> (1 + team * 2)) & 0x3;
}

static uint64_t board_castling_rights_flag_get(Team team, b32 king_side)
{
    return 1llu << (1 + team * 2 + !!king_side);
}

typedef struct Threats {
    uint64_t bitfield;
    int32_t count;
} Threats;

static void add_threat(Threats *threats, uint64_t index)
{
    threats->bitfield |= 1llu << index;
    threats->count++;
}

static void threats_in_direction_until_stopped(
        Board board, Threats *threats, JkIntVector2 src, JkIntVector2 direction)
{
    JkIntVector2 dest = src;
    for (;;) {
        dest = jk_int_vector_2_add(dest, direction);
        if (board_in_bounds(dest)) {
            uint8_t index = board_index_get(dest);
            add_threat(threats, index);
            if (board_piece_get_index(board, index).type != NONE) {
                return;
            }
        } else {
            return;
        }
    }
}

static Threats board_threats_get(Board board, Team threatened_by)
{
    JK_PLATFORM_PROFILE_ZONE_TIME_BEGIN(threats_get);

    Threats result = {0};

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
                            add_threat(&result, board_index_get(dest));
                        }
                    }
                } break;

                case QUEEN: {
                    for (uint64_t i = 0; i < JK_ARRAY_COUNT(all_directions); i++) {
                        threats_in_direction_until_stopped(board, &result, src, all_directions[i]);
                    }
                } break;

                case ROOK: {
                    for (uint64_t i = 0; i < JK_ARRAY_COUNT(straights); i++) {
                        threats_in_direction_until_stopped(board, &result, src, straights[i]);
                    }
                } break;

                case BISHOP: {
                    for (uint64_t i = 0; i < JK_ARRAY_COUNT(diagonals); i++) {
                        threats_in_direction_until_stopped(board, &result, src, diagonals[i]);
                    }
                } break;

                case KNIGHT: {
                    for (uint64_t i = 0; i < JK_ARRAY_COUNT(knight_moves); i++) {
                        JkIntVector2 dest = jk_int_vector_2_add(src, knight_moves[i]);
                        if (board_in_bounds(dest)) {
                            add_threat(&result, board_index_get(dest));
                        }
                    }
                } break;

                case PAWN: {
                    // Attacks
                    for (uint8_t i = 0; i < 2; i++) {
                        JkIntVector2 dest =
                                jk_int_vector_2_add(src, pawn_attacks[threatened_by][i]);
                        if (board_in_bounds(dest)) {
                            add_threat(&result, board_index_get(dest));
                        }
                    }
                } break;
                }
            }
        }
    }

    JK_PLATFORM_PROFILE_ZONE_END(threats_get);
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
        board.flags |= board_castling_rights_flag_get(team, src.x == 7);
    }
    if (move.piece.type == KING) {
        board.flags |= board_castling_rights_flag_get(team, 0);
        board.flags |= board_castling_rights_flag_get(team, 1);

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
    JK_PLATFORM_PROFILE_ZONE_TIME_BEGIN(moves_get);

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

                    uint8_t castling_rights = board_castling_rights_get(board, current_team);
                    if (castling_rights != 0x3) {
                        uint64_t threatened_squares =
                                board_threats_get(board, !current_team).bitfield;
                        for (b32 king_side = 0; king_side < 2; king_side++) {
                            if (!((castling_rights >> king_side) & 1)) {
                                // Find out whether any of the castling spaces are threatened
                                uint8_t step = king_side ? 1 : UINT8_MAX;
                                uint8_t index = board_index_get(src);
                                uint64_t threat_check_mask = (1llu << index)
                                        | (1llu << (index + step))
                                        | (1llu << (index + step + step));
                                uint64_t threatened = threatened_squares & threat_check_mask;

                                // Find out whether any of the castling spaces are blocked
                                b32 blocked = 0;
                                int32_t x_step = king_side ? 1 : -1;
                                uint8_t empty_space_count = king_side ? 2 : 3;
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

        uint64_t threatened = board_threats_get(hypothetical, !current_team).bitfield;
        if (threatened & (1llu << king_index)) {
            moves->data[move_index] = moves->data[--moves->count];
        } else {
            move_index++;
        }
    }

    JK_PLATFORM_PROFILE_ZONE_END(moves_get);
}

// ---- AI begin ---------------------------------------------------------------

typedef struct MoveNode {
    b32 expanded;
    MovePacked move;
    struct MoveNode *parent;
    struct MoveNode *next_sibling;
    struct MoveNode *first_child;

    int32_t board_score;
    int32_t score;
    uint32_t search_score;
    uint8_t top_level_index;

    uint64_t depth;
} MoveNode;

// -------- Arena begin ---------------------------------------------------------

typedef struct Arena {
    JkBuffer memory;
    uint64_t pos;
} Arena;

void *arena_alloc(Arena *arena, uint64_t byte_count)
{
    uint64_t new_pos = arena->pos + byte_count;
    if (new_pos <= arena->memory.size) {
        void *result = arena->memory.data + arena->pos;
        arena->pos = new_pos;
        return result;
    } else {
        return 0;
    }
}

void *arena_pointer_get(Arena *arena)
{
    return arena->memory.data + arena->pos;
}

void arena_pointer_set(Arena *arena, void *pointer)
{
    arena->pos = (uint8_t *)pointer - arena->memory.data;
}

// -------- Arena end -----------------------------------------------------------

static int32_t team_multiplier[2] = {1, -1};
static int32_t piece_value[PIECE_TYPE_COUNT] = {0, 0, 9, 5, 3, 3, 1};

static int32_t board_score(Board board, uint64_t depth)
{
    Team current_team = board_current_team_get(board);
    int32_t score = 0;
    uint8_t king_index = UINT8_MAX;
    for (uint8_t i = 0; i < 64; i++) {
        Piece piece = board_piece_get_index(board, i);
        score += team_multiplier[piece.team] * piece_value[piece.type] * 100;

        if (piece.type == KING && piece.team == current_team) {
            king_index = i;
        }

        if (piece.type == PAWN) { // Add points based on how far pawn is from promotion
            JkIntVector2 pos = board_index_to_vector_2(i);
            if (piece.team == WHITE) {
                score += (pos.y - 1) * 2;
            } else {
                score -= (6 - pos.y) * 2;
            }
        }
    }

    // Add points for the number of threatened squares
    Threats threats[2];
    for (Team team = 0; team < 2; team++) {
        threats[team] = board_threats_get(board, team);
        score += team_multiplier[team] * threats[team].count;
    }

    JK_ASSERT(king_index < 64);
    MoveArray moves = {0};
    moves_get(&moves, board);

    if (!moves.count) {
        if ((threats[!current_team].bitfield >> king_index) & 1) { // Checkmate
            score = team_multiplier[!current_team] * (100000 - (uint32_t)depth);
        } else { // Stalemate
            score = (score > 0 ? -1 : 1) * (50000 - (uint32_t)depth);
        }
    }

    return score;
}

typedef struct Target {
    Team assisting_team;
    int32_t score;
} Target;

typedef struct AiContext {
    Board board;
    Arena arena;
    uint8_t top_level_node_count;
    MoveNode top_level_nodes[256];
    Target targets[256];
    uint64_t time_started;
    uint64_t time_limit;
    uint64_t (*time_current_get)(void);
    void (*debug_print)(char *);
} AiContext;

typedef struct Scores {
    int32_t a[2];
} Scores;

static uint32_t distance_from_target(int32_t score, Target target)
{
    switch (target.assisting_team) {
    case WHITE: { // White wants higher scores
        return score < target.score ? target.score - score : 0;
    } break;

    case BLACK: { // Black wants lower scores
        return target.score < score ? score - target.score : 0;
    } break;

    default: {
        JK_ASSERT(0 && "invalid target.comparison");
        return 0;
    } break;
    }
}

static uint32_t search_score_get(AiContext *ctx, MoveNode *node, Team team)
{
    Target target = ctx->targets[node->top_level_index];
    uint32_t search_score;
    if (node->first_child) {
        if (team == target.assisting_team) {
            search_score = UINT32_MAX;
            for (MoveNode *child = node->first_child; child; child = child->next_sibling) {
                uint32_t child_search_score = (child->search_score * 3) / 2;
                if (child_search_score < search_score) {
                    search_score = child_search_score;
                }
            }
        } else {
            search_score = 0;
            for (MoveNode *child = node->first_child; child; child = child->next_sibling) {
                search_score += (child->search_score * 3) / 2;
            }
        }
    } else {
        search_score = distance_from_target(node->score, target);
        if (node->expanded && search_score) {
            // If the node has no children after it's been expanded, then it marks the end of the
            // game (checkmate or stalemate). Therefore, if the score does not already satisfy the
            // target, we assign maximum search score, because it's impossible for this node to ever
            // satisfy the target.
            search_score = UINT32_MAX;
        }
    }
    return search_score;
}

uint16_t max_line[] = {0x7e7e, 0x4cb1, 0xbfdc, 0xba66, 0xf74c, 0xf752, 0xf7eb};

#define MAX_STAGNATION 9

#pragma optimize("", off)

static void update_search_scores(AiContext *ctx, MoveNode *node, Team team, b32 verify)
{
    Target target = ctx->targets[node->top_level_index];
    uint32_t search_score;
    if (node->first_child) {
        if (team == target.assisting_team) {
            search_score = UINT32_MAX;
            for (MoveNode *child = node->first_child; child; child = child->next_sibling) {
                update_search_scores(ctx, child, !team, verify);
                uint32_t child_search_score = (child->search_score * 3) / 2;
                if (child_search_score < search_score) {
                    search_score = child_search_score;
                }
            }
        } else {
            search_score = 0;
            for (MoveNode *child = node->first_child; child; child = child->next_sibling) {
                update_search_scores(ctx, child, !team, verify);
                search_score += (child->search_score * 3) / 2;
            }
        }
    } else {
        search_score = distance_from_target(node->score, target);
        if (node->expanded && search_score) {
            // If the node has no children after it's been expanded, then it marks the end of the
            // game (checkmate or stalemate). Therefore, if the score does not already satisfy the
            // target, we assign maximum search score, because it's impossible for this node to ever
            // satisfy the target.
            search_score = UINT32_MAX;
        }
    }
    if (verify) {
        JK_ASSERT(search_score == node->search_score);
    } else {
        node->search_score = search_score;
    }
}

#pragma optimize("", on)

typedef struct ScoreIndexPair {
    int32_t index;
    int32_t score;
} ScoreIndexPair;

static int score_index_pair_compare(void *a, void *b)
{
    return ((ScoreIndexPair *)a)->score - ((ScoreIndexPair *)b)->score;
}

static void update_search_scores_root(AiContext *ctx, b32 verify)
{
    Team team = board_current_team_get(ctx->board);

    ScoreIndexPair top_two_pairs[2] = {{.score = INT32_MIN}, {.score = INT32_MIN}};
    for (int32_t i = 0; i < ctx->top_level_node_count; i++) {
        int32_t score = team_multiplier[team] * ctx->top_level_nodes[i].score;
        if (top_two_pairs[0].score < score) {
            top_two_pairs[0].score = score;
            top_two_pairs[0].index = i;
            if (top_two_pairs[1].score < top_two_pairs[0].score) {
                ScoreIndexPair tmp = top_two_pairs[1];
                top_two_pairs[1] = top_two_pairs[0];
                top_two_pairs[0] = tmp;
            }
        }
    }

    int32_t favorite_child_index = top_two_pairs[1].index;
    Target favorite_target = {
        .assisting_team = !team,
        .score = team_multiplier[team] * (top_two_pairs[0].score - 1),
    };
    Target target = {
        .assisting_team = team,
        .score = team_multiplier[team] * (top_two_pairs[1].score + 1),
    };
    for (int32_t i = 0; i < ctx->top_level_node_count; i++) {
        if (i == favorite_child_index) {
            ctx->targets[i] = favorite_target;
        } else {
            ctx->targets[i] = target;
        }
        update_search_scores(ctx, ctx->top_level_nodes + i, !team, verify);
        JK_ASSERT(ctx->top_level_nodes[i].search_score > 0);
    }
}

static void moves_shuffle(MoveArray *moves)
{
    for (int32_t i = 0; i < moves->count; i++) {
        int32_t swap_index = i + rand() % (moves->count - i);
        MovePacked tmp = moves->data[swap_index];
        moves->data[swap_index] = moves->data[i];
        moves->data[i] = tmp;
    }
}

static uint64_t debug_nodes_found;

static b32 expand_node(AiContext *ctx, MoveNode *node)
{
    Board node_board_state = ctx->board;
    {
        MovePacked *prev_moves = arena_pointer_get(&ctx->arena);
        for (MoveNode *ancestor = node; ancestor; ancestor = ancestor->parent) {
            MovePacked *move = arena_alloc(&ctx->arena, sizeof(*move));
            if (!move) {
                return 0;
            }
            *move = ancestor->move;
        }
        int64_t move_count = (MovePacked *)arena_pointer_get(&ctx->arena) - prev_moves;

        for (int64_t i = move_count - 1; i >= 0; i--) {
            node_board_state = board_move_perform(node_board_state, prev_moves[i]);
        }

        arena_pointer_set(&ctx->arena, prev_moves);
    }

    MoveArray moves;
    moves_get(&moves, node_board_state);
    MoveNode *prev_child = 0;
    MoveNode *first_child = 0;
    debug_nodes_found += moves.count;
    for (uint8_t i = 0; i < moves.count; i++) {
        MoveNode *new_child = arena_alloc(&ctx->arena, sizeof(*new_child));
        if (!new_child) {
            return 0;
        }
        new_child->depth = node->depth + 1;
        new_child->expanded = 0;
        new_child->move = moves.data[i];
        new_child->parent = node;
        new_child->next_sibling = 0;
        new_child->first_child = 0;
        new_child->board_score =
                board_score(board_move_perform(node_board_state, moves.data[i]), new_child->depth);
        new_child->score = new_child->board_score;
        new_child->search_score = UINT32_MAX;
        new_child->top_level_index = node->top_level_index;

        if (prev_child) {
            prev_child->next_sibling = new_child;
        } else {
            first_child = new_child;
        }
        prev_child = new_child;
    }
    node->first_child = first_child;
    node->expanded = 1;
    if (!node->first_child) {
        node->search_score = UINT32_MAX;
    }

    // TODO: We probably only need to look at all the children for node. For the ancestors, we
    // can probably compare node's score with their score to see if it would be an improvement.
    Team team = board_current_team_get(node_board_state);
    MoveNode *ancestor;
    b32 top_level_score_changed = 0;
    for (ancestor = node; ancestor; ancestor = ancestor->parent, team = !team) {
        if (ancestor->first_child) {
            int32_t score = INT32_MIN;
            for (MoveNode *child = ancestor->first_child; child; child = child->next_sibling) {
                int32_t child_score = team_multiplier[team] * child->score;
                if (score < child_score) {
                    score = child_score;
                }
            }
            score *= team_multiplier[team];
            if (ancestor->score == score) {
                break;
            } else {
                ancestor->score = score;
                if (!ancestor->parent) {
                    top_level_score_changed = 1;
                }
            }
        }
    }

    if (top_level_score_changed) {
        update_search_scores_root(ctx, 0);
    } else {
        team = board_current_team_get(node_board_state);
        for (MoveNode *child = node->first_child; child; child = child->next_sibling) {
            child->search_score = search_score_get(ctx, child, !team);
        }
        for (ancestor = node; ancestor; ancestor = ancestor->parent, team = !team) {
            uint32_t search_score = search_score_get(ctx, ancestor, team);
            if (search_score == ancestor->search_score) {
                break;
            } else {
                ancestor->search_score = search_score;
            }
        }
    }

    // update_search_scores_root(ctx, 1);

    // TODO: Find search score differences in updating only a subtree vs updating from the root

    // TODO: What happens if the AI runs out of moves to look at? E.g. all paths end in checkmate

    return 1;
}
#pragma optimize("", on)

typedef struct MoveTreeStats {
    uint64_t node_count;
    uint64_t leaf_count;
    uint64_t depth_sum;
    uint64_t min_depth;
    uint64_t max_depth;
    MoveArray min_line;
    MovePacked *max_line;
} MoveTreeStats;

void move_tree_stats_calculate(Arena *arena, MoveTreeStats *stats, MoveNode *node, uint64_t depth)
{
    stats->node_count++;
    if (node->first_child) {
        for (MoveNode *child = node->first_child; child; child = child->next_sibling) {
            move_tree_stats_calculate(arena, stats, child, depth + 1);
        }
    } else if (!node->expanded) {
        stats->leaf_count++;
        stats->depth_sum += depth;
        if (depth < stats->min_depth) {
            JK_ASSERT(depth < 256);
            stats->min_depth = depth;

            stats->min_line.count = 0;
            for (MoveNode *ancestor = node; ancestor; ancestor = ancestor->parent) {
                stats->min_line.data[stats->min_line.count++] = ancestor->move;
            }
        }
        if (stats->max_depth < depth) {
            stats->max_depth = depth;

            if (stats->max_line) {
                arena_pointer_set(arena, stats->max_line);
            }
            stats->max_line = arena_pointer_get(arena);
            for (MoveNode *ancestor = node; ancestor; ancestor = ancestor->parent) {
                MovePacked *move = arena_alloc(arena, sizeof(stats->max_line[0]));
                *move = ancestor->move;
            }
        }
    }
}

static uint64_t debug_time_current_get(void)
{
    return debug_nodes_found;
}

static void custom_profile_print(void *data, char *format, ...)
{
    void (*print)(char *) = (void (*)(char *))data;
    va_list args;
    va_start(args, format);
    vsnprintf(debug_print_buffer, JK_ARRAY_COUNT(debug_print_buffer), format, args);
    va_end(args);
    print(debug_print_buffer);
}

Move ai_move_get(Chess *chess)
{
    jk_platform_profile_begin();

    AiContext ctx = {
        .board = chess->board,
        .arena = {.memory = chess->ai_memory},

        .time_started = chess->cpu_timer_get(),
        .time_limit = chess->cpu_timer_frequency * 10,
        .time_current_get = chess->cpu_timer_get,

        .debug_print = chess->debug_print,
    };

    MoveArray moves;
    moves_get(&moves, chess->board);

    if (!moves.count) {
        JK_ASSERT(0 && "If there are no legal moves, the AI should never have been asked for one");
    } else if (moves.count == 1) {
        // If there's only one legal move, take it
        return move_unpack(moves.data[0]);
    }

    ctx.top_level_node_count = moves.count;
    moves_shuffle(&moves);
    MoveNode *prev_child = 0;
    for (uint8_t i = 0; i < moves.count; i++) {
        MoveNode *new_child = ctx.top_level_nodes + i;

        new_child->depth = 1;
        new_child->expanded = 0;
        new_child->move = moves.data[i];
        new_child->parent = 0;
        new_child->next_sibling = 0;
        new_child->first_child = 0;
        new_child->board_score =
                board_score(board_move_perform(chess->board, moves.data[i]), new_child->depth);
        new_child->score = new_child->board_score;
        new_child->search_score = UINT32_MAX;
        new_child->top_level_index = i;

        if (i) {
            prev_child->next_sibling = new_child;
        }

        prev_child = new_child;
    }

    update_search_scores_root(&ctx, 0);

    debug_nodes_found = 0;

    /* ctx.time_started = 0;
    ctx.time_limit = 298441;
    ctx.time_current_get = debug_time_current_get; */

    // Explore tree
    while (ctx.time_current_get() - ctx.time_started < ctx.time_limit) {
        MoveNode *node = 0;
        { // Find the node with the best search score
            Target target = {0};
            {
                uint32_t min_search_score = UINT32_MAX;
                for (int32_t i = 0; i < ctx.top_level_node_count; i++) {
                    if (ctx.top_level_nodes[i].search_score < min_search_score) {
                        node = ctx.top_level_nodes + i;
                        target = ctx.targets[i];
                        min_search_score = ctx.top_level_nodes[i].search_score;
                    }
                }
            }
            JK_ASSERT(node);

            {
                Team team = !board_current_team_get(ctx.board);
                uint32_t min_search_score = UINT32_MAX;
                while (node->first_child) {
                    min_search_score = UINT32_MAX;
                    MoveNode *min_child = 0;
                    uint32_t max_dist_from_target = 0;
                    MoveNode *max_dist_from_target_child = 0;
                    for (MoveNode *child = node->first_child; child; child = child->next_sibling) {
                        if (child->first_child || !child->expanded) {
                            if (child->search_score <= min_search_score) {
                                min_search_score = child->search_score;
                                min_child = child;
                            }
                            uint32_t dist_from_target = distance_from_target(child->score, target);
                            if (max_dist_from_target <= dist_from_target) {
                                max_dist_from_target = dist_from_target;
                                max_dist_from_target_child = child;
                            }
                        }
                    }
                    JK_ASSERT(min_child && max_dist_from_target_child);
                    node = team == target.assisting_team ? min_child : max_dist_from_target_child;
                    team = !team;
                }
            }
        }

        if (!expand_node(&ctx, node)) {
            chess->debug_print("Out of AI memory\n");
            break;
        }
    }

    int32_t max_score_i = 0;
    MovePacked move = {0};
    {
        // Pick move with the best score
        Team team = board_current_team_get(ctx.board);
        int32_t max_score = INT32_MIN;
        for (int32_t i = 0; i < ctx.top_level_node_count; i++) {
            int32_t score = team_multiplier[team] * ctx.top_level_nodes[i].score;
            if (max_score < score) {
                max_score_i = i;
                max_score = score;
                move = ctx.top_level_nodes[i].move;
            }
        }
    }

    uint64_t time_elapsed = chess->cpu_timer_get() - ctx.time_started;
    double seconds_elapsed = (double)time_elapsed / (double)chess->cpu_timer_frequency;

    // Print min and max depth
    MoveTreeStats stats = {.min_depth = UINT64_MAX};
    for (int32_t i = 0; i < ctx.top_level_node_count; i++) {
        move_tree_stats_calculate(&ctx.arena, &stats, ctx.top_level_nodes + i, 1);
    }

    debug_printf(chess->debug_print, "node_count: %llu\n", stats.node_count);
    debug_printf(chess->debug_print, "seconds_elapsed: %.1f\n", seconds_elapsed);
    debug_printf(chess->debug_print,
            "%.4f Mn/s\n",
            ((double)stats.node_count / 1000000.0) / seconds_elapsed);

    if (stats.max_depth < 300) {
        uint64_t max_score_depth = 0;
        {
            Team team = board_current_team_get(ctx.board);
            Board board = chess->board;
            MoveNode *node = ctx.top_level_nodes + max_score_i;
            while (node) {
                max_score_depth++;
                board = board_move_perform(board, node->move);
                debug_render(board);
                team = !team;
                int32_t max_score = INT32_MIN;
                MoveNode *max_score_node = 0;
                for (MoveNode *child = node->first_child; child; child = child->next_sibling) {
                    int32_t score = team_multiplier[team] * child->score;
                    if (max_score < score) {
                        max_score = score;
                        max_score_node = child;
                    }
                }
                node = max_score_node;
            }
        }

        debug_printf(chess->debug_print,
                "node_count\t%llu\nleaf_count\t%llu\nmin_depth\t%llu\navg_depth\t%llu\nmax_depth\t%"
                "llu\nmax_score_depth\t%llu\n",
                stats.node_count,
                stats.leaf_count,
                stats.min_depth,
                stats.depth_sum / stats.leaf_count,
                stats.max_depth,
                max_score_depth);

        debug_printf(chess->debug_print, "max_line:\n");
        for (int32_t i = 0; i < stats.max_depth; i++) {
            debug_printf(chess->debug_print, "\t0x%x\n", (uint32_t)stats.max_line[i].bits);
        }

        {
            Board board = chess->board;
            for (int32_t i = stats.min_line.count - 1; i >= 0; i--) {
                board = board_move_perform(board, stats.min_line.data[i]);
                debug_render(board);
            }
        }

        {
            Board board = chess->board;
            for (int32_t i = (int32_t)stats.max_depth - 1; i >= 0; i--) {
                board = board_move_perform(board, stats.max_line[i]);
                debug_printf(chess->debug_print,
                        "score: %d\n",
                        board_score(board, stats.max_depth - 1 - i));
                debug_render(board);
            }
        }
    }

    Move result = move_unpack(move);

    jk_platform_profile_end_and_print_custom(custom_profile_print, (void *)chess->debug_print);
    return result;
}

// ---- AI end -----------------------------------------------------------------

static void audio_write(Audio *audio)
{
    int64_t samples_since_sound_started = audio->time - audio->sound_started_time;
    int64_t sound_samples_remaining =
            audio->sounds[audio->sound].size - samples_since_sound_started;

    for (int64_t sample_index = 0; sample_index < audio->sample_count; sample_index++) {
        for (int64_t channel_index = 0; channel_index < AUDIO_CHANNEL_COUNT; channel_index++) {
            if (sample_index < sound_samples_remaining) {
                audio->sample_buffer[sample_index].channels[channel_index] =
                        audio->asset_data[audio->sounds[audio->sound].offset
                                + samples_since_sound_started + sample_index];
            } else {
                audio->sample_buffer[sample_index].channels[channel_index] = 0;
            }
        }
        audio->time++;
    }
}

static JkIntVector2 screen_board_origin_get(int32_t square_side_length)
{
    return (JkIntVector2){square_side_length, square_side_length};
}

static JkIntVector2 screen_to_board_pos_pixels(int32_t square_side_length, JkIntVector2 screen_pos)
{
    return jk_int_vector_2_sub(screen_pos, screen_board_origin_get(square_side_length));
}

static b32 screen_in_bounds(int32_t square_side_length, JkIntVector2 screen_pos)
{
    return screen_pos.x >= 0 && screen_pos.x < square_side_length * 10 && screen_pos.y >= 0
            && screen_pos.y < square_side_length * 10;
}

static b32 board_pos_pixels_in_bounds(int32_t square_side_length, JkIntVector2 board_pos_pixels)
{
    int32_t board_side_length = square_side_length * 8;
    return board_pos_pixels.x >= 0 && board_pos_pixels.x < board_side_length
            && board_pos_pixels.y >= 0 && board_pos_pixels.y < board_side_length;
}

static JkIntVector2 board_pos_pixels_to_squares(
        int32_t square_side_length, JkIntVector2 board_pos_pixels)
{
    if (board_pos_pixels_in_bounds(square_side_length, board_pos_pixels)) {
        return (JkIntVector2){
            .x = board_pos_pixels.x / square_side_length,
            .y = 7 - board_pos_pixels.y / square_side_length,
        };
    } else {
        return (JkIntVector2){-1, -1};
    }
}

static JkIntVector2 screen_to_board_pos(int32_t square_side_length, JkIntVector2 screen_pos)
{
    return board_pos_pixels_to_squares(
            square_side_length, screen_to_board_pos_pixels(square_side_length, screen_pos));
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

void print_nodes(void (*print)(char *), MoveNode *node, uint32_t depth)
{
    char buffer[1024];
    for (uint32_t i = 0; i < depth; i++) {
        print(" ");
    }
    snprintf(buffer, JK_ARRAY_COUNT(buffer), "%d, %u\n", node->score, node->search_score);
    print(buffer);
    for (MoveNode *child = node->first_child; child; child = child->next_sibling) {
        print_nodes(print, child, depth + 1);
    }
}

void debug_set_top_level_index(MoveNode *node, int8_t top_level_index)
{
    node->top_level_index = top_level_index;
    for (MoveNode *child = node->first_child; child; child = child->next_sibling) {
        debug_set_top_level_index(child, top_level_index);
    }
}

void update(Chess *chess)
{
    // Debug reset
    if (button_pressed(chess, INPUT_FLAG_RESET)) {
        chess->flags &= ~FLAG_INITIALIZED;
    }

    if (!(chess->flags & FLAG_INITIALIZED)) {
        memset(&debug_chess, 0, sizeof(debug_chess));
        memcpy(debug_chess.atlas, chess->atlas, sizeof(debug_chess.atlas));
        debug_chess.square_side_length = 64;
        debug_chess.selected_square = (JkIntVector2){-1, -1};
        debug_chess.promo_square = (JkIntVector2){-1, -1};
        debug_chess.draw_buffer = debug_draw_buffer;

        // Test search score calculation
        AiContext ctx = {
            .top_level_node_count = 2,
            .top_level_nodes =
                    {
                        {.score = -1},
                        {.score = 3},
                        {.score = 3},
                        {.score = -1},
                        {.score = 3},
                        {.score = 11},
                        {.score = 3},
                        {.score = 1},
                        {.score = -1},
                        {.score = -4},
                        {.score = 0},
                        {.score = 3},
                        {.score = 11},
                        {.score = 6},
                    },
        };
        for (int32_t i = 0; i < 14; i++) {
            if (i % 2 == 0 && i) {
                ctx.top_level_nodes[i / 2 - 1].first_child = ctx.top_level_nodes + i;
            } else {
                ctx.top_level_nodes[i - 1].next_sibling = ctx.top_level_nodes + i;
            }
        }
        for (uint8_t i = 0; i < ctx.top_level_node_count; i++) {
            debug_set_top_level_index(ctx.top_level_nodes + i, i);
        }
        update_search_scores_root(&ctx, WHITE);
        // JK_ASSERT(ctx.top_level_nodes[0].search_score == 6);
        // JK_ASSERT(ctx.top_level_nodes[1].search_score == 7);
        print_nodes(chess->debug_print, ctx.top_level_nodes, 0);
        print_nodes(chess->debug_print, ctx.top_level_nodes + 1, 0);

        chess->flags = FLAG_INITIALIZED;
        chess->player_types[0] = PLAYER_HUMAN;
        chess->player_types[1] = PLAYER_AI;
        chess->selected_square = (JkIntVector2){-1, -1};
        chess->promo_square = (JkIntVector2){-1, -1};
        chess->ai_move = (Move){.src = UINT8_MAX};
        chess->result = 0;
        // memcpy(&chess->board, &starting_state, sizeof(chess->board));
        chess->board = parse_fen(jk_buffer_from_null_terminated(wtf9_fen));
        moves_get(&chess->moves, chess->board);
        if (chess->player_types[board_current_team_get(chess->board)] == PLAYER_AI) {
            chess->flags |= FLAG_REQUEST_AI_MOVE;
        }

        chess->audio.time = 0;
        chess->audio.sound = 0;
        chess->audio.sound_started_time = 0;

        srand(0xd5717cc6);

        JK_DEBUG_ASSERT(board_castling_rights_flag_get(WHITE, 0)
                == BOARD_FLAG_WHITE_QUEEN_SIDE_CASTLING_RIGHTS);
        JK_DEBUG_ASSERT(board_castling_rights_flag_get(WHITE, 1)
                == BOARD_FLAG_WHITE_KING_SIDE_CASTLING_RIGHTS);
        JK_DEBUG_ASSERT(board_castling_rights_flag_get(BLACK, 0)
                == BOARD_FLAG_BLACK_QUEEN_SIDE_CASTLING_RIGHTS);
        JK_DEBUG_ASSERT(board_castling_rights_flag_get(BLACK, 1)
                == BOARD_FLAG_BLACK_KING_SIDE_CASTLING_RIGHTS);
    }

    if (button_pressed(chess, INPUT_FLAG_CANCEL)) {
        chess->selected_square = (JkIntVector2){-1, -1};
        chess->promo_square = (JkIntVector2){-1, -1};
    }

    // Start at an invalid value. If move.src remains at an invalid value, we should ignore it.
    // If move.src becomes valid, we should perform the move.
    Move move = {.src = UINT8_MAX};

    JkIntVector2 mouse_pos = screen_to_board_pos(chess->square_side_length, chess->input.mouse_pos);

    if (chess->player_types[board_current_team_get(chess->board)] == PLAYER_HUMAN) {
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
            uint64_t available_destinations = destinations_get_by_src(
                    chess, board_index_get_unbounded(chess->selected_square));
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
    } else { // Current player is an AI
        if (chess->ai_move.src < 64) {
            move = chess->ai_move;
            chess->ai_move.src = UINT8_MAX;
        }
    }

    if (move.src < 64) {
        JkIntVector2 dest = board_index_to_vector_2(move.dest);
        if (move.piece.type == PAWN && (dest.y == 0 || dest.y == 7)) { // Enter pawn promotion
            chess->promo_square = dest;
        } else { // Make a move
            chess->audio.sound = board_piece_get_index(chess->board, move.dest).type == NONE
                    ? SOUND_MOVE
                    : SOUND_CAPTURE;
            chess->audio.sound_started_time = chess->audio.time;

            chess->board = board_move_perform(chess->board, move_pack(move));
            debug_printf(chess->debug_print, "move.bits: %x\n", (uint32_t)move_pack(move).bits);
            debug_printf(chess->debug_print, "score: %d\n", board_score(chess->board, 0));

            chess->selected_square = (JkIntVector2){-1, -1};
            chess->promo_square = (JkIntVector2){-1, -1};
            Team current_team = board_current_team_get(chess->board);
            moves_get(&chess->moves, chess->board);

            if (chess->moves.count) {
                if (chess->player_types[current_team] == PLAYER_AI) {
                    chess->flags |= FLAG_REQUEST_AI_MOVE;
                }
            } else {
                chess->victor = !current_team;

                uint64_t threatened = board_threats_get(chess->board, chess->victor).bitfield;
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

static Color color_background = {CLEAR_COLOR_B, CLEAR_COLOR_G, CLEAR_COLOR_R};

// Color light_squares = {0xde, 0xe2, 0xde};
// Color dark_squares = {0x39, 0x41, 0x3a};

static Color color_light_squares = {0xe9, 0xe2, 0xd7};
static Color color_dark_squares = {0x50, 0x41, 0x2b};

// Blended halfway between the base square colors and #E26D5C

static Color color_selection = {0x5c, 0x6d, 0xe2};
static Color color_move_prev = {0x2b, 0xa6, 0xff};

// Color white = {0x8e, 0x8e, 0x8e};
static Color color_white_pieces = {0x82, 0x92, 0x85};
static Color color_black_pieces = {0xff, 0x73, 0xa2};

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

static uint8_t atlas_piece_get_alpha(
        uint8_t *atlas, int32_t square_side_length, PieceType piece_type, JkIntVector2 pos)
{
    int32_t y_offset = (piece_type - 1) * square_side_length;
    return atlas[(pos.y + y_offset) * square_side_length * 5 + pos.x];
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
            float max_y = ceilf(y2);
            float max_x = ceilf(x2);
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

void render(Chess *chess)
{
    static char string_buf[1024];

    if (chess->square_side_length_prev != chess->square_side_length) {
        int32_t scaled_atlas_height = chess->square_side_length * 6;
        int32_t scaled_atlas_width = chess->square_side_length * 5;

        // uint64_t time_before_scale = chess->cpu_timer_get();
        scale_alpha_map(chess->scaled_atlas,
                scaled_atlas_width,
                scaled_atlas_height,
                chess->atlas,
                ATLAS_WIDTH,
                ATLAS_HEIGHT);
        // snprintf(string_buf,
        //         JK_ARRAY_COUNT(string_buf),
        //         "%.0fms\n",
        //         (double)(chess->cpu_timer_get() - time_before_scale) * 1000.0
        //                 / (double)chess->cpu_timer_frequency);
        // chess->debug_print(string_buf);
    }

    // Figure out which squares should be highlighted
    uint8_t selected_index = board_index_get_unbounded(chess->selected_square);
    uint64_t destinations = destinations_get_by_src(chess, selected_index);
    uint8_t mouse_index = board_index_get_unbounded(
            screen_to_board_pos(chess->square_side_length, chess->input.mouse_pos));
    b32 promoting = board_in_bounds(chess->promo_square);

    // uint64_t threatened = board_threatened_squares_get(
    //         chess->board, !((chess->board.flags >> BOARD_FLAG_INDEX_CURRENT_PLAYER) & 1));

    Move move_prev = move_unpack(chess->board.move_prev);

    JkIntVector2 pos;
    for (pos.y = 0; pos.y < chess->square_side_length * 10; pos.y++) {
        for (pos.x = 0; pos.x < chess->square_side_length * 10; pos.x++) {
            JkIntVector2 board_pos_pixels =
                    screen_to_board_pos_pixels(chess->square_side_length, pos);

            JkIntVector2 result_origin = jk_int_vector_2_add(
                    screen_board_origin_get(chess->square_side_length),
                    (JkIntVector2){chess->square_side_length * 2, chess->square_side_length * 3});
            JkIntVector2 result_pos = jk_int_vector_2_sub(pos, result_origin);
            if (chess->result && result_pos.x >= 0 && result_pos.x < chess->square_side_length * 4
                    && result_pos.y >= 0 && result_pos.y < chess->square_side_length * 2) {
                int32_t result_offset = 0;
                if (chess->result == RESULT_CHECKMATE) {
                    result_offset = (chess->board.flags & BOARD_FLAG_CURRENT_PLAYER ? 1 : 2)
                            * chess->square_side_length * 2;
                }
                uint8_t alpha = chess->scaled_atlas[(result_pos.y + result_offset)
                                * chess->square_side_length * 5
                        + result_pos.x + chess->square_side_length];
                chess->draw_buffer[pos.y * DRAW_BUFFER_WIDTH + pos.x] =
                        blend_alpha((Color){255, 255, 255}, color_background, alpha);
            } else {
                Color color;
                if (board_pos_pixels_in_bounds(chess->square_side_length, board_pos_pixels)) {
                    JkIntVector2 square_pos =
                            jk_int_vector_2_remainder(chess->square_side_length, board_pos_pixels);

                    JkIntVector2 board_pos = board_pos_pixels_to_squares(
                            chess->square_side_length, board_pos_pixels);
                    uint8_t index = board_index_get(board_pos);
                    b32 light = (board_pos_pixels.x / chess->square_side_length) % 2
                            == (board_pos_pixels.y / chess->square_side_length) % 2;
                    Color square_color = light ? color_light_squares : color_dark_squares;
                    Piece piece = board_piece_get(chess->board, board_pos);

                    int32_t dist_from_promo_square =
                            absolute_value(board_pos.y - chess->promo_square.y);
                    if (promoting && board_pos.x == chess->promo_square.x
                            && dist_from_promo_square < JK_ARRAY_COUNT(promo_order)) {
                        color = color_background;
                        piece.team = board_current_team_get(chess->board);
                        piece.type = promo_order[dist_from_promo_square];
                    } else if (index == selected_index) {
                        color = blend(color_selection, square_color);
                    } else if (destinations & (1llu << index)) {
                        color = blend(color_selection, square_color);
                        if (index == mouse_index) {
                            int32_t x_dist_from_edge = square_pos.x < chess->square_side_length / 2
                                    ? square_pos.x
                                    : chess->square_side_length - 1 - square_pos.x;
                            int32_t y_dist_from_edge = square_pos.y < chess->square_side_length / 2
                                    ? square_pos.y
                                    : chess->square_side_length - 1 - square_pos.y;
                            if ((x_dist_from_edge >= 4 && y_dist_from_edge >= 4)
                                    && (x_dist_from_edge < 8 || y_dist_from_edge < 8)) {
                                color = square_color;
                            }
                        }
                    } else if ((move_prev.src || move_prev.dest)
                            && (index == move_prev.src || index == move_prev.dest)) {
                        color = blend(color_move_prev, square_color);
                    } else {
                        color = square_color;
                    }

                    if (piece.type != NONE) {
                        Color color_piece = piece.team ? color_black_pieces : color_white_pieces;
                        uint8_t alpha = atlas_piece_get_alpha(chess->scaled_atlas,
                                chess->square_side_length,
                                piece.type,
                                square_pos);
                        if (index == selected_index
                                && ((chess->flags & FLAG_HOLDING_PIECE) || promoting)) {
                            alpha /= 2;
                        }
                        color = blend_alpha(color_piece, color, alpha);
                    }
                } else {
                    color = color_background;
                }
                color.a = 255;
                chess->draw_buffer[pos.y * DRAW_BUFFER_WIDTH + pos.x] = color;
            }
        }
    }

    if ((chess->flags & FLAG_HOLDING_PIECE) && selected_index < 64) {
        Piece piece = board_piece_get_index(chess->board, selected_index);
        if (piece.type != NONE) {
            JkIntVector2 held_piece_offset = jk_int_vector_2_sub(chess->input.mouse_pos,
                    (JkIntVector2){chess->square_side_length / 2, chess->square_side_length / 2});
            for (pos.y = 0; pos.y < chess->square_side_length; pos.y++) {
                for (pos.x = 0; pos.x < chess->square_side_length; pos.x++) {
                    JkIntVector2 screen_pos = jk_int_vector_2_add(pos, held_piece_offset);
                    if (screen_in_bounds(chess->square_side_length, screen_pos)) {
                        int32_t index = screen_pos.y * DRAW_BUFFER_WIDTH + screen_pos.x;
                        Color color_piece = piece.team ? color_black_pieces : color_white_pieces;
                        Color color_bg = chess->draw_buffer[index];
                        uint8_t alpha = atlas_piece_get_alpha(
                                chess->scaled_atlas, chess->square_side_length, piece.type, pos);
                        chess->draw_buffer[index] = blend_alpha(color_piece, color_bg, alpha);
                    }
                }
            }
        }
    }

    chess->square_side_length_prev = chess->square_side_length;
}
