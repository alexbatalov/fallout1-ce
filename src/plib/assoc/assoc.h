#ifndef FALLOUT_PLIB_ASSOC_ASSOC_H_
#define FALLOUT_PLIB_ASSOC_ASSOC_H_

#include <stdio.h>

#include "plib/db/db.h"

namespace fallout {

typedef void*(assoc_malloc_func)(size_t size);
typedef void*(assoc_realloc_func)(void* ptr, size_t newSize);
typedef void(assoc_free_func)(void* ptr);
typedef int(assoc_load_func)(FILE* stream, void* buffer, size_t size, int flags);
typedef int(assoc_save_func)(FILE* stream, void* buffer, size_t size, int flags);
typedef int(assoc_load_func_db)(DB_FILE* stream, void* buffer, size_t size, int flags);
typedef int(assoc_save_func_db)(DB_FILE* stream, void* buffer, size_t size, int flags);

typedef struct assoc_func_list {
    assoc_load_func* loadFunc;
    assoc_save_func* saveFunc;
    assoc_load_func_db* loadFuncDB;
    assoc_save_func_db* saveFuncDB;
    assoc_load_func* newLoadFunc;
} assoc_func_list;

// A tuple containing individual key-value pair of an assoc array.
typedef struct assoc_pair {
    char* name;
    void* data;
} assoc_pair;

// A collection of key/value pairs.
//
// The keys in assoc array are always strings. Internally pairs are kept sorted
// by the key. Both keys and values are copied when new entry is added to
// assoc array. For this reason the size of the value's type is provided during
// assoc array initialization.
typedef struct assoc_array {
    int init_flag;

    // The number of key/value pairs in the array.
    int size;

    // The capacity of key/value pairs in [entries] array.
    int max;

    // The size of the values in bytes.
    size_t datasize;

    // IO callbacks.
    assoc_func_list load_save_funcs;

    // The array of key-value pairs.
    assoc_pair* list;
} assoc_array;

int assoc_init(assoc_array* a, int n, size_t datasize, assoc_func_list* assoc_funcs);
int assoc_resize(assoc_array* a, int n);
int assoc_free(assoc_array* a);
int assoc_search(assoc_array* a, const char* name);
int assoc_insert(assoc_array* a, const char* name, const void* data);
int assoc_delete(assoc_array* a, const char* name);
int assoc_copy(assoc_array* dst, assoc_array* src);
int assoc_load(FILE* fp, assoc_array* a, int flags);
int assoc_save(FILE* fp, assoc_array* a, int flags);
void assoc_register_mem(assoc_malloc_func* malloc_func, assoc_realloc_func* realloc_func, assoc_free_func* free_func);

} // namespace fallout

#endif /* FALLOUT_PLIB_ASSOC_ASSOC_H_ */
