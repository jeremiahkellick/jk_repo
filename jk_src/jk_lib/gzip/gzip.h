#ifndef JK_GZIP_H
#define JK_GZIP_H

#include <jk_src/jk_lib/jk_lib.h>

#if _MSC_VER && !__clang__
#pragma pack(push, 1)
typedef struct JkGzipTrailer {
#else
typedef struct __attribute__((packed)) JkGzipTrailer {
#endif
    uint32_t crc32;
    uint32_t uncompressed_size;
} JkGzipTrailer;
#if _MSC_VER && !__clang__
#pragma pack(pop)
#endif

JK_PUBLIC JkBuffer jk_inflate(JkArena *arena, JkBuffer data, int64_t uncompressed_size);

JK_PUBLIC JkBuffer jk_zlib_decompress(JkArena *arena, JkBuffer data, int64_t uncompressed_size);

typedef union JkGzipDecompressResult {
    JkBuffer buffers[3];
    struct {
        JkBuffer name;
        JkBuffer comment;
        JkBuffer contents;
    };
} JkGzipDecompressResult;

JK_PUBLIC JkBuffer jk_gzip_buffer_names[3];

JK_PUBLIC JkGzipDecompressResult jk_gzip_decompress(JkArena *arena, JkBuffer buffer);

#endif
