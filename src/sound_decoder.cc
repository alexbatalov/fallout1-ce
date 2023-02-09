// NOTE: Functions in these module are somewhat different from what can be seen
// in IDA because of two new helper functions that deal with incoming bits. I
// bet something like these were implemented via function-like macro in the
// same manner zlib deals with bits. The pattern is so common in this module so
// I made an exception and extracted it into separate functions to increase
// readability.

#include "sound_decoder.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define SOUND_DECODER_IN_BUFFER_SIZE 512

typedef int (*ReadBandFunc)(AudioDecoder* ad, int subband, int n);

typedef struct _byte_reader {
    AudioDecoderReadFunc* read;
    void* data;
    unsigned char* buf;
    size_t buf_size;
    unsigned char* buf_ptr;
    int buf_cnt;
} byte_reader;

typedef struct _bit_reader {
    byte_reader bytes;
    int data;
    int bitcnt;
} bit_reader;

typedef struct _AudioDecoder {
    bit_reader bits;
    int levels;
    int subbands;
    int samples_per_subband;
    int total_samples;
    unsigned char* prev_samples;
    unsigned char* samples;
    int block_samples_per_subband;
    int block_total_samples;
    int channels;
    int rate;
    int file_cnt;
    unsigned char* samp_ptr;
    int samp_cnt;
} AudioDecoder;

static bool bytes_init(byte_reader* bytes, AudioDecoderReadFunc* read, void* data);
static unsigned char ByteReaderFill(byte_reader* bytes);
static bool bits_init(bit_reader* bits, AudioDecoderReadFunc* read, void* data);
static void init_pack_tables();
static int ReadBand_Fail(AudioDecoder* ad, int subband, int n);
static int ReadBand_Fmt0(AudioDecoder* ad, int subband, int n);
static int ReadBand_Fmt3_16(AudioDecoder* ad, int subband, int n);
static int ReadBand_Fmt17(AudioDecoder* ad, int subband, int n);
static int ReadBand_Fmt18(AudioDecoder* ad, int subband, int n);
static int ReadBand_Fmt19(AudioDecoder* ad, int subband, int n);
static int ReadBand_Fmt20(AudioDecoder* ad, int subband, int n);
static int ReadBand_Fmt21(AudioDecoder* ad, int subband, int n);
static int ReadBand_Fmt22(AudioDecoder* ad, int subband, int n);
static int ReadBand_Fmt23(AudioDecoder* ad, int subband, int n);
static int ReadBand_Fmt24(AudioDecoder* ad, int subband, int n);
static int ReadBand_Fmt26(AudioDecoder* ad, int subband, int n);
static int ReadBand_Fmt27(AudioDecoder* ad, int subband, int n);
static int ReadBand_Fmt29(AudioDecoder* ad, int subband, int n);
static int ReadBands(AudioDecoder* ad);
static void untransform_subband0(unsigned char* prv, unsigned char* buf, int step, int count);
static void untransform_subband(unsigned char* prv, unsigned char* buf, int step, int count);
static void untransform_all(AudioDecoder* ad);

static inline void requireBits(AudioDecoder* ad, int n);
static inline void dropBits(AudioDecoder* ad, int n);

// 0x539E58
static int AudioDecoder_cnt = 0;

// 0x539E60
static ReadBandFunc ReadBand_tbl[32] = {
    ReadBand_Fmt0,
    ReadBand_Fail,
    ReadBand_Fail,
    ReadBand_Fmt3_16,
    ReadBand_Fmt3_16,
    ReadBand_Fmt3_16,
    ReadBand_Fmt3_16,
    ReadBand_Fmt3_16,
    ReadBand_Fmt3_16,
    ReadBand_Fmt3_16,
    ReadBand_Fmt3_16,
    ReadBand_Fmt3_16,
    ReadBand_Fmt3_16,
    ReadBand_Fmt3_16,
    ReadBand_Fmt3_16,
    ReadBand_Fmt3_16,
    ReadBand_Fmt3_16,
    ReadBand_Fmt17,
    ReadBand_Fmt18,
    ReadBand_Fmt19,
    ReadBand_Fmt20,
    ReadBand_Fmt21,
    ReadBand_Fmt22,
    ReadBand_Fmt23,
    ReadBand_Fmt24,
    ReadBand_Fail,
    ReadBand_Fmt26,
    ReadBand_Fmt27,
    ReadBand_Fail,
    ReadBand_Fmt29,
    ReadBand_Fail,
    ReadBand_Fail,
};

