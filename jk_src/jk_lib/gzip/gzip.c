#include <stdint.h>

// #jk_build dependencies_begin
#include <jk_src/jk_lib/jk_lib.h>
// #jk_build dependencies_end

#include "gzip.h"

typedef struct JkDeflateExtraBitsInfo {
    uint8_t extra_bits;
    uint16_t base_value;
} JkDeflateExtraBitsInfo;

JkDeflateExtraBitsInfo jk_deflate_length_table[] = {
    {0, 3},
    {0, 4},
    {0, 5},
    {0, 6},
    {0, 7},
    {0, 8},
    {0, 9},
    {0, 10},
    {1, 11},
    {1, 13},
    {1, 15},
    {1, 17},
    {2, 19},
    {2, 23},
    {2, 27},
    {2, 31},
    {3, 35},
    {3, 43},
    {3, 51},
    {3, 59},
    {4, 67},
    {4, 83},
    {4, 99},
    {4, 115},
    {5, 131},
    {5, 163},
    {5, 195},
    {5, 227},
    {0, 258},
};

JkDeflateExtraBitsInfo jk_deflate_distance_code_table[] = {
    {0, 1},
    {0, 2},
    {0, 3},
    {0, 4},
    {1, 5},
    {1, 7},
    {2, 9},
    {2, 13},
    {3, 17},
    {3, 25},
    {4, 33},
    {4, 49},
    {5, 65},
    {5, 97},
    {6, 129},
    {6, 193},
    {7, 257},
    {7, 385},
    {8, 513},
    {8, 769},
    {9, 1025},
    {9, 1537},
    {10, 2049},
    {10, 3073},
    {11, 4097},
    {11, 6145},
    {12, 8193},
    {12, 12289},
    {13, 16385},
    {13, 24577},
};

// clang-format off
static uint8_t jk_deflate_lit_len_fixed_code_lengths_bytes[] = {
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8,
};
// clang-format on

static JkBuffer jk_deflate_lit_len_fixed_code_lengths =
        JK_BUFFER_INIT_FROM_BYTE_ARRAY(jk_deflate_lit_len_fixed_code_lengths_bytes);

// clang-format off
static uint8_t jk_deflate_dist_fixed_code_lengths_bytes[] = {
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
};
// clang-format on

static JkBuffer jk_deflate_dist_fixed_code_lengths =
        JK_BUFFER_INIT_FROM_BYTE_ARRAY(jk_deflate_dist_fixed_code_lengths_bytes);

static uint8_t jk_deflate_code_length_order[] = {
    16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};

static uint16_t jk_deflate_pad(uint16_t code, uint8_t code_length)
{
    return code << (16 - code_length);
}

static uint16_t jk_deflate_unpad(uint16_t padded_code, uint8_t code_length)
{
    return padded_code >> (16 - code_length);
}

typedef struct JkDeflateU16Array {
    int64_t count;
    uint16_t *e;
} JkDeflateU16Array;

typedef struct JkDeflateHuffmanDecoderBucket {
    uint16_t padded_code;
    JkDeflateU16Array values;
} JkDeflateHuffmanDecoderBucket;

typedef struct JkDeflateHuffmanDecoder {
    b32 initialized;
    uint8_t max_code_length;
    JkDeflateHuffmanDecoderBucket buckets[16]; // Group values by the length of their codes
} JkDeflateHuffmanDecoder;

static void jk_deflate_huffman_decoder_init(
        JkArena *arena, JkDeflateHuffmanDecoder *decoder, JkBuffer code_lengths)
{
    jk_memset(decoder, 0, JK_SIZEOF(*decoder));
    decoder->initialized = 1;

    // Find the number of values for each code length and find the max code length
    decoder->max_code_length = 0;
    for (int64_t value = 0; value < code_lengths.size; value++) {
        uint8_t code_length = code_lengths.data[value];
        JK_DEBUG_ASSERT(code_length < JK_ARRAY_COUNT(decoder->buckets));
        if (code_length) {
            decoder->buckets[code_length].values.count++;
            if (decoder->max_code_length < code_length) {
                decoder->max_code_length = code_length;
            }
        }
    }

    // Allocate buckets
    for (uint8_t code_length = 1; code_length <= decoder->max_code_length; code_length++) {
        JkDeflateU16Array *values = &decoder->buckets[code_length].values;
        values->e = jk_arena_push(arena, values->count * JK_SIZEOF(*values->e));
    }

    // Fill buckets with values
    int64_t cursors[JK_ARRAY_COUNT(decoder->buckets)] = {0};
    for (int64_t value = 0; value < code_lengths.size; value++) {
        uint8_t code_length = code_lengths.data[value];
        if (code_length) {
            decoder->buckets[code_length].values.e[cursors[code_length]++] = value;
        }
    }

    // Find the padded starting code for each bucket
    uint16_t code = 0;
    for (uint8_t code_length = 1; code_length <= decoder->max_code_length; code_length++) {
        code = (code + decoder->buckets[code_length - 1].values.count) << 1;
        decoder->buckets[code_length].padded_code = jk_deflate_pad(code, code_length);
    }
}

