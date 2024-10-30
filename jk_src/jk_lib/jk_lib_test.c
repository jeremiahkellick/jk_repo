#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <jk_gen/single_translation_unit.h>

// #jk_build dependencies_begin
#include <jk_src/jk_lib/jk_lib.h>
#include <jk_src/jk_lib/platform/platform.h>
// #jk_build dependencies_end

static char string1[] = "memcpy_test1\n";
static char string2[] = "memcpy_test2\n";

static void print_buffer(JkBuffer buffer)
{
    for (size_t i = 0; i < buffer.size; i++) {
        putc(buffer.data[i], stdout);
    }
}

static void print_unicode(uint32_t codepoint32)
{
    JkUtf8Codepoint codepoint = {0};
    char null_terminated_codepoint[5] = {0};

    jk_utf8_codepoint_encode(codepoint32, &codepoint);
    memcpy(null_terminated_codepoint, codepoint.b, 4);

    printf("U+%04X: %s\n", codepoint32, null_terminated_codepoint);
}

#define LENGTH 10

static int numbers[LENGTH] = {40, 34, 49, 28, 4, 42, 30, 27, 23, 12};
static int sorted_numbers[LENGTH] = {4, 12, 23, 27, 28, 30, 34, 40, 42, 49};

static char *strings[LENGTH] = {
    "Pear",
    "Mango",
    "Strawberry",
    "Watermelon",
    "Orange",
    "Pineapple",
    "Banana",
    "Blueberry",
    "Kiwi",
    "Apple",
};

static char *sorted_strings[LENGTH] = {
    "Apple",
    "Banana",
    "Blueberry",
    "Kiwi",
    "Mango",
    "Orange",
    "Pear",
    "Pineapple",
    "Strawberry",
    "Watermelon",
};

static b32 string_arrays_are_equal(char **a, char **b, int length)
{
    for (int i = 0; i < length; i++) {
        if (strcmp(a[i], b[i]) != 0) {
            return 0;
        }
    }
    return 1;
}

static void print_int_array(int array[])
{
    printf("{");
    for (int i = 0; i < LENGTH; i++) {
        printf("%d%s", array[i], i == 9 ? "}" : ", ");
    }
}

static void print_string_arrays_side_by_side(char **a, char **b, int length, int indent, int width)
{
    for (int i = 0; i < length; i++) {
        // Indent
        for (int j = 0; j < indent; j++) {
            putchar(' ');
        }

        printf("%s", a[i]);

        // Pad to width with spaces
        int padding = width - (int)strlen(a[i]);
        for (int j = 0; j < padding; j++) {
            putchar(' ');
        }

        printf("%s\n", b[i]);
    }
}

int main(void)
{
    jk_platform_console_utf8_enable();

    // ---- Arena begin --------------------------------------------------------
    printf("Arena\n");

    size_t page_size = jk_platform_page_size();
    JkPlatformArena arena;
    JkPlatformArenaInitResult result = jk_platform_arena_init(&arena, page_size * 3);
    if (result == JK_PLATFORM_ARENA_INIT_FAILURE) {
        perror("jk_platform_arena_init");
        return 1;
    }

    char *push1 = jk_platform_arena_push(&arena, sizeof(string1));
    if (push1 == NULL) {
        perror("jk_platform_arena_push");
        return 1;
    }
    memcpy(push1, string1, sizeof(string1));
    printf("%s", push1);

    char *push2 = jk_platform_arena_push(&arena, page_size * 2);
    if (push2 == NULL) {
        perror("jk_platform_arena_push");
        return 1;
    }
    char *print = &push2[page_size * 2 - sizeof(string2)];
    memcpy(print, string2, sizeof(string2));
    printf("%s", print);

    size_t size_before = arena.size;
    char *push3 = jk_platform_arena_push(&arena, page_size * 2);
    size_t size_after = arena.size;
    assert(push3 == NULL);
    assert(size_before == size_after);

    assert(jk_platform_arena_pop(&arena, page_size * 2) == JK_PLATFORM_ARENA_POP_SUCCESS);
    assert(jk_platform_arena_pop(&arena, page_size * 2)
            == JK_PLATFORM_ARENA_POP_TRIED_TO_POP_MORE_THAN_POS);

    jk_platform_arena_terminate(&arena);
    // ---- Arena end ----------------------------------------------------------

    // ---- Buffer begin -------------------------------------------------------
    printf("\nBuffer\n");

    char *null_terminated = "Null terminated string\n";
    JkBuffer buffer = jk_buffer_from_null_terminated(null_terminated);

    print_buffer(JKS("Hello, world!\n"));
    print_buffer(buffer);

    JkBufferPointer pointer = {.buffer = buffer, .index = 5};
    printf("Character in bounds: %d\n", jk_buffer_character_next(&pointer));
    pointer.index = 9001;
    printf("Character out of bounds: %d\n", jk_buffer_character_next(&pointer));

    // ---- Buffer end ---------------------------------------------------------

    // ---- UTF-8 begin --------------------------------------------------------
    printf("\nUTF-8\n");

    print_unicode(0x0024);
    print_unicode(0x00A3);
    print_unicode(0x0418);
    print_unicode(0x0939);
    print_unicode(0x20AC);
    print_unicode(0xD55C);
    print_unicode(0x10348);
    // ---- UTF-8 end ----------------------------------------------------------

    // ---- Quicksort begin ----------------------------------------------------
    printf("\nQuicksort\n");

    // Test int sorting
    printf("jk_quicksort_ints()\n    ");
    jk_quicksort_ints(numbers, LENGTH);
    if (memcmp(numbers, sorted_numbers, LENGTH) == 0) {
        printf("SUCCESS\n");
    } else {
        printf("FAIL:\n        expected ");
        print_int_array(sorted_numbers);
        printf("\n        actual   ");
        print_int_array(numbers);
        printf("\n");
    }

    // Test string sorting
    printf("jk_quicksort_strings()\n");
    jk_quicksort_strings(strings, LENGTH);
    if (string_arrays_are_equal(strings, sorted_strings, LENGTH)) {
        printf("    SUCCESS\n");
    } else {
        printf("    FAIL:\n        expected    actual\n\n");
        print_string_arrays_side_by_side(sorted_strings, strings, LENGTH, 8, 12);
    }

    // ---- Quicksort end ------------------------------------------------------

    return 0;
}
