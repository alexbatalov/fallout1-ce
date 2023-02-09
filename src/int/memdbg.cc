#include "int/memdbg.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace fallout {

static void defaultOutput(const char* string);
static int debug_printf(const char* format, ...);
static void error(const char* func, size_t size, const char* file, int line);
static void* defaultMalloc(size_t size);
static void* defaultRealloc(void* ptr, size_t size);
static void defaultFree(void* ptr);

// 0x505B00
static MemDbgDebugFunc* outputFunc = defaultOutput;

// 0x505B04
static MemDbgMallocFunc* mallocPtr = defaultMalloc;

// 0x505B08
static MemDbgReallocFunc* reallocPtr = defaultRealloc;

// 0x505B0C
static MemDbgFreeFunc* freePtr = defaultFree;

// 0x476320
static void defaultOutput(const char* string)
{
    printf("%s", string);
}

// 0x476330
void memoryRegisterDebug(MemDbgDebugFunc* func)
{
    outputFunc = func;
}

// 0x476338
static int debug_printf(const char* format, ...)
{
    // 0x631F7C
    static char buf[256];

    int length = 0;

    if (outputFunc != NULL) {
        va_list args;
        va_start(args, format);
        length = vsnprintf(buf, sizeof(buf), format, args);
        va_end(args);

        outputFunc(buf);
    }

    return length;
}

// 0x476380
static void error(const char* func, size_t size, const char* file, int line)
{
    debug_printf("%s: Error allocating block of size %ld (%x), %s %d\n", func, size, size, file, line);
    exit(1);
}

// 0x47639C
static void* defaultMalloc(size_t size)
{
    return malloc(size);
}

// 0x4763A4
static void* defaultRealloc(void* ptr, size_t size)
{
    return realloc(ptr, size);
}

// 0x4763AC
static void defaultFree(void* ptr)
{
    free(ptr);
}

// 0x4763B4
void memoryRegisterAlloc(MemDbgMallocFunc* mallocFunc, MemDbgReallocFunc* reallocFunc, MemDbgFreeFunc* freeFunc)
{
    mallocPtr = mallocFunc;
    reallocPtr = reallocFunc;
    freePtr = freeFunc;
}

// 0x4763CC
int my_check_all()
{
    return 0;
}

// 0x4763D0
void* mymalloc(size_t size, const char* file, int line)
{
    void* ptr = mallocPtr(size);
    if (ptr == NULL) {
        error("malloc", size, file, line);
    }

    return ptr;
}

// 0x476424
void* myrealloc(void* ptr, size_t size, const char* file, int line)
{
    ptr = reallocPtr(ptr, size);
    if (ptr == NULL) {
        error("realloc", size, file, line);
    }

    return ptr;
}

// 0x4763F8
void myfree(void* ptr, const char* file, int line)
{
    if (ptr == NULL) {
        debug_printf("free: free of a null ptr, %s %d\n", file, line);
        exit(1);
    }

    freePtr(ptr);
}

// 0x476448
void* mycalloc(int count, int size, const char* file, int line)
{
    void* ptr = mallocPtr(count * size);
    if (ptr == NULL) {
        error("calloc", size, file, line);
    }

    memset(ptr, 0, count * size);

    return ptr;
}

// 0x476480
char* mystrdup(const char* string, const char* file, int line)
{
    size_t size = strlen(string) + 1;
    char* copy = (char*)mallocPtr(size);
    if (copy == NULL) {
        error("strdup", size, file, line);
    }

    strcpy(copy, string);

    return copy;
}

} // namespace fallout