// 0x6730D0
static unsigned char pack11_2[128];

// 0x673150
static unsigned char pack3_3[32];

// 0x673170
static unsigned short pack5_3[128];

// 0x673270
static unsigned char* AudioDecoder_scale0;

// 0x673274
static unsigned char* AudioDecoder_scale_tbl;

// 0x4D3BB0
static bool bytes_init(byte_reader* bytes, AudioDecoderReadFunc* read, void* data)
{
    bytes->read = read;
    bytes->data = data;

    bytes->buf = (unsigned char*)malloc(SOUND_DECODER_IN_BUFFER_SIZE);
    if (bytes->buf == NULL) {
        return false;
    }

    bytes->buf_size = SOUND_DECODER_IN_BUFFER_SIZE;
    bytes->buf_cnt = 0;

    return true;
}

// 0x4D3BE0
static unsigned char ByteReaderFill(byte_reader* bytes)
{
    bytes->buf_cnt = bytes->read(bytes->data, bytes->buf, bytes->buf_size);
    if (bytes->buf_cnt == 0) {
        memset(bytes->buf, 0, bytes->buf_size);
        bytes->buf_cnt = bytes->buf_size;
    }

    bytes->buf_ptr = bytes->buf;
    bytes->buf_cnt -= 1;
    return *bytes->buf_ptr++;
}

// 0x4BE4C0
static bool bits_init(bit_reader* bits, AudioDecoderReadFunc* read, void* data)
{
    if (!bytes_init(&(bits->bytes), read, data)) {
        return false;
    }

    bits->data = 0;
    bits->bitcnt = 0;

    return true;
}

// 0x4BE508
static void init_pack_tables()
{
    // 0x539E5C
    static bool inited = false;

    int i;
    int j;
    int m;

    if (inited) {
        return;
    }

    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            for (m = 0; m < 3; m++) {
                pack3_3[i + j * 3 + m * 9] = i + j * 4 + m * 16;
            }
        }
    }

    for (i = 0; i < 5; i++) {
        for (j = 0; j < 5; j++) {
            for (m = 0; m < 5; m++) {
                pack5_3[i + j * 5 + m * 25] = i + j * 8 + m * 64;
            }
        }
    }

    for (i = 0; i < 11; i++) {
        for (j = 0; j < 11; j++) {
            pack11_2[i + j * 11] = i + j * 16;
        }
    }

    inited = true;
}

// 0x4BE62C
static int ReadBand_Fail(AudioDecoder* ad, int subband, int n)
{
    return 0;
}

// 0x4BE630
static int ReadBand_Fmt0(AudioDecoder* ad, int subband, int n)
{
    int* p = (int*)ad->samples;
    p += subband;

    int i = ad->samples_per_subband;
    while (i != 0) {
        *p = 0;
        p += ad->subbands;
        i--;
    }

    return 1;
}

// 0x4BE658
static int ReadBand_Fmt3_16(AudioDecoder* ad, int subband, int n)
{
    int value;
    int v14;

    short* base = (short*)AudioDecoder_scale0;
    base += (int)(UINT_MAX << (n - 1));

    int* p = (int*)ad->samples;
    p += subband;

    v14 = (1 << n) - 1;

    int i = ad->samples_per_subband;
    while (i != 0) {
        requireBits(ad, n);
        value = ad->bits.data;
        dropBits(ad, n);

        *p = base[v14 & value];
        p += ad->subbands;

        i--;
    }

    return 1;
}

