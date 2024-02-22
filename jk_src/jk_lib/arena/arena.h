#ifndef JK_ARENA_H
#define JK_ARENA_H

#include <stddef.h>

#define JK_PAGE_SIZE 4096

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

JkArenaInitResult jk_arena_init(JkArena *arena, size_t virtual_size);

void jk_arena_terminate(JkArena *arena);

void *jk_arena_push(JkArena *arena, size_t size);

typedef enum JkArenaPopResult {
    JK_ARENA_POP_SUCCESS,
    JK_ARENA_POP_TRIED_TO_POP_MORE_THAN_POS,
} JkArenaPopResult;

JkArenaPopResult jk_arena_pop(JkArena *arena, size_t size);

#endif
