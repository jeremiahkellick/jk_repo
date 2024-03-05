#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <jk_gen/single_translation_unit.h>

// #jk_build dependencies_begin
#include <jk_src/jk_lib/arena/arena.h>
// #jk_build dependencies_end

#define BIG_PUSH_SIZE (JK_PAGE_SIZE * 2)

char string1[] = "memcpy_test1\n";
char string2[] = "memcpy_test2\n";

int main(int argc, char **argv)
{
    JkArena arena;
    jk_arena_init(&arena, JK_PAGE_SIZE * 3);

    char *push1 = jk_arena_push(&arena, sizeof(string1));
    memcpy(push1, string1, sizeof(string1));
    printf("%s", push1);

    char *push2 = jk_arena_push(&arena, BIG_PUSH_SIZE);
    char *print = &push2[BIG_PUSH_SIZE - sizeof(string2)];
    memcpy(print, string2, sizeof(string2));
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
