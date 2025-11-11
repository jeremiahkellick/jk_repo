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

    uint64_t pos = 0;
    JkGzipHeader header = JK_BUFFER_FIELD_READ(buffer, &pos, JkGzipHeader, (JkGzipHeader){0});
    JkBuffer extra_data = {0};
    JkGzipTrailer trailer = {0};

    if (jk_buffer_compare(JK_BUFFER_FROM_ARRAY(header.magic), JK_BUFFER_FROM_ARRAY(jk_gzip_magic))
            != 0) {
        jk_print(JKS("jk_gzip_decompress: buffer did not start with the expected magic bytes\n"));
    }
    if (JK_FLAG_GET(header.flags, JK_GZIP_FLAG_EXTRA)) {
        extra_data.size = JK_BUFFER_FIELD_READ(buffer, &pos, uint16_t, 0);
        extra_data.data = buffer.data + pos;
        pos += extra_data.size;
    }
    if (JK_FLAG_GET(header.flags, JK_GZIP_FLAG_NAME)) {
        result.name = jk_buffer_null_terminated_next(buffer, &pos);
    }
    if (JK_FLAG_GET(header.flags, JK_GZIP_FLAG_COMMENT)) {
        result.comment = jk_buffer_null_terminated_next(buffer, &pos);
    }
    if (JK_FLAG_GET(header.flags, JK_GZIP_FLAG_CRC)) {
        pos += sizeof(uint16_t);
    }
    if (pos + sizeof(JkGzipTrailer) < buffer.size) {
        trailer = *(JkGzipTrailer *)(buffer.data + (buffer.size - sizeof(JkGzipTrailer)));
    }

    return result;
}