// 0x4BE720
static int ReadBand_Fmt17(AudioDecoder* ad, int subband, int n)
{
    short* base = (short*)AudioDecoder_scale0;

    int* p = (int*)ad->samples;
    p += subband;

    int i = ad->samples_per_subband;
    while (i != 0) {
        requireBits(ad, 3);

        int value = ad->bits.data & 0xFF;
        if (!(value & 0x01)) {
            dropBits(ad, 1);

            *p = 0;
            p += ad->subbands;

            if (--i == 0) {
                break;
            }

            *p = 0;
            p += ad->subbands;

            if (--i == 0) {
                break;
            }
        } else if (!(value & 0x02)) {
            dropBits(ad, 2);

            *p = 0;
            p += ad->subbands;

            if (--i == 0) {
                break;
            }
        } else {
            dropBits(ad, 3);

            if (value & 0x04) {
                *p = base[1];
            } else {
                *p = base[-1];
            }

            p += ad->subbands;
            i--;
        }
    }
    return 1;
}

// 0x4BE828
static int ReadBand_Fmt18(AudioDecoder* ad, int subband, int n)
{
    short* base = (short*)AudioDecoder_scale0;

    int* p = (int*)ad->samples;
    p += subband;

    int i = ad->samples_per_subband;
    while (i != 0) {
        requireBits(ad, 2);

        int value = ad->bits.data;
        if (!(value & 0x01)) {
            dropBits(ad, 1);

            *p = 0;
            p += ad->subbands;

            if (--i == 0) {
                return 1;
            }
        } else {
            dropBits(ad, 2);

            if (value & 0x02) {
                *p = base[1];
            } else {
                *p = base[-1];
            }

            p += ad->subbands;
            i--;
        }
    }
    return 1;
}

// 0x4BE8F8
static int ReadBand_Fmt19(AudioDecoder* ad, int subband, int n)
{
    short* base = (short*)AudioDecoder_scale0;
    base -= 1;

    int* p = (int*)ad->samples;
    p += subband;

    int i = ad->samples_per_subband;
    while (i != 0) {
        requireBits(ad, 5);
        int value = ad->bits.data & 0x1F;
        dropBits(ad, 5);

        value = pack3_3[value];

        *p = base[value & 0x03];
        p += ad->subbands;
        if (--i == 0) {
            break;
        }

        *p = base[(value >> 2) & 0x03];
        p += ad->subbands;
        if (--i == 0) {
            break;
        }

        *p = base[value >> 4];
        p += ad->subbands;

        i--;
    }

    return 1;
}

// 0x4BE9E8
static int ReadBand_Fmt20(AudioDecoder* ad, int subband, int n)
{
    short* base = (short*)AudioDecoder_scale0;

    int* p = (int*)ad->samples;
    p += subband;

    int i = ad->samples_per_subband;
    while (i != 0) {
        requireBits(ad, 4);

        int value = ad->bits.data & 0xFF;
        if (!(value & 0x01)) {
            dropBits(ad, 1);

            *p = 0;
            p += ad->subbands;

            if (--i == 0) {
                break;
            }

            *p = 0;
            p += ad->subbands;

            if (--i == 0) {
                break;
            }
        } else if (!(value & 0x02)) {
            dropBits(ad, 2);

            *p = 0;
            p += ad->subbands;

            if (--i == 0) {
                break;
            }
        } else {
            dropBits(ad, 4);

            if (value & 0x08) {
                if (value & 0x04) {
                    *p = base[2];
                } else {
                    *p = base[1];
                }
            } else {
                if (value & 0x04) {
                    *p = base[-1];
                } else {
                    *p = base[-2];
                }
            }

            p += ad->subbands;
            i--;
        }
    }

    return 1;
}

