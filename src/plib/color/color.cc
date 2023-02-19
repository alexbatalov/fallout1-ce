#include "plib/color/color.h"

#include <math.h>
#include <string.h>

#include <algorithm>

#include "plib/gnw/input.h"
#include "plib/gnw/svga.h"

namespace fallout {

static void* colorOpen(const char* filePath);
static int colorRead(void* handle, void* buffer, size_t size);
static int colorClose(void* handle);
static void* defaultMalloc(size_t size);
static void* defaultRealloc(void* ptr, size_t size);
static void defaultFree(void* ptr);
static void setIntensityTableColor(int a1);
static void setIntensityTables();
static void setMixTableColor(int a1);
static void setMixTable();
static void buildBlendTable(unsigned char* ptr, unsigned char ch);
static void rebuildColorBlendTables();
static void maxfill();

// 0x4FE0DC
static char _aColor_cNoError[] = "color.c: No errors\n";

// 0x4FE108
static char _aColor_cColorTa[] = "color.c: color table not found\n";

// 0x4FE130
static char _aColor_cColorpa[] = "color.c: colorpalettestack overflow";

// 0x4FE158
static char aColor_cColor_0[] = "color.c: colorpalettestack underflow";

// 0x539EE0
static char* errorStr = _aColor_cNoError;

// 0x539EE4
static bool colorsInited = false;

// 0x539EE8
static double currentGamma = 1.0;

// 0x539EF0
static fade_bk_func* colorFadeBkFuncP = NULL;

// 0x539EF4
static ColorMallocFunc* mallocPtr = defaultMalloc;

// 0x539EF8
static ColorReallocFunc* reallocPtr = defaultRealloc;

// 0x539EFC
static ColorFreeFunc* freePtr = defaultFree;

// 0x539F00
static ColorNameMangleFunc* colorNameMangler = NULL;

// 0x539F04
unsigned char cmap[768] = {
    0x3F, 0x3F, 0x3F
};

// 0x673280
static ColorPaletteStackEntry* colorPaletteStack[COLOR_PALETTE_STACK_CAPACITY];

// 0x6732C0
static unsigned char systemCmap[256 * 3];

// 0x6735C0
static unsigned char currentGammaTable[64];

// 0x673600
static unsigned char* blendTable[256];

// 0x673A00
unsigned char mappedColor[256];

// 0x673B00
Color colorMixAddTable[256][256];

// 0x683B00
Color intensityColorTable[256][256];

// 0x693B00
Color colorMixMulTable[256][256];

// 0x6A3B00
unsigned char colorTable[32768];

// 0x6ABB00
static int tos;

// 0x6ABB58
static ColorReadFunc* readFunc;

// 0x6ABB5C
static ColorCloseFunc* closeFunc;

// 0x6ABB60
static ColorOpenFunc* openFunc;

// 0x4BFDC0
static void* colorOpen(const char* filePath)
{
    if (openFunc != NULL) {
        return openFunc(filePath);
    }

    return nullptr;
}

// 0x4BFDD8
static int colorRead(void* fd, void* buffer, size_t size)
{
    if (readFunc != NULL) {
        return readFunc(fd, buffer, size);
    }

    return -1;
}

// 0x4BFDF0
static int colorClose(void* fd)
{
    if (closeFunc != NULL) {
        return closeFunc(fd);
    }

    return -1;
}

// 0x4BFE08
void colorInitIO(ColorOpenFunc* openProc, ColorReadFunc* readProc, ColorCloseFunc* closeProc)
{
    openFunc = openProc;
    readFunc = readProc;
    closeFunc = closeProc;
}

// 0x4BFE1C
static void* defaultMalloc(size_t size)
{
    return malloc(size);
}

// 0x4BFE24
static void* defaultRealloc(void* ptr, size_t size)
{
    return realloc(ptr, size);
}

// 0x4BFE2C
static void defaultFree(void* ptr)
{
    free(ptr);
}

// 0x4BFE34
void colorSetNameMangler(ColorNameMangleFunc* c)
{
    colorNameMangler = c;
}

// 0x4BFE3C
Color colorMixAdd(Color a, Color b)
{
    return colorMixAddTable[a][b];
}

// 0x4BFE58
Color colorMixMul(Color a, Color b)
{
    return colorMixMulTable[a][b];
}

// 0x4BFE74
int calculateColor(int a1, int a2)
{
    return intensityColorTable[a2][a1 >> 9];
}

// 0x4BFE8C
Color RGB2Color(ColorRGB c)
{
    return colorTable[c];
}

// 0x4BFEA0
int Color2RGB(int a1)
{
    int v1, v2, v3;

    v1 = cmap[3 * a1] >> 1;
    v2 = cmap[3 * a1 + 1] >> 1;
    v3 = cmap[3 * a1 + 2] >> 1;

    return (((v1 << 5) | v2) << 5) | v3;
}

// Performs animated palette transition.
//
// 0x4BFEE0
void fadeSystemPalette(unsigned char* oldPalette, unsigned char* newPalette, int steps)
{
    for (int step = 0; step < steps; step++) {
        sharedFpsLimiter.mark();

        unsigned char palette[768];

        for (int index = 0; index < 768; index++) {
            palette[index] = oldPalette[index] - (oldPalette[index] - newPalette[index]) * step / steps;
        }

        if (colorFadeBkFuncP != NULL) {
            if (step % 128 == 0) {
                colorFadeBkFuncP();
            }
        }

        setSystemPalette(palette);
        renderPresent();
        sharedFpsLimiter.throttle();
    }

    sharedFpsLimiter.mark();
    setSystemPalette(newPalette);
    renderPresent();
    sharedFpsLimiter.throttle();
}

// 0x4BFF94
void colorSetFadeBkFunc(fade_bk_func* callback)
{
    colorFadeBkFuncP = callback;
}

// 0x4BFF9C
void setBlackSystemPalette()
{
    // 0x6AB934
    static unsigned char tmp[768];
    setSystemPalette(tmp);
}

// 0x4BFFA4
void setSystemPalette(unsigned char* palette)
{
    unsigned char newPalette[768];

    for (int index = 0; index < 768; index++) {
        newPalette[index] = currentGammaTable[palette[index]];
        systemCmap[index] = palette[index];
    }

    GNW95_SetPalette(newPalette);
}

// 0x4BFFE0
unsigned char* getSystemPalette()
{
    return systemCmap;
}

// 0x4BFFE8
void setSystemPaletteEntries(unsigned char* palette, int start, int end)
{
    unsigned char newPalette[768];

    int length = end - start + 1;
    for (int index = 0; index < length; index++) {
        newPalette[index * 3] = currentGammaTable[palette[index * 3]];
        newPalette[index * 3 + 1] = currentGammaTable[palette[index * 3 + 1]];
        newPalette[index * 3 + 2] = currentGammaTable[palette[index * 3 + 2]];

        systemCmap[start * 3 + index * 3] = palette[index * 3];
        systemCmap[start * 3 + index * 3 + 1] = palette[index * 3 + 1];
        systemCmap[start * 3 + index * 3 + 2] = palette[index * 3 + 2];
    }

    GNW95_SetPaletteEntries(newPalette, start, end - start + 1);
}

// 0x4C00D8
void getSystemPaletteEntry(int entry, unsigned char* r, unsigned char* g, unsigned char* b)
{
    int baseIndex;

    baseIndex = entry * 3;
    *r = systemCmap[baseIndex];
    *g = systemCmap[baseIndex + 1];
    *b = systemCmap[baseIndex + 2];
}

// 0x4C00FC
static void setIntensityTableColor(int a1)
{
    int v1, v2, v3, v4, v5, v6, v7, v8, v9;

    v5 = 0;

    for (int index = 0; index < 128; index++) {
        v1 = (Color2RGB(a1) & 0x7C00) >> 10;
        v2 = (Color2RGB(a1) & 0x3E0) >> 5;
        v3 = (Color2RGB(a1) & 0x1F);

        v4 = (((v1 * v5) >> 16) << 10) | (((v2 * v5) >> 16) << 5) | ((v3 * v5) >> 16);
        intensityColorTable[a1][index] = colorTable[v4];

        v6 = v1 + (((0x1F - v1) * v5) >> 16);
        v7 = v2 + (((0x1F - v2) * v5) >> 16);
        v8 = v3 + (((0x1F - v3) * v5) >> 16);

        v9 = (v6 << 10) | (v7 << 5) | v8;
        intensityColorTable[a1][0x7F + index + 1] = colorTable[v9];

        v5 += 0x200;
    }
}

// 0x4C0204
static void setIntensityTables()
{
    for (int index = 0; index < 256; index++) {
        if (mappedColor[index] != 0) {
            setIntensityTableColor(index);
        } else {
            memset(intensityColorTable[index], 0, 256);
        }
    }
}

// 0x4C0248
static void setMixTableColor(int a1)
{
    int i;
    int v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19;
    int v20, v21, v22, v23, v24, v25, v26, v27, v28, v29;

    for (i = 0; i < 256; i++) {
        if (mappedColor[a1] && mappedColor[i]) {
            v2 = (Color2RGB(a1) & 0x7C00) >> 10;
            v3 = (Color2RGB(a1) & 0x3E0) >> 5;
            v4 = (Color2RGB(a1) & 0x1F);

            v5 = (Color2RGB(i) & 0x7C00) >> 10;
            v6 = (Color2RGB(i) & 0x3E0) >> 5;
            v7 = (Color2RGB(i) & 0x1F);

            v8 = v2 + v5;
            v9 = v3 + v6;
            v10 = v4 + v7;

            v11 = v8;

            if (v9 > v11) {
                v11 = v9;
            }

            if (v10 > v11) {
                v11 = v10;
            }

            if (v11 <= 0x1F) {
                int paletteIndex = (v8 << 10) | (v9 << 5) | v10;
                v12 = colorTable[paletteIndex];
            } else {
                v13 = v11 - 0x1F;

                v14 = v8 - v13;
                v15 = v9 - v13;
                v16 = v10 - v13;

                if (v14 < 0) {
                    v14 = 0;
                }

                if (v15 < 0) {
                    v15 = 0;
                }

                if (v16 < 0) {
                    v16 = 0;
                }

                v17 = (v14 << 10) | (v15 << 5) | v16;
                v18 = colorTable[v17];

                v19 = (int)((((double)v11 + (-31.0)) * 0.0078125 + 1.0) * 65536.0);
                v12 = calculateColor(v19, v18);
            }

            colorMixAddTable[a1][i] = v12;

            v20 = (Color2RGB(a1) & 0x7C00) >> 10;
            v21 = (Color2RGB(a1) & 0x3E0) >> 5;
            v22 = (Color2RGB(a1) & 0x1F);

            v23 = (Color2RGB(i) & 0x7C00) >> 10;
            v24 = (Color2RGB(i) & 0x3E0) >> 5;
            v25 = (Color2RGB(i) & 0x1F);

            v26 = (v20 * v23) >> 5;
            v27 = (v21 * v24) >> 5;
            v28 = (v22 * v25) >> 5;

            v29 = (v26 << 10) | (v27 << 5) | v28;
            colorMixMulTable[a1][i] = colorTable[v29];
        } else {
            if (mappedColor[i]) {
                colorMixAddTable[a1][i] = i;
                colorMixMulTable[a1][i] = i;
            } else {
                colorMixAddTable[a1][i] = a1;
                colorMixMulTable[a1][i] = a1;
            }
        }
    }
}

// 0x4C0454
static void setMixTable()
{
    int i;

    for (i = 0; i < 256; i++) {
        setMixTableColor(i);
    }
}

// 0x4C046C
bool loadColorTable(const char* path)
{
    if (colorNameMangler != NULL) {
        path = colorNameMangler(path);
    }

    // NOTE: Uninline.
    void* handle = colorOpen(path);
    if (handle == nullptr) {
        errorStr = _aColor_cColorTa;
        return false;
    }

    for (int index = 0; index < 256; index++) {
        unsigned char r;
        unsigned char g;
        unsigned char b;

        // NOTE: Uninline.
        colorRead(handle, &r, sizeof(r));

        // NOTE: Uninline.
        colorRead(handle, &g, sizeof(g));

        // NOTE: Uninline.
        colorRead(handle, &b, sizeof(b));

        if (r <= 0x3F && g <= 0x3F && b <= 0x3F) {
            mappedColor[index] = 1;
        } else {
            r = 0;
            g = 0;
            b = 0;
            mappedColor[index] = 0;
        }

        cmap[index * 3] = r;
        cmap[index * 3 + 1] = g;
        cmap[index * 3 + 2] = b;
    }

    // NOTE: Uninline.
    colorRead(handle, colorTable, 0x8000);

    unsigned int type;
    // NOTE: Uninline.
    colorRead(handle, &type, sizeof(type));

    if (type == 'NEWC') {
        // NOTE: Uninline.
        colorRead(handle, intensityColorTable, 0x10000);

        // NOTE: Uninline.
        colorRead(handle, colorMixAddTable, 0x10000);

        // NOTE: Uninline.
        colorRead(handle, colorMixMulTable, 0x10000);
    } else {
        setIntensityTables();

        for (int index = 0; index < 256; index++) {
            setMixTableColor(index);
        }
    }

    rebuildColorBlendTables();

    // NOTE: Uninline.
    colorClose(handle);

    return true;
}

// 0x4C063C
char* colorError()
{
    return errorStr;
}

// 0x4C0644
void setColorPalette(unsigned char* pal)
{
    memcpy(cmap, pal, sizeof(cmap));
    memset(mappedColor, 1, sizeof(mappedColor));
}

// 0x4C0680
void setColorPaletteEntry(int entry, unsigned char r, unsigned char g, unsigned char b)
{
    int baseIndex;

    baseIndex = entry * 3;
    cmap[baseIndex] = r;
    cmap[baseIndex + 1] = g;
    cmap[baseIndex + 2] = b;
}

// 0x4C06A8
void getColorPaletteEntry(int entry, unsigned char* r, unsigned char* g, unsigned char* b)
{
    int baseIndex;

    baseIndex = entry * 3;
    *r = cmap[baseIndex];
    *g = cmap[baseIndex + 1];
    *b = cmap[baseIndex + 2];
}

// 0x4C06CC
static void buildBlendTable(unsigned char* ptr, unsigned char ch)
{
    int r, g, b;
    int i, j;
    int v12, v14, v16;
    unsigned char* beg;

    beg = ptr;

    r = (Color2RGB(ch) & 0x7C00) >> 10;
    g = (Color2RGB(ch) & 0x3E0) >> 5;
    b = (Color2RGB(ch) & 0x1F);

    for (i = 0; i < 256; i++) {
        ptr[i] = i;
    }

    ptr += 256;

    int b_1 = b;
    int v31 = 6;
    int g_1 = g;
    int r_1 = r;

    int b_2 = b_1;
    int g_2 = g_1;
    int r_2 = r_1;

    for (j = 0; j < 7; j++) {
        for (i = 0; i < 256; i++) {
            v12 = (Color2RGB(i) & 0x7C00) >> 10;
            v14 = (Color2RGB(i) & 0x3E0) >> 5;
            v16 = (Color2RGB(i) & 0x1F);
            int index = 0;
            index |= (r_2 + v12 * v31) / 7 << 10;
            index |= (g_2 + v14 * v31) / 7 << 5;
            index |= (b_2 + v16 * v31) / 7;
            ptr[i] = colorTable[index];
        }
        v31--;
        ptr += 256;
        r_2 += r_1;
        g_2 += g_1;
        b_2 += b_1;
    }

    int v18 = 0;
    for (j = 0; j < 6; j++) {
        int v20 = v18 / 7 + 0xFFFF;

        for (i = 0; i < 256; i++) {
            ptr[i] = calculateColor(v20, ch);
        }

        v18 += 0x10000;
        ptr += 256;
    }
}

// 0x4C0918
static void rebuildColorBlendTables()
{
    int i;

    for (i = 0; i < 256; i++) {
        if (blendTable[i]) {
            buildBlendTable(blendTable[i], i);
        }
    }
}

// 0x4C0948
unsigned char* getColorBlendTable(int ch)
{
    unsigned char* ptr;

    if (blendTable[ch] == NULL) {
        ptr = (unsigned char*)mallocPtr(4100);
        *(int*)ptr = 1;
        blendTable[ch] = ptr + 4;
        buildBlendTable(blendTable[ch], ch);
    }

    ptr = blendTable[ch];
    *(int*)((unsigned char*)ptr - 4) = *(int*)((unsigned char*)ptr - 4) + 1;

    return ptr;
}

// 0x4C09A8
void freeColorBlendTable(int a1)
{
    unsigned char* v2 = blendTable[a1];
    if (v2 != NULL) {
        int* count = (int*)(v2 - sizeof(int));
        *count -= 1;
        if (*count == 0) {
            freePtr(count);
            blendTable[a1] = NULL;
        }
    }
}

// 0x4C09E0
void colorRegisterAlloc(ColorMallocFunc* mallocProc, ColorReallocFunc* reallocProc, ColorFreeFunc* freeProc)
{
    mallocPtr = mallocProc;
    reallocPtr = reallocProc;
    freePtr = freeProc;
}

// 0x4C09F4
void colorGamma(double value)
{
    currentGamma = value;

    for (int i = 0; i < 64; i++) {
        double value = pow(i, currentGamma);
        currentGammaTable[i] = (unsigned char)std::clamp(value, 0.0, 63.0);
    }

    setSystemPalette(systemCmap);
}

// 0x4C0A90
double colorGetGamma()
{
    return currentGamma;
}

// 0x4C0A98
int colorMappedColor(ColorIndex i)
{
    return mappedColor[i];
}

// 0x4C1388
static void maxfill(unsigned long* buffer, int side)
{
    unsigned long maxv;
    long i;
    unsigned long* bp;

    bp = buffer;
    maxv = side * side * side;
    for (i = maxv; i > 0; i--) {
        *bp++ = -1;
    }
}

// 0x4C13AC
bool colorPushColorPalette()
{
    if (tos >= COLOR_PALETTE_STACK_CAPACITY) {
        errorStr = _aColor_cColorpa;
        return false;
    }

    ColorPaletteStackEntry* entry = (ColorPaletteStackEntry*)malloc(sizeof(*entry));
    colorPaletteStack[tos] = entry;

    memcpy(entry->mappedColors, mappedColor, sizeof(mappedColor));
    memcpy(entry->cmap, cmap, sizeof(cmap));
    memcpy(entry->colorTable, colorTable, sizeof(colorTable));

    tos++;

    return true;
}

// 0x4C1464
bool colorPopColorPalette()
{
    if (tos == 0) {
        errorStr = aColor_cColor_0;
        return false;
    }

    tos--;

    ColorPaletteStackEntry* entry = colorPaletteStack[tos];

    memcpy(mappedColor, entry->mappedColors, sizeof(mappedColor));
    memcpy(cmap, entry->cmap, sizeof(cmap));
    memcpy(colorTable, entry->colorTable, sizeof(colorTable));

    free(entry);
    colorPaletteStack[tos] = NULL;

    setIntensityTables();

    for (int index = 0; index < 256; index++) {
        setMixTableColor(index);
    }

    rebuildColorBlendTables();

    return true;
}

// 0x4C1550
bool initColors()
{
    if (colorsInited) {
        return true;
    }

    colorsInited = true;

    colorGamma(1.0);

    if (!loadColorTable("color.pal")) {
        return false;
    }

    setSystemPalette(cmap);

    return true;
}

// 0x4C159C
void colorsClose()
{
    for (int index = 0; index < 256; index++) {
        freeColorBlendTable(index);
    }

    for (int index = 0; index < tos; index++) {
        free(colorPaletteStack[index]);
    }

    tos = 0;
}

// 0x4C15E8
unsigned char* getColorPalette()
{
    return cmap;
}

} // namespace fallout
