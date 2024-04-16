#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "buffer.h"

// #jk_build dependencies_begin
#include <jk_src/jk_lib/arena/arena.h>
#include <jk_src/jk_lib/profile/profile.h>
#include <jk_src/jk_lib/utils.h>
// #jk_build dependencies_end

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

JK_PUBLIC JkBuffer jk_file_read_full(char *file_name, JkArena *storage)
{
    JK_PROFILE_ZONE_TIME_BEGIN(jk_file_read_full);

    FILE *file = fopen(file_name, "rb");
    if (!file) {
        JK_PROFILE_ZONE_END(jk_file_read_full);
        fprintf(stderr,
                "jk_file_read_full: Failed to open file '%s': %s\n",
                file_name,
                strerror(errno));
        exit(1);
    }

    JkBuffer buffer = {.size = jk_file_size(file_name)};
    buffer.data = jk_arena_push(storage, buffer.size);
    if (!buffer.data) {
        JK_PROFILE_ZONE_END(jk_file_read_full);
        fprintf(stderr, "jk_file_read_full: Failed to allocate memory for file '%s'\n", file_name);
        exit(1);
    }

    JK_PROFILE_ZONE_BANDWIDTH_BEGIN(fread, buffer.size);
    if (fread(buffer.data, buffer.size, 1, file) != 1) {
        JK_PROFILE_ZONE_END(fread);
        JK_PROFILE_ZONE_END(jk_file_read_full);
        fprintf(stderr, "jk_file_read_full: fread failed\n");
        exit(1);
    }
    JK_PROFILE_ZONE_END(fread);

    JK_PROFILE_ZONE_END(jk_file_read_full);
    return buffer;
}
