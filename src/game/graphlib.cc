#include "game/graphlib.h"

#include <string.h>

#include <algorithm>

#include "plib/color/color.h"
#include "plib/gnw/debug.h"
#include "plib/gnw/memory.h"

namespace fallout {

static void InitTree();
static void InsertNode(int a1);
static void DeleteNode(int a1);

// 0x59532C
static unsigned char GreyTable[256];

// 0x59542C
int* dad;

// 0x595430
int match_length;

// 0x595434
int textsize;

// 0x595438
int* rson;

// 0x59543C
int* lson;

// 0x595440
unsigned char* text_buf;

// 0x595444
int codesize;

// 0x595448
int match_position;

// 0x4464F0
int HighRGB(int a1)
{
    // TODO: Some strange bit arithmetic.
    int v1 = Color2RGB(a1);
    int r = (v1 & 0x7C00) >> 10;
    int g = (v1 & 0x3E0) >> 5;
    int b = (v1 & 0x1F);

    int result = g;
    if (r > result) {
        result = r;
    }

    result = result & 0xFF;
    if (result <= b) {
        result = b;
    }

    return result;
}

// 0x44652C
void bit1exbit8(int ulx, int uly, int lrx, int lry, int offset_x, int offset_y, unsigned char* src, unsigned char* dest, int src_pitch, int dest_pitch, unsigned char color)
{
    int x;
    int y;
    int width;
    int height;
    int mask;
    int bits;
    unsigned char* src_base;
    unsigned char* src_curr;

    width = lrx - ulx + 1;
    height = lry - uly + 1;

    src_base = src + uly * (src_pitch / 8);
    src_curr = src_base + ulx / 8;
    bits = *src_curr++;

    dest += dest_pitch * offset_y + offset_x;

    for (y = 0; y < height; y++) {
        mask = 128 >> (ulx % 8);

        for (x = 0; x < width; x++) {
            if ((bits & mask) != 0) {
                *dest = color;
            }

            mask >>= 1;
            if (mask == 0) {
                bits = *src_curr++;
                mask = 128;
            }

            dest++;
        }

        src_base += src_pitch / 8;
        src_curr = src_base + ulx / 8;
        bits = *src_curr++;

        dest += dest_pitch - width;
    }
}

// 0x446B80
int CompLZS(unsigned char* a1, unsigned char* a2, int a3)
{
    dad = NULL;
    rson = NULL;
    lson = NULL;
    text_buf = NULL;

    // NOTE: Original code is slightly different, it uses deep nesting or a
    // bunch of gotos.
    lson = (int*)mem_malloc(sizeof(*lson) * 4104);
    rson = (int*)mem_malloc(sizeof(*rson) * 4376);
    dad = (int*)mem_malloc(sizeof(*dad) * 4104);
    text_buf = (unsigned char*)mem_malloc(sizeof(*text_buf) * 4122);

    if (lson == NULL || rson == NULL || dad == NULL || text_buf == NULL) {
        debug_printf("\nGRAPHLIB: Error allocating compression buffers!\n");

        if (dad != NULL) {
            mem_free(dad);
        }

        if (rson != NULL) {
            mem_free(rson);
        }
        if (lson != NULL) {
            mem_free(lson);
        }
        if (text_buf != NULL) {
            mem_free(text_buf);
        }

        return -1;
    }

    InitTree();

    memset(text_buf, ' ', 4078);

    int count = 0;
    int v30 = 0;
    for (int index = 4078; index < 4096; index++) {
        text_buf[index] = *a1++;
        int v8 = v30++;
        if (v8 > a3) {
            break;
        }
        count++;
    }

    textsize = count;

    for (int index = 4077; index > 4059; index--) {
        InsertNode(index);
    }

    InsertNode(4078);

    unsigned char v29[32];
    v29[1] = 0;

    int v3 = 4078;
    int v4 = 0;
    int v10 = 0;
    int v36 = 1;
    unsigned char v41 = 1;
    int rc = 0;
    while (count != 0) {
        if (count < match_length) {
            match_length = count;
        }

        int v11 = v36 + 1;
        if (match_length > 2) {
            v29[v36 + 1] = match_position;
            v29[v36 + 2] = ((match_length - 3) | ((match_position >> 4) & 0xF0));
            v36 = v11 + 1;
        } else {
            match_length = 1;
            v29[1] |= v41;
            int v13 = v36++;
            v29[v13 + 1] = text_buf[v3];
        }

        v41 *= 2;

        if (v41 == 0) {
            v11 = 0;
            if (v36 != 0) {
                for (;;) {
                    v4++;
                    *a2++ = v29[v11 + 1];
                    if (v4 > a3) {
                        rc = -1;
                        break;
                    }

                    v11++;
                    if (v11 >= v36) {
                        break;
                    }
                }

                if (rc == -1) {
                    break;
                }
            }

            codesize += v36;
            v29[1] = 0;
            v36 = 1;
            v41 = 1;
        }

        int v16;
        int v38 = match_length;
        for (v16 = 0; v16 < v38; v16++) {
            unsigned char v34 = *a1++;
            int v17 = v30++;
            if (v17 >= a3) {
                break;
            }

            DeleteNode(v10);

            unsigned char* v19 = text_buf + v10;
            text_buf[v10] = v34;

            if (v10 < 17) {
                v19[4096] = v34;
            }

            v3 = (v3 + 1) & 0xFFF;
            v10 = (v10 + 1) & 0xFFF;
            InsertNode(v3);
        }

        for (; v16 < v38; v16++) {
            DeleteNode(v10);
            v3 = (v3 + 1) & 0xFFF;
            v10 = (v10 + 1) & 0xFFF;
            if (--count != 0) {
                InsertNode(v3);
            }
        }
    }

    if (rc != -1) {
        for (int v23 = 0; v23 < v36; v23++) {
            v4++;
            v10++;
            *a2++ = v29[v23 + 1];
            if (v10 > a3) {
                rc = -1;
                break;
            }
        }

        codesize += v36;
    }

    mem_free(lson);
    mem_free(rson);
    mem_free(dad);
    mem_free(text_buf);

    if (rc == -1) {
        v4 = -1;
    }

    return v4;
}

// 0x446F20
static void InitTree()
{
    for (int index = 4097; index < 4353; index++) {
        rson[index] = 4096;
    }

    for (int index = 0; index < 4096; index++) {
        dad[index] = 4096;
    }
}

// 0x446F6C
static void InsertNode(int a1)
{
    lson[a1] = 4096;
    rson[a1] = 4096;
    match_length = 0;

    unsigned char* v2 = text_buf + a1;

    int v21 = 4097 + text_buf[a1];
    int v5 = 1;
    for (;;) {
        int v6 = v21;
        if (v5 < 0) {
            if (lson[v6] == 4096) {
                lson[v6] = a1;
                dad[a1] = v21;
                return;
            }
            v21 = lson[v6];
        } else {
            if (rson[v6] == 4096) {
                rson[v6] = a1;
                dad[a1] = v21;
                return;
            }
            v21 = rson[v6];
        }

        int v9;
        unsigned char* v10 = v2 + 1;
        int v11 = v21 + 1;
        for (v9 = 1; v9 < 18; v9++) {
            v5 = *v10 - text_buf[v11];
            if (v5 != 0) {
                break;
            }
            v10++;
            v11++;
        }

        if (v9 > match_length) {
            match_length = v9;
            match_position = v21;
            if (v9 >= 18) {
                break;
            }
        }
    }

    dad[a1] = dad[v21];
    lson[a1] = lson[v21];
    rson[a1] = rson[v21];

    dad[lson[v21]] = a1;
    dad[rson[v21]] = a1;

    if (rson[dad[v21]] == v21) {
        rson[dad[v21]] = a1;
    } else {
        lson[dad[v21]] = a1;
    }

    dad[v21] = 4096;
}

// 0x44711C
static void DeleteNode(int a1)
{
    if (dad[a1] != 4096) {
        int v5;
        if (rson[a1] == 4096) {
            v5 = lson[a1];
        } else {
            if (lson[a1] == 4096) {
                v5 = rson[a1];
            } else {
                v5 = lson[a1];

                if (rson[v5] != 4096) {
                    do {
                        v5 = rson[v5];
                    } while (rson[v5] != 4096);

                    rson[dad[v5]] = lson[v5];
                    dad[lson[v5]] = dad[v5];
                    lson[v5] = lson[a1];
                    dad[lson[a1]] = v5;
                }

                rson[v5] = rson[a1];
                dad[rson[a1]] = v5;
            }
        }

        dad[v5] = dad[a1];

        if (rson[dad[a1]] == a1) {
            rson[dad[a1]] = v5;
        } else {
            lson[dad[a1]] = v5;
        }
        dad[a1] = 4096;
    }
}

// 0x44725C
int DecodeLZS(unsigned char* src, unsigned char* dest, int length)
{
    text_buf = (unsigned char*)mem_malloc(sizeof(*text_buf) * 4122);
    if (text_buf == NULL) {
        debug_printf("\nGRAPHLIB: Error allocating decompression buffer!\n");
        return -1;
    }

    int v8 = 4078;
    memset(text_buf, ' ', v8);

    int v21 = 0;
    int index = 0;
    while (index < length) {
        v21 >>= 1;
        if ((v21 & 0x100) == 0) {
            v21 = *src++;
            v21 |= 0xFF00;
        }

        if ((v21 & 0x01) == 0) {
            int v10 = *src++;
            int v11 = *src++;

            v10 |= (v11 & 0xF0) << 4;
            v11 &= 0x0F;
            v11 += 2;

            for (int v16 = 0; v16 <= v11; v16++) {
                int v17 = (v10 + v16) & 0xFFF;

                unsigned char ch = text_buf[v17];
                text_buf[v8] = ch;
                *dest++ = ch;

                v8 = (v8 + 1) & 0xFFF;

                index++;
                if (index >= length) {
                    break;
                }
            }
        } else {
            unsigned char ch = *src++;
            text_buf[v8] = ch;
            *dest++ = ch;

            v8 = (v8 + 1) & 0xFFF;

            index++;
        }
    }

    mem_free(text_buf);

    return 0;
}

// 0x4473A8
void InitGreyTable(int a1, int a2)
{
    if (a1 >= 0 && a2 <= 255) {
        for (int index = a1; index <= a2; index++) {
            // NOTE: The only way to explain so much calls to [Color2RGB] with
            // the same repeated pattern is by the use of min/max macros.

            int v1 = std::max((Color2RGB(index) & 0x7C00) >> 10, std::max((Color2RGB(index) & 0x3E0) >> 5, Color2RGB(index) & 0x1F));
            int v2 = std::min((Color2RGB(index) & 0x7C00) >> 10, std::min((Color2RGB(index) & 0x3E0) >> 5, Color2RGB(index) & 0x1F));
            int v3 = v1 + v2;
            int v4 = (int)((double)v3 * 240.0 / 510.0);

            int paletteIndex = ((v4 & 0xFF) << 10) | ((v4 & 0xFF) << 5) | (v4 & 0xFF);
            GreyTable[index] = colorTable[paletteIndex];
        }
    }
}

// 0x447570
void grey_buf(unsigned char* buffer, int width, int height, int pitch)
{
    unsigned char* ptr = buffer;
    int skip = pitch - width;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            unsigned char c = *ptr;
            *ptr++ = GreyTable[c];
        }
        ptr += skip;
    }
}

} // namespace fallout