// 0x4BEAE4
static int ReadBand_Fmt21(AudioDecoder* ad, int subband, int n)
{
    short* base = (short*)AudioDecoder_scale0;

    int* p = (int*)ad->samples;
    p += subband;

    int i = ad->samples_per_subband;
    while (i != 0) {
        requireBits(ad, 3);

        int value = ad->bits.data & 0xFF;
        if (!(value & 0x01)) {
            dropBits(ad, 1);

            *p = 0;
            p += ad->subbands;

            if (--i == 0) {
                break;
            }
        } else {
            dropBits(ad, 3);

            if (value & 0x04) {
                if (value & 0x02) {
                    *p = base[2];
                } else {
                    *p = base[1];
                }
            } else {
                if (value & 0x02) {
                    *p = base[-1];
                } else {
                    *p = base[-2];
                }
            }

            p += ad->subbands;
            i--;
        }
    }

    return 1;
}

// 0x4BEBC8
static int ReadBand_Fmt22(AudioDecoder* ad, int subband, int n)
{
    short* base = (short*)AudioDecoder_scale0;
    base -= 2;

    int* p = (int*)ad->samples;
    p += subband;

    int i = ad->samples_per_subband;
    while (i != 0) {
        requireBits(ad, 7);
        int value = ad->bits.data & 0x7F;
        dropBits(ad, 7);

        value = pack5_3[value];

        *p = base[value & 7];
        p += ad->subbands;

        if (--i == 0) {
            break;
        }

        *p = base[((value >> 3) & 7)];
        p += ad->subbands;

        if (--i == 0) {
            break;
        }

        *p = base[value >> 6];
        p += ad->subbands;

        if (--i == 0) {
            break;
        }
    }

    return 1;
}

// 0x4BECC4
static int ReadBand_Fmt23(AudioDecoder* ad, int subband, int n)
{
    short* base = (short*)AudioDecoder_scale0;

    int* p = (int*)ad->samples;
    p += subband;

    int i = ad->samples_per_subband;
    while (i != 0) {
        requireBits(ad, 5);

        int value = ad->bits.data;
        if (!(value & 0x01)) {
            dropBits(ad, 1);

            *p = 0;
            p += ad->subbands;

            if (--i == 0) {
                break;
            }

            *p = 0;
            p += ad->subbands;

            if (--i == 0) {
                break;
            }
        } else if (!(value & 0x02)) {
            dropBits(ad, 2);

            *p = 0;
            p += ad->subbands;

            if (--i == 0) {
                break;
            }
        } else if (!(value & 0x04)) {
            dropBits(ad, 4);

            if (value & 0x08) {
                *p = base[1];
            } else {
                *p = base[-1];
            }

            p += ad->subbands;
            if (--i == 0) {
                break;
            }
        } else {
            dropBits(ad, 5);

            value >>= 3;
            value &= 0x03;
            if (value >= 2) {
                value += 3;
            }

            *p = base[value - 3];
            p += ad->subbands;
            i--;
        }
    }

    return 1;
}

// 0x4BEE14
static int ReadBand_Fmt24(AudioDecoder* ad, int subband, int n)
{
    short* base = (short*)AudioDecoder_scale0;

    int* p = (int*)ad->samples;
    p += subband;

    int i = ad->samples_per_subband;
    while (i != 0) {
        requireBits(ad, 4);

        int value = ad->bits.data & 0xFF;
        if (!(value & 0x01)) {
            dropBits(ad, 1);

            *p = 0;
            p += ad->subbands;

            if (--i == 0) {
                break;
            }
        } else if (!(value & 0x02)) {
            dropBits(ad, 3);

            if (value & 0x04) {
                *p = base[1];
            } else {
                *p = base[-1];
            }

            p += ad->subbands;

            if (--i == 0) {
                break;
            }
        } else {
            dropBits(ad, 4);

            value >>= 2;
            value &= 0x03;
            if (value >= 2) {
                value += 3;
            }

            *p = base[value - 3];
            p += ad->subbands;
            i--;
        }
    }

    return 1;
}

