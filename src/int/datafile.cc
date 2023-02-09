#include "int/datafile.h"

#include <string.h>

#include "int/memdbg.h"
#include "int/pcx.h"
#include "platform_compat.h"
#include "plib/color/color.h"
#include "plib/db/db.h"

namespace fallout {

static char* defaultMangleName(char* path);

// 0x504EAC
static DatafileLoader* loadFunc = NULL;

// 0x504EB0
static DatafileNameMangler* mangleName = defaultMangleName;

// 0x56BF70
static unsigned char pal[768];

// 0x429450
static char* defaultMangleName(char* path)
{
    return path;
}

// 0x429454
void datafileSetFilenameFunc(DatafileNameMangler* mangler)
{
    mangleName = mangler;
}

// 0x42945C
void setBitmapLoadFunc(DatafileLoader* loader)
{
    loadFunc = loader;
}

// 0x429464
void datafileConvertData(unsigned char* data, unsigned char* palette, int width, int height)
{
    unsigned char indexedPalette[256];

    indexedPalette[0] = 0;
    for (int index = 1; index < 256; index++) {
        // TODO: Check.
        int r = palette[index * 3 + 2] >> 3;
        int g = palette[index * 3 + 1] >> 3;
        int b = palette[index * 3] >> 3;
        int colorTableIndex = (r << 10) | (g << 5) | b;
        indexedPalette[index] = colorTable[colorTableIndex];
    }

    int size = width * height;
    for (int index = 0; index < size; index++) {
        data[index] = indexedPalette[data[index]];
    }
}

// 0x4294D8
void datafileConvertDataVGA(unsigned char* data, unsigned char* palette, int width, int height)
{
    unsigned char indexedPalette[256];

    indexedPalette[0] = 0;
    for (int index = 1; index < 256; index++) {
        // TODO: Check.
        int r = palette[index * 3 + 2] >> 1;
        int g = palette[index * 3 + 1] >> 1;
        int b = palette[index * 3] >> 1;
        int colorTableIndex = (r << 10) | (g << 5) | b;
        indexedPalette[index] = colorTable[colorTableIndex];
    }

    int size = width * height;
    for (int index = 0; index < size; index++) {
        data[index] = indexedPalette[data[index]];
    }
}

// 0x429540
unsigned char* loadRawDataFile(char* path, int* widthPtr, int* heightPtr)
{
    char* mangledPath = mangleName(path);
    char* dot = strrchr(mangledPath, '.');
    if (dot != NULL) {
        if (compat_stricmp(dot + 1, "pcx") == 0) {
            return loadPCX(mangledPath, widthPtr, heightPtr, pal);
        }
    }

    if (loadFunc != NULL) {
        return loadFunc(mangledPath, pal, widthPtr, heightPtr);
    }

    return NULL;
}

// 0x4295AC
unsigned char* loadDataFile(char* path, int* widthPtr, int* heightPtr)
{
    unsigned char* v1 = loadRawDataFile(path, widthPtr, heightPtr);
    if (v1 != NULL) {
        datafileConvertData(v1, pal, *widthPtr, *heightPtr);
    }
    return v1;
}

// 0x4295D4
unsigned char* load256Palette(char* path)
{
    int width;
    int height;
    unsigned char* v3 = loadRawDataFile(path, &width, &height);
    if (v3 != NULL) {
        myfree(v3, __FILE__, __LINE__); // "..\\int\\DATAFILE.C", 148
        return pal;
    }

    return NULL;
}

// 0x429604
void trimBuffer(unsigned char* data, int* widthPtr, int* heightPtr)
{
    int width = *widthPtr;
    int height = *heightPtr;
    unsigned char* temp = (unsigned char*)mymalloc(width * height, __FILE__, __LINE__); // "..\\int\\DATAFILE.C", 157

    // NOTE: Original code does not initialize `x`.
    int y = 0;
    int x = 0;
    unsigned char* src1 = data;

    for (y = 0; y < height; y++) {
        if (*src1 == 0) {
            break;
        }

        unsigned char* src2 = src1;
        for (x = 0; x < width; x++) {
            if (*src2 == 0) {
                break;
            }

            *temp++ = *src2++;
        }

        src1 += width;
    }

    memcpy(data, temp, x * y);
    myfree(temp, __FILE__, __LINE__); // // "..\\int\\DATAFILE.C", 171
}

// 0x4296C4
unsigned char* datafileGetPalette()
{
    return pal;
}

// 0x4296CC
unsigned char* datafileLoadBlock(char* path, int* sizePtr)
{
    const char* mangledPath = mangleName(path);
    DB_FILE* stream = db_fopen(mangledPath, "rb");
    if (stream == NULL) {
        return NULL;
    }

    int size = db_filelength(stream);
    unsigned char* data = (unsigned char*)mymalloc(size, __FILE__, __LINE__); // "..\\int\\DATAFILE.C", 185
    if (data == NULL) {
        // NOTE: This code is unreachable, mymalloc never fails.
        // Otherwise it leaks stream.
        *sizePtr = 0;
        return NULL;
    }

    db_fread(data, 1, size, stream);
    db_fclose(stream);
    *sizePtr = size;
    return data;
}

} // namespace fallout
