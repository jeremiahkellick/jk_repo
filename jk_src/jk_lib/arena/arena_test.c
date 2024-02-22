#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "arena.c"

#define BIG_PUSH_SIZE (JK_PAGE_SIZE * 2)

char string[] = "memcpy_test\n";

int main(int argc, char **argv)
{
    JkArena arena;
    jk_arena_init(&arena, JK_PAGE_SIZE * 3);

    char *push1 = jk_arena_push(&arena, sizeof(string));
    memcpy(push1, string, sizeof(string));
    printf("%s", push1);

    char *push2 = jk_arena_push(&arena, BIG_PUSH_SIZE);
    char *print = &push2[BIG_PUSH_SIZE - sizeof(string)];
    memcpy(print, string, sizeof(string));
    printf("%s", print);

    size_t size_before = arena.size;
    char *push3 = jk_arena_push(&arena, BIG_PUSH_SIZE);
    size_t size_after = arena.size;
    assert(push3 == NULL);
    assert(size_before == size_after);

    assert(jk_arena_pop(&arena, BIG_PUSH_SIZE) == JK_ARENA_POP_SUCCESS);
    assert(jk_arena_pop(&arena, BIG_PUSH_SIZE) == JK_ARENA_POP_TRIED_TO_POP_MORE_THAN_POS);

    jk_arena_terminate(&arena);
}