// 0x4BEF28
static int ReadBand_Fmt26(AudioDecoder* ad, int subband, int n)
{
    short* base = (short*)AudioDecoder_scale0;

    int* p = (int*)ad->samples;
    p += subband;

    int i = ad->samples_per_subband;
    while (i != 0) {
        requireBits(ad, 5);

        int value = ad->bits.data;
        if (!(value & 0x01)) {
            dropBits(ad, 1);

            *p = 0;
            p += ad->subbands;

            if (--i == 0) {
                break;
            }

            *p = 0;
            p += ad->subbands;

            if (--i == 0) {
                break;
            }
        } else if (!(value & 0x02)) {
            dropBits(ad, 2);

            *p = 0;
            p += ad->subbands;

            if (--i == 0) {
                break;
            }
        } else {
            dropBits(ad, 5);

            value >>= 2;
            value &= 0x07;
            if (value >= 4) {
                value += 1;
            }

            *p = base[value - 4];
            p += ad->subbands;
            i--;
        }
    }

    return 1;
}

// 0x4BF034
static int ReadBand_Fmt27(AudioDecoder* ad, int subband, int n)
{
    short* base = (short*)AudioDecoder_scale0;

    int* p = (int*)ad->samples;
    p += subband;

    int i = ad->samples_per_subband;
    while (i != 0) {
        requireBits(ad, 4);

        int value = ad->bits.data;
        if (!(value & 0x01)) {
            dropBits(ad, 1);

            *p = 0;
            p += ad->subbands;

            if (--i == 0) {
                break;
            }
        } else {
            dropBits(ad, 4);

            value >>= 1;
            value &= 0x07;
            if (value >= 4) {
                value += 1;
            }

            *p = base[value - 4];
            p += ad->subbands;
            i--;
        }
    }

    return 1;
}

// 0x4BF100
static int ReadBand_Fmt29(AudioDecoder* ad, int subband, int n)
{
    short* base = (short*)AudioDecoder_scale0;

    int* p = (int*)ad->samples;
    p += subband;

    int i = ad->samples_per_subband;
    while (i != 0) {
        requireBits(ad, 7);
        int value = ad->bits.data & 0x7F;
        dropBits(ad, 7);

        value = pack11_2[value];

        *p = base[(value & 0x0F) - 5];
        p += ad->subbands;
        if (--i == 0) {
            break;
        }

        *p = base[(value >> 4) - 5];
        p += ad->subbands;
        if (--i == 0) {
            break;
        }
    }

    return 1;
}

// 0x4BF1CC
static int ReadBands(AudioDecoder* ad)
{
    int v9;
    int v15;
    int v17;
    int v19;
    unsigned short* v18;
    int v21;
    ReadBandFunc fn;

    requireBits(ad, 4);
    v9 = ad->bits.data & 0xF;
    dropBits(ad, 4);

    requireBits(ad, 16);
    v15 = ad->bits.data & 0xFFFF;
    dropBits(ad, 16);

    v17 = 1 << v9;

    v18 = (unsigned short*)AudioDecoder_scale0;
    v19 = v17;
    v21 = 0;
    while (v19--) {
        *v18++ = v21;
        v21 += v15;
    }

    v18 = (unsigned short*)AudioDecoder_scale0;
    v19 = v17;
    v21 = -v15;
    while (v19--) {
        v18--;
        *v18 = v21;
        v21 -= v15;
    }

    init_pack_tables();

    for (int index = 0; index < ad->subbands; index++) {
        requireBits(ad, 5);
        int bits = ad->bits.data & 0x1F;
        dropBits(ad, 5);

        fn = ReadBand_tbl[bits];
        if (!fn(ad, index, bits)) {
            return 0;
        }
    }
    return 1;
}

