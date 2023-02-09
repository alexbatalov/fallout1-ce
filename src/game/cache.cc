#include "game/cache.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "int/sound.h"
#include "plib/gnw/debug.h"
#include "plib/gnw/memory.h"

namespace fallout {

static bool cache_add(Cache* cache, int key, int* indexPtr);
static bool cache_insert(Cache* cache, CacheEntry* cacheEntry, int index);
static int cache_find(Cache* cache, int key, int* indexPtr);
static int cache_create_item(CacheEntry** cacheEntryPtr);
static bool cache_init_item(CacheEntry* cacheEntry);
static bool cache_destroy_item(Cache* cache, CacheEntry* cacheEntry);
static bool cache_unlock_all(Cache* cache);
static bool cache_reset_counter(Cache* cache);
static bool cache_make_room(Cache* cache, int size);
static bool cache_purge(Cache* cache);
static bool cache_resize_array(Cache* cache, int newCapacity);
static int cache_compare_make_room(const void* a1, const void* a2);
static int cache_compare_reset_counter(const void* a1, const void* a2);

// 0x4FEC7C
static int lock_sound_ticker = 0;

// 0x41E9C0
bool cache_init(Cache* cache, CacheSizeProc* sizeProc, CacheReadProc* readProc, CacheFreeProc* freeProc, int maxSize)
{
    if (!heap_init(&(cache->heap), maxSize)) {
        return false;
    }

    cache->size = 0;
    cache->maxSize = maxSize;
    cache->entriesLength = 0;
    cache->entriesCapacity = CACHE_ENTRIES_INITIAL_CAPACITY;
    cache->hits = 0;
    cache->entries = (CacheEntry**)mem_malloc(sizeof(*cache->entries) * cache->entriesCapacity);
    cache->sizeProc = sizeProc;
    cache->readProc = readProc;
    cache->freeProc = freeProc;

    if (cache->entries == NULL) {
        return false;
    }

    memset(cache->entries, 0, sizeof(*cache->entries) * cache->entriesCapacity);

    return true;
}

// 0x41EA50
bool cache_exit(Cache* cache)
{
    if (cache == NULL) {
        return false;
    }

    cache_unlock_all(cache);
    cache_flush(cache);
    heap_exit(&(cache->heap));

    cache->size = 0;
    cache->maxSize = 0;
    cache->entriesLength = 0;
    cache->entriesCapacity = 0;
    cache->hits = 0;

    if (cache->entries != NULL) {
        mem_free(cache->entries);
        cache->entries = NULL;
    }

    cache->sizeProc = NULL;
    cache->readProc = NULL;
    cache->freeProc = NULL;

    return true;
}

// 0x41EAC0
int cache_query(Cache* cache, int key)
{
    int index;

    if (cache == NULL) {
        return 0;
    }

    if (cache_find(cache, key, &index) != 2) {
        return 0;
    }

    return 1;
}

// 0x41EAE8
bool cache_lock(Cache* cache, int key, void** data, CacheEntry** cacheEntryPtr)
{
    if (cache == NULL || data == NULL || cacheEntryPtr == NULL) {
        return false;
    }

    *cacheEntryPtr = NULL;

    int index;
    int rc = cache_find(cache, key, &index);
    if (rc == 2) {
        // Use existing cache entry.
        CacheEntry* cacheEntry = cache->entries[index];
        cacheEntry->hits++;
    } else if (rc == 3) {
        // New cache entry is required.
        if (cache->entriesLength >= INT_MAX) {
            return false;
        }

        if (!cache_add(cache, key, &index)) {
            return false;
        }

        lock_sound_ticker %= 4;
        if (lock_sound_ticker == 0) {
            soundUpdate();
        }
    } else {
        return false;
    }

    CacheEntry* cacheEntry = cache->entries[index];
    if (cacheEntry->referenceCount == 0) {
        if (!heap_lock(&(cache->heap), cacheEntry->heapHandleIndex, &(cacheEntry->data))) {
            return false;
        }
    }

    cacheEntry->referenceCount++;

    cache->hits++;
    cacheEntry->mru = cache->hits;

    if (cache->hits == UINT_MAX) {
        cache_reset_counter(cache);
    }

    *data = cacheEntry->data;
    *cacheEntryPtr = cacheEntry;

    return true;
}

// 0x41EDB8
bool cache_unlock(Cache* cache, CacheEntry* cacheEntry)
{
    if (cache == NULL || cacheEntry == NULL) {
        return false;
    }

    if (cacheEntry->referenceCount == 0) {
        return false;
    }

    cacheEntry->referenceCount--;

    if (cacheEntry->referenceCount == 0) {
        heap_unlock(&(cache->heap), cacheEntry->heapHandleIndex);
    }

    return true;
}

// 0x41EDEC
int cache_discard(Cache* cache, int key)
{
    int index;
    CacheEntry* cacheEntry;

    if (cache == NULL) {
        return 0;
    }

    if (cache_find(cache, key, &index) != 2) {
        return 0;
    }

    cacheEntry = cache->entries[index];
    if (cacheEntry->referenceCount != 0) {
        return 0;
    }

    cacheEntry->flags |= CACHE_ENTRY_MARKED_FOR_EVICTION;

    cache_purge(cache);

    return 1;
}

// 0x41EE2C
bool cache_flush(Cache* cache)
{
    if (cache == NULL) {
        return false;
    }

    // Loop thru cache entries and mark those with no references for eviction.
    for (int index = 0; index < cache->entriesLength; index++) {
        CacheEntry* cacheEntry = cache->entries[index];
        if (cacheEntry->referenceCount == 0) {
            cacheEntry->flags |= CACHE_ENTRY_MARKED_FOR_EVICTION;
        }
    }

    // Sweep cache entries marked earlier.
    cache_purge(cache);

    // Shrink cache entries array if it's too big.
    int optimalCapacity = cache->entriesLength + CACHE_ENTRIES_GROW_CAPACITY;
    if (optimalCapacity < cache->entriesCapacity) {
        cache_resize_array(cache, optimalCapacity);
    }

    return true;
}

// 0x41EE84
int cache_size(Cache* cache, int* sizePtr)
{
    if (cache == NULL) {
        return 0;
    }

    if (sizePtr == NULL) {
        return 0;
    }

    *sizePtr = cache->size;

    return 1;
}

// 0x41EE9C
bool cache_stats(Cache* cache, char* dest, size_t size)
{
    if (cache == NULL || dest == NULL) {
        return false;
    }

    snprintf(dest, size, "Cache stats are disabled.%s", "\n");

    return true;
}

// 0x41EEC0
int cache_create_list(Cache* cache, unsigned int a2, int** tagsPtr, int* tagsLengthPtr)
{
    int cacheItemIndex;
    int tagIndex;

    if (cache == NULL) {
        return 0;
    }

    if (tagsPtr == NULL) {
        return 0;
    }

    if (tagsLengthPtr == NULL) {
        return 0;
    }

    *tagsLengthPtr = 0;

    switch (a2) {
    case CACHE_LIST_REQUEST_TYPE_ALL_ITEMS:
        *tagsPtr = (int*)mem_malloc(sizeof(*tagsPtr) * cache->entriesLength);
        if (*tagsPtr == NULL) {
            return 0;
        }

        for (cacheItemIndex = 0; cacheItemIndex < cache->entriesLength; cacheItemIndex++) {
            (*tagsPtr)[cacheItemIndex] = cache->entries[cacheItemIndex]->key;
        }

        *tagsLengthPtr = cache->entriesLength;

        break;
    case CACHE_LIST_REQUEST_TYPE_LOCKED_ITEMS:
        for (cacheItemIndex = 0; cacheItemIndex < cache->entriesLength; cacheItemIndex++) {
            if (cache->entries[cacheItemIndex]->referenceCount != 0) {
                (*tagsLengthPtr)++;
            }
        }

        *tagsPtr = (int*)mem_malloc(sizeof(*tagsPtr) * (*tagsLengthPtr));
        if (*tagsPtr == NULL) {
            return 0;
        }

        tagIndex = 0;
        for (cacheItemIndex = 0; cacheItemIndex < cache->entriesLength; cacheItemIndex++) {
            if (cache->entries[cacheItemIndex]->referenceCount != 0) {
                if (tagIndex < *tagsLengthPtr) {
                    (*tagsPtr)[tagIndex++] = cache->entries[cacheItemIndex]->key;
                }
            }
        }

        break;
    case CACHE_LIST_REQUEST_TYPE_UNLOCKED_ITEMS:
        for (cacheItemIndex = 0; cacheItemIndex < cache->entriesLength; cacheItemIndex++) {
            if (cache->entries[cacheItemIndex]->referenceCount == 0) {
                (*tagsLengthPtr)++;
            }
        }

        *tagsPtr = (int*)mem_malloc(sizeof(*tagsPtr) * (*tagsLengthPtr));
        if (*tagsPtr == NULL) {
            return 0;
        }

        tagIndex = 0;
        for (cacheItemIndex = 0; cacheItemIndex < cache->entriesLength; cacheItemIndex++) {
            if (cache->entries[cacheItemIndex]->referenceCount == 0) {
                if (tagIndex < *tagsLengthPtr) {
                    (*tagsPtr)[tagIndex++] = cache->entries[cacheItemIndex]->key;
                }
            }
        }

        break;
    }

    return 1;
}

// 0x41F084
int cache_destroy_list(int** tagsPtr)
{
    if (tagsPtr == NULL) {
        return 0;
    }

    if (*tagsPtr == NULL) {
        return 0;
    }

    mem_free(*tagsPtr);
    *tagsPtr = NULL;

    return 1;
}

// Fetches entry for the specified key into the cache.
//
// 0x41F0AC
static bool cache_add(Cache* cache, int key, int* indexPtr)
{
    CacheEntry* cacheEntry;

    // NOTE: Uninline.
    if (cache_create_item(&cacheEntry) != 1) {
        return 0;
    }

    do {
        int size;
        if (cache->sizeProc(key, &size) != 0) {
            break;
        }

        if (!cache_make_room(cache, size)) {
            break;
        }

        bool allocated = false;
        int cacheEntrySize = size;
        for (int attempt = 0; attempt < 10; attempt++) {
            if (heap_allocate(&(cache->heap), &(cacheEntry->heapHandleIndex), size, 1)) {
                allocated = true;
                break;
            }

            cacheEntrySize = (int)((double)cacheEntrySize + (double)size * 0.25);
            if (cacheEntrySize > cache->maxSize) {
                break;
            }

            if (!cache_make_room(cache, cacheEntrySize)) {
                break;
            }
        }

        if (!allocated) {
            cache_flush(cache);

            allocated = true;
            if (!heap_allocate(&(cache->heap), &(cacheEntry->heapHandleIndex), size, 1)) {
                if (!heap_allocate(&(cache->heap), &(cacheEntry->heapHandleIndex), size, 0)) {
                    allocated = false;
                }
            }
        }

        if (!allocated) {
            break;
        }

        do {
            if (!heap_lock(&(cache->heap), cacheEntry->heapHandleIndex, &(cacheEntry->data))) {
                break;
            }

            if (cache->readProc(key, &size, cacheEntry->data) != 0) {
                break;
            }

            heap_unlock(&(cache->heap), cacheEntry->heapHandleIndex);

            cacheEntry->size = size;
            cacheEntry->key = key;

            bool isNewKey = true;
            if (*indexPtr < cache->entriesLength) {
                if (key < cache->entries[*indexPtr]->key) {
                    if (*indexPtr == 0 || key > cache->entries[*indexPtr - 1]->key) {
                        isNewKey = false;
                    }
                }
            }

            if (isNewKey) {
                if (cache_find(cache, key, indexPtr) != 3) {
                    break;
                }
            }

            if (!cache_insert(cache, cacheEntry, *indexPtr)) {
                break;
            }

            return true;
        } while (0);

        heap_unlock(&(cache->heap), cacheEntry->heapHandleIndex);
    } while (0);

    // NOTE: Uninline.
    cache_destroy_item(cache, cacheEntry);

    return false;
}

// 0x41F2E8
static bool cache_insert(Cache* cache, CacheEntry* cacheEntry, int index)
{
    // Ensure cache have enough space for new entry.
    if (cache->entriesLength == cache->entriesCapacity - 1) {
        if (!cache_resize_array(cache, cache->entriesCapacity + CACHE_ENTRIES_GROW_CAPACITY)) {
            return false;
        }
    }

    // Move entries below insertion point.
    memmove(&(cache->entries[index + 1]), &(cache->entries[index]), sizeof(*cache->entries) * (cache->entriesLength - index));

    cache->entries[index] = cacheEntry;
    cache->entriesLength++;
    cache->size += cacheEntry->size;

    return true;
}

// Finds index for given key.
//
// Returns 2 if entry already exists in cache, or 3 if entry does not exist. In
// this case indexPtr represents insertion point.
//
// 0x41F354
static int cache_find(Cache* cache, int key, int* indexPtr)
{
    int length = cache->entriesLength;
    if (length == 0) {
        *indexPtr = 0;
        return 3;
    }

    int r = length - 1;
    int l = 0;
    int mid;
    int cmp;

    do {
        mid = (l + r) / 2;

        cmp = key - cache->entries[mid]->key;
        if (cmp == 0) {
            *indexPtr = mid;
            return 2;
        }

        if (cmp > 0) {
            l = l + 1;
        } else {
            r = r - 1;
        }
    } while (r >= l);

    if (cmp < 0) {
        *indexPtr = mid;
    } else {
        *indexPtr = mid + 1;
    }

    return 3;
}

// 0x41F3C0
static int cache_create_item(CacheEntry** cacheEntryPtr)
{
    *cacheEntryPtr = (CacheEntry*)mem_malloc(sizeof(**cacheEntryPtr));

    // FIXME: Wrong check, should be *cacheEntryPtr != NULL.
    if (cacheEntryPtr != NULL) {
        // NOTE: Uninline.
        return cache_init_item(*cacheEntryPtr);
    }

    return 0;
}

// 0x41F408
static bool cache_init_item(CacheEntry* cacheEntry)
{
    cacheEntry->key = 0;
    cacheEntry->size = 0;
    cacheEntry->data = NULL;
    cacheEntry->referenceCount = 0;
    cacheEntry->hits = 0;
    cacheEntry->flags = 0;
    cacheEntry->mru = 0;
    return true;
}

// 0x41F440
static bool cache_destroy_item(Cache* cache, CacheEntry* cacheEntry)
{
    if (cacheEntry->data != NULL) {
        heap_deallocate(&(cache->heap), &(cacheEntry->heapHandleIndex));
    }

    mem_free(cacheEntry);

    return true;
}

// 0x41F464
static bool cache_unlock_all(Cache* cache)
{
    Heap* heap = &(cache->heap);
    for (int index = 0; index < cache->entriesLength; index++) {
        CacheEntry* cacheEntry = cache->entries[index];

        // NOTE: Original code is slightly different. For unknown reason it uses
        // inner loop to decrement `referenceCount` one by one. Probably using
        // some inlined function.
        if (cacheEntry->referenceCount != 0) {
            heap_unlock(heap, cacheEntry->heapHandleIndex);
            cacheEntry->referenceCount = 0;
        }
    }

    return true;
}

// 0x41F4D4
static bool cache_reset_counter(Cache* cache)
{
    if (cache == NULL) {
        return false;
    }

    CacheEntry** entries = (CacheEntry**)mem_malloc(sizeof(*entries) * cache->entriesLength);
    if (entries == NULL) {
        return false;
    }

    memcpy(entries, cache->entries, sizeof(*entries) * cache->entriesLength);

    qsort(entries, cache->entriesLength, sizeof(*entries), cache_compare_reset_counter);

    for (int index = 0; index < cache->entriesLength; index++) {
        CacheEntry* cacheEntry = entries[index];
        cacheEntry->mru = index;
    }

    cache->hits = cache->entriesLength;

    // FIXME: Obviously leak `entries`.

    return true;
}

// Prepare cache for storing new entry with the specified size.
//
// 0x41F54C
static bool cache_make_room(Cache* cache, int size)
{
    if (size > cache->maxSize) {
        // The entry of given size is too big for caching, no matter what.
        return false;
    }

    if (cache->maxSize - cache->size >= size) {
        // There is space available for entry of given size, there is no need to
        // evict anything.
        return true;
    }

    CacheEntry** entries = (CacheEntry**)mem_malloc(sizeof(*entries) * cache->entriesLength);
    if (entries != NULL) {
        memcpy(entries, cache->entries, sizeof(*entries) * cache->entriesLength);
        qsort(entries, cache->entriesLength, sizeof(*entries), cache_compare_make_room);

        // The sweeping threshold is 20% of cache size plus size for the new
        // entry. Once the threshold is reached the marking process stops.
        int threshold = size + (int)((double)cache->size * 0.2);

        int accum = 0;
        int index;
        for (index = 0; index < cache->entriesLength; index++) {
            CacheEntry* entry = entries[index];
            if (entry->referenceCount == 0) {
                if (entry->size >= threshold) {
                    entry->flags |= CACHE_ENTRY_MARKED_FOR_EVICTION;

                    // We've just found one huge entry, there is no point to
                    // mark individual smaller entries in the code path below,
                    // reset the accumulator to skip it entirely.
                    accum = 0;
                    break;
                } else {
                    accum += entry->size;

                    if (accum >= threshold) {
                        break;
                    }
                }
            }
        }

        if (accum != 0) {
            // The loop below assumes index to be positioned on the entry, where
            // accumulator stopped. If we've reached the end, reposition
            // it to the last entry.
            if (index == cache->entriesLength) {
                index -= 1;
            }

            // Loop backwards from the point we've stopped and mark all
            // unreferenced entries for sweeping.
            for (; index >= 0; index--) {
                CacheEntry* entry = entries[index];
                if (entry->referenceCount == 0) {
                    entry->flags |= CACHE_ENTRY_MARKED_FOR_EVICTION;
                }
            }
        }

        mem_free(entries);
    }

    cache_purge(cache);

    if (cache->maxSize - cache->size >= size) {
        return true;
    }

    return false;
}

// 0x41F69C
static bool cache_purge(Cache* cache)
{
    for (int index = 0; index < cache->entriesLength; index++) {
        CacheEntry* cacheEntry = cache->entries[index];
        if ((cacheEntry->flags & CACHE_ENTRY_MARKED_FOR_EVICTION) != 0) {
            if (cacheEntry->referenceCount != 0) {
                // Entry was marked for eviction but still has references,
                // unmark it.
                cacheEntry->flags &= ~CACHE_ENTRY_MARKED_FOR_EVICTION;
            } else {
                int cacheEntrySize = cacheEntry->size;

                // NOTE: Uninline.
                cache_destroy_item(cache, cacheEntry);

                // Move entries up.
                memmove(&(cache->entries[index]), &(cache->entries[index + 1]), sizeof(*cache->entries) * ((cache->entriesLength - index) - 1));

                cache->entriesLength--;
                cache->size -= cacheEntrySize;

                // The entry was removed, compensate index.
                index--;
            }
        }
    }

    return true;
}

// 0x41F740
static bool cache_resize_array(Cache* cache, int newCapacity)
{
    if (newCapacity < cache->entriesLength) {
        return false;
    }

    CacheEntry** entries = (CacheEntry**)mem_realloc(cache->entries, sizeof(*cache->entries) * newCapacity);
    if (entries == NULL) {
        return false;
    }

    cache->entries = entries;
    cache->entriesCapacity = newCapacity;

    return true;
}

// 0x41F774
static int cache_compare_make_room(const void* a1, const void* a2)
{
    CacheEntry* v1 = *(CacheEntry**)a1;
    CacheEntry* v2 = *(CacheEntry**)a2;

    if (v1->referenceCount != 0 && v2->referenceCount == 0) {
        return 1;
    }

    if (v2->referenceCount != 0 && v1->referenceCount == 0) {
        return -1;
    }

    if (v1->hits < v2->hits) {
        return -1;
    } else if (v1->hits > v2->hits) {
        return 1;
    }

    if (v1->mru < v2->mru) {
        return -1;
    } else if (v1->mru > v2->mru) {
        return 1;
    }

    return 0;
}

// 0x41F7E8
static int cache_compare_reset_counter(const void* a1, const void* a2)
{
    CacheEntry* v1 = *(CacheEntry**)a1;
    CacheEntry* v2 = *(CacheEntry**)a2;

    if (v1->mru < v2->mru) {
        return 1;
    } else if (v1->mru > v2->mru) {
        return -1;
    } else {
        return 0;
    }
}

} // namespace fallout
