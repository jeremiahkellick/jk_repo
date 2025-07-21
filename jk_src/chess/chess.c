#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// clang-format off

// #jk_build compiler_arguments /LD
// #jk_build linker_arguments /OUT:chess.dll /EXPORT:update /EXPORT:render /EXPORT:ai_init /EXPORT:ai_running /EXPORT:profile_print
// #jk_build single_translation_unit

// clang-format on

// #jk_build dependencies_begin
#include <jk_src/jk_lib/platform/platform.h>
#include <jk_src/jk_shapes/jk_shapes.h>
// #jk_build dependencies_end

#include "chess.h"
#include "jk_src/jk_lib/jk_lib.h"

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

static JkBuffer puzzle_fen = JKSI("8/1bp1rkp1/p4p2/1p1p1QPR/3q3P/8/5PK1/8 b - - 0 1");

static JkBuffer wtf_fen = JKSI("rnb1kbnr/pppp1ppp/5q2/4p3/3PP3/5N2/PPP2PPP/RNBQKB1R b - - 0 1");

static JkBuffer wtf2_fen =
        JKSI("1nb1k1nr/r2p1qp1/1pp3B1/3pP1Qp/p4p2/2N2N2/PPP2PPP/R4RK1 b k - 1 1");

static JkBuffer wtf5_fen = JKSI("r1b1k1nr/1p4pp/p3p3/5p2/5B2/nP4P1/PbP2P1P/1R1R2K1 b kq - 4 3");

static JkBuffer wtf9_fen = JKSI("r2k1b2/p3r3/npp2p2/1P1p4/P4Bb1/1BP3P1/3RNP1P/4R1K1 b - - 0 2");

static JkBuffer king_fight_fen = JKSI("8/8/8/4k3/8/8/8/3QK3 w - - 0 1");

static char debug_print_buffer[4096];

static int32_t piece_counts[PIECE_TYPE_COUNT] = {0, 1, 1, 2, 2, 2, 8};

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

static uint8_t debug_render_memory[1024 * 1024 * 1024];

static ChessAssets *debug_assets;

static JkColor debug_draw_buffer[DRAW_BUFFER_SIZE];

static void debug_render(Board board)
{
    debug_chess.board = board;

    // We'll crash if we don't fill out the following
    debug_chess.os_timer_frequency = 1;
    debug_chess.render_memory.size = sizeof(debug_render_memory);
    debug_chess.render_memory.data = debug_render_memory;

    render(debug_assets, &debug_chess);
}