// 0x4BF36C
static void untransform_subband0(unsigned char* prv, unsigned char* buf, int step, int count)
{
    short* p;

    p = (short*)buf;
    p += step;

    if (count == 2) {
        int i = step;
        while (i != 0) {
            i--;
        }
    } else if (count == 4) {
        int v31 = step;
        int* v9 = (int*)buf;
        v9 += step;

        int* v10 = (int*)buf;
        v10 += step * 3;

        int* v11 = (int*)buf;
        v11 += step * 2;

        while (v31 != 0) {
            int* v33 = (int*)buf;
            int* v34 = (int*)prv;

            int v12 = *v34 >> 16;

            int v13 = *v33;
            *v33 = (int)(*(short*)v34) + 2 * v12 + v13;

            int v14 = *v9;
            *v9 = 2 * v13 - v12 - v14;

            int v15 = *v11;
            *v11 = 2 * v14 + v15 + v13;

            int v16 = *v10;
            *v10 = 2 * v15 - v14 - v16;

            v10++;
            v11++;
            v9++;

            *(short*)prv = v15 & 0xFFFF;
            *(short*)(prv + 2) = v16 & 0xFFFF;

            prv += 4;
            buf += 4;

            v31--;
        }
    } else {
        int v30 = count >> 1;
        int v32 = step;
        while (v32 != 0) {
            int* v19 = (int*)buf;

            int v20;
            int v22;
            if (v30 & 0x01) {

            } else {
                v20 = (int)*(short*)prv;
                v22 = *(int*)prv >> 16;
            }

            int v23 = v30 >> 1;
            while (--v23 != -1) {
                int v24 = *v19;
                *v19 += 2 * v22 + v20;
                v19 += step;

                int v26 = *v19;
                *v19 = 2 * v24 - v22 - v26;
                v19 += step;

                v20 = *v19;
                *v19 += 2 * v26 + v24;
                v19 += step;

                v22 = *v19;
                *v19 = 2 * v20 - v26 - v22;
                v19 += step;
            }

            *(short*)prv = v20 & 0xFFFF;
            *(short*)(prv + 2) = v22 & 0xFFFF;

            prv += 4;
            buf += 4;
            v32--;
        }
    }
}

// 0x4BF5AC
static void untransform_subband(unsigned char* prv, unsigned char* buf, int step, int count)
{
    int v13;
    int* v14;
    int* v25;
    int* v26;
    int v15;
    int v16;
    int v17;

    int* v18;
    int v19;
    int* v20;
    int* v21;

    v26 = (int*)prv;
    v25 = (int*)buf;

    if (count == 4) {
        unsigned char* v4 = buf + 4 * step;
        unsigned char* v5 = buf + 3 * step;
        unsigned char* v6 = buf + 2 * step;
        int v7;
        int v8;
        int v9;
        int v10;
        int v11;
        while (step--) {
            v7 = *(unsigned int*)(v26 + 4);
            v8 = *(unsigned int*)v25;
            *(unsigned int*)v25 = *(unsigned int*)v26 + 2 * v7;

            v9 = *(unsigned int*)v4;
            *(unsigned int*)v4 = 2 * v8 - v7 - v9;

            v10 = *(unsigned int*)v6;
            v5 += 4;
            *v6 += 2 * v9 + v8;

            v11 = *(unsigned int*)(v5 - 4);
            v6 += 4;

            *(unsigned int*)(v5 - 4) = 2 * v10 - v9 - v11;
            v4 += 4;

            *(unsigned int*)v26 = v10;
            *(unsigned int*)(v26 + 4) = v11;

            v26 += 2;
            v25 += 1;
        }
    } else {
        int v24 = step;

        while (v24 != 0) {
            v13 = count >> 2;
            v14 = v25;
            v15 = v26[0];
            v16 = v26[1];

            while (--v13 != -1) {
                v17 = *v14;
                *v14 += 2 * v16 + v15;

                v18 = v14 + step;
                v19 = *v18;
                *v18 = 2 * v17 - v16 - v19;

                v20 = v18 + step;
                v15 = *v20;
                *v20 += 2 * v19 + v17;

                v21 = v20 + step;
                v16 = *v21;
                *v21 = 2 * v15 - v19 - v16;

                v14 = v21 + step;
            }

            v26[0] = v15;
            v26[1] = v16;

            v26 += 2;
            v25 += 1;

            v24--;
        }
    }
}

