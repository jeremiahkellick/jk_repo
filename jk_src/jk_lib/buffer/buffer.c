#include <stdio.h>
#include <string.h>

#include "buffer.h"

JK_PUBLIC JkBuffer jk_buffer_from_null_terminated(char *string)
{
    JkBuffer buffer = {.size = strlen(string), .data = (uint8_t *)string};
    return buffer;
}

JK_PUBLIC int jk_buffer_character_peek(JkBufferPointer *pointer)
{
    return pointer->index < pointer->buffer.size ? pointer->buffer.data[pointer->index] : EOF;
}

JK_PUBLIC int jk_buffer_character_next(JkBufferPointer *pointer)
{
    int c = jk_buffer_character_peek(pointer);
    pointer->index++;
    return c;
}
