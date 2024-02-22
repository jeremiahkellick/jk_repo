#include "arena.h"

#ifdef _WIN32
#include <windows.h>
#endif

#define JK_PAGE_ROUND_UP(size) (((size) + JK_PAGE_SIZE - 1) & ~(JK_PAGE_SIZE - 1))

JkArenaInitResult jk_arena_init(JkArena *arena, size_t virtual_size)
{
    arena->virtual_size = virtual_size;
    arena->size = JK_PAGE_SIZE;
    arena->pos = 0;

#ifdef _WIN32
    arena->address = VirtualAlloc(NULL, virtual_size, MEM_RESERVE, PAGE_NOACCESS);
    if (!arena->address) {
        return JK_ARENA_INIT_FAILURE;
    }
    void *first_page = VirtualAlloc(arena->address, JK_PAGE_SIZE, MEM_COMMIT, PAGE_READWRITE);
    if (!first_page) {
        return JK_ARENA_INIT_FAILURE;
    }
#endif

    return JK_ARENA_INIT_SUCCESS;
}

void jk_arena_terminate(JkArena *arena)
{
#ifdef _WIN32
    VirtualFree(arena->address, 0, MEM_RELEASE);
#endif
}

void *jk_arena_push(JkArena *arena, size_t size)
{
    size_t new_pos = arena->pos + size;
    if (new_pos > arena->virtual_size) {
        return NULL;
    }
    if (new_pos > arena->size) {
        size_t expansion_size = JK_PAGE_ROUND_UP(new_pos - arena->size);

#ifdef _WIN32
        if (!VirtualAlloc(
                    arena->address + arena->size, expansion_size, MEM_COMMIT, PAGE_READWRITE)) {
            return NULL;
        }
#endif

        arena->size += expansion_size;
    }
    void *address = arena->address + arena->pos;
    arena->pos = new_pos;
    return address;
}

JkArenaPopResult jk_arena_pop(JkArena *arena, size_t size)
{
    if (size > arena->pos) {
        return JK_ARENA_POP_TRIED_TO_POP_MORE_THAN_POS;
    }
    arena->pos -= size;
    return JK_ARENA_POP_SUCCESS;
}

void jk_arena_clear(JkArena *arena)
{
    arena->pos = 0;
}