// 0x4BF710
static void untransform_all(AudioDecoder* ad)
{
    int v8;
    unsigned char* ptr;
    int v3;
    int v4;
    unsigned char* j;
    int v6;
    int* v5;

    if (!ad->levels) {
        return;
    }

    ptr = ad->samples;

    v8 = ad->samples_per_subband;
    while (v8 > 0) {
        v3 = ad->subbands >> 1;
        v4 = ad->block_samples_per_subband;
        if (v4 > v8) {
            v4 = v8;
        }

        v4 *= 2;

        untransform_subband0(ad->prev_samples, ptr, v3, v4);

        v5 = (int*)ptr;
        for (v6 = 0; v6 < v4; v6++) {
            *v5 += 1;
            v5 += v3;
        }

        j = 4 * v3 + ad->prev_samples;
        while (1) {
            v3 >>= 1;
            v4 *= 2;
            if (v3 == 0) {
                break;
            }
            untransform_subband(j, ptr, v3, v4);
            j += 8 * v3;
        }

        ptr += ad->block_total_samples * 4;
        v8 -= ad->block_samples_per_subband;
    }
}

// 0x4BF7E8
static bool AudioDecoder_fill(AudioDecoder* ad)
{
    if (!ReadBands(ad)) {
        return false;
    }

    untransform_all(ad);

    ad->file_cnt -= ad->total_samples;
    ad->samp_ptr = ad->samples;
    ad->samp_cnt = ad->total_samples;

    if (ad->file_cnt < 0) {
        ad->samp_cnt += ad->file_cnt;
        ad->file_cnt = 0;
    }

    return true;
}

// 0x4BF830
size_t AudioDecoder_Read(AudioDecoder* ad, void* buffer, size_t size)
{
    unsigned char* dest;
    unsigned char* samp_ptr;
    int samp_cnt;

    dest = (unsigned char*)buffer;
    samp_ptr = ad->samp_ptr;
    samp_cnt = ad->samp_cnt;

    size_t bytesRead;
    for (bytesRead = 0; bytesRead < size; bytesRead += 2) {
        if (samp_cnt == 0) {
            if (ad->file_cnt == 0) {
                break;
            }

            if (!AudioDecoder_fill(ad)) {
                break;
            }

            samp_ptr = ad->samp_ptr;
            samp_cnt = ad->samp_cnt;
        }

        int v13 = *(int*)samp_ptr;
        samp_ptr += 4;
        *(unsigned short*)(dest + bytesRead) = (v13 >> ad->levels) & 0xFFFF;
        samp_cnt--;
    }

    ad->samp_ptr = samp_ptr;
    ad->samp_cnt = samp_cnt;

    return bytesRead;
}

// 0x4BF8D8
void AudioDecoder_Close(AudioDecoder* ad)
{
    if (ad->bits.bytes.buf != NULL) {
        free(ad->bits.bytes.buf);
    }

    if (ad->prev_samples != NULL) {
        free(ad->prev_samples);
    }

    if (ad->samples != NULL) {
        free(ad->samples);
    }

    free(ad);

    AudioDecoder_cnt--;

    if (AudioDecoder_cnt == 0) {
        if (AudioDecoder_scale_tbl != NULL) {
            free(AudioDecoder_scale_tbl);
            AudioDecoder_scale_tbl = NULL;
        }
    }
}

