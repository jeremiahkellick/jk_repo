#include <stdio.h>
#include <sys/stat.h>

// #jk_build dependencies_begin
#include <jk_src/jk_lib/arena/arena.h>
#include <jk_src/jk_lib/buffer/buffer.h>
// #jk_build dependencies_end

#include "utils.h"

#ifdef _WIN32

#include <windows.h>

typedef struct __stat64 StatStruct;
#define stat _stat64

#else

typedef struct stat StatStruct;

#endif

/**
 * @brief Returns a hash for the given 32 bit value
 *
 * From https://github.com/skeeto/hash-prospector
 */
JK_PUBLIC uint32_t jk_hash_uint32(uint32_t x)
{
    x ^= x >> 16;
    x *= 0x21f0aaad;
    x ^= x >> 15;
    x *= 0xd35a2d97;
    x ^= x >> 15;
    return x;
}

JK_PUBLIC bool jk_is_power_of_two(size_t x)
{
    return (x & (x - 1)) == 0;
}

JK_PUBLIC JkBuffer jk_file_read_full(char *file_name, JkArena *storage)
{
    FILE *file = fopen(file_name, "rb");
    if (!file) {
        fprintf(stderr,
                "jk_file_read_full: Failed to open file '%s': %s\n",
                file_name,
                strerror(errno));
        exit(1);
    }

    StatStruct stat_struct = {0};
    stat(file_name, &stat_struct);

    JkBuffer buffer = {.size = stat_struct.st_size};
    buffer.data = jk_arena_push(storage, stat_struct.st_size);
    if (!buffer.data) {
        fprintf(stderr, "jk_file_read_full: Failed to allocate memory for file '%s'\n", file_name);
        exit(1);
    }

    size_t bytes_read = fread(buffer.data, 1, stat_struct.st_size, file);
    if (bytes_read != (size_t)stat_struct.st_size) {
        fprintf(stderr, "jk_file_read_full: fread failed\n");
        exit(1);
    }

    return buffer;
}
