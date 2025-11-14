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

static uint16_t jk_deflate_fixed_code_lengths_table[5][2] = {
    {0, 8},
    {144, 9},
    {256, 7},
    {280, 8},
    {288},
};

static JkBuffer jk_deflate_fixed_code_lengths_get(JkArena *arena)
{
    JkBuffer result;
    result.size =
            jk_deflate_fixed_code_lengths_table[JK_ARRAY_COUNT(jk_deflate_fixed_code_lengths_table)
                    - 1][0];
    result.data = jk_arena_push(arena, result.size * sizeof(*result.data));

    for (uint16_t range = 0; range < JK_ARRAY_COUNT(jk_deflate_fixed_code_lengths_table) - 1;
            range++) {
        for (uint16_t i = jk_deflate_fixed_code_lengths_table[range][0];
                i < jk_deflate_fixed_code_lengths_table[range + 1][0];
                i++) {
            result.data[i] = jk_deflate_fixed_code_lengths_table[range][1];
        }
    }

    return result;
}

static uint16_t jk_deflate_pad(uint16_t code, uint8_t code_length)
{
    return code << (16 - code_length);
}

static uint16_t jk_deflate_unpad(uint16_t padded_code, uint8_t code_length)
{
    return padded_code >> (16 - code_length);
}

typedef struct JkDeflateU16Array {
    uint64_t count;
    uint16_t *items;
} JkDeflateU16Array;

typedef struct JkDeflateHuffmanDecoderBucket {
    uint16_t padded_code;
    JkDeflateU16Array values;
} JkDeflateHuffmanDecoderBucket;

typedef struct JkDeflateHuffmanDecoder {
    b32 initialized;
    JkDeflateHuffmanDecoderBucket buckets[16]; // Group values by the length of their codes
} JkDeflateHuffmanDecoder;

static void jk_deflate_huffman_decoder_init(
        JkArena *arena, JkDeflateHuffmanDecoder *decoder, JkBuffer code_lengths)
{
    jk_memset(decoder, 0, sizeof(*decoder));
    decoder->initialized = 1;

    // Find the number of values for each code length
    for (uint64_t value = 0; value < code_lengths.size; value++) {
        uint8_t code_length = code_lengths.data[value];
        JK_DEBUG_ASSERT(code_length < JK_ARRAY_COUNT(decoder->buckets));
        decoder->buckets[code_length].values.count++;
    }

    // Allocate buckets
    for (uint8_t code_length = 1; code_length < (uint8_t)JK_ARRAY_COUNT(decoder->buckets);
            code_length++) {
        JkDeflateU16Array *values = &decoder->buckets[code_length].values;
        values->items = jk_arena_push(arena, values->count * sizeof(*values->items));
    }

    // Fill buckets with values
    uint64_t cursors[JK_ARRAY_COUNT(decoder->buckets)] = {0};
    for (uint64_t value = 0; value < code_lengths.size; value++) {
        uint8_t code_length = code_lengths.data[value];
        decoder->buckets[code_length].values.items[cursors[code_length]++] = value;
    }

    // Find the padded starting code for each bucket
    uint16_t code = 0;
    for (uint8_t code_length = 1; code_length < (uint8_t)JK_ARRAY_COUNT(decoder->buckets);
            code_length++) {
        code = (code + decoder->buckets[code_length - 1].values.count) << 1;
        decoder->buckets[code_length].padded_code = jk_deflate_pad(code, code_length);
    }
}

typedef struct JkDeflateHuffmanDecodeResult {
    uint8_t code_length;
    uint16_t value;
} JkDeflateHuffmanDecodeResult;

static JkDeflateHuffmanDecodeResult jk_deflate_huffman_decode(
        JkDeflateHuffmanDecoder *decoder, uint16_t padded_code)
{
    JkDeflateHuffmanDecodeResult result = {.code_length = 1};

    for (; result.code_length < JK_ARRAY_COUNT(decoder->buckets); result.code_length++) {
        if (result.code_length == JK_ARRAY_COUNT(decoder->buckets) - 1
                || padded_code < decoder->buckets[result.code_length + 1].padded_code) {
            uint16_t index = jk_deflate_unpad(padded_code, result.code_length)
                    - jk_deflate_unpad(
                            decoder->buckets[result.code_length].padded_code, result.code_length);
            if (index < decoder->buckets[result.code_length].values.count) {
                result.value = decoder->buckets[result.code_length].values.items[index];
            } else {
                jk_print(JKS("DEFLATE compressed data contained an invalid Huffman code\n"));
            }
            break;
        }
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

    JkDeflateHuffmanDecoder lit_len_decoder;
    jk_deflate_huffman_decoder_init(arena, &lit_len_decoder, jk_deflate_fixed_code_lengths_get(arena));

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
        } else if (mode == 1) {
            b32 end_of_block = 0;
            while (!end_of_block) {
                uint16_t next_16_bits = jk_buffer_bits_peek(buffer, bit_cursor, 16);
                uint16_t padded_code = jk_bit_reverse_u16(next_16_bits);
                JkDeflateHuffmanDecodeResult d = jk_deflate_huffman_decode(&lit_len_decoder, padded_code);
                bit_cursor += d.code_length;
                if (d.value < 256) {
                    result.contents.data[result.contents.size++] = d.value;
                } else if (d.value == 256) {
                    end_of_block = 1;
                } else if (d.value < 286) {
                    // Find length
                    JkDeflateExtraBitsInfo *len_info = jk_deflate_length_table + (d.value - 257);
                    uint16_t length = len_info->base_value;
                    if (len_info->extra_bits) {
                        uint32_t padded = jk_bit_reverse_u16(
                                jk_buffer_bits_read(buffer, &bit_cursor, len_info->extra_bits));
                        length += jk_deflate_unpad(padded, len_info->extra_bits);
                    }

                    // Find distance
                    uint16_t distance_code = jk_deflate_unpad(
                            jk_bit_reverse_u16(jk_buffer_bits_read(buffer, &bit_cursor, 5)), 5);
                    if (distance_code < 30) {
                        JkDeflateExtraBitsInfo *dist_info =
                                jk_deflate_distance_code_table + distance_code;
                        uint16_t distance = dist_info->base_value;
                        if (dist_info->extra_bits) {
                            uint32_t padded = jk_bit_reverse_u16(jk_buffer_bits_read(
                                    buffer, &bit_cursor, dist_info->extra_bits));
                            distance += jk_deflate_unpad(padded, dist_info->extra_bits);
                        }
                        jk_memcpy(result.contents.data + result.contents.size,
                                result.contents.data + result.contents.size - distance,
                                length);
                        result.contents.size += length;
                    } else {
                        jk_print(JKS("DEFLATE data contained an invalid distance code\n"));
                    }
                } else {
                    jk_print(JKS("DELFATE data contained an invalid length code\n"));
                }
            }
        } else {
            jk_print(JKS("DELFATE data contained an invalid block header\n"));
        }
    }

    return result;
}
