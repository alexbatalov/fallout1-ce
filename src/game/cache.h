#ifndef FALLOUT_GAME_CACHE_H_
#define FALLOUT_GAME_CACHE_H_

#include <stddef.h>

#include "game/heap.h"

namespace fallout {

#define INVALID_CACHE_ENTRY ((CacheEntry*)-1)

// The initial number of cache entries in new cache.
#define CACHE_ENTRIES_INITIAL_CAPACITY 100

// The number of cache entries added when cache capacity is reached.
#define CACHE_ENTRIES_GROW_CAPACITY 50

typedef enum CacheEntryFlags {
    // Specifies that cache entry has no references as should be evicted during
    // the next sweep operation.
    CACHE_ENTRY_MARKED_FOR_EVICTION = 0x01,
} CacheEntryFlags;

typedef enum CacheListRequestType {
    CACHE_LIST_REQUEST_TYPE_ALL_ITEMS = 0,
    CACHE_LIST_REQUEST_TYPE_LOCKED_ITEMS = 1,
    CACHE_LIST_REQUEST_TYPE_UNLOCKED_ITEMS = 2,
} CacheListRequestType;

typedef int CacheSizeProc(int key, int* sizePtr);
typedef int CacheReadProc(int key, int* sizePtr, unsigned char* buffer);
typedef void CacheFreeProc(void* ptr);

typedef struct CacheEntry {
    int key;
    int size;
    unsigned char* data;
    unsigned int referenceCount;

    // Total number of hits that this cache entry received during it's
    // lifetime.
    unsigned int hits;

    unsigned int flags;

    // The most recent hit in terms of cache hit counter. Used to track most
    // recently used entries in eviction strategy.
    unsigned int mru;

    int heapHandleIndex;
} CacheEntry;

typedef struct Cache {
    // Current size of entries in cache.
    int size;

    // Maximum size of entries in cache.
    int maxSize;

    // The length of `entries` array.
    int entriesLength;

    // The capacity of `entries` array.
    int entriesCapacity;

    // Total number of hits during cache lifetime.
    unsigned int hits;

    // List of cache entries.
    CacheEntry** entries;

    CacheSizeProc* sizeProc;
    CacheReadProc* readProc;
    CacheFreeProc* freeProc;
    Heap heap;
} Cache;

bool cache_init(Cache* cache, CacheSizeProc* sizeProc, CacheReadProc* readProc, CacheFreeProc* freeProc, int maxSize);
bool cache_exit(Cache* cache);
int cache_query(Cache* cache, int key);
bool cache_lock(Cache* cache, int key, void** data, CacheEntry** cacheEntryPtr);
bool cache_unlock(Cache* cache, CacheEntry* cacheEntry);
int cache_discard(Cache* cache, int key);
bool cache_flush(Cache* cache);
int cache_size(Cache* cache, int* sizePtr);
bool cache_stats(Cache* cache, char* dest, size_t size);
int cache_create_list(Cache* cache, unsigned int a2, int** tagsPtr, int* tagsLengthPtr);
int cache_destroy_list(int** tagsPtr);

} // namespace fallout

#endif /* FALLOUT_GAME_CACHE_H_ */
