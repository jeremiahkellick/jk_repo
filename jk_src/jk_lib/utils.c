#include <stdio.h>
#include <sys/stat.h>

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

JK_PUBLIC size_t jk_file_size(char *file_name)
{
    StatStruct stat_struct = {0};
    stat(file_name, &stat_struct);
    return (size_t)stat_struct.st_size;
}
