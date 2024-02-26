#include <stdbool.h>
#include <stdint.h>

typedef struct JkUtf8Codepoint {
    char b[4];
} JkUtf8Codepoint;

void jk_utf8_codepoint_encode(uint32_t codepoint32, JkUtf8Codepoint *codepoint);

bool jk_utf8_is_continuation(char byte);

typedef enum JkUtf8CodepointGetStatus {
    JK_UTF8_CODEPOINT_GET_SUCCESS,
    JK_UTF8_CODEPOINT_GET_UNEXPECTED_BYTE,
    JK_UTF8_CODEPOINT_GET_EOF,
} JkUtf8CodepointGetStatus;

JkUtf8CodepointGetStatus jk_utf8_codepoint_get(
        size_t (*stream_read)(size_t byte_count, void *buffer, void *stream),
        int (*stream_seek_relative)(long byte_offset, void *stream),
        void *stream,
        JkUtf8Codepoint *codepoint);
