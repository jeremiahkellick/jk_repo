#include <stdio.h>
#include <string.h>

// #jk_build single_translation_unit

// #jk_build dependencies_begin
#include <jk_src/jk_lib/jk_lib.h>
#include <jk_src/jk_lib/platform/platform.h>
// #jk_build dependencies_end

static char string1[] = "memcpy_test1\n";
static char string2[] = "memcpy_test2\n";

static int32_t unicode_codepoints[] = {
    0x0024,
    0x00A3,
    0x0418,
    0x0939,
    0x20AC,
    0xD55C,
    0x10348,
};

static void print_buffer(JkBuffer buffer)
{
    for (size_t i = 0; i < buffer.size; i++) {
        putc(buffer.data[i], stdout);
    }
}

static void print_unicode(uint32_t codepoint32)
{
    char null_terminated_codepoint[5] = {0};

    JkUtf8Codepoint codepoint = jk_utf8_codepoint_encode(codepoint32);
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
    if (!jk_platform_arena_init(&arena, page_size * 3)) {
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
    JK_ASSERT(push3 == NULL);
    JK_ASSERT(size_before == size_after);

    JK_ASSERT(jk_platform_arena_pop(&arena, page_size * 2));
    JK_ASSERT(!jk_platform_arena_pop(&arena, page_size * 2));

    jk_platform_arena_terminate(&arena);
    // ---- Arena end ----------------------------------------------------------

    // ---- Buffer begin -------------------------------------------------------
    printf("\nBuffer\n");

    char *null_terminated = "Null terminated string\n";
    JkBuffer buffer = jk_buffer_from_null_terminated(null_terminated);

    print_buffer(JKS("Hello, world!\n"));
    print_buffer(buffer);

    uint64_t pos = 5;
    printf("Character in bounds: %d\n", jk_buffer_character_next(buffer, &pos));
    pos = 9001;
    printf("Character out of bounds: %d\n", jk_buffer_character_next(buffer, &pos));

    // ---- Buffer end ---------------------------------------------------------

    // ---- UTF-8 begin --------------------------------------------------------
    printf("\nUTF-8\n");

    for (int32_t i = 0; i < JK_ARRAY_COUNT(unicode_codepoints); i++) {
        print_unicode(unicode_codepoints[i]);

        JkUtf8Codepoint utf8 = jk_utf8_codepoint_encode(unicode_codepoints[i]);
        JK_ASSERT(jk_utf8_codepoint_decode(utf8) == unicode_codepoints[i]);
    }
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

    JK_ASSERT(!jk_is_power_of_two(0));
    JK_ASSERT(jk_round_up_to_power_of_2(0) == 0);
    JK_ASSERT(jk_round_down_to_power_of_2(0) == 0);

    JK_ASSERT(jk_is_power_of_two(1));
    JK_ASSERT(jk_round_up_to_power_of_2(1) == 1);
    JK_ASSERT(jk_round_down_to_power_of_2(1) == 1);

    JK_ASSERT(jk_is_power_of_two(16));
    JK_ASSERT(jk_round_up_to_power_of_2(16) == 16);
    JK_ASSERT(jk_round_down_to_power_of_2(16) == 16);

    JK_ASSERT(!jk_is_power_of_two(18));
    JK_ASSERT(jk_round_up_to_power_of_2(18) == 32);
    JK_ASSERT(jk_round_down_to_power_of_2(18) == 16);

    return 0;
}
