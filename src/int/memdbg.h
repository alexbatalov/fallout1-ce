#ifndef FALLOUT_INT_MEMDBG_H_
#define FALLOUT_INT_MEMDBG_H_

#include <stddef.h>

namespace fallout {

typedef void*(MemDbgMallocFunc)(size_t size);
typedef void*(MemDbgReallocFunc)(void* ptr, size_t size);
typedef void(MemDbgFreeFunc)(void* ptr);
typedef void(MemDbgDebugFunc)(const char* string);

void memoryRegisterDebug(MemDbgDebugFunc* func);
void memoryRegisterAlloc(MemDbgMallocFunc* mallocProc, MemDbgReallocFunc* reallocProc, MemDbgFreeFunc* freeProc);
void* mymalloc(size_t size, const char* file, int line);
void* myrealloc(void* ptr, size_t size, const char* file, int line);
void myfree(void* ptr, const char* file, int line);
void* mycalloc(int count, int size, const char* file, int line);
char* mystrdup(const char* string, const char* file, int line);

} // namespace fallout

#endif /* FALLOUT_INT_MEMDBG_H_ */
