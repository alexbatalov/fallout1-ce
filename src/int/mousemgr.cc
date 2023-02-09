#include "int/mousemgr.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "int/datafile.h"
#include "int/memdbg.h"
#include "platform_compat.h"
#include "plib/db/db.h"
#include "plib/gnw/debug.h"
#include "plib/gnw/input.h"

namespace fallout {

#define MOUSE_MGR_CACHE_CAPACITY 32

typedef enum MouseManagerMouseType {
    MOUSE_MANAGER_MOUSE_TYPE_NONE,
    MOUSE_MANAGER_MOUSE_TYPE_STATIC,
    MOUSE_MANAGER_MOUSE_TYPE_ANIMATED,
} MouseManagerMouseType;

typedef struct MouseManagerStaticData {
    unsigned char* data;
    int field_4;
    int field_8;
    int width;
    int height;
} MouseManagerStaticData;

typedef struct MouseManagerAnimatedData {
    unsigned char** field_0;
    unsigned char** field_4;
    int* field_8;
    int* field_C;
    int width;
    int height;
    float field_18;
    int field_1C;
    int field_20;
    signed char field_24;
    signed char frameCount;
    signed char field_26;
} MouseManagerAnimatedData;

typedef struct MouseManagerCacheEntry {
    union {
        void* data;
        MouseManagerStaticData* staticData;
        MouseManagerAnimatedData* animatedData;
    };
    int type;
    unsigned char palette[256 * 3];
    int ref;
    char fileName[32];
    char field_32C[32];
} MouseManagerCacheEntry;

static char* defaultNameMangler(char* a1);
static int defaultRateCallback();
static int defaultTimeCallback();
static void setShape(unsigned char* buf, int width, int length, int full, int hotx, int hoty, char trans);
static void freeCacheEntry(MouseManagerCacheEntry* entry);
static int cacheInsert(void** data, int type, unsigned char* palette, const char* fileName);
static void cacheFlush();
static MouseManagerCacheEntry* cacheFind(const char* fileName, unsigned char** palettePtr, int* a3, int* a4, int* widthPtr, int* heightPtr, int* typePtr);

// 0x505B20
static MouseManagerNameMangler* mouseNameMangler = defaultNameMangler;

// 0x505B24
static MouseManagerRateProvider* rateCallback = defaultRateCallback;

// 0x505B28
static MouseManagerTimeProvider* currentTimeCallback = defaultTimeCallback;

// 0x505B2C
static int curref = 1;

// 0x6309D0
static MouseManagerCacheEntry Cache[MOUSE_MGR_CACHE_CAPACITY];

// 0x637358
static bool animating;

// 0x637350
static unsigned char* curPal;

// 0x637354
static MouseManagerAnimatedData* curAnim;

// 0x63735C
static unsigned char* curMouseBuf;

// 0x637360
static int lastMouseIndex;

// 0x477060
static char* defaultNameMangler(char* a1)
{
    return a1;
}

// 0x477064
static int defaultRateCallback()
{
    return 1000;
}

// 0x47706C
static int defaultTimeCallback()
{
    return get_time();
}

// 0x477074
static void setShape(unsigned char* buf, int width, int length, int full, int hotx, int hoty, char trans)
{
    mouse_set_shape(buf, width, length, full, hotx, hoty, trans);
}

// 0x477098
void mousemgrSetNameMangler(MouseManagerNameMangler* func)
{
    mouseNameMangler = func;
}

// 0x4770A0
void mousemgrSetTimeCallback(MouseManagerRateProvider* rateFunc, MouseManagerTimeProvider* currentTimeFunc)
{
    if (rateFunc != NULL) {
        rateCallback = rateFunc;
    } else {
        rateCallback = defaultRateCallback;
    }

    if (currentTimeFunc != NULL) {
        currentTimeCallback = currentTimeFunc;
    } else {
        currentTimeCallback = defaultTimeCallback;
    }
}

// 0x4770C8
static void freeCacheEntry(MouseManagerCacheEntry* entry)
{
    switch (entry->type) {
    case MOUSE_MANAGER_MOUSE_TYPE_STATIC:
        if (entry->staticData != NULL) {
            if (entry->staticData->data != NULL) {
                myfree(entry->staticData->data, __FILE__, __LINE__); // "..\\int\\MOUSEMGR.C", 120
                entry->staticData->data = NULL;
            }
            myfree(entry->staticData, __FILE__, __LINE__); // "..\\int\\MOUSEMGR.C", 123
            entry->staticData = NULL;
        }
        break;
    case MOUSE_MANAGER_MOUSE_TYPE_ANIMATED:
        if (entry->animatedData != NULL) {
            if (entry->animatedData->field_0 != NULL) {
                for (int index = 0; index < entry->animatedData->frameCount; index++) {
                    myfree(entry->animatedData->field_0[index], __FILE__, __LINE__); // "..\\int\\MOUSEMGR.C", 134
                    myfree(entry->animatedData->field_4[index], __FILE__, __LINE__); // "..\\int\\MOUSEMGR.C", 135
                }
                myfree(entry->animatedData->field_0, __FILE__, __LINE__); // "..\\int\\MOUSEMGR.C", 137
                myfree(entry->animatedData->field_4, __FILE__, __LINE__); // "..\\int\\MOUSEMGR.C", 138
                myfree(entry->animatedData->field_8, __FILE__, __LINE__); // "..\\int\\MOUSEMGR.C", 139
                myfree(entry->animatedData->field_C, __FILE__, __LINE__); // "..\\int\\MOUSEMGR.C", 140
            }
            myfree(entry->animatedData, __FILE__, __LINE__); // "..\\int\\MOUSEMGR.C", 143
            entry->animatedData = NULL;
        }
        break;
    }

    entry->type = 0;
    entry->fileName[0] = '\0';
}

// 0x477208
static int cacheInsert(void** data, int type, unsigned char* palette, const char* fileName)
{
    int foundIndex = -1;
    int index;
    for (index = 0; index < MOUSE_MGR_CACHE_CAPACITY; index++) {
        MouseManagerCacheEntry* cacheEntry = &(Cache[index]);
        if (cacheEntry->type == MOUSE_MANAGER_MOUSE_TYPE_NONE && foundIndex == -1) {
            foundIndex = index;
        }

        if (compat_stricmp(fileName, cacheEntry->fileName) == 0) {
            freeCacheEntry(cacheEntry);
            foundIndex = index;
            break;
        }
    }

    if (foundIndex != -1) {
        index = foundIndex;
    }

    if (index == MOUSE_MGR_CACHE_CAPACITY) {
        int v2 = -1;
        int v1 = curref;
        for (int index = 0; index < MOUSE_MGR_CACHE_CAPACITY; index++) {
            MouseManagerCacheEntry* cacheEntry = &(Cache[index]);
            if (v1 > cacheEntry->ref) {
                v1 = cacheEntry->ref;
                v2 = index;
            }
        }

        if (v2 == -1) {
            debug_printf("Mouse cache overflow!!!!\n");
            exit(1);
        }

        index = v2;
        freeCacheEntry(&(Cache[index]));
    }

    MouseManagerCacheEntry* cacheEntry = &(Cache[index]);
    cacheEntry->type = type;
    memcpy(cacheEntry->palette, palette, sizeof(cacheEntry->palette));
    cacheEntry->ref = curref++;
    strncpy(cacheEntry->fileName, fileName, sizeof(cacheEntry->fileName) - 1);
    cacheEntry->field_32C[0] = '\0';
    cacheEntry->data = *data;

    return index;
}

// 0x4771E4
static void cacheFlush()
{
    for (int index = 0; index < MOUSE_MGR_CACHE_CAPACITY; index++) {
        freeCacheEntry(&(Cache[index]));
    }
}

// 0x47735C
static MouseManagerCacheEntry* cacheFind(const char* fileName, unsigned char** palettePtr, int* a3, int* a4, int* widthPtr, int* heightPtr, int* typePtr)
{
    for (int index = 0; index < MOUSE_MGR_CACHE_CAPACITY; index++) {
        MouseManagerCacheEntry* cacheEntry = &(Cache[index]);
        if (compat_strnicmp(cacheEntry->fileName, fileName, 31) == 0 || compat_strnicmp(cacheEntry->field_32C, fileName, 31) == 0) {
            *palettePtr = cacheEntry->palette;
            *typePtr = cacheEntry->type;

            lastMouseIndex = index;

            switch (cacheEntry->type) {
            case MOUSE_MANAGER_MOUSE_TYPE_STATIC:
                *a3 = cacheEntry->staticData->field_4;
                *a4 = cacheEntry->staticData->field_8;
                *widthPtr = cacheEntry->staticData->width;
                *heightPtr = cacheEntry->staticData->height;
                break;
            case MOUSE_MANAGER_MOUSE_TYPE_ANIMATED:
                *widthPtr = cacheEntry->animatedData->width;
                *heightPtr = cacheEntry->animatedData->height;
                *a3 = cacheEntry->animatedData->field_8[cacheEntry->animatedData->field_26];
                *a4 = cacheEntry->animatedData->field_C[cacheEntry->animatedData->field_26];
                break;
            }

            return cacheEntry;
        }
    }

    return NULL;
}

// 0x47749C
void initMousemgr()
{
    mouse_set_sensitivity(1.0);
}

// 0x4774AC
void mousemgrClose()
{
    setShape(NULL, 0, 0, 0, 0, 0, 0);

    if (curMouseBuf != NULL) {
        myfree(curMouseBuf, __FILE__, __LINE__); // "..\\int\\MOUSEMGR.C", 243
        curMouseBuf = NULL;
    }

    // NOTE: Uninline.
    cacheFlush();

    curPal = NULL;
    curAnim = 0;
}

// 0x477514
void mousemgrUpdate()
{
    if (!animating) {
        return;
    }

    if (curAnim == NULL) {
        debug_printf("Animating == 1 but curAnim == 0\n");
    }

    if (currentTimeCallback() >= curAnim->field_1C) {
        curAnim->field_1C = (int)(curAnim->field_18 / curAnim->frameCount * rateCallback() + currentTimeCallback());
        if (curAnim->field_24 != curAnim->field_26) {
            int v1 = curAnim->field_26 + curAnim->field_20;
            if (v1 < 0) {
                v1 = curAnim->frameCount - 1;
            } else if (v1 >= curAnim->frameCount) {
                v1 = 0;
            }

            curAnim->field_26 = v1;
            memcpy(curAnim->field_0[curAnim->field_26],
                curAnim->field_4[curAnim->field_26],
                curAnim->width * curAnim->height);

            datafileConvertData(curAnim->field_0[curAnim->field_26],
                curPal,
                curAnim->width,
                curAnim->height);

            setShape(curAnim->field_0[v1],
                curAnim->width,
                curAnim->height,
                curAnim->width,
                curAnim->field_8[v1],
                curAnim->field_C[v1],
                0);
        }
    }
}

// 0x477678
int mouseSetFrame(char* fileName, int a2)
{
    char* mangledFileName = mouseNameMangler(fileName);

    unsigned char* palette;
    int temp;
    int type;
    MouseManagerCacheEntry* cacheEntry = cacheFind(fileName, &palette, &temp, &temp, &temp, &temp, &type);
    if (cacheEntry != NULL) {
        if (type == MOUSE_MANAGER_MOUSE_TYPE_ANIMATED) {
            cacheEntry->animatedData->field_24 = a2;
            if (cacheEntry->animatedData->field_24 >= cacheEntry->animatedData->field_26) {
                int v1 = cacheEntry->animatedData->field_24 - cacheEntry->animatedData->field_26;
                int v2 = cacheEntry->animatedData->frameCount + cacheEntry->animatedData->field_26 - cacheEntry->animatedData->field_24;
                if (v1 >= v2) {
                    cacheEntry->animatedData->field_20 = -1;
                } else {
                    cacheEntry->animatedData->field_20 = 1;
                }
            } else {
                int v1 = cacheEntry->animatedData->field_26 - cacheEntry->animatedData->field_24;
                int v2 = cacheEntry->animatedData->frameCount + cacheEntry->animatedData->field_24 - cacheEntry->animatedData->field_26;
                if (v1 < v2) {
                    cacheEntry->animatedData->field_20 = -1;
                } else {
                    cacheEntry->animatedData->field_20 = 1;
                }
            }

            if (!animating || curAnim != cacheEntry->animatedData) {
                memcpy(cacheEntry->animatedData->field_0[cacheEntry->animatedData->field_26],
                    cacheEntry->animatedData->field_4[cacheEntry->animatedData->field_26],
                    cacheEntry->animatedData->width * cacheEntry->animatedData->height);

                setShape(cacheEntry->animatedData->field_0[cacheEntry->animatedData->field_26],
                    cacheEntry->animatedData->width,
                    cacheEntry->animatedData->height,
                    cacheEntry->animatedData->width,
                    cacheEntry->animatedData->field_8[cacheEntry->animatedData->field_26],
                    cacheEntry->animatedData->field_C[cacheEntry->animatedData->field_26],
                    0);

                animating = true;
            }

            curAnim = cacheEntry->animatedData;
            curPal = palette;
            curAnim->field_1C = currentTimeCallback();
            return true;
        }

        mouseSetMousePointer(fileName);
        return true;
    }

    if (animating) {
        curPal = 0;
        animating = 0;
        curAnim = 0;
    } else {
        if (curMouseBuf != NULL) {
            myfree(curMouseBuf, __FILE__, __LINE__); // "..\\int\\MOUSEMGR.C", 337
            curMouseBuf = NULL;
        }
    }

    DB_FILE* stream = db_fopen(mangledFileName, "r");
    if (stream == NULL) {
        debug_printf("mouseSetFrame: couldn't find %s\n", mangledFileName);
        return false;
    }

    char string[80];
    db_fgets(string, sizeof(string), stream);
    if (compat_strnicmp(string, "anim", 4) != 0) {
        db_fclose(stream);
        mouseSetMousePointer(fileName);
        return true;
    }

    // NOTE: Uninline.
    char* sep = strchr(string, ' ');
    if (sep == NULL) {
        // FIXME: Leaks stream.
        return false;
    }

    int v3;
    float v4;
    sscanf(sep + 1, "%d %f", &v3, &v4);

    MouseManagerAnimatedData* animatedData = (MouseManagerAnimatedData*)mymalloc(sizeof(*animatedData), __FILE__, __LINE__); // "..\\int\\MOUSEMGR.C", 359
    animatedData->field_0 = (unsigned char**)mymalloc(sizeof(*animatedData->field_0) * v3, __FILE__, __LINE__); // "..\\int\\MOUSEMGR.C", 360
    animatedData->field_4 = (unsigned char**)mymalloc(sizeof(*animatedData->field_4) * v3, __FILE__, __LINE__); // "..\\int\\MOUSEMGR.C", 361
    animatedData->field_8 = (int*)mymalloc(sizeof(*animatedData->field_8) * v3, __FILE__, __LINE__); // "..\\int\\MOUSEMGR.C", 362
    animatedData->field_C = (int*)mymalloc(sizeof(*animatedData->field_8) * v3, __FILE__, __LINE__); // "..\\int\\MOUSEMGR.C", 363
    animatedData->field_18 = v4;
    animatedData->field_1C = currentTimeCallback();
    animatedData->field_26 = 0;
    animatedData->field_24 = a2;
    animatedData->frameCount = v3;
    if (animatedData->frameCount / 2 <= a2) {
        animatedData->field_20 = -1;
    } else {
        animatedData->field_20 = 1;
    }

    int width;
    int height;
    for (int index = 0; index < v3; index++) {
        string[0] = '\0';
        db_fgets(string, sizeof(string), stream);
        if (string[0] == '\0') {
            debug_printf("Not enough frames in %s, got %d, needed %d", mangledFileName, index, v3);
            break;
        }

        // NOTE: Uninline.
        char* sep = strchr(string, ' ');
        if (sep == NULL) {
            debug_printf("Bad line %s in %s\n", string, fileName);
            // FIXME: Leaking stream.
            return false;
        }

        *sep = '\0';

        int v5;
        int v6;
        sscanf(sep + 1, "%d %d", &v5, &v6);

        animatedData->field_4[index] = loadRawDataFile(mouseNameMangler(string), &width, &height);
        animatedData->field_0[index] = (unsigned char*)mymalloc(width * height, __FILE__, __LINE__); // "..\\int\\MOUSEMGR.C", 390
        memcpy(animatedData->field_0[index], animatedData->field_4[index], width * height);
        datafileConvertData(animatedData->field_0[index], datafileGetPalette(), width, height);
        animatedData->field_8[index] = v5;
        animatedData->field_C[index] = v6;
    }

    db_fclose(stream);

    animatedData->width = width;
    animatedData->height = height;

    lastMouseIndex = cacheInsert(reinterpret_cast<void**>(&animatedData), MOUSE_MANAGER_MOUSE_TYPE_ANIMATED, datafileGetPalette(), fileName);
    strncpy(Cache[lastMouseIndex].field_32C, fileName, 31);

    curAnim = animatedData;
    curPal = Cache[lastMouseIndex].palette;
    animating = true;

    setShape(animatedData->field_0[0],
        animatedData->width,
        animatedData->height,
        animatedData->width,
        animatedData->field_8[0],
        animatedData->field_C[0],
        0);

    return true;
}

// 0x477C68
bool mouseSetMouseShape(char* fileName, int a2, int a3)
{
    unsigned char* palette;
    int temp;
    int width;
    int height;
    int type;
    MouseManagerCacheEntry* cacheEntry = cacheFind(fileName, &palette, &temp, &temp, &width, &height, &type);
    char* mangledFileName = mouseNameMangler(fileName);

    if (cacheEntry == NULL) {
        MouseManagerStaticData* staticData;
        staticData = (MouseManagerStaticData*)mymalloc(sizeof(*staticData), __FILE__, __LINE__); // "..\\int\\MOUSEMGR.C", 430
        staticData->data = loadRawDataFile(mangledFileName, &width, &height);
        staticData->field_4 = a2;
        staticData->field_8 = a3;
        staticData->width = width;
        staticData->height = height;
        lastMouseIndex = cacheInsert(reinterpret_cast<void**>(&staticData), MOUSE_MANAGER_MOUSE_TYPE_STATIC, datafileGetPalette(), fileName);

        // NOTE: Original code is slightly different. It obtains address of
        // `staticData` and sets it's it into `cacheEntry`, which is a bit
        // awkward. Maybe there is more level on indirection was used. Any way
        // in order to make code path below unaltered take entire cache entry.
        cacheEntry = &(Cache[lastMouseIndex]);

        type = MOUSE_MANAGER_MOUSE_TYPE_STATIC;
        palette = Cache[lastMouseIndex].palette;
    }

    switch (type) {
    case MOUSE_MANAGER_MOUSE_TYPE_STATIC:
        if (curMouseBuf != NULL) {
            myfree(curMouseBuf, __FILE__, __LINE__); // "..\\int\\MOUSEMGR.C", 446
        }

        curMouseBuf = (unsigned char*)mymalloc(width * height, __FILE__, __LINE__); // "..\\int\\MOUSEMGR.C", 448
        memcpy(curMouseBuf, cacheEntry->staticData->data, width * height);
        datafileConvertData(curMouseBuf, palette, width, height);
        setShape(curMouseBuf, width, height, width, a2, a3, 0);
        animating = false;
        break;
    case MOUSE_MANAGER_MOUSE_TYPE_ANIMATED:
        curAnim = cacheEntry->animatedData;
        animating = true;
        curPal = palette;
        break;
    }

    return true;
}

// 0x477E20
bool mouseSetMousePointer(char* fileName)
{
    unsigned char* palette;
    int v1;
    int v2;
    int width;
    int height;
    int type;
    MouseManagerCacheEntry* cacheEntry = cacheFind(fileName, &palette, &v1, &v2, &width, &height, &type);
    if (cacheEntry != NULL) {
        if (curMouseBuf != NULL) {
            myfree(curMouseBuf, __FILE__, __LINE__); // "..\\int\\MOUSEMGR.C", 482
            curMouseBuf = NULL;
        }

        curPal = NULL;
        animating = false;
        curAnim = 0;

        switch (type) {
        case MOUSE_MANAGER_MOUSE_TYPE_STATIC:
            curMouseBuf = (unsigned char*)mymalloc(width * height, __FILE__, __LINE__); // "..\\int\\MOUSEMGR.C", 492
            memcpy(curMouseBuf, cacheEntry->staticData->data, width * height);
            datafileConvertData(curMouseBuf, palette, width, height);
            setShape(curMouseBuf, width, height, width, v1, v2, 0);
            animating = false;
            break;
        case MOUSE_MANAGER_MOUSE_TYPE_ANIMATED:
            curAnim = cacheEntry->animatedData;
            curPal = palette;
            curAnim->field_26 = 0;
            curAnim->field_24 = 0;
            setShape(curAnim->field_0[0],
                curAnim->width,
                curAnim->height,
                curAnim->width,
                curAnim->field_8[0],
                curAnim->field_C[0],
                0);
            animating = true;
            break;
        }
        return true;
    }

    char* dot = strrchr(fileName, '.');
    if (dot != NULL && compat_stricmp(dot + 1, "mou") == 0) {
        return mouseSetMouseShape(fileName, 0, 0);
    }

    char* mangledFileName = mouseNameMangler(fileName);
    DB_FILE* stream = db_fopen(mangledFileName, "r");
    if (stream == NULL) {
        debug_printf("Can't find %s\n", mangledFileName);
        return false;
    }

    char string[80];
    string[0] = '\0';
    db_fgets(string, sizeof(string) - 1, stream);
    if (string[0] == '\0') {
        return false;
    }

    bool rc;
    if (compat_strnicmp(string, "anim", 4) == 0) {
        db_fclose(stream);
        rc = mouseSetFrame(fileName, 0);
    } else {
        // NOTE: Uninline.
        char* sep = strchr(string, ' ');
        if (sep != NULL) {
            return 0;
        }

        *sep = '\0';

        int v3;
        int v4;
        sscanf(sep + 1, "%d %d", &v3, &v4);

        db_fclose(stream);

        rc = mouseSetMouseShape(string, v3, v4);
    }

    strncpy(Cache[lastMouseIndex].field_32C, fileName, 31);

    return rc;
}

// 0x4780BC
void mousemgrResetMouse()
{
    MouseManagerCacheEntry* entry = &(Cache[lastMouseIndex]);

    int imageWidth;
    int imageHeight;
    switch (entry->type) {
    case MOUSE_MANAGER_MOUSE_TYPE_STATIC:
        imageWidth = entry->staticData->width;
        imageHeight = entry->staticData->height;
        break;
    case MOUSE_MANAGER_MOUSE_TYPE_ANIMATED:
        imageWidth = entry->animatedData->width;
        imageHeight = entry->animatedData->height;
        break;
    }

    switch (entry->type) {
    case MOUSE_MANAGER_MOUSE_TYPE_STATIC:
        if (curMouseBuf != NULL) {
            if (curMouseBuf != NULL) {
                myfree(curMouseBuf, __FILE__, __LINE__); // "..\\int\\MOUSEMGR.C", 572
            }

            curMouseBuf = (unsigned char*)mymalloc(imageWidth * imageHeight, __FILE__, __LINE__); // "..\\int\\MOUSEMGR.C", 574
            memcpy(curMouseBuf, entry->staticData->data, imageWidth * imageHeight);
            datafileConvertData(curMouseBuf, entry->palette, imageWidth, imageHeight);

            setShape(curMouseBuf,
                imageWidth,
                imageHeight,
                imageWidth,
                entry->staticData->field_4,
                entry->staticData->field_8,
                0);
        } else {
            debug_printf("Hm, current mouse type is M_STATIC, but no current mouse pointer\n");
        }
        break;
    case MOUSE_MANAGER_MOUSE_TYPE_ANIMATED:
        if (curAnim != NULL) {
            for (int index = 0; index < curAnim->frameCount; index++) {
                memcpy(curAnim->field_0[index], curAnim->field_4[index], imageWidth * imageHeight);
                datafileConvertData(curAnim->field_0[index], entry->palette, imageWidth, imageHeight);
            }

            setShape(curAnim->field_0[curAnim->field_26],
                imageWidth,
                imageHeight,
                imageWidth,
                curAnim->field_8[curAnim->field_26],
                curAnim->field_C[curAnim->field_26],
                0);
        } else {
            debug_printf("Hm, current mouse type is M_ANIMATED, but no current mouse pointer\n");
        }
    }
}

// 0x4783D4
void mouseHide()
{
    mouse_hide();
}

// 0x4783DC
void mouseShow()
{
    mouse_show();
}

} // namespace fallout
