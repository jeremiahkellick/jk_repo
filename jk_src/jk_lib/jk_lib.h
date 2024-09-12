#ifndef JK_LIB_H
#define JK_LIB_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

// ---- Arena begin ------------------------------------------------------------

typedef struct JkArena {
    size_t virtual_size;
    size_t size;
    size_t pos;
    uint8_t *address;
} JkArena;

typedef enum JkArenaInitResult {
    JK_ARENA_INIT_SUCCESS,
    JK_ARENA_INIT_FAILURE,
} JkArenaInitResult;

JK_PUBLIC JkArenaInitResult jk_arena_init(JkArena *arena, size_t virtual_size);

JK_PUBLIC void jk_arena_terminate(JkArena *arena);

JK_PUBLIC void *jk_arena_push(JkArena *arena, size_t size);

JK_PUBLIC void *jk_arena_push_zero(JkArena *arena, size_t size);

typedef enum JkArenaPopResult {
    JK_ARENA_POP_SUCCESS,
    JK_ARENA_POP_TRIED_TO_POP_MORE_THAN_POS,
} JkArenaPopResult;

JK_PUBLIC JkArenaPopResult jk_arena_pop(JkArena *arena, size_t size);

// ---- Arena end --------------------------------------------------------------

// ---- Buffer begin -----------------------------------------------------------

typedef struct JkBuffer {
    size_t size;
    uint8_t *data;
} JkBuffer;

typedef struct JkBufferPointer {
    JkBuffer buffer;
    size_t index;
} JkBufferPointer;

#define JK_STRING(string_literal) \
    ((JkBuffer){sizeof(string_literal) - 1, (uint8_t *)string_literal})

#define JKS JK_STRING

JK_PUBLIC JkBuffer jk_buffer_from_null_terminated(char *string);

JK_PUBLIC int jk_buffer_character_peek(JkBufferPointer *pointer);

JK_PUBLIC int jk_buffer_character_next(JkBufferPointer *pointer);

// ---- Buffer end -------------------------------------------------------------

// ---- UTF-8 begin ------------------------------------------------------------

typedef struct JkUtf8Codepoint {
    uint8_t b[4];
} JkUtf8Codepoint;

JK_PUBLIC void jk_utf8_codepoint_encode(uint32_t codepoint32, JkUtf8Codepoint *codepoint);

JK_PUBLIC bool jk_utf8_byte_is_continuation(char byte);

typedef enum JkUtf8CodepointGetResult {
    JK_UTF8_CODEPOINT_GET_SUCCESS,
    JK_UTF8_CODEPOINT_GET_UNEXPECTED_BYTE,
    JK_UTF8_CODEPOINT_GET_EOF,
} JkUtf8CodepointGetResult;

JK_PUBLIC JkUtf8CodepointGetResult jk_utf8_codepoint_get(
        JkBufferPointer *cursor, JkUtf8Codepoint *codepoint);

// ---- UTF-8 end --------------------------------------------------------------

// ---- Command line arguments parsing begin -----------------------------------

typedef struct JkOption {
    /**
     * Character used as the short-option flag. The null character means there is no short form of
     * this option. An option must have some way to refer to it. If this is the null character,
     * long_name must not be null.
     */
    char flag;

    /**
     * The long name of this option. NULL means there is no long name for this option. An option
     * must have some way to refer to it. If this is NULL, flag must not be the null character.
     */
    char *long_name;

    /** Name of the argument for this option. NULL if this option does not accept an argument. */
    char *arg_name;

    /** Description of this option used to print help text */
    char *description;
} JkOption;

typedef union JkOptionResult {
    bool present;
    char *arg;
} JkOptionResult;

typedef struct JkOptionsParseResult {
    /** Pointer to the first operand (first non-option argument) */
    char **operands;
    size_t operand_count;
    bool usage_error;
} JkOptionsParseResult;

JK_PUBLIC void jk_options_parse(int argc,
        char **argv,
        JkOption *options_in,
        JkOptionResult *options_out,
        size_t option_count,
        JkOptionsParseResult *result);

JK_PUBLIC void jk_options_print_help(FILE *file, JkOption *options, int option_count);

int jk_parse_positive_integer(char *string);

// ---- Command line arguments parsing end -------------------------------------

// ---- Quicksort begin --------------------------------------------------------

JK_PUBLIC void jk_quicksort(void *array,
        size_t element_count,
        size_t element_size,
        void *tmp,
        int (*compare)(void *a, void *b));

JK_PUBLIC void jk_quicksort_ints(int *array, int length);

JK_PUBLIC void jk_quicksort_strings(char **array, int length);

// ---- Quicksort end ----------------------------------------------------------

#define JK_ARRAY_COUNT(array) (sizeof(array) / sizeof((array)[0]))

#define JK_DATA_GET(pointer, index, type) (*(type *)((uint8_t *)(pointer) + (index) * sizeof(type)))

JK_PUBLIC JkBuffer jk_file_read_full(char *file_name, JkArena *storage);

JK_PUBLIC uint32_t jk_hash_uint32(uint32_t x);

JK_PUBLIC bool jk_is_power_of_two(size_t x);

JK_PUBLIC size_t jk_page_size_round_up(size_t n);

JK_PUBLIC size_t jk_page_size_round_down(size_t n);

JK_PUBLIC void jk_print_bytes_uint64(FILE *file, char *format, uint64_t byte_count);

JK_PUBLIC void jk_print_bytes_double(FILE *file, char *format, double byte_count);

#endif
