#include "chess.h"

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

typedef enum PieceType {
    NONE,
    KING,
    QUEEN,
    ROOK,
    BISHOP,
    KNIGHT,
    PAWN,
} PieceType;

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
    return (Piece){.type = byte & 0x7, .team = (byte >> 3) & 1}; // Mask to lower 4 bits
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

static b32 square_available(Board board, Team team, JkIntVector2 square)
{
    if (board_in_bounds(square)) {
        Piece piece = board_piece_get(board, square);
        return piece.type == NONE || piece.team != team;
    } else {
        return 0;
    }
}

static void moves_append(MoveArray *moves, JkIntVector2 src, JkIntVector2 dest)
{
    moves->data[moves->count++] = (Move){
        .src = board_index_get(src),
        .dest = board_index_get(dest),
    };
}

static void moves_append_in_direction_until_stopped(
        MoveArray *moves, Board board, Team current_team, JkIntVector2 src, JkIntVector2 direction)
{
    JkIntVector2 dest = src;
    for (;;) {
        dest = jk_int_vector_2_add(dest, direction);
        if (board_in_bounds(dest)) {
            Piece piece = board_piece_get(board, dest);
            if (piece.type == NONE) {
                moves_append(moves, src, dest);
            } else {
                if (piece.team != current_team) {
                    moves_append(moves, src, dest);
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

static uint64_t board_threatened_squares_get(Board board, Team team)
{
    uint64_t result = 0;

    JkIntVector2 src;
    for (src.y = 0; src.y < 8; src.y++) {
        for (src.x = 0; src.x < 8; src.x++) {
            Piece piece = board_piece_get(board, src);
            if (piece.team == team) {
                switch (piece.type) {
                case NONE: {
                } break;

                case KING: {
                    for (int32_t i = 0; i < JK_ARRAY_COUNT(all_directions); i++) {
                        JkIntVector2 dest = jk_int_vector_2_add(src, all_directions[i]);
                        if (board_in_bounds(dest)) {
                            result |= 1llu << board_index_get(dest);
                        }
                    }
                } break;

                case QUEEN: {
                    for (int32_t i = 0; i < JK_ARRAY_COUNT(all_directions); i++) {
                        result |= threats_in_direction_until_stopped(board, src, all_directions[i]);
                    }
                } break;

                case ROOK: {
                    for (int32_t i = 0; i < JK_ARRAY_COUNT(straights); i++) {
                        result |= threats_in_direction_until_stopped(board, src, straights[i]);
                    }
                } break;

                case BISHOP: {
                    for (int32_t i = 0; i < JK_ARRAY_COUNT(diagonals); i++) {
                        result |= threats_in_direction_until_stopped(board, src, diagonals[i]);
                    }
                } break;

                case KNIGHT: {
                    for (int32_t i = 0; i < JK_ARRAY_COUNT(knight_moves); i++) {
                        JkIntVector2 dest = jk_int_vector_2_add(src, knight_moves[i]);
                        if (board_in_bounds(dest)) {
                            result |= 1llu << board_index_get(dest);
                        }
                    }
                } break;

                case PAWN: {
                    // Attacks
                    for (uint8_t i = 0; i < 2; i++) {
                        JkIntVector2 dest = jk_int_vector_2_add(src, pawn_attacks[team][i]);
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

static void board_move_perform(Board *board, Move move)
{
    JkIntVector2 src = board_index_to_vector_2(move.src);
    JkIntVector2 dest = board_index_to_vector_2(move.dest);
    Team team = (board->flags >> BOARD_FLAG_INDEX_CURRENT_PLAYER) & 1;

    // Move a piece
    Piece piece = board_piece_get_index(*board, move.src);

    // En-passant handling
    if (piece.type == PAWN && src.x != dest.x
            && board_piece_get_index(*board, move.dest).type == NONE) {
        int32_t y_delta = (board->flags & BOARD_FLAG_CURRENT_PLAYER) ? 1 : -1;
        board_piece_set(board, jk_int_vector_2_add(dest, (JkIntVector2){0, y_delta}), (Piece){0});
    }

    // Castling handling
    int32_t delta_x = dest.x - src.x;
    if (piece.type == ROOK && src.y == (team ? 7 : 0) && (src.x == 0 || src.x == 7)) {
        board->flags |= board_flag_rook_moved_get(team, src.x == 7);
    }
    if (piece.type == KING) {
        board->flags |= board_flag_king_moved_get(team);

        if (absolute_value(delta_x) == 2) {
            int32_t rook_from_x, rook_to_x;
            if (delta_x > 0) {
                rook_from_x = 7;
                rook_to_x = 5;
            } else {
                rook_from_x = 0;
                rook_to_x = 3;
            }
            board_piece_set(board, (JkIntVector2){rook_from_x, src.y}, (Piece){0});
            board_piece_set(board,
                    (JkIntVector2){rook_to_x, src.y},
                    (Piece){.type = ROOK, .team = piece.team});
        }
    }

    board_piece_set(board, src, (Piece){0});
    board_piece_set(board, dest, piece);

    board->move_prev = move;
    board->flags ^= BOARD_FLAG_CURRENT_PLAYER;
}

// Returns the number of moves written to the buffer
static void moves_get(MoveArray *moves, Board board, Team current_team)
{
    moves->count = 0;

    JkIntVector2 src;
    for (src.y = 0; src.y < 8; src.y++) {
        for (src.x = 0; src.x < 8; src.x++) {
            Piece piece = board_piece_get(board, src);
            if (piece.team == current_team) {
                switch (piece.type) {
                case NONE: {
                } break;

                case KING: {
                    for (int32_t i = 0; i < JK_ARRAY_COUNT(all_directions); i++) {
                        JkIntVector2 dest = jk_int_vector_2_add(src, all_directions[i]);
                        if (square_available(board, current_team, dest)) {
                            moves_append(moves, src, dest);
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
                                            jk_int_vector_2_add(
                                                    src, (JkIntVector2){2 * x_step, 0}));
                                }
                            }
                        }
                    }
                } break;

                case QUEEN: {
                    for (int32_t i = 0; i < JK_ARRAY_COUNT(all_directions); i++) {
                        moves_append_in_direction_until_stopped(
                                moves, board, current_team, src, all_directions[i]);
                    }
                } break;

                case ROOK: {
                    for (int32_t i = 0; i < JK_ARRAY_COUNT(straights); i++) {
                        moves_append_in_direction_until_stopped(
                                moves, board, current_team, src, straights[i]);
                    }
                } break;

                case BISHOP: {
                    for (int32_t i = 0; i < JK_ARRAY_COUNT(diagonals); i++) {
                        moves_append_in_direction_until_stopped(
                                moves, board, current_team, src, diagonals[i]);
                    }
                } break;

                case KNIGHT: {
                    for (int32_t i = 0; i < JK_ARRAY_COUNT(knight_moves); i++) {
                        JkIntVector2 dest = jk_int_vector_2_add(src, knight_moves[i]);
                        if (square_available(board, current_team, dest)) {
                            moves_append(moves, src, dest);
                        }
                    }
                } break;

                case PAWN: {
                    // Move
                    {
                        JkIntVector2 move = jk_int_vector_2_add(src, pawn_moves[current_team]);
                        if (board_in_bounds(move) && board_piece_get(board, move).type == NONE) {
                            moves_append(moves, src, move);

                            // Extended move
                            if ((current_team == WHITE && src.y == 1)
                                    || (current_team == BLACK && src.y == 6)) {
                                JkIntVector2 extended_move =
                                        jk_int_vector_2_add(src, pawn_extended_moves[current_team]);
                                if (board_in_bounds(extended_move)
                                        && board_piece_get(board, extended_move).type == NONE) {
                                    moves_append(moves, src, extended_move);
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
                                moves_append(moves, src, dest);
                            }
                        }
                    }
                } break;
                }
            }
        }
    }

    // En-passant
    if (absolute_value((int32_t)board.move_prev.src - (int32_t)board.move_prev.dest) == 16) {
        Piece move_prev_piece = board_piece_get_index(board, board.move_prev.dest);
        if (move_prev_piece.type == PAWN) {
            JkIntVector2 pos = board_index_to_vector_2(board.move_prev.dest);
            JkIntVector2 directions[] = {{-1, 0}, {1, 0}};
            for (uint8_t i = 0; i < JK_ARRAY_COUNT(directions); i++) {
                JkIntVector2 en_passant_src = jk_int_vector_2_add(pos, directions[i]);
                if (board_in_bounds(en_passant_src)) {
                    Piece piece = board_piece_get(board, en_passant_src);
                    if (piece.type == PAWN && piece.team == current_team) {
                        int32_t y_delta = board.move_prev.src < board.move_prev.dest ? -1 : 1;
                        JkIntVector2 en_passant_dest =
                                jk_int_vector_2_add(pos, (JkIntVector2){0, y_delta});
                        moves_append(moves, en_passant_src, en_passant_dest);
                    }
                }
            }
        }
    }
}

static void moves_remove_if_leaves_king_in_check(MoveArray *moves, Board board, Team current_team)
{
    uint8_t move_index = 0;
    while (move_index < moves->count) {
        Board hypothetical = board;
        board_move_perform(&hypothetical, moves->data[move_index]);

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

// ---- AI begin ---------------------------------------------------------------

#define MAX_DEPTH 5

typedef struct MoveNode {
    Move move;
    struct MoveNode *next_sibling;
    struct MoveNode *first_child;
} MoveNode;

typedef struct MovePool {
    uint64_t max_count;
    uint64_t free_count;
    uint64_t allocated_count;
    MoveNode **free_pointers;
    MoveNode *allocated;
} MovePool;

void move_pool_init(MovePool *pool, JkBuffer memory)
{
    uint64_t bytes_for_free_pointers =
            memory.size * sizeof(MoveNode *) / (sizeof(MoveNode) + sizeof(MoveNode *));

    pool->max_count = bytes_for_free_pointers / sizeof(MoveNode *);
    pool->free_count = 0;
    pool->allocated_count = 0;
    pool->free_pointers = (MoveNode **)memory.data;
    pool->allocated = (MoveNode *)(memory.data + pool->max_count * sizeof(MoveNode *));
}

MoveNode *move_pool_alloc(MovePool *pool)
{
    MoveNode *result;
    if (pool->free_count) {
        result = pool->free_pointers[--pool->free_count];
    } else {
        JK_ASSERT(pool->allocated_count < pool->max_count);
        result = pool->allocated + pool->allocated_count++;
    }
    *result = (MoveNode){0};
    return result;
}

void move_pool_free(MovePool *pool, MoveNode *node)
{
    JK_DEBUG_ASSERT(pool->free_count < pool->max_count);
    pool->free_pointers[pool->free_count++] = node;
}

MovePool move_pool;
MoveArray moves_tmp;

void explore_tree(Board board, MoveNode *node, uint64_t depth, Team current_team)
{
    if (!(depth < MAX_DEPTH)) {
        return;
    }

    moves_get(&moves_tmp, board, current_team);
    MoveNode *current_child = 0;
    for (int i = 0; i < moves_tmp.count; i++) {
        if (current_child) {
            MoveNode *next_child = move_pool_alloc(&move_pool);
            current_child->next_sibling = next_child;
            current_child = next_child;
        } else {
            current_child = move_pool_alloc(&move_pool);
            node->first_child = current_child;
        }
        current_child->move = moves_tmp.data[i];
    }

    for (MoveNode *child = node->first_child; child; child = child->next_sibling) {
        Board child_board = board;
        board_move_perform(&child_board, child->move);
        explore_tree(child_board, child, depth + 1, !current_team);
    }

    return;
}

// ---- AI end -----------------------------------------------------------------

static void audio_write(Audio *audio)
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
        if (chess->moves.data[i].src == src) {
            result |= 1llu << chess->moves.data[i].dest;
        }
    }
    return result;
}

char string_buf[512];
void print_move_tree(void (*debug_print)(char *), MoveNode *node, uint64_t depth)
{
    JkIntVector2 dest = board_index_to_vector_2(node->move.dest);
    JkIntVector2 src = board_index_to_vector_2(node->move.src);
    snprintf(string_buf,
            sizeof(string_buf),
            "%c%c <- %c%c\n",
            'a' + dest.x,
            '1' + dest.y,
            'a' + src.x,
            '1' + src.y);
    for (int i = 0; i < depth; i++) {
        debug_print("  ");
    }
    debug_print(string_buf);

    for (MoveNode *child = node->first_child; child; child = child->next_sibling) {
        print_move_tree(debug_print, child, depth + 1);
    }
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
        chess->result = 0;
        memcpy(&chess->board, &starting_state, sizeof(chess->board));
        moves_get(&chess->moves, chess->board, WHITE);
        moves_remove_if_leaves_king_in_check(&chess->moves, chess->board, WHITE);

        JK_DEBUG_ASSERT(board_flag_king_moved_get(WHITE) == BOARD_FLAG_WHITE_KING_MOVED);
        JK_DEBUG_ASSERT(board_flag_king_moved_get(BLACK) == BOARD_FLAG_BLACK_KING_MOVED);
        JK_DEBUG_ASSERT(board_flag_rook_moved_get(WHITE, 0) == BOARD_FLAG_A1_ROOK_MOVED);
        JK_DEBUG_ASSERT(board_flag_rook_moved_get(WHITE, 1) == BOARD_FLAG_H1_ROOK_MOVED);
        JK_DEBUG_ASSERT(board_flag_rook_moved_get(BLACK, 0) == BOARD_FLAG_A8_ROOK_MOVED);
        JK_DEBUG_ASSERT(board_flag_rook_moved_get(BLACK, 1) == BOARD_FLAG_H8_ROOK_MOVED);

        move_pool_init(&move_pool, chess->move_pool_memory);
        MoveNode root = {0};

        uint64_t time_start = chess->cpu_time();
        explore_tree(chess->board, &root, 0, WHITE);
        snprintf(string_buf,
                sizeof(string_buf),
                "%.0fms\n",
                (double)(chess->cpu_time() - time_start) / (double)chess->cpu_frequency * 1000.0);
        chess->debug_print(string_buf);

        /*
        MoveNode *node = root.first_child;
        for (int i = 0; i < 2; i++) {
            print_move_tree(chess->debug_print, node, 0);
            node = node->next_sibling;
        }
        */
    }

    if (button_pressed(chess, INPUT_FLAG_CANCEL)) {
        chess->selected_square = (JkIntVector2){-1, -1};
    }

    uint64_t available_destinations =
            destinations_get_by_src(chess, board_index_get_unbounded(chess->selected_square));
    JkIntVector2 mouse_pos = screen_to_board_pos(&chess->bitmap, chess->input.mouse_pos);
    uint8_t mouse_index = board_index_get_unbounded(mouse_pos);
    b32 mouse_on_destination = mouse_index < 64 && (available_destinations & (1llu << mouse_index));
    uint8_t piece_drop_index = UINT8_MAX;

    if (button_pressed(chess, INPUT_FLAG_CONFIRM)) {
        chess->flags |= FLAG_HOLDING_PIECE;

        if (mouse_on_destination) {
            piece_drop_index = mouse_index;
        } else {
            chess->selected_square = (JkIntVector2){-1, -1};
            for (uint8_t i = 0; i < chess->moves.count; i++) {
                if (chess->moves.data[i].src == mouse_index) {
                    chess->selected_square = mouse_pos;
                }
            }
        }
    }

    if (!(chess->input.flags & INPUT_FLAG_CONFIRM)) {
        if (chess->flags & FLAG_HOLDING_PIECE) {
            chess->flags &= ~FLAG_HOLDING_PIECE;

            if (mouse_on_destination) {
                piece_drop_index = mouse_index;
            }
        }
    }

    if (piece_drop_index < 64) {
        board_move_perform(&chess->board,
                (Move){.src = board_index_get(chess->selected_square),
                    .dest = (uint8_t)piece_drop_index});

        chess->selected_square = (JkIntVector2){-1, -1};
        Team current_team = (chess->board.flags >> BOARD_FLAG_INDEX_CURRENT_PLAYER) & 1l;
        moves_get(&chess->moves, chess->board, current_team);
        moves_remove_if_leaves_king_in_check(&chess->moves, chess->board, current_team);

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

            chess->result = (threatened >> king_index) & 1 ? RESULT_CHECKMATE : RESULT_STALEMATE;
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
    JK_DEBUG_ASSERT(!(
            pos.x >= 0 && pos.x < SQUARE_SIDE_LENGTH && pos.y >= 0 && pos.y < SQUARE_SIDE_LENGTH));

    int32_t y_offset = (piece_type - 1) * 112;
    return atlas[(pos.y + y_offset) * ATLAS_WIDTH + pos.x];
}

RENDER_FUNCTION(render)
{
    // Figure out which squares should be highlighted
    uint8_t selected_index = board_index_get_unbounded(chess->selected_square);
    uint64_t destinations = destinations_get_by_src(chess, selected_index);
    uint8_t mouse_index =
            board_index_get_unbounded(screen_to_board_pos(&chess->bitmap, chess->input.mouse_pos));

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
                Color background = light ? color_light_squares : color_dark_squares;
                if (index == selected_index) {
                    color = blend(color_selection, background);
                } else if (destinations & (1llu << index)) {
                    color = blend(color_selection, background);
                    if (index == mouse_index) {
                        int32_t x_dist_from_edge = square_pos.x < SQUARE_SIDE_LENGTH / 2
                                ? square_pos.x
                                : SQUARE_SIDE_LENGTH - 1 - square_pos.x;
                        int32_t y_dist_from_edge = square_pos.y < SQUARE_SIDE_LENGTH / 2
                                ? square_pos.y
                                : SQUARE_SIDE_LENGTH - 1 - square_pos.y;
                        if ((x_dist_from_edge >= 4 && y_dist_from_edge >= 4)
                                && (x_dist_from_edge < 8 || y_dist_from_edge < 8)) {
                            color = background;
                        }
                    }
                } else {
                    color = background;
                }

                Piece piece = board_piece_get(chess->board, board_pos);
                if (piece.type != NONE) {
                    Color color_piece = piece.team ? color_black_pieces : color_white_pieces;
                    uint8_t alpha = atlas_piece_get_alpha(chess->atlas, piece.type, square_pos);
                    if (index == selected_index && (chess->flags & FLAG_HOLDING_PIECE)) {
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
}