static Team board_current_team_get(Board board)
{
    return (board.flags >> BOARD_FLAG_CURRENT_PLAYER) & 1;
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

static JkBuffer team_choice_strings[TEAM_CHOICE_COUNT] = {
    JKSI("White"), JKSI("Black"), JKSI("Random")};

static JkBuffer opponent_type_strings[PLAYER_TYPE_COUNT] = {JKSI("You"), JKSI("AI")};

static JkBuffer timer_strings[TIMER_COUNT] = {
    JKSI("1 min"), JKSI("3 min"), JKSI("10 min"), JKSI("30 min")};

static int64_t timer_minutes[TIMER_COUNT] = {1, 3, 10, 30};

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
    Board board = {.flags = JK_MASK(BOARD_FLAG_WHITE_QUEEN_SIDE_CASTLING_RIGHTS)
                | JK_MASK(BOARD_FLAG_WHITE_KING_SIDE_CASTLING_RIGHTS)
                | JK_MASK(BOARD_FLAG_BLACK_QUEEN_SIDE_CASTLING_RIGHTS)
                | JK_MASK(BOARD_FLAG_BLACK_KING_SIDE_CASTLING_RIGHTS)};
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
        board.flags |= JK_MASK(BOARD_FLAG_CURRENT_PLAYER);
    }

    while (isspace(fen.data[i])) {
        i++;
    }

    // Parse castling rights. Flag set means disallowed.
    uint8_t c;
    while (!isspace(c = fen.data[i++])) {
        if (c == 'Q') {
            board.flags &= ~JK_MASK(BOARD_FLAG_WHITE_QUEEN_SIDE_CASTLING_RIGHTS);
        } else if (c == 'K') {
            board.flags &= ~JK_MASK(BOARD_FLAG_WHITE_KING_SIDE_CASTLING_RIGHTS);
        } else if (c == 'q') {
            board.flags &= ~JK_MASK(BOARD_FLAG_BLACK_QUEEN_SIDE_CASTLING_RIGHTS);
        } else if (c == 'k') {
            board.flags &= ~JK_MASK(BOARD_FLAG_BLACK_KING_SIDE_CASTLING_RIGHTS);
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

static uint64_t board_castling_rights_mask_get(Team team, b32 king_side)
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

    return result;
}

static Board board_move_perform(Board board, MovePacked move_packed)
{
    Move move = move_unpack(move_packed);
    JkIntVector2 src = board_index_to_vector_2(move.src);
    JkIntVector2 dest = board_index_to_vector_2(move.dest);
    Team team = (board.flags >> BOARD_FLAG_CURRENT_PLAYER) & 1;

    // En-passant handling
    if (move.piece.type == PAWN && src.x != dest.x
            && board_piece_get_index(board, move.dest).type == NONE) {
        int32_t y_delta = JK_FLAG_GET(board.flags, BOARD_FLAG_CURRENT_PLAYER) ? 1 : -1;
        board_piece_set(&board, jk_int_vector_2_add(dest, (JkIntVector2){0, y_delta}), (Piece){0});
    }

    // Castling handling
    int32_t delta_x = dest.x - src.x;
    if (move.piece.type == ROOK && src.y == (team ? 7 : 0) && (src.x == 0 || src.x == 7)) {
        board.flags |= board_castling_rights_mask_get(team, src.x == 7);
    }
    if (move.piece.type == KING) {
        board.flags |= board_castling_rights_mask_get(team, 0);
        board.flags |= board_castling_rights_mask_get(team, 1);

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
    board.flags ^= JK_MASK(BOARD_FLAG_CURRENT_PLAYER);

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
}

// ---- AI begin ---------------------------------------------------------------

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

typedef struct Scores {
    int32_t a[2];
} Scores;

static uint32_t distance_from_target(int32_t score, AiTarget target)
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

static uint32_t search_score_get(Ai *ctx, MoveNode *node, Team team)
{
    AiTarget target = ctx->targets[node->top_level_index];
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

static void update_search_scores(Ai *ctx, MoveNode *node, Team team, b32 verify)
{
    AiTarget target = ctx->targets[node->top_level_index];
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

static void update_search_scores_root(Ai *ctx, b32 verify)
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
    AiTarget favorite_target = {
        .assisting_team = !team,
        .score = team_multiplier[team] * (top_two_pairs[0].score - 1),
    };
    AiTarget target = {
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

static b32 expand_node(Ai *ctx, MoveNode *node)
{
    Board node_board_state = ctx->board;
    {
        MoveArray prev_moves;
        prev_moves.count = 0;
        for (MoveNode *ancestor = node; ancestor; ancestor = ancestor->parent) {
            prev_moves.data[prev_moves.count++] = ancestor->move;
        }

        for (int64_t i = prev_moves.count - 1; i >= 0; i--) {
            node_board_state = board_move_perform(node_board_state, prev_moves.data[i]);
        }
    }

    MoveArray moves;
    moves_get(&moves, node_board_state);
    MoveNode *prev_child = 0;
    MoveNode *first_child = 0;
    debug_nodes_found += moves.count;
    for (uint8_t i = 0; i < moves.count; i++) {
        MoveNode *new_child = jk_arena_push(&ctx->arena, sizeof(*new_child));
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
    MoveNode *deepest_leaf;
    MovePacked *max_line;
} MoveTreeStats;

void move_tree_stats_calculate(JkArena *arena, MoveTreeStats *stats, MoveNode *node, uint64_t depth)
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
            stats->deepest_leaf = node;
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

void ai_init(Ai *ai,
        Board board,
        JkBuffer memory,
        uint64_t time,
        uint64_t time_frequency,
        void (*debug_print)(char *))
{
    ai->response.board = (Board){0};
    ai->response.move = (Move){0};

    JkArenaRoot arena_root;
    ai->arena = jk_arena_fixed_init(&arena_root, memory);

    ai->board = board;
    ai->time = time;
    ai->time_frequency = time_frequency;
    ai->time_started = time;
    ai->time_limit = 5 * time_frequency;
    ai->debug_print = debug_print;

    MoveArray moves;
    moves_get(&moves, ai->board);

    debug_nodes_found = 0;

    // ai->time_current_get = debug_time_current_get;
    // ai->time_limit = 298441;
    // ai->time_limit = 100;

    ai->top_level_node_count = moves.count;

    if (!moves.count) {
        JK_ASSERT(0 && "If there are no legal moves, the AI should never have been asked for one");
    } else if (moves.count == 1) {
        // If there's only one legal move, take it
        ai->response.move = move_unpack(moves.data[0]);
    } else {
        moves_shuffle(&moves);
        MoveNode *prev_child = 0;
        for (uint8_t i = 0; i < moves.count; i++) {
            MoveNode *new_child = ai->top_level_nodes + i;

            new_child->depth = 1;
            new_child->expanded = 0;
            new_child->move = moves.data[i];
            new_child->parent = 0;
            new_child->next_sibling = 0;
            new_child->first_child = 0;
            new_child->board_score =
                    board_score(board_move_perform(ai->board, moves.data[i]), new_child->depth);
            new_child->score = new_child->board_score;
            new_child->search_score = UINT32_MAX;
            new_child->top_level_index = i;

            if (i) {
                prev_child->next_sibling = new_child;
            }

            prev_child = new_child;
        }

        update_search_scores_root(ai, 0);
    }
}

b32 ai_running(Ai *ai)
{
    if (ai->top_level_node_count < 2) {
        return 0;
    }

    b32 running = ai->time - ai->time_started < ai->time_limit;

    if (running) {
        MoveNode *node = 0;
        { // Find the node with the best search score
            AiTarget target = {0};
            {
                uint32_t min_search_score = UINT32_MAX;
                for (int32_t i = 0; i < ai->top_level_node_count; i++) {
                    if (ai->top_level_nodes[i].search_score < min_search_score) {
                        node = ai->top_level_nodes + i;
                        target = ai->targets[i];
                        min_search_score = ai->top_level_nodes[i].search_score;
                    }
                }
            }
            JK_ASSERT(node);

            {
                Team team = !board_current_team_get(ai->board);
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

        if (!expand_node(ai, node)) {
            ai->debug_print("Out of AI memory\n");
            running = 0;
        }
    }

    if (!running) {
        int32_t max_score_i = 0;
        MovePacked move = {0};
        {
            // Pick move with the best score
            Team team = board_current_team_get(ai->board);
            int32_t max_score = INT32_MIN;
            for (int32_t i = 0; i < ai->top_level_node_count; i++) {
                int32_t score = team_multiplier[team] * ai->top_level_nodes[i].score;
                if (max_score < score) {
                    max_score_i = i;
                    max_score = score;
                    move = ai->top_level_nodes[i].move;
                }
            }
        }

        uint64_t time_elapsed = ai->time - ai->time_started;
        double seconds_elapsed = (double)time_elapsed / (double)ai->time_frequency;

        // Print min and max depth
        MoveTreeStats stats = {.min_depth = UINT64_MAX};
        for (int32_t i = 0; i < ai->top_level_node_count; i++) {
            move_tree_stats_calculate(&ai->arena, &stats, ai->top_level_nodes + i, 1);
        }
        stats.max_line = jk_arena_pointer_current(&ai->arena);
        for (MoveNode *ancestor = stats.deepest_leaf; ancestor; ancestor = ancestor->parent) {
            MovePacked *line_move = jk_arena_push(&ai->arena, sizeof(*line_move));
            *line_move = ancestor->move;
        }

        debug_printf(ai->debug_print, "node_count: %llu\n", stats.node_count);
        debug_printf(ai->debug_print, "seconds_elapsed: %.1f\n", seconds_elapsed);
        debug_printf(ai->debug_print,
                "%.4f Mn/s\n",
                ((double)stats.node_count / 1000000.0) / seconds_elapsed);

        if (stats.max_depth < 300) {
            uint64_t max_score_depth = 0;
            {
                Team team = board_current_team_get(ai->board);
                Board board = ai->board;
                MoveNode *node = ai->top_level_nodes + max_score_i;
                while (node) {
                    max_score_depth++;
                    board = board_move_perform(board, node->move);
                    // debug_render(board);
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

            debug_printf(ai->debug_print,
                    "node_count\t%llu\nleaf_count\t%llu\nmin_depth\t%llu\navg_depth\t%llu\nmax_"
                    "depth\t%"
                    "llu\nmax_score_depth\t%llu\n",
                    stats.node_count,
                    stats.leaf_count,
                    stats.min_depth,
                    stats.depth_sum / stats.leaf_count,
                    stats.max_depth,
                    max_score_depth);

            debug_printf(ai->debug_print, "max_line:\n");
            for (int32_t i = 0; i < stats.max_depth; i++) {
                debug_printf(ai->debug_print, "\t0x%x\n", (uint32_t)stats.max_line[i].bits);
            }

            /*
            {
                Board board = ai->board;
                for (int32_t i = stats.min_line.count - 1; i >= 0; i--) {
                    board = board_move_perform(board, stats.min_line.data[i]);
                    debug_render(board);
                }
            }

            {
                Board board = ai->board;
                for (int32_t i = (int32_t)stats.max_depth - 1; i >= 0; i--) {
                    board = board_move_perform(board, stats.max_line[i]);
                    debug_printf(ai->debug_print,
                            "score: %d\n",
                            board_score(board, stats.max_depth - 1 - i));
                    debug_render(board);
                }
            }
            */
        }

        ai->response.move = move_unpack(move);
    }

    return running;
}

// ---- AI end -----------------------------------------------------------------

static void audio_write(ChessAssets *assets, Audio *audio)
{
    int64_t sound_sample_count = (int64_t)assets->sounds[audio->sound].size / sizeof(uint16_t);
    uint16_t *sound_samples = (uint16_t *)((uint8_t *)assets + assets->sounds[audio->sound].offset);

    int64_t samples_since_sound_started = audio->time - audio->sound_started_time;
    int64_t sound_samples_remaining = sound_sample_count - samples_since_sound_started;

    for (int64_t sample_index = 0; sample_index < audio->sample_count; sample_index++) {
        for (int64_t channel_index = 0; channel_index < AUDIO_CHANNEL_COUNT; channel_index++) {
            if (sample_index < sound_samples_remaining) {
                audio->sample_buffer[sample_index].channels[channel_index] =
                        sound_samples[samples_since_sound_started + sample_index];
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

static JkIntVector2 apply_perspective(Team perspective, JkIntVector2 board_position)
{
    if (perspective) {
        board_position.x = 7 - board_position.x;
    } else {
        board_position.y = 7 - board_position.y;
    }
    return board_position;
}

static JkIntVector2 board_pos_pixels_to_squares_raw(Chess *chess, JkIntVector2 board_pos_pixels)
{
    return apply_perspective(chess->perspective,
            (JkIntVector2){
                .x = board_pos_pixels.x / chess->square_side_length,
                .y = board_pos_pixels.y / chess->square_side_length,
            });
}

static JkIntVector2 board_pos_pixels_to_squares(Chess *chess, JkIntVector2 board_pos_pixels)
{
    if (board_pos_pixels_in_bounds(chess->square_side_length, board_pos_pixels)) {
        return board_pos_pixels_to_squares_raw(chess, board_pos_pixels);
    } else {
        return (JkIntVector2){-1, -1};
    }
}

static JkIntVector2 screen_to_board_pos(Chess *chess, JkIntVector2 screen_pos)
{
    return board_pos_pixels_to_squares(
            chess, screen_to_board_pos_pixels(chess->square_side_length, screen_pos));
}

static JkVector2 board_to_canvas_pos(Team perspective, float square_size, JkIntVector2 board_pos)
{
    board_pos = apply_perspective(perspective, board_pos);
    JkVector2 board_pos_f = {(float)board_pos.x, (float)board_pos.y};
    return jk_vector_2_add(
            (JkVector2){square_size, square_size}, jk_vector_2_mul(square_size, board_pos_f));
}

static b32 input_button_pressed(Chess *chess, InputId id)
{
    return JK_FLAG_GET(chess->input.flags, id) && !JK_FLAG_GET(chess->input_prev.flags, id);
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

void update(ChessAssets *assets, Chess *chess)
{
    if (!debug_assets) {
        debug_assets = assets;
        memset(&debug_chess, 0, sizeof(debug_chess));
        debug_chess.square_side_length = 64;
        debug_chess.selected_square = (JkIntVector2){-1, -1};
        debug_chess.promo_square = (JkIntVector2){-1, -1};
        debug_chess.draw_buffer = debug_draw_buffer;
    }

    // Debug reset
    if (input_button_pressed(chess, INPUT_RESET)) {
        chess->flags &= ~JK_MASK(CHESS_FLAG_INITIALIZED);
    }

    b32 start_new_game = 0;

    // Process UI button presses
    switch (chess->screen) {
    case SCREEN_GAME: {
        if (input_button_pressed(chess, INPUT_CONFIRM)
                && jk_int_rect_point_test(
                        chess->buttons[BUTTON_MENU_OPEN].rect, chess->input.mouse_pos)) {
            chess->screen = SCREEN_MENU;
        }
    } break;

    case SCREEN_MENU: {
        if (input_button_pressed(chess, INPUT_CONFIRM)) {
            if (jk_int_rect_point_test(
                        chess->buttons[BUTTON_MENU_CLOSE].rect, chess->input.mouse_pos)) {
                chess->screen = SCREEN_GAME;
            }

            for (TeamChoice team_choice = 0; team_choice < TEAM_CHOICE_COUNT; team_choice++) {
                if (jk_int_rect_point_test(chess->buttons[BUTTON_WHITE + team_choice].rect,
                            chess->input.mouse_pos)) {
                    chess->settings.team_choice = team_choice;
                }
            }

            for (PlayerType opponent_type = 0; opponent_type < PLAYER_TYPE_COUNT; opponent_type++) {
                if (jk_int_rect_point_test(chess->buttons[BUTTON_YOU + opponent_type].rect,
                            chess->input.mouse_pos)) {
                    chess->settings.opponent_type = opponent_type;
                }
            }

            for (Timer timer = 0; timer < TIMER_COUNT; timer++) {
                if (jk_int_rect_point_test(
                            chess->buttons[BUTTON_1_MIN + timer].rect, chess->input.mouse_pos)) {
                    chess->settings.timer = timer;
                }
            }

            if (jk_int_rect_point_test(
                        chess->buttons[BUTTON_START_GAME].rect, chess->input.mouse_pos)) {
                start_new_game = 1;
            }
        }
    } break;

    case SCREEN_COUNT: {
        chess->debug_print("Invalid chess->screen value\n");
    } break;
    }

    if (start_new_game || !JK_FLAG_GET(chess->flags, CHESS_FLAG_INITIALIZED)) {
        // Test search score calculation
        Ai ctx = {
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

        if (!JK_FLAG_GET(chess->flags, CHESS_FLAG_INITIALIZED)) {
            // If we got here because the state is not initialized (as opposed to the player
            // clicking the "Start new game" button in the UI), then we should initialize the
            // settings to their default values.
            chess->settings.team_choice = TEAM_CHOICE_WHITE;
            chess->settings.opponent_type = PLAYER_AI;
            chess->settings.timer = TIMER_10_MIN;
        }

        chess->flags = JK_MASK(CHESS_FLAG_INITIALIZED);
        chess->turn_index = 0;

        if (chess->settings.team_choice == TEAM_CHOICE_RANDOM) {
            srand((unsigned int)time(0));
            chess->perspective = rand() % 2;
        } else {
            chess->perspective = chess->settings.team_choice == TEAM_CHOICE_BLACK;
        }
        chess->player_types[chess->perspective] = PLAYER_HUMAN;
        chess->player_types[!chess->perspective] = chess->settings.opponent_type;

        chess->selected_square = (JkIntVector2){-1, -1};
        chess->promo_square = (JkIntVector2){-1, -1};
        chess->result = 0;
        chess->piece_prev_type = NONE;
        chess->time_move_prev = 0;
        memcpy(&chess->board, &starting_state, sizeof(chess->board));
        // chess->board = parse_fen(wtf9_fen);
        moves_get(&chess->moves, chess->board);

        chess->audio.time = 0;
        chess->audio.sound = 0;
        chess->audio.sound_started_time = 0;

        chess->os_time_turn_start = chess->os_time;
        for (Team team = 0; team < TEAM_COUNT; team++) {
            chess->os_time_player[team] =
                    timer_minutes[chess->settings.timer] * 60 * chess->os_timer_frequency;
        }

        chess->screen = SCREEN_GAME;

        srand(0xd5717cc6);

        JK_DEBUG_ASSERT(board_castling_rights_mask_get(WHITE, 0)
                == JK_MASK(BOARD_FLAG_WHITE_QUEEN_SIDE_CASTLING_RIGHTS));
        JK_DEBUG_ASSERT(board_castling_rights_mask_get(WHITE, 1)
                == JK_MASK(BOARD_FLAG_WHITE_KING_SIDE_CASTLING_RIGHTS));
        JK_DEBUG_ASSERT(board_castling_rights_mask_get(BLACK, 0)
                == JK_MASK(BOARD_FLAG_BLACK_QUEEN_SIDE_CASTLING_RIGHTS));
        JK_DEBUG_ASSERT(board_castling_rights_mask_get(BLACK, 1)
                == JK_MASK(BOARD_FLAG_BLACK_KING_SIDE_CASTLING_RIGHTS));
    }

    if (chess->turn_index && chess->result == RESULT_NONE
            && chess->os_time_player[board_current_team_get(chess->board)]
                            - (int64_t)(chess->os_time - chess->os_time_turn_start)
                    <= 0) {
        chess->os_time_player[board_current_team_get(chess->board)] = 0;
        chess->result = RESULT_TIME;
    }

    switch (chess->screen) {
    case SCREEN_GAME: {
        if (input_button_pressed(chess, INPUT_CANCEL)) {
            chess->selected_square = (JkIntVector2){-1, -1};
            chess->promo_square = (JkIntVector2){-1, -1};
        }

        // Start at an invalid value. If move.src remains at an invalid value, we should ignore it.
        // If move.src becomes valid, we should perform the move.
        Move move = {.src = UINT8_MAX};

        JkIntVector2 mouse_pos = screen_to_board_pos(chess, chess->input.mouse_pos);

        if (chess->result == RESULT_NONE) {
            if (chess->player_types[board_current_team_get(chess->board)] == PLAYER_HUMAN) {
                if (board_in_bounds(chess->promo_square)) {
                    int32_t dist_from_promo_square =
                            absolute_value(mouse_pos.y - chess->promo_square.y);
                    if (input_button_pressed(chess, INPUT_CONFIRM)) {
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

                    if (input_button_pressed(chess, INPUT_CONFIRM)) {
                        if (mouse_on_destination) {
                            piece_drop_index = mouse_index;
                        } else {
                            chess->selected_square = (JkIntVector2){-1, -1};
                            for (uint8_t i = 0; i < chess->moves.count; i++) {
                                Move available_move = move_unpack(chess->moves.data[i]);
                                if (available_move.src == mouse_index) {
                                    chess->flags |= JK_MASK(CHESS_FLAG_HOLDING_PIECE);
                                    chess->selected_square = mouse_pos;
                                }
                            }
                        }
                    }

                    if (!(chess->input.flags & JK_MASK(INPUT_CONFIRM))
                            && (chess->flags & JK_MASK(CHESS_FLAG_HOLDING_PIECE))) {
                        chess->flags &= ~JK_MASK(CHESS_FLAG_HOLDING_PIECE);

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
                if (memcmp(&chess->ai_response.board, &chess->board, sizeof(Board)) == 0) {
                    move = chess->ai_response.move;
                }
            }
        }

        if (move.src < 64) {
            JkIntVector2 dest = board_index_to_vector_2(move.dest);
            if (move.piece.type == PAWN && (dest.y == 0 || dest.y == 7)) { // Enter pawn promotion
                chess->promo_square = dest;
            } else { // Make a move
                chess->time_move_prev = chess->time;
                chess->piece_prev_type = board_piece_get_index(chess->board, move.dest).type;

                if (chess->turn_index) {
                    chess->os_time_player[board_current_team_get(chess->board)] -=
                            (int64_t)(chess->os_time - chess->os_time_turn_start);
                }
                chess->os_time_turn_start = chess->os_time;
                chess->turn_index++;

                chess->board = board_move_perform(chess->board, move_pack(move));
                debug_printf(chess->debug_print, "move.bits: %x\n", (uint32_t)move_pack(move).bits);
                debug_printf(chess->debug_print, "score: %d\n", board_score(chess->board, 0));

                chess->selected_square = (JkIntVector2){-1, -1};
                chess->promo_square = (JkIntVector2){-1, -1};
                Team current_team = board_current_team_get(chess->board);
                moves_get(&chess->moves, chess->board);

                uint8_t king_index = 0;
                for (uint8_t square_index = 0; square_index < 64; square_index++) {
                    Piece piece = board_piece_get_index(chess->board, square_index);
                    if (piece.type == KING && piece.team == current_team) {
                        king_index = square_index;
                        break;
                    }
                }
                b32 in_check =
                        (board_threats_get(chess->board, !current_team).bitfield >> king_index) & 1;

                if (!chess->moves.count) {
                    chess->result = in_check ? RESULT_CHECKMATE : RESULT_STALEMATE;
                }

                SoundIndex sound;
                if (chess->result) {
                    if (chess->result == RESULT_STALEMATE) {
                        sound = SOUND_DRAW;
                    } else if (chess->perspective == current_team) {
                        sound = SOUND_LOSE;
                    } else {
                        sound = SOUND_WIN;
                    }
                } else if (in_check) {
                    sound = SOUND_CHECK;
                } else if (chess->piece_prev_type) {
                    sound = SOUND_CAPTURE;
                } else {
                    sound = SOUND_MOVE;
                }
                chess->audio.sound = sound;
                chess->audio.sound_started_time = chess->audio.time;
            }
        }
    } break;

    case SCREEN_MENU: {
    } break;

    case SCREEN_COUNT: {
        chess->debug_print("Invalid chess->screen value\n");
    } break;
    }

    if (input_button_pressed(chess, INPUT_LEFT)) {
        chess->board = parse_fen(king_fight_fen);
        moves_get(&chess->moves, chess->board);
    }

    audio_write(assets, &chess->audio);

    // Do some end-of-frame updating of data
    chess->flags = JK_FLAG_SET(chess->flags,
            CHESS_FLAG_WANTS_AI_MOVE,
            chess->result == RESULT_NONE
                    && chess->player_types[board_current_team_get(chess->board)] == PLAYER_AI);
    chess->time++;
    chess->input_prev = chess->input;
}

static JkColor color_background = {CLEAR_COLOR_B, CLEAR_COLOR_G, CLEAR_COLOR_R, 0xff};

// JkColor light_squares = {0xde, 0xe2, 0xde};
// JkColor dark_squares = {0x39, 0x41, 0x3a};

static JkColor color_light_squares = {0xe2, 0xdb, 0xd0, 0xff};
static JkColor color_dark_squares = {0x50, 0x41, 0x2b, 0xff};

// Blended halfway between the base square colors and #E26D5C

static JkColor color_selection = {0x5c, 0x6d, 0xe2, 0xff};
static JkColor color_move_prev = {0x11, 0x88, 0xff, 0xff};

// JkColor white = {0x8e, 0x8e, 0x8e};
static JkColor color_teams[TEAM_COUNT] = {
    {0x82, 0x92, 0x85, 0xff},
    {0xfb, 0x6f, 0x9d, 0xff},
};

static uint8_t uint8_average(uint8_t a, uint8_t b)
{
    return (uint8_t)(((uint32_t)a + (uint32_t)b) / 2);
}

static JkColor blend(JkColor a, JkColor b)
{
    JkColor result;
    for (int32_t i = 0; i < 4; i++) {
        result.v[i] = uint8_average(a.v[i], b.v[i]);
    }
    return result;
}

static uint8_t color_multiply(uint8_t a, uint8_t b)
{
    return ((uint32_t)a * (uint32_t)b) / 255;
}

static JkColor blend_alpha(JkColor foreground, JkColor background, uint8_t alpha)
{
    JkColor result = {0, 0, 0, 255};
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

typedef struct TextLayout {
    JkVector2 offset;
    JkVector2 dimensions;
} TextLayout;

static TextLayout text_layout_get(ChessAssets *assets, JkBuffer text, float scale)
{
    TextLayout result = {0};
    result.dimensions.x = 0.0f;
    for (int32_t i = 0; i < text.size; i++) {
        JkShape *shape = assets->shapes + text.data[i] + CHARACTER_SHAPE_OFFSET;
        if (i == 0) {
            result.offset.x = -shape->offset.x;
            result.dimensions.x = result.offset.x;
        }
        if (i == text.size - 1) {
            result.dimensions.x += shape->offset.x + shape->dimensions.x;
        } else {
            result.dimensions.x += shape->advance_width;
        }
    }

    // Layout looks more natural if we weight the descenders less than ascenders when considering
    // font "height". This is probably because we imagine the descenders dipping below the baseline.
    float descent = 0.4 * assets->font_descent;

    result.offset.y = -assets->font_ascent;
    result.dimensions.y = descent - assets->font_ascent;

    result.offset = jk_vector_2_mul(scale, result.offset);
    result.dimensions = jk_vector_2_mul(scale, result.dimensions);

    return result;
}

static void draw_text(
        JkShapesRenderer *renderer, JkBuffer text, JkVector2 cursor, float scale, JkColor color)
{
    for (int32_t i = 0; i < text.size; i++) {
        cursor.x += jk_shapes_draw(
                renderer, text.data[i] + CHARACTER_SHAPE_OFFSET, cursor, scale, color);
    }
}

static uint8_t region_code(float side_length, JkVector2 v)
{
    return ((v.x < 0.0f) << 0) | ((side_length - 1.0f < v.x) << 1) | ((v.y < 0.0f) << 2)
            | ((side_length - 1.0f < v.y) << 3);
}

typedef struct Endpoint {
    uint8_t code;
    JkVector2 *point;
} Endpoint;

static b32 clip_to_draw_region(float side_length, JkVector2 *a, JkVector2 *b)
{
    Endpoint endpoint_a = {.code = region_code(side_length, *a), .point = a};
    Endpoint endpoint_b = {.code = region_code(side_length, *b), .point = b};

    for (;;) {
        if (!(endpoint_a.code | endpoint_b.code)) {
            return 1;
        } else if (endpoint_a.code & endpoint_b.code) {
            return 0;
        } else {
            JkVector2 u = *a;
            JkVector2 v = *b;
            Endpoint *endpoint = endpoint_a.code < endpoint_b.code ? &endpoint_b : &endpoint_a;
            if ((endpoint->code >> 0) & 1) {
                endpoint->point->x = 0.0f;
                endpoint->point->y = u.y + (v.y - u.y) * (0.0f - u.x) / (v.x - u.x);
            } else if ((endpoint->code >> 1) & 1) {
                endpoint->point->x = side_length - 1.0f;
                endpoint->point->y = u.y + (v.y - u.y) * (side_length - 1.0f - u.x) / (v.x - u.x);
            } else if ((endpoint->code >> 2) & 1) {
                endpoint->point->x = u.x + (v.x - u.x) * (0.0f - u.y) / (v.y - u.y);
                endpoint->point->y = 0.0f;
            } else if ((endpoint->code >> 3) & 1) {
                endpoint->point->x = u.x + (v.x - u.x) * (side_length - 1.0f - u.y) / (v.y - u.y);
                endpoint->point->y = side_length - 1.0f;
            }
            endpoint->code = region_code(side_length, *endpoint->point);
        }
    }
}

static float fpart(float x)
{
    return x - floorf(x);
}

static float fpart_complement(float x)
{
    return 1.0f - fpart(x);
}

static void plot(JkColor *draw_buffer, JkColor color, int32_t x, int32_t y, float brightness)
{
    int32_t brightness_i = (int32_t)(brightness * 255.0f);
    if (brightness_i > 255) {
        brightness_i = 255;
    }
    draw_buffer[y * DRAW_BUFFER_SIDE_LENGTH + x] = blend_alpha(color,
            draw_buffer[y * DRAW_BUFFER_SIDE_LENGTH + x],
            color_multiply(color.a, (uint8_t)brightness_i));
}

static void draw_line(Chess *chess, JkColor color, JkVector2 a, JkVector2 b)
{
    if (!clip_to_draw_region((float)(chess->square_side_length * 10), &a, &b)) {
        return;
    }

    JkColor *draw_buffer = chess->draw_buffer;

    b32 steep = jk_abs(b.y - a.y) > jk_abs(b.x - a.x);

    if (steep) {
        JK_SWAP(a.x, a.y, float);
        JK_SWAP(b.x, b.y, float);
    }
    if (a.x > b.x) {
        JK_SWAP(a, b, JkVector2);
    }

    JkVector2 delta = jk_vector_2_sub(b, a);

    float gradient;
    if (delta.x) {
        gradient = delta.y / delta.x;
    } else {
        gradient = 1.0f;
    }

    // handle first endpoint
    int32_t x_pixel_1;
    float intery;
    {
        x_pixel_1 = jk_round(a.x);
        float yend = a.y + gradient * (x_pixel_1 - a.x);
        float xcoverage = fpart_complement(a.x + 0.5f);
        int32_t y_pixel_1 = (int32_t)yend;
        if (steep) {
            plot(draw_buffer, color, y_pixel_1, x_pixel_1, fpart_complement(yend) * xcoverage);
            plot(draw_buffer, color, y_pixel_1 + 1, x_pixel_1, fpart(yend) * xcoverage);
        } else {
            plot(draw_buffer, color, x_pixel_1, y_pixel_1, fpart_complement(yend) * xcoverage);
            plot(draw_buffer, color, x_pixel_1, y_pixel_1 + 1, fpart(yend) * xcoverage);
        }
        intery = yend + gradient;
    }

    // handle second endpoint
    int32_t x_pixel_2;
    {
        x_pixel_2 = jk_round(b.x);
        float yend = b.y + gradient * (x_pixel_2 - b.x);
        float xcoverage = fpart(b.x + 0.5f);
        int32_t y_pixel_2 = (int32_t)yend;
        if (steep) {
            plot(draw_buffer, color, y_pixel_2, x_pixel_2, fpart_complement(yend) * xcoverage);
            plot(draw_buffer, color, y_pixel_2 + 1, x_pixel_2, fpart(yend) * xcoverage);
        } else {
            plot(draw_buffer, color, x_pixel_2, y_pixel_2, fpart_complement(yend) * xcoverage);
            plot(draw_buffer, color, x_pixel_2, y_pixel_2 + 1, fpart(yend) * xcoverage);
        }
    }

    if (steep) {
        for (int32_t x = x_pixel_1 + 1; x < x_pixel_2; x++) {
            plot(draw_buffer, color, (int32_t)intery, x, fpart_complement(intery));
            plot(draw_buffer, color, (int32_t)intery + 1, x, fpart(intery));
            intery += gradient;
        }
    } else {
        for (int32_t x = x_pixel_1 + 1; x < x_pixel_2; x++) {
            plot(draw_buffer, color, x, (int32_t)intery, fpart_complement(intery));
            plot(draw_buffer, color, x, (int32_t)intery + 1, fpart(intery));
            intery += gradient;
        }
    }
}

#define MENU_WIDTH 512.0f
#define BUTTON_TEXT_SCALE 0.025f
#define BUTTON_PADDING 9.0f
#define RECT_THICKNESS 1.0f

static void draw_radio_buttons(ChessAssets *assets,
        Chess *chess,
        JkShapesRenderer *renderer,
        JkVector2 *top_left,
        uint64_t choice_count,
        uint64_t choice_selected,
        JkBuffer *choice_strings,
        uint64_t first_button_id)
{
    top_left->y += 12;

    float spacing = 22;

    TextLayout base_layout = text_layout_get(assets, choice_strings[0], BUTTON_TEXT_SCALE);
    JkVector2 base_dimensions = {(MENU_WIDTH - (choice_count - 1) * spacing) / choice_count,
        base_layout.dimensions.y + 2 * (BUTTON_PADDING + RECT_THICKNESS)};

    float scale_factor = (base_dimensions.x + spacing) / base_dimensions.x;
    JkVector2 scaled_dimensions = jk_vector_2_mul(scale_factor, base_dimensions);

    JkVector2 position = *top_left;
    position.y += (scaled_dimensions.y - base_dimensions.y) / 2;
    float text_y_offset = BUTTON_PADDING + RECT_THICKNESS + base_layout.offset.y;
    float text_y = position.y + text_y_offset;
    for (int32_t choice_id = 0; choice_id < choice_count;
            choice_id++, position.x += base_dimensions.x + spacing) {
        JkBuffer text = choice_strings[choice_id];
        Button *button = chess->buttons + (first_button_id + choice_id);

        if (choice_id == choice_selected) {
            button->rect = (JkIntRect){0};

            JkVector2 scaled_position = jk_vector_2_add(position,
                    jk_vector_2_mul(0.5f, jk_vector_2_sub(base_dimensions, scaled_dimensions)));

            jk_shapes_rect_draw_outline(
                    renderer, scaled_position, scaled_dimensions, RECT_THICKNESS, color_move_prev);

            TextLayout text_layout =
                    text_layout_get(assets, text, scale_factor * BUTTON_TEXT_SCALE);
            JkVector2 cursor = {
                scaled_position.x + scale_factor * (BUTTON_PADDING + RECT_THICKNESS)
                        + text_layout.offset.x,
                scaled_position.y
                        + scale_factor * (BUTTON_PADDING + RECT_THICKNESS + base_layout.offset.y),
            };
            draw_text(renderer, text, cursor, scale_factor * BUTTON_TEXT_SCALE, color_move_prev);
        } else {
            JkColor text_color;
            JkColor outline_color;
            button->rect = jk_shapes_pixel_rect_get(renderer, position, base_dimensions);
            if (jk_int_rect_point_test(button->rect, chess->input.mouse_pos)) {
                text_color = (JkColor){255, 255, 255, 255};
                outline_color = color_light_squares;
            } else {
                text_color = color_light_squares;
                outline_color = color_light_squares;
                outline_color.a = 185;
            }

            jk_shapes_pixel_rect_draw_outline(
                    renderer, button->rect, RECT_THICKNESS, outline_color);

            TextLayout text_layout = text_layout_get(assets, text, BUTTON_TEXT_SCALE);

            JkVector2 cursor = {
                position.x + BUTTON_PADDING + RECT_THICKNESS + text_layout.offset.x, text_y};
            draw_text(renderer, text, cursor, BUTTON_TEXT_SCALE, outline_color);
        }
    }

    top_left->y += scaled_dimensions.y;
}

void render(ChessAssets *assets, Chess *chess)
{
    jk_platform_profile_frame_begin();

    JkIntVector2 pos;

    JkArenaRoot arena_root;
    JkArena arena = jk_arena_fixed_init(&arena_root, chess->render_memory);

    // Figure out which squares should be highlighted
    Team team = board_current_team_get(chess->board);
    uint8_t selected_index = board_index_get_unbounded(chess->selected_square);
    uint64_t destinations = destinations_get_by_src(chess, selected_index);
    JkIntVector2 mouse_pos = screen_to_board_pos(chess, chess->input.mouse_pos);
    uint8_t mouse_index = board_index_get_unbounded(mouse_pos);
    JkColor drop_indicator_color =
            mouse_pos.x % 2 == mouse_pos.y % 2 ? color_light_squares : color_dark_squares;
    int32_t drop_indicator_width = JK_MAX(1, chess->square_side_length / 18);
    b32 promoting = board_in_bounds(chess->promo_square);
    b32 holding_piece = JK_FLAG_GET(chess->flags, CHESS_FLAG_HOLDING_PIECE) && selected_index < 64;
    b32 ready_to_drop = holding_piece && mouse_index < 64 && ((destinations >> mouse_index) & 1);

    int64_t time_player_seconds[TEAM_COUNT];
    for (Team i = 0; i < TEAM_COUNT; i++) {
        int64_t time = chess->os_time_player[i];
        if (i == team && chess->result == RESULT_NONE && chess->turn_index) {
            time -= (int64_t)(chess->os_time - chess->os_time_turn_start);
        }
        time_player_seconds[i] = (time + chess->os_timer_frequency - 1) / chess->os_timer_frequency;
    }

    // uint64_t threatened = board_threatened_squares_get(
    //         chess->board, !((chess->board.flags >> BOARD_FLAG_CURRENT_PLAYER) & 1));

    Move move_prev = move_unpack(chess->board.move_prev);

    JkIntVector2 result_origin = {2, 3};
    JkIntVector2 result_dimensions = {4, 2};
    JkIntVector2 result_extent = jk_int_vector_2_add(result_origin, result_dimensions);

    int32_t captured_pieces[TEAM_COUNT][PIECE_TYPE_COUNT];
    memcpy(captured_pieces[0], piece_counts, sizeof(int32_t) * PIECE_TYPE_COUNT);
    memcpy(captured_pieces[1], piece_counts, sizeof(int32_t) * PIECE_TYPE_COUNT);
    for (pos.y = 0; pos.y < 8; pos.y++) {
        for (pos.x = 0; pos.x < 8; pos.x++) {
            Piece piece = board_piece_get(chess->board, pos);
            captured_pieces[piece.team][piece.type]--;
        }
    }

    // Do vector drawing
    JK_PLATFORM_PROFILE_ZONE_TIME_BEGIN(draw_vectors);

    JkShapesRenderer renderer;
    JkShapeArray shapes =
            (JkShapeArray){.count = JK_ARRAY_COUNT(assets->shapes), .items = assets->shapes};
    float square_size = 64.0f;
    float pixels_per_unit = (float)chess->square_side_length / square_size;
    jk_shapes_renderer_init(&renderer, pixels_per_unit, assets, shapes, &arena);

    JkColor square_colors[10][10];

    JkColor color_faded = color_light_squares;
    color_faded.a = 185;

    switch (chess->screen) {
    case SCREEN_GAME: {
        // Draw pieces on board and compute square colors
        for (pos.y = 0; pos.y < 10; pos.y++) {
            for (pos.x = 0; pos.x < 10; pos.x++) {
                JkIntVector2 board_pos = apply_perspective(
                        chess->perspective, jk_int_vector_2_add(pos, (JkIntVector2){-1, -1}));
                int32_t index = board_index_get_unbounded(board_pos);
                if (index < 64) {
                    Piece piece = board_piece_get(chess->board, board_pos);
                    int32_t dist_from_promo_square =
                            absolute_value(board_pos.y - chess->promo_square.y);
                    JkColor square_color = board_pos.x % 2 == board_pos.y % 2 ? color_dark_squares
                                                                              : color_light_squares;

                    if (chess->result && result_origin.x <= board_pos.x
                            && board_pos.x < result_extent.x && result_origin.y <= board_pos.y
                            && board_pos.y < result_extent.y) {
                        square_color = color_background;
                    } else {
                        if (promoting && board_pos.x == chess->promo_square.x
                                && dist_from_promo_square < JK_ARRAY_COUNT(promo_order)) {
                            square_color = color_background;
                            piece.team = team;
                            piece.type = promo_order[dist_from_promo_square];
                        } else if (index == selected_index) {
                            square_color = blend(color_selection, square_color);
                        } else if (destinations & (1llu << index)) {
                            square_color = blend(color_selection, square_color);
                        } else if ((move_prev.src || move_prev.dest)
                                && (index == move_prev.src || index == move_prev.dest)) {
                            square_color = blend_alpha(color_move_prev, square_color, 115);
                        }

                        JkColor piece_color = color_teams[piece.team];
                        if (board_index_get(board_pos) == selected_index
                                && JK_FLAG_GET(chess->flags, CHESS_FLAG_HOLDING_PIECE)) {
                            piece_color.a /= 2;
                        }

                        jk_shapes_draw(&renderer,
                                piece.type,
                                board_to_canvas_pos(chess->perspective, square_size, board_pos),
                                1.0f,
                                piece_color);
                    }

                    square_colors[pos.y][pos.x] = square_color;
                } else {
                    square_colors[pos.y][pos.x] = color_background;
                }
            }
        }

        // Draw horizontal square coordinates
        float coords_scale = 0.0192f;
        for (int32_t x = 0; x < 8; x++) {
            uint32_t shape_id = 'a' + apply_perspective(chess->perspective, (JkIntVector2){x, 0}).x
                    + CHARACTER_SHAPE_OFFSET;
            JkShape *shape = shapes.items + shape_id;
            float width = coords_scale * shape->dimensions.x;
            float x_offset = coords_scale * shape->offset.x;
            float padding_top = square_size * 0.12f;
            float padding_bottom = square_size * 0.3f;
            float cursor_x = square_size * (x + 1.5f) - (width / 2.0f) - x_offset;
            float cursor_ys[] = {
                square_size - padding_top,
                (square_size * 9.0f) + padding_bottom,
            };
            for (int32_t i = 0; i < JK_ARRAY_COUNT(cursor_ys); i++) {
                jk_shapes_draw(&renderer,
                        shape_id,
                        (JkVector2){cursor_x, cursor_ys[i]},
                        coords_scale,
                        color_faded);
            }
        }

        // Draw vertical square coordinates
        for (int32_t y = 0; y < 8; y++) {
            uint32_t shape_id = '1' + apply_perspective(chess->perspective, (JkIntVector2){0, y}).y
                    + CHARACTER_SHAPE_OFFSET;
            JkShape *shape = shapes.items + shape_id;
            JkVector2 dimensions = jk_vector_2_mul(coords_scale, shape->dimensions);
            JkVector2 offset = jk_vector_2_mul(coords_scale, shape->offset);
            float padding = square_size * 0.15f;
            float cursor_xs[] = {
                square_size - padding - dimensions.x * 0.5f - offset.x,
                (square_size * 9) + padding - dimensions.x * 0.5f - offset.x,
            };
            float cursor_y = square_size * (y + 1) + (square_size - dimensions.y) * 0.5f - offset.y;
            JkColor color = color_light_squares;
            color.a = 200;
            for (int32_t i = 0; i < JK_ARRAY_COUNT(cursor_xs); i++) {
                jk_shapes_draw(&renderer,
                        shape_id,
                        (JkVector2){cursor_xs[i], cursor_y},
                        coords_scale,
                        color_faded);
            }
        }

        // Draw timers and captured pieces
        float timer_scale = 0.025f;
        float bar_padding = 5.0f;
        float y_value[TEAM_COUNT];
        y_value[!chess->perspective] = bar_padding;
        y_value[chess->perspective] = 640.0f - 32.0f - bar_padding;
        float bar_text_y[TEAM_COUNT];
        for (Team team_index = 0; team_index < TEAM_COUNT; team_index++) {
            JkShape *zero_shape = assets->shapes + ('0' + CHARACTER_SHAPE_OFFSET);
            bar_text_y[team_index] = y_value[team_index]
                    + 0.5f * (32.0f - timer_scale * zero_shape->dimensions.y)
                    - timer_scale * zero_shape->offset.y;
        }
        {
            for (Team team_index = 0; team_index < TEAM_COUNT; team_index++) {
                // Draw timer
                uint64_t remaining = time_player_seconds[team_index];
                uint8_t digits[5];
                digits[4] = remaining % 10; // seconds
                remaining /= 10;
                digits[3] = remaining % 6; // ten seconds
                remaining /= 6;
                digits[1] = remaining % 10; // minutes
                remaining /= 10;
                digits[0] = (uint8_t)remaining; // ten minutes
                JkVector2 digit_pos = {.y = bar_text_y[team_index]};
                float raw_x = 64.0f;
                float width = 13.2f;
                for (int32_t i = 0; i < JK_ARRAY_COUNT(digits); i++) {
                    int32_t shape_index;
                    if (i == 2) { // Draw colon separator
                        shape_index = ':' + CHARACTER_SHAPE_OFFSET;
                    } else { // Draw digit
                        shape_index = '0' + digits[i] + CHARACTER_SHAPE_OFFSET;
                    }
                    JkShape *shape = assets->shapes + shape_index;
                    digit_pos.x = raw_x + 0.5f * (width - timer_scale * shape->dimensions.x)
                            - timer_scale * shape->offset.x;
                    jk_shapes_draw(
                            &renderer, shape_index, digit_pos, timer_scale, color_light_squares);
                    raw_x += width;
                }

                // Draw captured pieces
                JkVector2 draw_pos = {140.0f, y_value[!team_index]};
                for (PieceType piece_type = 1; piece_type < PIECE_TYPE_COUNT; piece_type++) {
                    JkColor color = color_teams[team_index];
                    color.a = 200;
                    if (captured_pieces[team_index][piece_type] < 3) {
                        for (int32_t i = 0; i < captured_pieces[team_index][piece_type]; i++) {
                            jk_shapes_draw(&renderer, piece_type, draw_pos, 0.5f, color);
                            draw_pos.x += 32.0f;
                        }
                    } else {
                        jk_shapes_draw(&renderer, piece_type, draw_pos, 0.5f, color);

                        char characters[2];
                        characters[0] = 'x';
                        characters[1] = '0' + (uint8_t)captured_pieces[team_index][piece_type];
                        JkBuffer text = {
                            .size = JK_ARRAY_COUNT(characters),
                            .data = (uint8_t *)characters,
                        };
                        TextLayout layout = text_layout_get(assets, text, coords_scale);
                        JkVector2 cursor_pos;
                        cursor_pos.x = draw_pos.x + 32.0f;
                        cursor_pos.y = draw_pos.y + 0.5f * (32.0f - layout.dimensions.y);
                        cursor_pos = jk_vector_2_add(cursor_pos, layout.offset);

                        draw_text(&renderer, text, cursor_pos, coords_scale, color_faded);

                        draw_pos.x += 64.0f;
                    }
                }
            }
        }

        // Draw menu button
        {
            JkBuffer text = JKS("Menu");
            TextLayout layout = text_layout_get(assets, text, coords_scale);

            JkVector2 dimensions = jk_vector_2_add(layout.dimensions,
                    (JkVector2){2 * (BUTTON_PADDING + RECT_THICKNESS),
                        2 * (BUTTON_PADDING + RECT_THICKNESS)});

            JkVector2 top_left = {.x = 64.0f * 9.0f - dimensions.x};
            JkVector2 cursor = {
                top_left.x + BUTTON_PADDING + RECT_THICKNESS + layout.offset.x,
                bar_text_y[!chess->perspective],
            };
            top_left.y = cursor.y - layout.offset.y - (BUTTON_PADDING + RECT_THICKNESS);

            JkColor text_color;
            JkColor outline_color;
            chess->buttons[BUTTON_MENU_OPEN].rect =
                    jk_shapes_pixel_rect_get(&renderer, top_left, dimensions);
            if (jk_int_rect_point_test(
                        chess->buttons[BUTTON_MENU_OPEN].rect, chess->input.mouse_pos)) {
                text_color = (JkColor){255, 255, 255, 255};
                outline_color = color_light_squares;
            } else {
                text_color = color_light_squares;
                outline_color = color_faded;
            }

            jk_shapes_pixel_rect_draw_outline(&renderer,
                    chess->buttons[BUTTON_MENU_OPEN].rect,
                    RECT_THICKNESS,
                    outline_color);
            draw_text(&renderer, text, cursor, coords_scale, text_color);
        }

        // If the game is over, display the result
        if (chess->result) {
            float result_scale = 0.05f;
            JkVector2 result_origin_f = {192.0f, 256.0f};
            JkVector2 result_dimensions_f = {256.0f, 128.0f};

            if (chess->result == RESULT_STALEMATE) {
                JkBuffer text = JKS("Stalemate");

                TextLayout layout = text_layout_get(assets, text, result_scale);
                JkVector2 cursor_pos = jk_vector_2_mul(
                        0.5f, jk_vector_2_sub(result_dimensions_f, layout.dimensions));
                cursor_pos = jk_vector_2_add(cursor_pos, result_origin_f);
                cursor_pos = jk_vector_2_add(cursor_pos, layout.offset);

                draw_text(&renderer, text, cursor_pos, result_scale, color_light_squares);
            } else {
                JkBuffer victor = team ? JKS("White won") : JKS("Black won");
                JkBuffer condition =
                        chess->result == RESULT_CHECKMATE ? JKS("by checkmate") : JKS("on time");
                float condition_scale = 0.03f;
                float padding = 8.0f;

                TextLayout victor_layout = text_layout_get(assets, victor, result_scale);
                TextLayout condition_layout = text_layout_get(assets, condition, condition_scale);

                float y_start = result_origin_f.y
                        + 0.5f
                                * (result_dimensions_f.y
                                        - (victor_layout.dimensions.y + padding
                                                + condition_layout.dimensions.y));

                JkVector2 victor_cursor;
                victor_cursor.x = result_origin_f.x
                        + 0.5f * (result_dimensions_f.x - victor_layout.dimensions.x)
                        + victor_layout.offset.x;
                victor_cursor.y = y_start + victor_layout.offset.y;

                JkVector2 condition_cursor;
                condition_cursor.x = result_origin_f.x
                        + 0.5f * (result_dimensions_f.x - condition_layout.dimensions.x)
                        + condition_layout.offset.x;
                condition_cursor.y =
                        y_start + padding + victor_layout.dimensions.y + condition_layout.offset.y;

                draw_text(&renderer, victor, victor_cursor, result_scale, color_light_squares);

                draw_text(&renderer,
                        condition,
                        condition_cursor,
                        condition_scale,
                        color_light_squares);
            }
        }
    } break;

    case SCREEN_MENU: {
        // Fill all the chess board squares with the background color
        for (pos.y = 0; pos.y < 10; pos.y++) {
            for (pos.x = 0; pos.x < 10; pos.x++) {
                square_colors[pos.y][pos.x] = color_background;
            }
        }

        JkVector2 top_left = {(640 - MENU_WIDTH) / 2, 98.88f};

        {
            JkBuffer text = JKS("Close menu");
            TextLayout text_layout = text_layout_get(assets, text, BUTTON_TEXT_SCALE);

            JkVector2 dimensions = {
                MENU_WIDTH,
                text_layout.dimensions.y + 2 * (BUTTON_PADDING + RECT_THICKNESS),
            };

            JkVector2 cursor = jk_vector_2_add(jk_vector_2_add(top_left,
                                                       (JkVector2){BUTTON_PADDING + RECT_THICKNESS,
                                                           BUTTON_PADDING + RECT_THICKNESS}),
                    text_layout.offset);

            JkColor text_color;
            JkColor outline_color;
            chess->buttons[BUTTON_MENU_CLOSE].rect =
                    jk_shapes_pixel_rect_get(&renderer, top_left, dimensions);
            if (jk_int_rect_point_test(
                        chess->buttons[BUTTON_MENU_CLOSE].rect, chess->input.mouse_pos)) {
                text_color = (JkColor){255, 255, 255, 255};
                outline_color = color_light_squares;
            } else {
                text_color = color_light_squares;
                outline_color = color_faded;
            }

            jk_shapes_pixel_rect_draw_outline(&renderer,
                    chess->buttons[BUTTON_MENU_CLOSE].rect,
                    RECT_THICKNESS,
                    outline_color);
            draw_text(&renderer, text, cursor, BUTTON_TEXT_SCALE, text_color);

            top_left.y += dimensions.y;
        }

        {
            float heading_scale = 0.03;

            JkBuffer text = JKS("Configure new game");
            TextLayout text_layout = text_layout_get(assets, text, heading_scale);

            top_left.y += 32;

            JkVector2 cursor = jk_vector_2_add(top_left, text_layout.offset);
            draw_text(&renderer, text, cursor, heading_scale, color_light_squares);

            top_left.y += text_layout.dimensions.y;
        }

        {
            JkBuffer text = JKS("Play as");
            TextLayout text_layout = text_layout_get(assets, text, BUTTON_TEXT_SCALE);

            top_left.y += 20;

            JkVector2 cursor = jk_vector_2_add(top_left, text_layout.offset);
            draw_text(&renderer, text, cursor, BUTTON_TEXT_SCALE, color_light_squares);

            top_left.y += text_layout.dimensions.y;
        }

        draw_radio_buttons(assets,
                chess,
                &renderer,
                &top_left,
                TEAM_CHOICE_COUNT,
                chess->settings.team_choice,
                team_choice_strings,
                BUTTON_WHITE);

        {
            JkBuffer text = JKS("Opponent controlled by");
            TextLayout text_layout = text_layout_get(assets, text, BUTTON_TEXT_SCALE);

            top_left.y += 20;

            JkVector2 cursor = jk_vector_2_add(top_left, text_layout.offset);
            draw_text(&renderer, text, cursor, BUTTON_TEXT_SCALE, color_light_squares);

            top_left.y += text_layout.dimensions.y;
        }

        draw_radio_buttons(assets,
                chess,
                &renderer,
                &top_left,
                PLAYER_TYPE_COUNT,
                chess->settings.opponent_type,
                opponent_type_strings,
                BUTTON_YOU);

        {
            JkBuffer text = JKS("Timer");
            TextLayout text_layout = text_layout_get(assets, text, BUTTON_TEXT_SCALE);

            top_left.y += 20;

            JkVector2 cursor = jk_vector_2_add(top_left, text_layout.offset);
            draw_text(&renderer, text, cursor, BUTTON_TEXT_SCALE, color_light_squares);

            top_left.y += text_layout.dimensions.y;
        }

        draw_radio_buttons(assets,
                chess,
                &renderer,
                &top_left,
                TIMER_COUNT,
                chess->settings.timer,
                timer_strings,
                BUTTON_1_MIN);

        {
            top_left.y += 20;

            JkBuffer text = JKS("Start new game");
            TextLayout text_layout = text_layout_get(assets, text, BUTTON_TEXT_SCALE);

            JkVector2 dimensions = {
                MENU_WIDTH,
                text_layout.dimensions.y + 2 * (BUTTON_PADDING + RECT_THICKNESS),
            };

            JkVector2 cursor = jk_vector_2_add(jk_vector_2_add(top_left,
                                                       (JkVector2){BUTTON_PADDING + RECT_THICKNESS,
                                                           BUTTON_PADDING + RECT_THICKNESS}),
                    text_layout.offset);

            JkColor text_color;
            JkColor outline_color;
            chess->buttons[BUTTON_START_GAME].rect =
                    jk_shapes_pixel_rect_get(&renderer, top_left, dimensions);
            if (jk_int_rect_point_test(
                        chess->buttons[BUTTON_START_GAME].rect, chess->input.mouse_pos)) {
                text_color = (JkColor){255, 255, 255, 255};
                outline_color = color_light_squares;
            } else {
                text_color = color_light_squares;
                outline_color = color_faded;
            }

            jk_shapes_pixel_rect_draw_outline(&renderer,
                    chess->buttons[BUTTON_START_GAME].rect,
                    RECT_THICKNESS,
                    outline_color);
            draw_text(&renderer, text, cursor, BUTTON_TEXT_SCALE, text_color);

            top_left.y += dimensions.y;
        }
    } break;

    case SCREEN_COUNT: {
        chess->debug_print("Invalid chess->screen value\n");
    } break;
    }

    JkShapesDrawCommandArray draw_commands = jk_shapes_draw_commands_get(&renderer);
    JK_PLATFORM_PROFILE_ZONE_END(draw_vectors);

    JK_PLATFORM_PROFILE_ZONE_TIME_BEGIN(fill_draw_buffer);
    int32_t cs = 0;
    int32_t ce = 0;
    JkIntVector2 pos_in_square;
    JkIntVector2 square_pos;
    JkIntVector2 mouse_square_pos =
            jk_int_vector_2_div(chess->square_side_length, chess->input.mouse_pos);
    for (pos.y = 0, pos_in_square.y = 0, square_pos.y = 0; pos.y < chess->square_side_length * 10;
            pos.y++) {
        while (ce < draw_commands.count && draw_commands.items[ce].position.y <= pos.y) {
            ce++;
        }
        while (cs < draw_commands.count
                && !(pos.y < draw_commands.items[cs].position.y
                                + draw_commands.items[cs].dimensions.y)) {
            cs++;
        }

        for (pos.x = 0, pos_in_square.x = 0, square_pos.x = 0;
                pos.x < chess->square_side_length * 10;
                pos.x++) {
            JkColor color = square_colors[square_pos.y][square_pos.x];

            // Shows a gap in the selection to indicate which square the held piece will drop on
            if (jk_int_vector_2_equal(square_pos, mouse_square_pos) && ready_to_drop) {
                int32_t x_dist_from_edge = pos_in_square.x < chess->square_side_length / 2
                        ? pos_in_square.x
                        : chess->square_side_length - 1 - pos_in_square.x;
                int32_t y_dist_from_edge = pos_in_square.y < chess->square_side_length / 2
                        ? pos_in_square.y
                        : chess->square_side_length - 1 - pos_in_square.y;
                if ((drop_indicator_width <= x_dist_from_edge
                            && drop_indicator_width <= y_dist_from_edge)
                        && (x_dist_from_edge < 2 * drop_indicator_width
                                || y_dist_from_edge < 2 * drop_indicator_width)) {
                    color = drop_indicator_color;
                }
            }

            for (int32_t i = cs; i < ce; i++) {
                JkShapesDrawCommand *command = draw_commands.items + i;
                JkIntVector2 pos_in_rect = jk_int_vector_2_sub(pos, command->position);
                if (0 <= pos_in_rect.x && pos_in_rect.x < command->dimensions.x
                        && pos_in_rect.y < command->dimensions.y) {
                    uint8_t alpha;
                    if (command->alpha_map) {
                        uint8_t bitmap_alpha =
                                command->alpha_map[pos_in_rect.y * command->dimensions.x
                                        + pos_in_rect.x];
                        alpha = color_multiply(command->color.a, bitmap_alpha);
                    } else {
                        alpha = command->color.a;
                    }
                    color = blend_alpha(command->color, color, alpha);
                    break;
                }
            }

            color.a = 255;
            chess->draw_buffer[pos.y * DRAW_BUFFER_SIDE_LENGTH + pos.x] = color;

            if (++pos_in_square.x >= chess->square_side_length) {
                pos_in_square.x = 0;
                square_pos.x++;
            }
        }

        if (++pos_in_square.y >= chess->square_side_length) {
            pos_in_square.y = 0;
            square_pos.y++;
        }
    }
    JK_PLATFORM_PROFILE_ZONE_END(fill_draw_buffer);

    if (holding_piece) {
        Piece piece = board_piece_get_index(chess->board, selected_index);
        JkShapesBitmap bitmap = jk_shapes_bitmap_get(&renderer, piece.type, 1.0f);
        if (bitmap.data) {
            JkIntVector2 held_piece_offset = jk_int_vector_2_sub(chess->input.mouse_pos,
                    (JkIntVector2){chess->square_side_length / 2, chess->square_side_length / 2});
            for (pos.y = 0; pos.y < bitmap.dimensions.x; pos.y++) {
                for (pos.x = 0; pos.x < bitmap.dimensions.y; pos.x++) {
                    JkIntVector2 screen_pos = jk_int_vector_2_add(pos, held_piece_offset);
                    if (screen_in_bounds(chess->square_side_length, screen_pos)) {
                        int32_t index = screen_pos.y * DRAW_BUFFER_SIDE_LENGTH + screen_pos.x;
                        JkColor color_piece = color_teams[piece.team];
                        JkColor color_bg = chess->draw_buffer[index];
                        uint8_t alpha = bitmap.data[pos.y * bitmap.dimensions.x + pos.x];
                        chess->draw_buffer[index] = blend_alpha(color_piece, color_bg, alpha);
                    }
                }
            }
        }
    }

    int64_t frames_since_last_move = chess->time - chess->time_move_prev;
    if (frames_since_last_move < 43 && chess->piece_prev_type) {
        JkColor piece_color = color_teams[team];
        JkShapesBitmap bitmap = jk_shapes_bitmap_get(&renderer, chess->piece_prev_type, 1.0f);
        JkIntVector2 src = board_index_to_vector_2(move_prev.src);
        JkIntVector2 dest = board_index_to_vector_2(move_prev.dest);
        JkVector2 src_pos = board_to_canvas_pos(chess->perspective, square_size, src);
        JkVector2 canvas_pos = board_to_canvas_pos(chess->perspective, square_size, dest);
        JkVector2 origin_direction = jk_vector_2_normalized(jk_vector_2_sub(src_pos, canvas_pos));
        JkVector2 blast_center =
                jk_vector_2_add(jk_vector_2_add(canvas_pos, (JkVector2){32.0f, 32.0f}),
                        jk_vector_2_mul(64.0f, origin_direction));

        float speed = 30.0f;
        float deceleration = 0.32f;
        float distance = speed * frames_since_last_move
                - deceleration * (frames_since_last_move * frames_since_last_move);
        int64_t prev_frames_since_last_move = JK_MAX(0, frames_since_last_move - 2);
        float prev_distance = speed * prev_frames_since_last_move
                - deceleration * (prev_frames_since_last_move * prev_frames_since_last_move);

        int32_t skip = 1 + (chess->square_side_length * 2 / 100);
        for (pos.y = 0; pos.y < bitmap.dimensions.x; pos.y += skip) {
            for (pos.x = 0; pos.x < bitmap.dimensions.y; pos.x += skip) {
                JkVector2 offset = {(float)pos.x / pixels_per_unit, (float)pos.y / pixels_per_unit};
                JkVector2 pixel_pos = jk_vector_2_add(canvas_pos, offset);
                JkVector2 direction =
                        jk_vector_2_normalized(jk_vector_2_sub(pixel_pos, blast_center));
                JkVector2 delta = jk_vector_2_mul(distance, direction);
                JkVector2 prev_delta = jk_vector_2_mul(prev_distance, direction);
                JkVector2 pixel_dest = jk_vector_2_add(pixel_pos, delta);
                JkVector2 prev_dest = jk_vector_2_add(pixel_pos, prev_delta);

                piece_color.a = bitmap.data[pos.y * bitmap.dimensions.x + pos.x];
                if (piece_color.a) {
                    draw_line(chess,
                            piece_color,
                            jk_vector_2_mul(pixels_per_unit, prev_dest),
                            jk_vector_2_mul(pixels_per_unit, pixel_dest));
                }
            }
        }
    }

    jk_platform_profile_frame_end();
}

void profile_print(void (*print)(char *))
{
    print("\n");
    jk_platform_profile_print_custom(custom_profile_print, (void *)print);
}
