#ifndef JK_BUFFER_H
#define JK_BUFFER_H

#include <stddef.h>
#include <stdint.h>

typedef struct JkBuffer {
    size_t size;
    uint8_t *data;
} JkBuffer;

#define JK_STRING(string_literal) \
    ((JkBuffer){sizeof(string_literal) - 1, (uint8_t *)string_literal})

#define JKS JK_STRING

JkBuffer jk_buffer_from_null_terminated(char *string);

int jk_buffer_character_get(JkBuffer buffer, size_t position);

#endif