static uint16_t jk_deflate_huffman_decode(
        JkDeflateHuffmanDecoder *decoder, JkBuffer buffer, int64_t *bit_cursor)
{
    uint16_t next_16_bits = jk_buffer_bits_peek(buffer, *bit_cursor, 16);
    uint16_t padded_code = jk_bit_reverse_u16(next_16_bits);

    uint8_t code_length = 0;
    uint16_t value = 0;

    for (; code_length <= decoder->max_code_length; code_length++) {
        if (code_length == decoder->max_code_length
                || padded_code < decoder->buckets[code_length + 1].padded_code) {
            uint16_t index = jk_deflate_unpad(padded_code, code_length)
                    - jk_deflate_unpad(decoder->buckets[code_length].padded_code, code_length);
            if (index < decoder->buckets[code_length].values.count) {
                value = decoder->buckets[code_length].values.e[index];
            } else {
                jk_print(JKS("DEFLATE compressed data contained an invalid Huffman code\n"));
            }
            break;
        }
    }

    *bit_cursor += code_length;
    return value;
}

// Decompresses data in the DEFLATE format
JK_PUBLIC JkBuffer jk_inflate(JkArena *arena, JkBuffer data, int64_t uncompressed_size)
{
    JkBuffer result = {.data = jk_arena_push(arena, uncompressed_size)};

    int64_t bit_cursor = 0;
    b32 done = 0;
    while (!done) {
        JkArena block_arena = jk_arena_child_get(arena);

        uint64_t block_header = jk_buffer_bits_read(data, &bit_cursor, 3);
        uint64_t mode = block_header >> 1;
        if (block_header & 1) {
            done = 1;
        }
        if (mode == 0) { // Uncompressed block
            bit_cursor = (bit_cursor + 7) & ~7; // Round bit cursor to nearest byte
            uint16_t length = jk_buffer_bits_read(data, &bit_cursor, 16);
            uint16_t length_check = ~(uint16_t)jk_buffer_bits_read(data, &bit_cursor, 16);
            if (length != length_check) {
                jk_print(JKS("DEFLATE uncompressed block length check failed\n"));
            }
            jk_memcpy(result.data + result.size, data.data + (bit_cursor / 8), length);
            result.size += length;
            bit_cursor += 8 * length;
        } else if (mode == 1 || mode == 2) {
            uint8_t const lit_len = 0;
            uint8_t const dist = 1;
            JkBuffer code_length_buffers[2];
            if (mode == 1) { // Use fixed Huffman codes
                code_length_buffers[lit_len] = jk_deflate_lit_len_fixed_code_lengths;
                code_length_buffers[dist] = jk_deflate_dist_fixed_code_lengths;
            } else { // Use dynamic Huffman codes
                // Find code length buffer sizes
                int64_t code_length_count[JK_ARRAY_COUNT(code_length_buffers)] = {
                    jk_buffer_bits_read(data, &bit_cursor, 5) + 257,
                    jk_buffer_bits_read(data, &bit_cursor, 5) + 1,
                };
                int64_t code_length_code_length_count =
                        jk_buffer_bits_read(data, &bit_cursor, 4) + 4;

                // Allocate the code length buffers
                code_length_buffers[lit_len] = jk_buffer_alloc_zero(&block_arena, 286);
                code_length_buffers[dist] = jk_buffer_alloc_zero(&block_arena, 32);
                JkBuffer code_length_code_lengths = jk_buffer_alloc_zero(&block_arena, 19);

                // Read in the code length alphabet code lengths
                for (int64_t i = 0; i < code_length_code_length_count; i++) {
                    code_length_code_lengths.data[jk_deflate_code_length_order[i]] =
                            jk_buffer_bits_read(data, &bit_cursor, 3);
                }
                JkDeflateHuffmanDecoder code_length_decoder;
                jk_deflate_huffman_decoder_init(
                        &block_arena, &code_length_decoder, code_length_code_lengths);

                // Decode code lengths for literal/length and distance alphabets
                for (uint8_t buf_i = 0; buf_i < JK_ARRAY_COUNT(code_length_buffers); buf_i++) {
                    JkBuffer *code_lengths = code_length_buffers + buf_i;
                    int64_t count = code_length_count[buf_i];
                    int64_t i = 0;
                    while (i < count) {
                        uint16_t code =
                                jk_deflate_huffman_decode(&code_length_decoder, data, &bit_cursor);
                        if (code < 16) {
                            code_lengths->data[i++] = code;
                        } else {
                            if (code == 16) {
                                if (0 < i) {
                                    int64_t rep_count =
                                            3 + jk_buffer_bits_read(data, &bit_cursor, 2);
                                    while (rep_count--) {
                                        code_lengths->data[i] = code_lengths->data[i - 1];
                                        i++;
                                    }
                                } else {
                                    jk_print(JKS(
                                            "DEFLATE block tried to repeat a code length before a "
                                            "previous code length was established.\n"));
                                }
                            } else if (code == 17) {
                                i += 3 + jk_buffer_bits_read(data, &bit_cursor, 3);
                            } else {
                                i += 11 + jk_buffer_bits_read(data, &bit_cursor, 7);
                            }
                        }
                    }
                }
            }

            JkDeflateHuffmanDecoder lit_len_decoder;
            JkDeflateHuffmanDecoder dist_decoder;
            jk_deflate_huffman_decoder_init(
                    &block_arena, &lit_len_decoder, code_length_buffers[lit_len]);
            jk_deflate_huffman_decoder_init(&block_arena, &dist_decoder, code_length_buffers[dist]);

            b32 end_of_block = 0;
            while (!end_of_block) {
                uint16_t lit_len_value =
                        jk_deflate_huffman_decode(&lit_len_decoder, data, &bit_cursor);
                if (lit_len_value < 256) {
                    result.data[result.size++] = lit_len_value;
                } else if (lit_len_value == 256) {
                    end_of_block = 1;
                } else {
                    // Find length
                    JkDeflateExtraBitsInfo *len_info =
                            jk_deflate_length_table + (lit_len_value - 257);
                    uint16_t length = len_info->base_value;
                    if (len_info->extra_bits) {
                        length += jk_buffer_bits_read(data, &bit_cursor, len_info->extra_bits);
                    }

                    // Find distance
                    uint16_t distance_code =
                            jk_deflate_huffman_decode(&dist_decoder, data, &bit_cursor);
                    JkDeflateExtraBitsInfo *dist_info =
                            jk_deflate_distance_code_table + distance_code;
                    uint16_t distance = dist_info->base_value;
                    if (dist_info->extra_bits) {
                        distance += jk_buffer_bits_read(data, &bit_cursor, dist_info->extra_bits);
                    }
                    jk_memcpy(result.data + result.size,
                            result.data + result.size - distance,
                            length);
                    result.size += length;
                }
            }
        } else {
            jk_print(JKS("DELFATE data contained an invalid block header\n"));
        }
    }

    return result;
}

