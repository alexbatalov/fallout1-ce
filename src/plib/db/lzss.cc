// NOTE: Functions in these module are somewhat different from what can be seen
// in IDA because of two new helper functions that deal with incoming bits. I
// bet something like these were implemented via function-like macro in the
// same manner zlib deals with bits. The pattern is so common in this module so
// I made an exception and extracted it into separate functions to increase
// readability.

#include "plib/db/lzss.h"

#include <string.h>

namespace fallout {

static inline void lzss_fill_decode_buffer(FILE* stream);
static inline void lzss_decode_chunk_to_buf(unsigned int type, unsigned char** dest, unsigned int* length);
static inline void lzss_decode_chunk_to_file(unsigned int type, FILE* stream, unsigned int* length);

// 0x6B0860
static unsigned char decode_buffer[1024];

// 0x6B0C60
static unsigned char* decode_buffer_position;

// 0x6B0C64
static unsigned int decode_bytes_left;

// 0x6B0C68
static int ring_buffer_index;

// 0x6B0C6C
static unsigned char* decode_buffer_end;

// 0x6B0C70
static unsigned char ring_buffer[4116];

// 0x4CA260
int lzss_decode_to_buf(FILE* in, unsigned char* dest, unsigned int length)
{
    unsigned char* curr;
    unsigned char byte;

    curr = dest;
    memset(ring_buffer, ' ', 4078);
    ring_buffer_index = 4078;
    decode_buffer_end = decode_buffer;
    decode_buffer_position = decode_buffer;
    decode_bytes_left = length;

    while (length > 16) {
        lzss_fill_decode_buffer(in);

        length -= 1;
        byte = *decode_buffer_position++;
        lzss_decode_chunk_to_buf(byte & 0x01, &curr, &length);
        lzss_decode_chunk_to_buf(byte & 0x02, &curr, &length);
        lzss_decode_chunk_to_buf(byte & 0x04, &curr, &length);
        lzss_decode_chunk_to_buf(byte & 0x08, &curr, &length);
        lzss_decode_chunk_to_buf(byte & 0x10, &curr, &length);
        lzss_decode_chunk_to_buf(byte & 0x20, &curr, &length);
        lzss_decode_chunk_to_buf(byte & 0x40, &curr, &length);
        lzss_decode_chunk_to_buf(byte & 0x80, &curr, &length);
    }

    do {
        if (length == 0) break;

        lzss_fill_decode_buffer(in);

        length -= 1;
        byte = *decode_buffer_position++;

        if (length == 0) break;
        lzss_decode_chunk_to_buf(byte & 0x01, &curr, &length);

        if (length == 0) break;
        lzss_decode_chunk_to_buf(byte & 0x02, &curr, &length);

        if (length == 0) break;
        lzss_decode_chunk_to_buf(byte & 0x04, &curr, &length);

        if (length == 0) break;
        lzss_decode_chunk_to_buf(byte & 0x08, &curr, &length);

        if (length == 0) break;
        lzss_decode_chunk_to_buf(byte & 0x10, &curr, &length);

        if (length == 0) break;
        lzss_decode_chunk_to_buf(byte & 0x20, &curr, &length);

        if (length == 0) break;
        lzss_decode_chunk_to_buf(byte & 0x40, &curr, &length);

        if (length == 0) break;
        lzss_decode_chunk_to_buf(byte & 0x80, &curr, &length);

        if (length == 0) break;

        length -= 1;
        byte = *decode_buffer_position++;

        if (length == 0) break;
        lzss_decode_chunk_to_buf(byte & 0x01, &curr, &length);

        if (length == 0) break;
        lzss_decode_chunk_to_buf(byte & 0x02, &curr, &length);

        if (length == 0) break;
        lzss_decode_chunk_to_buf(byte & 0x04, &curr, &length);

        if (length == 0) break;
        lzss_decode_chunk_to_buf(byte & 0x08, &curr, &length);

        if (length == 0) break;
        lzss_decode_chunk_to_buf(byte & 0x10, &curr, &length);

        if (length == 0) break;
        lzss_decode_chunk_to_buf(byte & 0x20, &curr, &length);

        if (length == 0) break;
        lzss_decode_chunk_to_buf(byte & 0x40, &curr, &length);

        if (length == 0) break;
        lzss_decode_chunk_to_buf(byte & 0x80, &curr, &length);
    } while (0);

    return curr - dest;
}

// 0x4CB570
void lzss_decode_to_file(FILE* in, FILE* out, unsigned int length)
{
    unsigned char byte;

    memset(ring_buffer, ' ', 4078);
    ring_buffer_index = 4078;
    decode_buffer_end = decode_buffer;
    decode_buffer_position = decode_buffer;
    decode_bytes_left = length;

    while (length > 16) {
        lzss_fill_decode_buffer(in);

        length -= 1;
        byte = *decode_buffer_position++;
        lzss_decode_chunk_to_file(byte & 0x01, out, &length);
        lzss_decode_chunk_to_file(byte & 0x02, out, &length);
        lzss_decode_chunk_to_file(byte & 0x04, out, &length);
        lzss_decode_chunk_to_file(byte & 0x08, out, &length);
        lzss_decode_chunk_to_file(byte & 0x10, out, &length);
        lzss_decode_chunk_to_file(byte & 0x20, out, &length);
        lzss_decode_chunk_to_file(byte & 0x40, out, &length);
        lzss_decode_chunk_to_file(byte & 0x80, out, &length);
    }

    do {
        if (length == 0) break;

        lzss_fill_decode_buffer(in);

        length -= 1;
        byte = *decode_buffer_position++;

        if (length == 0) break;
        lzss_decode_chunk_to_file(byte & 0x01, out, &length);

        if (length == 0) break;
        lzss_decode_chunk_to_file(byte & 0x02, out, &length);

        if (length == 0) break;
        lzss_decode_chunk_to_file(byte & 0x04, out, &length);

        if (length == 0) break;
        lzss_decode_chunk_to_file(byte & 0x08, out, &length);

        if (length == 0) break;
        lzss_decode_chunk_to_file(byte & 0x10, out, &length);

        if (length == 0) break;
        lzss_decode_chunk_to_file(byte & 0x20, out, &length);

        if (length == 0) break;
        lzss_decode_chunk_to_file(byte & 0x40, out, &length);

        if (length == 0) break;
        lzss_decode_chunk_to_file(byte & 0x80, out, &length);

        if (length == 0) break;

        length -= 1;
        byte = *decode_buffer_position++;

        if (length == 0) break;
        lzss_decode_chunk_to_file(byte & 0x01, out, &length);

        if (length == 0) break;
        lzss_decode_chunk_to_file(byte & 0x02, out, &length);

        if (length == 0) break;
        lzss_decode_chunk_to_file(byte & 0x04, out, &length);

        if (length == 0) break;
        lzss_decode_chunk_to_file(byte & 0x08, out, &length);

        if (length == 0) break;
        lzss_decode_chunk_to_file(byte & 0x10, out, &length);

        if (length == 0) break;
        lzss_decode_chunk_to_file(byte & 0x20, out, &length);

        if (length == 0) break;
        lzss_decode_chunk_to_file(byte & 0x40, out, &length);

        if (length == 0) break;
        lzss_decode_chunk_to_file(byte & 0x80, out, &length);
    } while (0);
}

static inline void lzss_fill_decode_buffer(FILE* stream)
{
    size_t bytes_to_read;
    size_t bytes_read;

    if (decode_bytes_left != 0 && decode_buffer_end - decode_buffer_position <= 16) {
        if (decode_buffer_position == decode_buffer_end) {
            decode_buffer_end = decode_buffer;
        } else {
            memmove(decode_buffer, decode_buffer_position, decode_buffer_end - decode_buffer_position);
            decode_buffer_end = decode_buffer + (decode_buffer_end - decode_buffer_position);
        }

        decode_buffer_position = decode_buffer;

        bytes_to_read = 1024 - (decode_buffer_end - decode_buffer);
        if (bytes_to_read > decode_bytes_left) {
            bytes_to_read = decode_bytes_left;
        }

        bytes_read = fread(decode_buffer_end, 1, bytes_to_read, stream);
        decode_buffer_end += bytes_read;
        decode_bytes_left -= bytes_read;
    }
}

static inline void lzss_decode_chunk_to_buf(unsigned int type, unsigned char** dest, unsigned int* length)
{
    unsigned char low;
    unsigned char high;
    int dict_offset;
    int dict_index;
    int chunk_length;
    int index;

    if (type != 0) {
        *length -= 1;
        *(*dest) = *decode_buffer_position++;
        ring_buffer[ring_buffer_index] = *(*dest)++;
        ring_buffer_index += 1;
        ring_buffer_index &= 0xFFF;
    } else {
        *length -= 2;
        low = *decode_buffer_position++;
        high = *decode_buffer_position++;
        dict_offset = low | ((high & 0xF0) << 4);
        chunk_length = (high & 0x0F) + 3;

        for (index = 0; index < chunk_length; index++) {
            dict_index = (dict_offset + index) & 0xFFF;
            *(*dest) = ring_buffer[dict_index];
            ring_buffer[ring_buffer_index] = *(*dest)++;
            ring_buffer_index += 1;
            ring_buffer_index &= 0xFFF;
        }
    }
}

static inline void lzss_decode_chunk_to_file(unsigned int type, FILE* stream, unsigned int* length)
{
    unsigned char low;
    unsigned char high;
    int dict_offset;
    int dict_index;
    int chunk_length;
    int index;

    if (type != 0) {
        *length -= 1;
        fputc(*decode_buffer_position, stream);
        ring_buffer[ring_buffer_index] = *decode_buffer_position++;
        ring_buffer_index += 1;
        ring_buffer_index &= 0xFFF;
    } else {
        *length -= 2;
        low = *decode_buffer_position++;
        high = *decode_buffer_position++;
        dict_offset = low | ((high & 0xF0) << 4);
        chunk_length = (high & 0x0F) + 3;

        for (index = 0; index < chunk_length; index++) {
            dict_index = (dict_offset + index) & 0xFFF;
            fputc(ring_buffer[dict_index], stream);
            ring_buffer[ring_buffer_index] = ring_buffer[dict_index];
            ring_buffer_index += 1;
            ring_buffer_index &= 0xFFF;
        }
    }
}

} // namespace fallout
