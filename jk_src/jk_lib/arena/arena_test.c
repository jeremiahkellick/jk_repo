#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <jk_gen/single_translation_unit.h>

// #jk_build dependencies_begin
#include <jk_src/jk_lib/arena/arena.h>
// #jk_build dependencies_end

char string1[] = "memcpy_test1\n";
char string2[] = "memcpy_test2\n";

#include <unistd.h>

int main(void)
{
    size_t page_size = jk_page_size();

    JkArena arena;
    JkArenaInitResult result = jk_arena_init(&arena, page_size * 3);
    if (result == JK_ARENA_INIT_FAILURE) {
        perror("jk_arena_init");
        return 1;
    }

    char *push1 = jk_arena_push(&arena, sizeof(string1));
    if (push1 == NULL) {
        perror("jk_arena_push");
        return 1;
    }
    memcpy(push1, string1, sizeof(string1));
    printf("%s", push1);

    char *push2 = jk_arena_push(&arena, page_size * 2);
    if (push2 == NULL) {
        perror("jk_arena_push");
        return 1;
    }
    char *print = &push2[page_size * 2 - sizeof(string2)];
    memcpy(print, string2, sizeof(string2));
    printf("%s", print);

    size_t size_before = arena.size;
    char *push3 = jk_arena_push(&arena, page_size * 2);
    size_t size_after = arena.size;
    assert(push3 == NULL);
    assert(size_before == size_after);

    assert(jk_arena_pop(&arena, page_size * 2) == JK_ARENA_POP_SUCCESS);
    assert(jk_arena_pop(&arena, page_size * 2) == JK_ARENA_POP_TRIED_TO_POP_MORE_THAN_POS);

    jk_arena_terminate(&arena);

    return 0;
}
