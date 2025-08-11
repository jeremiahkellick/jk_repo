#include <math.h>
#include <stdio.h>
#include <string.h>

// #jk_build single_translation_unit

// #jk_build dependencies_begin
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
    for (uint64_t i = 0; i < buffer.size; i++) {
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

static void expect_string(JkBuffer expected, JkBuffer actual)
{
    if (jk_buffer_compare(expected, actual) != 0) {
        printf("FAIL: expected \"%.*s\" but got \"%.*s\"\n",
                (int)expected.size,
                expected.data,
                (int)actual.size,
                actual.data);
    }
}

typedef union FloatUnion {
    float f;
    uint32_t bits;
} FloatUnion;

FloatUnion infinity = {.bits = 0x7f800000};
FloatUnion some_nan = {.bits = 0x7fE9605C};

int main(void)
{
    jk_platform_console_utf8_enable();

    // ---- Arena begin --------------------------------------------------------
    printf("Arena\n");

    uint64_t page_size = jk_platform_page_size();
    JkPlatformArenaVirtualRoot arena_root;
    JkArena arena = jk_platform_arena_virtual_init(&arena_root, page_size * 3);
    JK_ASSERT(jk_arena_valid(&arena));

    {
        JkArena child = jk_arena_child_get(&arena);

        uint8_t *push1 = jk_arena_push(&child, sizeof(string1));
        JK_ASSERT(push1);
        memcpy(push1, string1, sizeof(string1));
        printf("%s", push1);

        uint8_t *push2 = jk_arena_push(&child, page_size * 2);
        JK_ASSERT(push2);
        uint8_t *print = &push2[page_size * 2 - sizeof(string2)];
        memcpy(print, string2, sizeof(string2));
        printf("%s", print);

        uint64_t size_before = child.root->memory.size;
        uint64_t pos_before = child.pos;
        uint8_t *push3 = jk_arena_push(&child, page_size * 2);
        uint64_t size_after = child.root->memory.size;
        uint64_t pos_after = child.pos;
        JK_ASSERT(push3 == NULL);
        JK_ASSERT(size_before == size_after);
        JK_ASSERT(pos_before == pos_after);
    }

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

    JK_ASSERT(jk_string_find(JKS("Hello, world!"), JKS(" wor")) == 6);
    JK_ASSERT(jk_string_find(JKS("Hello, world!"), JKS(" wort")) == -1);
    JK_ASSERT(jk_string_find(JKS("brief"), JKS("long winded")) == -1);
    JK_ASSERT(jk_string_find(JKS("start end"), JKS("start")) == 0);
    JK_ASSERT(jk_string_find(JKS("start end"), JKS("end")) == 6);

    printf("\n");

    expect_string(JKS("8699171359359057110"), jk_int_to_string(&arena, 8699171359359057110ll));
    expect_string(JKS("-12"), jk_int_to_string(&arena, -12));
    expect_string(JKS("1"), jk_int_to_string(&arena, 1));
    expect_string(JKS("0"), jk_int_to_string(&arena, 0));

    expect_string(
            JKS("16210279753379821762"), jk_unsigned_to_string(&arena, 16210279753379821762llu));
    expect_string(JKS("1"), jk_unsigned_to_string(&arena, 1));
    expect_string(JKS("0"), jk_unsigned_to_string(&arena, 0));

    expect_string(JKS("c5d530ba1de82861"),
            jk_unsigned_to_hexadecimal_string(&arena, 0xc5d530ba1de82861llu, 0));
    expect_string(JKS("0001"), jk_unsigned_to_hexadecimal_string(&arena, 1, 4));
    expect_string(JKS("0"), jk_unsigned_to_hexadecimal_string(&arena, 0, 0));

    expect_string(JKS("1101101010010110001100101010110110111111110101011001011110011011"),
            jk_unsigned_to_binary_string(&arena, 0xda9632adbfd5979b, 0));
    expect_string(JKS("0001"), jk_unsigned_to_binary_string(&arena, 1, 4));
    expect_string(JKS("0"), jk_unsigned_to_binary_string(&arena, 0, 0));

    expect_string(JKS("5 + -3 = 2"),
            JK_FORMAT(&arena, jkfu(5), jkfn(" + "), jkfi(-3), jkfn(" = "), jkfi(2)));
    expect_string(JKS("0x00ff"), JK_FORMAT(&arena, jkfn("0x"), jkfh(0xff, 4)));
    expect_string(
            JKS("Hello, sailor!"), JK_FORMAT(&arena, jkfs(JKS("Hello, ")), jkfs(JKS("sailor!"))));

    // ---- Buffer end ---------------------------------------------------------

    // ---- Math begin ---------------------------------------------------------

    printf("\nsome_nan: %f\n", (double)some_nan.f);
    printf("infinity: %f\n", (double)infinity.f);

    JK_ASSERT(jk_ceil_f32(5.2f) == ceilf(5.2f));
    JK_ASSERT(jk_ceil_f32(-5.2f) == ceilf(-5.2f));
    JK_ASSERT(jk_ceil_f32(0.0f) == ceilf(0.0f));
    JK_ASSERT(jk_ceil_f32(-0.0f) == ceilf(-0.0f));
    JK_ASSERT(jk_ceil_f32(1.0f) == ceilf(1.0f));
    JK_ASSERT(jk_ceil_f32(-1.0f) == ceilf(-1.0f));
    JK_ASSERT(jk_ceil_f32(infinity.f) == ceilf(infinity.f));
    JK_ASSERT(jk_ceil_f32(-infinity.f) == ceilf(-infinity.f));

    FloatUnion my_nan_ceil = {.f = jk_ceil_f32(some_nan.f)};
    FloatUnion reference_nan_ceil = {.f = ceilf(some_nan.f)};
    JK_ASSERT(my_nan_ceil.bits == reference_nan_ceil.bits);

    FloatUnion my_nan_ceil_negative = {.f = jk_ceil_f32(-some_nan.f)};
    FloatUnion reference_nan_ceil_negative = {.f = ceilf(-some_nan.f)};
    JK_ASSERT(my_nan_ceil_negative.bits == reference_nan_ceil_negative.bits);

    JkRandomGeneratorU64 generator = jk_random_generator_new_u64(3523520312864767571);
    for (uint64_t i = 0; i < 10000; i++) {
        FloatUnion value = {.bits = jk_random_u64(&generator)};
        double reference = ceilf(value.f);
        if (isnan(reference)) {
            JK_ASSERT(isnan(jk_ceil_f32(value.f)));
        } else {
            JK_ASSERT(jk_ceil_f32(value.f) == reference);
        }
    }

    // ---- Math end -----------------------------------------------------------

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

    jk_platform_arena_virtual_release(&arena_root);

    return 0;
}
