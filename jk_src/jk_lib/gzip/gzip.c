#include <stdint.h>

// #jk_build dependencies_begin
#include <jk_src/jk_lib/jk_lib.h>
// #jk_build dependencies_end

#include "gzip.h"

static uint8_t jk_gzip_magic[] = {0x1f, 0x8b, 0x08};

typedef enum JkGzipFlag {
    JK_GZIP_FLAG_TEXT,
    JK_GZIP_FLAG_CRC,
    JK_GZIP_FLAG_EXTRA,
    JK_GZIP_FLAG_NAME,
    JK_GZIP_FLAG_COMMENT,
} JkGzipFlag;

#if _MSC_VER && !__clang__
#pragma pack(push, 1)
typedef struct JkGzipHeader {
#else
typedef struct __attribute__((packed)) JkGzipHeader {
#endif
    uint8_t magic[JK_ARRAY_COUNT(jk_gzip_magic)];
    uint8_t flags;
    uint32_t modified_time;
    uint8_t extra_flags;
    uint8_t os;
} JkGzipHeader;
#if _MSC_VER && !__clang__
#pragma pack(pop)
#endif

JK_PUBLIC JkBuffer jk_gzip_buffer_names[3] = {
    JKSI("name"),
    JKSI("comment"),
    JKSI("contents"),
};

JkGzipDecompressResult jk_gzip_decompress(JkArena *arena, JkBuffer buffer)
{
    JkGzipDecompressResult result = {0};

    uint64_t byte_cursor = 0;
    JkGzipHeader header =
            JK_BUFFER_FIELD_READ(buffer, &byte_cursor, JkGzipHeader, (JkGzipHeader){0});
    JkBuffer extra_data = {0};
    JkGzipTrailer trailer = {0};

    if (jk_buffer_compare(JK_BUFFER_FROM_ARRAY(header.magic), JK_BUFFER_FROM_ARRAY(jk_gzip_magic))
            != 0) {
        jk_print(JKS("jk_gzip_decompress: buffer did not start with the expected magic bytes\n"));
    }
    if (JK_FLAG_GET(header.flags, JK_GZIP_FLAG_EXTRA)) {
        extra_data.size = JK_BUFFER_FIELD_READ(buffer, &byte_cursor, uint16_t, 0);
        extra_data.data = buffer.data + byte_cursor;
        byte_cursor += extra_data.size;
    }
    if (JK_FLAG_GET(header.flags, JK_GZIP_FLAG_NAME)) {
        result.name = jk_buffer_null_terminated_next(buffer, &byte_cursor);
    }
    if (JK_FLAG_GET(header.flags, JK_GZIP_FLAG_COMMENT)) {
        result.comment = jk_buffer_null_terminated_next(buffer, &byte_cursor);
    }
    if (JK_FLAG_GET(header.flags, JK_GZIP_FLAG_CRC)) {
        byte_cursor += sizeof(uint16_t);
    }
    if (byte_cursor + sizeof(JkGzipTrailer) < buffer.size) {
        trailer = *(JkGzipTrailer *)(buffer.data + (buffer.size - sizeof(JkGzipTrailer)));
    }

    result.contents.data = jk_arena_push(arena, trailer.uncompressed_size);

    uint64_t bit_cursor = byte_cursor * 8;
    b32 done = 0;
    while (!done) {
        uint64_t block_header = jk_buffer_bits_read(buffer, &bit_cursor, 3);
        uint64_t mode = block_header >> 1;
        if (block_header & 1) {
            done = 1;
        }
        if (mode == 0) {
            bit_cursor = (bit_cursor + 7) & ~7; // Round bit cursor to nearest byte
            uint16_t length = jk_buffer_bits_read(buffer, &bit_cursor, 16);
            bit_cursor += 16;
            jk_memcpy(result.contents.data + result.contents.size,
                    buffer.data + (bit_cursor / 8),
                    length);
            result.contents.size += length;
            bit_cursor += 8 * length;
        }
    }

    return result;
}
