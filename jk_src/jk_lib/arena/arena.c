#include "arena.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

static size_t jk_page_size_internal = 0;

JK_PUBLIC size_t jk_page_size(void)
{
#ifdef _WIN32
    return 4096;
#else
    if (jk_page_size_internal == 0) {
        jk_page_size_internal = getpagesize();
    }
    return jk_page_size_internal;
#endif
}

JK_PUBLIC size_t jk_page_size_round_up(size_t n)
{
    size_t page_size = jk_page_size();
    return (n + page_size - 1) & ~(page_size - 1);
}

JK_PUBLIC size_t jk_page_size_round_down(size_t n)
{
    size_t page_size = jk_page_size();
    return n & ~(page_size - 1);
}

JK_PUBLIC JkArenaInitResult jk_arena_init(JkArena *arena, size_t virtual_size)
{
    size_t page_size = jk_page_size();

    arena->virtual_size = virtual_size;
    arena->size = page_size;
    arena->pos = 0;

#ifdef _WIN32
    arena->address = VirtualAlloc(NULL, virtual_size, MEM_RESERVE, PAGE_NOACCESS);
    if (!arena->address) {
        return JK_ARENA_INIT_FAILURE;
    }
    if (!VirtualAlloc(arena->address, page_size, MEM_COMMIT, PAGE_READWRITE)) {
        return JK_ARENA_INIT_FAILURE;
    }
#else
    arena->address = mmap(NULL, virtual_size, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (arena->address == MAP_FAILED) {
        return JK_ARENA_INIT_FAILURE;
    }
    if (mprotect(arena->address, page_size, PROT_READ | PROT_WRITE) == -1) {
        return JK_ARENA_INIT_FAILURE;
    }
#endif

    return JK_ARENA_INIT_SUCCESS;
}

JK_PUBLIC void jk_arena_terminate(JkArena *arena)
{
#ifdef _WIN32
    VirtualFree(arena->address, 0, MEM_RELEASE);
#else
    munmap(arena->address, arena->virtual_size);
#endif
}

JK_PUBLIC void *jk_arena_push(JkArena *arena, size_t size)
{
    size_t new_pos = arena->pos + size;
    if (new_pos > arena->virtual_size) {
        return NULL;
    }
    if (new_pos > arena->size) {
        size_t expansion_size = jk_page_size_round_up(new_pos - arena->size);

#ifdef _WIN32
        if (!VirtualAlloc(
                    arena->address + arena->size, expansion_size, MEM_COMMIT, PAGE_READWRITE)) {
            return NULL;
        }
#else
        if (mprotect(arena->address + arena->size, expansion_size, PROT_READ | PROT_WRITE) == -1) {
            return NULL;
        }
#endif

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
