#ifndef JK_BUFFER_H
#define JK_BUFFER_H

#include <stddef.h>
#include <stdint.h>

#include <jk_src/jk_lib/arena/arena.h>

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

JK_PUBLIC JkBuffer jk_file_read_full(char *file_name, JkArena *storage);

#endif
