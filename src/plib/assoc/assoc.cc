#include "plib/assoc/assoc.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "platform_compat.h"

namespace fallout {

// NOTE: I guess this marker is used as a type discriminator for implementing
// nested dictionaries. That's why every dictionary-related function starts
// with a check for this value.
#define DICTIONARY_MARKER 0xFEBAFEBA

static void* default_malloc(size_t t);
static void* default_realloc(void* p, size_t t);
static void default_free(void* p);
static int assoc_find(assoc_array* a, const char* name, int* position);
static int assoc_read_long(FILE* fp, long* theLong);
static int assoc_read_assoc_array(FILE* fp, assoc_array* a);
static int assoc_write_long(FILE* fp, long theLong);
static int assoc_write_assoc_array(FILE* fp, assoc_array* a);

// 0x51E408
static assoc_malloc_func* internal_malloc = default_malloc;

// 0x51E40C
static assoc_realloc_func* internal_realloc = default_realloc;

// 0x51E410
static assoc_free_func* internal_free = default_free;

// 0x4D9B90
static void* default_malloc(size_t t)
{
    return malloc(t);
}

// 0x4D9B98
static void* default_realloc(void* p, size_t t)
{
    return realloc(p, t);
}

// 0x4D9BA0
static void default_free(void* p)
{
    free(p);
}

// 0x4D9BA8
int assoc_init(assoc_array* a, int n, size_t datasize, assoc_func_list* assoc_funcs)
{
    a->max = n;
    a->datasize = datasize;
    a->size = 0;

    if (assoc_funcs != NULL) {
        memcpy(&(a->load_save_funcs), assoc_funcs, sizeof(*assoc_funcs));
    } else {
        a->load_save_funcs.loadFunc = NULL;
        a->load_save_funcs.saveFunc = NULL;
        a->load_save_funcs.loadFuncDB = NULL;
        a->load_save_funcs.saveFuncDB = NULL;
    }

    int rc = 0;

    if (n != 0) {
        a->list = (assoc_pair*)internal_malloc(sizeof(*a->list) * n);
        if (a->list == NULL) {
            rc = -1;
        }
    } else {
        a->list = NULL;
    }

    if (rc != -1) {
        a->init_flag = DICTIONARY_MARKER;
    }

    return rc;
}

// 0x4D9C0C
int assoc_resize(assoc_array* a, int n)
{
    if (a->init_flag != DICTIONARY_MARKER) {
        return -1;
    }

    if (n < a->size) {
        return -1;
    }

    assoc_pair* entries = (assoc_pair*)internal_realloc(a->list, sizeof(*a->list) * n);
    if (entries == NULL) {
        return -1;
    }

    a->max = n;
    a->list = entries;

    return 0;
}

// 0x4D9C48
int assoc_free(assoc_array* a)
{
    if (a->init_flag != DICTIONARY_MARKER) {
        return -1;
    }

    for (int index = 0; index < a->size; index++) {
        assoc_pair* entry = &(a->list[index]);
        if (entry->name != NULL) {
            internal_free(entry->name);
        }

        if (entry->data != NULL) {
            internal_free(entry->data);
        }
    }

    if (a->list != NULL) {
        internal_free(a->list);
    }

    memset(a, 0, sizeof(*a));

    return 0;
}

// Finds index for the given key.
//
// Returns 0 if key is found. Otherwise returns -1, in this case [indexPtr]
// specifies an insertion point for given key.
//
// 0x4D9CC4
static int assoc_find(assoc_array* a, const char* name, int* position)
{
    if (a->init_flag != DICTIONARY_MARKER) {
        return -1;
    }

    if (a->size == 0) {
        *position = 0;
        return -1;
    }

    int r = a->size - 1;
    int l = 0;
    int mid = 0;
    int cmp = 0;
    while (r >= l) {
        mid = (l + r) / 2;

        cmp = compat_stricmp(name, a->list[mid].name);
        if (cmp == 0) {
            break;
        }

        if (cmp > 0) {
            l = l + 1;
        } else {
            r = r - 1;
        }
    }

    if (cmp == 0) {
        *position = mid;
        return 0;
    }

    if (cmp < 0) {
        *position = mid;
    } else {
        *position = mid + 1;
    }

    return -1;
}

// Returns the index of the entry for the specified key, or -1 if it's not
// present in the dictionary.
//
// 0x4D9D5C
int assoc_search(assoc_array* a, const char* name)
{
    if (a->init_flag != DICTIONARY_MARKER) {
        return -1;
    }

    int index;
    if (assoc_find(a, name, &index) != 0) {
        return -1;
    }

    return index;
}

// Adds key-value pair to the dictionary if the specified key is not already
// present.
//
// Returns 0 on success, or -1 on any error (including key already exists
// error).
//
// 0x4D9D88
int assoc_insert(assoc_array* a, const char* name, const void* data)
{
    if (a->init_flag != DICTIONARY_MARKER) {
        return -1;
    }

    int newElementIndex;
    if (assoc_find(a, name, &newElementIndex) == 0) {
        // Element for this key is already exists.
        return -1;
    }

    if (a->size == a->max) {
        // assoc array reached it's capacity and needs to be enlarged.
        if (assoc_resize(a, 2 * (a->max + 1)) == -1) {
            return -1;
        }
    }

    // Make a copy of the key.
    char* keyCopy = (char*)internal_malloc(strlen(name) + 1);
    if (keyCopy == NULL) {
        return -1;
    }

    strcpy(keyCopy, name);

    // Make a copy of the value.
    void* valueCopy = NULL;
    if (data != NULL && a->datasize != 0) {
        valueCopy = internal_malloc(a->datasize);
        if (valueCopy == NULL) {
            internal_free(keyCopy);
            return -1;
        }
    }

    if (valueCopy != NULL && a->datasize != 0) {
        memcpy(valueCopy, data, a->datasize);
    }

    // Starting at the end of entries array loop backwards and move entries down
    // one by one until we reach insertion point.
    for (int index = a->size; index > newElementIndex; index--) {
        assoc_pair* src = &(a->list[index - 1]);
        assoc_pair* dest = &(a->list[index]);
        memcpy(dest, src, sizeof(*a->list));
    }

    assoc_pair* entry = &(a->list[newElementIndex]);
    entry->name = keyCopy;
    entry->data = valueCopy;

    a->size++;

    return 0;
}

// Removes key-value pair from the dictionary if specified key is present in
// the dictionary.
//
// Returns 0 on success, -1 on any error (including key not present error).
//
// 0x4D9EE8
int assoc_delete(assoc_array* a, const char* name)
{
    if (a->init_flag != DICTIONARY_MARKER) {
        return -1;
    }

    int indexToRemove;
    if (assoc_find(a, name, &indexToRemove) == -1) {
        return -1;
    }

    assoc_pair* entry = &(a->list[indexToRemove]);

    // Free key and value (which are copies).
    internal_free(entry->name);
    if (entry->data != NULL) {
        internal_free(entry->data);
    }

    a->size--;

    // Starting from the index of the entry we've just removed, loop thru the
    // remaining of the array and move entries up one by one.
    for (int index = indexToRemove; index < a->size; index++) {
        assoc_pair* src = &(a->list[index + 1]);
        assoc_pair* dest = &(a->list[index]);
        memcpy(dest, src, sizeof(*a->list));
    }

    return 0;
}

// NOTE: Unused.
//
// 0x4D9F84
int assoc_copy(assoc_array* dst, assoc_array* src)
{
    if (src->init_flag != DICTIONARY_MARKER) {
        return -1;
    }

    if (assoc_init(dst, src->max, src->datasize, &(src->load_save_funcs)) != 0) {
        // FIXME: Should return -1, as we were unable to initialize dictionary.
        return 0;
    }

    for (int index = 0; index < src->size; index++) {
        assoc_pair* entry = &(src->list[index]);
        if (assoc_insert(dst, entry->name, entry->data) == -1) {
            return -1;
        }
    }

    return 0;
}

// NOTE: Unused.
//
// 0x4DA090
static int assoc_read_long(FILE* fp, long* theLong)
{
    int c;
    int temp;

    c = fgetc(fp);
    if (c == -1) {
        return -1;
    }

    temp = (c & 0xFF);

    c = fgetc(fp);
    if (c == -1) {
        return -1;
    }

    temp = (temp << 8) | (c & 0xFF);

    c = fgetc(fp);
    if (c == -1) {
        return -1;
    }

    temp = (temp << 8) | (c & 0xFF);

    c = fgetc(fp);
    if (c == -1) {
        return -1;
    }

    temp = (temp << 8) | (c & 0xFF);

    *theLong = temp;

    return 0;
}

// NOTE: Unused.
//
// 0x4DA0F4
static int assoc_read_assoc_array(FILE* fp, assoc_array* a)
{
    long temp;

    if (assoc_read_long(fp, &temp) != 0) return -1;
    a->size = temp;

    if (assoc_read_long(fp, &temp) != 0) return -1;
    a->max = temp;

    if (assoc_read_long(fp, &temp) != 0) return -1;
    a->datasize = temp;

    // NOTE: original code reads `a->list` pointer which is meaningless.
    if (assoc_read_long(fp, &temp) != 0) return -1;

    return 0;
}

// NOTE: Unused.
//
// 0x4DA158
int assoc_load(FILE* fp, assoc_array* a, int flags)
{
    if (a->init_flag != DICTIONARY_MARKER) {
        return -1;
    }

    for (int index = 0; index < a->size; index++) {
        assoc_pair* entry = &(a->list[index]);
        if (entry->name != NULL) {
            internal_free(entry->name);
        }

        if (entry->data != NULL) {
            internal_free(entry->data);
        }
    }

    if (a->list != NULL) {
        internal_free(a->list);
    }

    if (assoc_read_assoc_array(fp, a) != 0) {
        return -1;
    }

    a->list = NULL;

    if (a->max <= 0) {
        return 0;
    }

    a->list = (assoc_pair*)internal_malloc(sizeof(*a->list) * a->max);
    if (a->list == NULL) {
        return -1;
    }

    for (int index = 0; index < a->size; index++) {
        assoc_pair* entry = &(a->list[index]);
        entry->name = NULL;
        entry->data = NULL;
    }

    if (a->size <= 0) {
        return 0;
    }

    for (int index = 0; index < a->size; index++) {
        assoc_pair* entry = &(a->list[index]);
        int keyLength = fgetc(fp);
        if (keyLength == -1) {
            return -1;
        }

        entry->name = (char*)internal_malloc(keyLength + 1);
        if (entry->name == NULL) {
            return -1;
        }

        if (fgets(entry->name, keyLength + 1, fp) == NULL) {
            return -1;
        }

        if (a->datasize != 0) {
            entry->data = internal_malloc(a->datasize);
            if (entry->data == NULL) {
                return -1;
            }

            if (a->load_save_funcs.loadFunc != NULL) {
                if (a->load_save_funcs.loadFunc(fp, entry->data, a->datasize, flags) != 0) {
                    return -1;
                }
            } else {
                if (fread(entry->data, a->datasize, 1, fp) != 1) {
                    return -1;
                }
            }
        }
    }

    return 0;
}

// NOTE: Unused.
//
// 0x4DA2EC
static int assoc_write_long(FILE* fp, long theLong)
{
    if (fputc((theLong >> 24) & 0xFF, fp) == -1) return -1;
    if (fputc((theLong >> 16) & 0xFF, fp) == -1) return -1;
    if (fputc((theLong >> 8) & 0xFF, fp) == -1) return -1;
    if (fputc(theLong & 0xFF, fp) == -1) return -1;

    return 0;
}

// NOTE: Unused.
//
// 0x4DA360
static int assoc_write_assoc_array(FILE* fp, assoc_array* a)
{
    if (assoc_write_long(fp, a->size) != 0) return -1;
    if (assoc_write_long(fp, a->max) != 0) return -1;
    if (assoc_write_long(fp, a->datasize) != 0) return -1;
    // NOTE: Original code writes `a->list` pointer which is meaningless.
    if (assoc_write_long(fp, 0) != 0) return -1;

    return 0;
}

// NOTE: Unused.
//
// 0x4DA3A4
int assoc_save(FILE* fp, assoc_array* a, int flags)
{
    if (a->init_flag != DICTIONARY_MARKER) {
        return -1;
    }

    if (assoc_write_assoc_array(fp, a) != 0) {
        return -1;
    }

    for (int index = 0; index < a->size; index++) {
        assoc_pair* entry = &(a->list[index]);
        int keyLength = strlen(entry->name);
        if (fputc(keyLength, fp) == -1) {
            return -1;
        }

        if (fputs(entry->name, fp) == -1) {
            return -1;
        }

        if (a->load_save_funcs.saveFunc != NULL) {
            if (a->datasize != 0) {
                if (a->load_save_funcs.saveFunc(fp, entry->data, a->datasize, flags) != 0) {
                    return -1;
                }
            }
        } else {
            if (a->datasize != 0) {
                if (fwrite(entry->data, a->datasize, 1, fp) != 1) {
                    return -1;
                }
            }
        }
    }

    return 0;
}

// 0x4DA498
void assoc_register_mem(assoc_malloc_func* malloc_func, assoc_realloc_func* realloc_func, assoc_free_func* free_func)
{
    if (malloc_func != NULL && realloc_func != NULL && free_func != NULL) {
        internal_malloc = malloc_func;
        internal_realloc = realloc_func;
        internal_free = free_func;
    } else {
        internal_malloc = default_malloc;
        internal_realloc = default_realloc;
        internal_free = default_free;
    }
}

} // namespace fallout