// 0x4BF938
AudioDecoder* Create_AudioDecoder(AudioDecoderReadFunc* reader, void* data, int* channels, int* sampleRate, int* sampleCount)
{
    int v14;
    int v20;
    int v73;

    AudioDecoder* ad = (AudioDecoder*)malloc(sizeof(*ad));
    if (ad == NULL) {
        return NULL;
    }

    memset(ad, 0, sizeof(*ad));

    AudioDecoder_cnt++;

    if (!bits_init(&(ad->bits), reader, data)) {
        goto L66;
    }

    requireBits(ad, 24);
    v14 = ad->bits.data;
    dropBits(ad, 24);

    if ((v14 & 0xFFFFFF) != 0x32897) {
        goto L66;
    }

    requireBits(ad, 8);
    v20 = ad->bits.data;
    dropBits(ad, 8);

    if (v20 != 1) {
        goto L66;
    }

    requireBits(ad, 16);
    ad->file_cnt = ad->bits.data & 0xFFFF;
    dropBits(ad, 16);

    requireBits(ad, 16);
    ad->file_cnt |= (ad->bits.data & 0xFFFF) << 16;
    dropBits(ad, 16);

    requireBits(ad, 16);
    ad->channels = ad->bits.data & 0xFFFF;
    dropBits(ad, 16);

    requireBits(ad, 16);
    ad->rate = ad->bits.data & 0xFFFF;
    dropBits(ad, 16);

    requireBits(ad, 4);
    ad->levels = ad->bits.data & 0x0F;
    dropBits(ad, 4);

    requireBits(ad, 12);
    ad->subbands = 1 << ad->levels;
    ad->samples_per_subband = ad->bits.data & 0x0FFF;
    ad->total_samples = ad->samples_per_subband * ad->subbands;
    dropBits(ad, 12);

    if (ad->levels != 0) {
        v73 = 3 * ad->subbands / 2 - 2;
    } else {
        v73 = 0;
    }

    ad->block_samples_per_subband = 2048 / ad->subbands - 2;
    if (ad->block_samples_per_subband < 1) {
        ad->block_samples_per_subband = 1;
    }

    ad->block_total_samples = ad->block_samples_per_subband * ad->subbands;

    if (v73 != 0) {
        ad->prev_samples = (unsigned char*)malloc(sizeof(unsigned char*) * v73);
        if (ad->prev_samples == NULL) {
            goto L66;
        }

        memset(ad->prev_samples, 0, sizeof(unsigned char*) * v73);
    }

    ad->samples = (unsigned char*)malloc(sizeof(unsigned char*) * ad->total_samples);
    if (ad->samples == NULL) {
        goto L66;
    }

    ad->samp_cnt = 0;

    if (AudioDecoder_cnt == 1) {
        AudioDecoder_scale_tbl = (unsigned char*)malloc(0x20000);
        AudioDecoder_scale0 = AudioDecoder_scale_tbl + 0x10000;
    }

    *channels = ad->channels;
    *sampleRate = ad->rate;
    *sampleCount = ad->file_cnt;

    return ad;

L66:

    AudioDecoder_Close(ad);

    *channels = 0;
    *sampleRate = 0;
    *sampleCount = 0;

    return 0;
}

static inline void requireBits(AudioDecoder* ad, int n)
{
    while (ad->bits.bitcnt < n) {
        ad->bits.bytes.buf_cnt--;

        unsigned char ch;
        if (ad->bits.bytes.buf_cnt < 0) {
            ch = ByteReaderFill(&(ad->bits.bytes));
        } else {
            ch = *ad->bits.bytes.buf_ptr++;
        }
        ad->bits.data |= ch << ad->bits.bitcnt;
        ad->bits.bitcnt += 8;
    }
}

static inline void dropBits(AudioDecoder* ad, int n)
{
    ad->bits.data >>= n;
    ad->bits.bitcnt -= n;
}
