#ifndef JK_UTF8_C
#define JK_UTF8_C

#include "utf8.h"

void jk_utf8_codepoint_encode(uint32_t codepoint32, JkUtf8Codepoint *codepoint)
{
    if (codepoint32 < 0x80) {
        codepoint->b[0] = (unsigned char)codepoint32;
    } else if (codepoint32 < 0x800) {
        codepoint->b[0] = (unsigned char)(0xc0 | (codepoint32 >> 6));
        codepoint->b[1] = (unsigned char)(0x80 | (codepoint32 & 0x3f));
    } else if (codepoint32 < 0x10000) {
        codepoint->b[0] = (unsigned char)(0xe0 | (codepoint32 >> 12));
        codepoint->b[1] = (unsigned char)(0x80 | ((codepoint32 >> 6) & 0x3f));
        codepoint->b[2] = (unsigned char)(0x80 | (codepoint32 & 0x3f));
    } else {
        codepoint->b[0] = (unsigned char)(0xf0 | (codepoint32 >> 18));
        codepoint->b[1] = (unsigned char)(0x80 | ((codepoint32 >> 12) & 0x3f));
        codepoint->b[2] = (unsigned char)(0x80 | ((codepoint32 >> 6) & 0x3f));
        codepoint->b[3] = (unsigned char)(0x80 | (codepoint32 & 0x3f));
    }
}

bool jk_utf8_byte_is_continuation(char byte)
{
    return (byte & 0xc0) == 0x80;
}

JkUtf8CodepointGetStatus jk_utf8_codepoint_get(
        size_t (*stream_read)(size_t byte_count, void *buffer, void *stream),
        int (*stream_seek_relative)(long byte_offset, void *stream),
        void *stream,
        JkUtf8Codepoint *codepoint)
{
    long read_count = (long)stream_read(sizeof(codepoint->b), codepoint->b, stream);
    if (read_count == 0) {
        return JK_UTF8_CODEPOINT_GET_EOF;
    }
    int num_bytes;
    if (jk_utf8_byte_is_continuation(codepoint->b[0])) {
        stream_seek_relative(-read_count, stream);
        return JK_UTF8_CODEPOINT_GET_UNEXPECTED_BYTE;
    } else {
        num_bytes = 1;
        while (num_bytes < 4 && jk_utf8_byte_is_continuation(codepoint->b[num_bytes])) {
            num_bytes++;
        }
    }
    stream_seek_relative(-read_count + num_bytes, stream);
    return JK_UTF8_CODEPOINT_GET_SUCCESS;
}

#endif