static uint8_t jk_zlib_magic[] = {0x78, 0x8b, 0x08};

#if _MSC_VER && !__clang__
#pragma pack(push, 1)
typedef struct JkZlibHeader {
#else
typedef struct __attribute__((packed)) JkZlibHeader {
#endif
    uint8_t cmf;
    uint8_t flg;
} JkZlibHeader;
#if _MSC_VER && !__clang__
#pragma pack(pop)
#endif

JK_PUBLIC JkBuffer jk_zlib_decompress(JkArena *arena, JkBuffer data, int64_t uncompressed_size)
{
    JkBuffer result = {0};

    int64_t pos = 0;
    JkZlibHeader header = JK_BUFFER_FIELD_READ(data, &pos, JkZlibHeader, (JkZlibHeader){0});
    if (header.cmf == 0x78 && (((header.cmf << 8) | header.flg) % 31) == 0
            && !(header.flg & 0x20)) {
        result = jk_inflate(arena,
                (JkBuffer){.size = data.size - pos, .data = data.data + pos},
                uncompressed_size);
    } else {
        jk_print(JKS("jk_zlib_decompress: Invalid or unsupported format\n"));
    }

    return result;
}

typedef enum JkGzipFlag {
    JK_GZIP_FLAG_TEXT,
    JK_GZIP_FLAG_CRC,
    JK_GZIP_FLAG_EXTRA,
    JK_GZIP_FLAG_NAME,
    JK_GZIP_FLAG_COMMENT,
} JkGzipFlag;

static uint8_t jk_gzip_magic[] = {0x1f, 0x8b, 0x08};

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

    int64_t byte_cursor = 0;
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
        byte_cursor += JK_SIZEOF(uint16_t);
    }
    if (byte_cursor + JK_SIZEOF(JkGzipTrailer) < buffer.size) {
        trailer = *(JkGzipTrailer *)(buffer.data + (buffer.size - JK_SIZEOF(JkGzipTrailer)));
    }

    result.contents = jk_inflate(arena,
            (JkBuffer){.size = buffer.size - byte_cursor, .data = buffer.data + byte_cursor},
            trailer.uncompressed_size);

    return result;
}
