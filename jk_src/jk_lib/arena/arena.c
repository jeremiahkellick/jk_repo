#include <string.h>

#include "arena.h"

// #jk_build dependencies_begin
#include <jk_src/jk_lib/platform/platform.h>
// #jk_build dependencies_end

JK_PUBLIC size_t jk_page_size_round_up(size_t n)
{
    size_t page_size = jk_platform_page_size();
    return (n + page_size - 1) & ~(page_size - 1);
}

JK_PUBLIC size_t jk_page_size_round_down(size_t n)
{
    size_t page_size = jk_platform_page_size();
    return n & ~(page_size - 1);
}

JK_PUBLIC JkArenaInitResult jk_arena_init(JkArena *arena, size_t virtual_size)
{
    size_t page_size = jk_platform_page_size();

    arena->virtual_size = virtual_size;
    arena->size = page_size;
    arena->pos = 0;

    arena->address = jk_platform_memory_reserve(virtual_size);
    if (!arena->address) {
        return JK_ARENA_INIT_FAILURE;
    }
    if (!jk_platform_memory_commit(arena->address, page_size)) {
        return JK_ARENA_INIT_FAILURE;
    }

    return JK_ARENA_INIT_SUCCESS;
}

JK_PUBLIC void jk_arena_terminate(JkArena *arena)
{
    jk_platform_memory_free(arena->address, arena->virtual_size);
}

JK_PUBLIC void *jk_arena_push(JkArena *arena, size_t size)
{
    size_t new_pos = arena->pos + size;
    if (new_pos > arena->virtual_size) {
        return NULL;
    }
    if (new_pos > arena->size) {
        size_t expansion_size = jk_page_size_round_up(new_pos - arena->size);
        if (!jk_platform_memory_commit(arena->address + arena->size, expansion_size)) {
            return NULL;
        }
        arena->size += expansion_size;
    }
    void *address = arena->address + arena->pos;
    arena->pos = new_pos;
    return address;
}

JK_PUBLIC void *jk_arena_push_zero(JkArena *arena, size_t size)
{
    void *pointer = jk_arena_push(arena, size);
    memset(pointer, 0, size);
    return pointer;
}

JK_PUBLIC JkArenaPopResult jk_arena_pop(JkArena *arena, size_t size)
{
    if (size > arena->pos) {
        return JK_ARENA_POP_TRIED_TO_POP_MORE_THAN_POS;
    }
    arena->pos -= size;
    return JK_ARENA_POP_SUCCESS;
}

JK_PUBLIC void jk_arena_clear(JkArena *arena)
{
    arena->pos = 0;
}
