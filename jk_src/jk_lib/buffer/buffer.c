#ifndef JK_BUFFER_C
#define JK_BUFFER_C

#include <stdio.h>
#include <string.h>

#include "buffer.h"

JkBuffer jk_buffer_from_null_terminated(char *string)
{
    JkBuffer buffer = {.size = strlen(string), .data = (uint8_t *)string};
    return buffer;
}

int jk_buffer_character_get(JkBuffer buffer, size_t position)
{
    return position < buffer.size ? buffer.data[position] : EOF;
}

#endif
