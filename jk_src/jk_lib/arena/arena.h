#ifndef JK_ARENA_H
#define JK_ARENA_H

#include <stddef.h>

JK_PUBLIC size_t jk_page_size(void);

JK_PUBLIC size_t jk_page_size_round_up(size_t n);

JK_PUBLIC size_t jk_page_size_round_down(size_t n);

typedef struct JkArena {
    size_t virtual_size;
    size_t size;
    size_t pos;
    char *address;
} JkArena;

typedef enum JkArenaInitResult {
    JK_ARENA_INIT_SUCCESS,
    JK_ARENA_INIT_FAILURE,
} JkArenaInitResult;

JK_PUBLIC JkArenaInitResult jk_arena_init(JkArena *arena, size_t virtual_size);

JK_PUBLIC void jk_arena_terminate(JkArena *arena);

JK_PUBLIC void *jk_arena_push(JkArena *arena, size_t size);

JK_PUBLIC void *jk_arena_push_zero(JkArena *arena, size_t size);

typedef enum JkArenaPopResult {
    JK_ARENA_POP_SUCCESS,
    JK_ARENA_POP_TRIED_TO_POP_MORE_THAN_POS,
} JkArenaPopResult;

JK_PUBLIC JkArenaPopResult jk_arena_pop(JkArena *arena, size_t size);

#endif
