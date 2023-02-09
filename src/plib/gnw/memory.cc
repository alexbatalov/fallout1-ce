#include "plib/gnw/memory.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "plib/gnw/debug.h"
#include "plib/gnw/gnw.h"

namespace fallout {

// A special value that denotes a beginning of a memory block data.
#define MEMORY_BLOCK_HEADER_GUARD 0xFEEDFACE

// A special value that denotes an ending of a memory block data.
#define MEMORY_BLOCK_FOOTER_GUARD 0xBEEFCAFE

// A header of a memory block.
typedef struct MemoryBlockHeader {
    // Size of the memory block including header and footer.
    size_t size;

    // See `MEMORY_BLOCK_HEADER_GUARD`.
    int guard;
} MemoryBlockHeader;

// A footer of a memory block.
typedef struct MemoryBlockFooter {
    // See `MEMORY_BLOCK_FOOTER_GUARD`.
    int guard;
} MemoryBlockFooter;

static void* my_malloc(size_t size);
static void* my_realloc(void* ptr, size_t size);
static void my_free(void* ptr);
static void* mem_prep_block(void* block, size_t size);
static void mem_check_block(void* block);

// 0x539D18
static MallocFunc* p_malloc = my_malloc;

// 0x539D1C
static ReallocFunc* p_realloc = my_realloc;

// 0x539D20
static FreeFunc* p_free = my_free;

// 0x539D24
static int num_blocks = 0;

// 0x539D28
static int max_blocks = 0;

// 0x539D2C
static size_t mem_allocated = 0;

// 0x539D30
static size_t max_allocated = 0;

// 0x4AEBE0
char* mem_strdup(const char* string)
{
    char* copy = NULL;
    if (string != NULL) {
        copy = (char*)p_malloc(strlen(string) + 1);
        strcpy(copy, string);
    }
    return copy;
}

// 0x4AEC30
void* mem_malloc(size_t size)
{
    return p_malloc(size);
}

// 0x4AEC38
static void* my_malloc(size_t size)
{
    void* ptr = NULL;

    if (size != 0) {
        size += sizeof(MemoryBlockHeader) + sizeof(MemoryBlockFooter);
        size += sizeof(int) - size % sizeof(int);

        unsigned char* block = (unsigned char*)malloc(size);
        if (block != NULL) {
            // NOTE: Uninline.
            ptr = mem_prep_block(block, size);

            num_blocks++;
            if (num_blocks > max_blocks) {
                max_blocks = num_blocks;
            }

            mem_allocated += size;
            if (mem_allocated > max_allocated) {
                max_allocated = mem_allocated;
            }
        }
    }

    return ptr;
}

// 0x4AECB0
void* mem_realloc(void* ptr, size_t size)
{
    return p_realloc(ptr, size);
}

// 0x4AECB8
static void* my_realloc(void* ptr, size_t size)
{
    if (ptr != NULL) {
        unsigned char* block = (unsigned char*)ptr - sizeof(MemoryBlockHeader);

        MemoryBlockHeader* header = (MemoryBlockHeader*)block;
        size_t oldSize = header->size;

        mem_allocated -= oldSize;

        mem_check_block(block);

        if (size != 0) {
            size += sizeof(MemoryBlockHeader) + sizeof(MemoryBlockFooter);
            size += sizeof(int) - size % sizeof(int);
        }

        unsigned char* newBlock = (unsigned char*)realloc(block, size);
        if (newBlock != NULL) {
            mem_allocated += size;
            if (mem_allocated > max_allocated) {
                max_allocated = mem_allocated;
            }

            // NOTE: Uninline.
            ptr = mem_prep_block(newBlock, size);
        } else {
            if (size != 0) {
                mem_allocated += oldSize;

                debug_printf("%s,%u: ", __FILE__, __LINE__); // "Memory.c", 195
                debug_printf("Realloc failure.\n");
            } else {
                num_blocks--;
            }
            ptr = NULL;
        }
    } else {
        ptr = p_malloc(size);
    }

    return ptr;
}

// 0x4AED84
void mem_free(void* ptr)
{
    p_free(ptr);
}

// 0x4AED8C
static void my_free(void* ptr)
{
    if (ptr != NULL) {
        void* block = (unsigned char*)ptr - sizeof(MemoryBlockHeader);
        MemoryBlockHeader* header = (MemoryBlockHeader*)block;

        mem_check_block(block);

        mem_allocated -= header->size;
        num_blocks--;

        free(block);
    }
}

// 0x4AEDBC
void mem_check()
{
    if (p_malloc == my_malloc) {
        debug_printf("Current memory allocated: %6d blocks, %9u bytes total\n", num_blocks, mem_allocated);
        debug_printf("Max memory allocated:     %6d blocks, %9u bytes total\n", max_blocks, max_allocated);
    }
}

// 0x4AEE08
void mem_register_func(MallocFunc* mallocFunc, ReallocFunc* reallocFunc, FreeFunc* freeFunc)
{
    if (!GNW_win_init_flag) {
        p_malloc = mallocFunc;
        p_realloc = reallocFunc;
        p_free = freeFunc;
    }
}

// 0x4AEE24
static void* mem_prep_block(void* block, size_t size)
{
    MemoryBlockHeader* header;
    MemoryBlockFooter* footer;

    header = (MemoryBlockHeader*)block;
    header->guard = MEMORY_BLOCK_HEADER_GUARD;
    header->size = size;

    footer = (MemoryBlockFooter*)((unsigned char*)block + size - sizeof(*footer));
    footer->guard = MEMORY_BLOCK_FOOTER_GUARD;

    return (unsigned char*)block + sizeof(*header);
}

// 0x4AEE44
static void mem_check_block(void* block)
{
    MemoryBlockHeader* header;
    MemoryBlockFooter* footer;

    header = (MemoryBlockHeader*)block;
    if (header->guard != MEMORY_BLOCK_HEADER_GUARD) {
        debug_printf("Memory header stomped.\n");
    }

    footer = (MemoryBlockFooter*)((unsigned char*)block + header->size - sizeof(MemoryBlockFooter));
    if (footer->guard != MEMORY_BLOCK_FOOTER_GUARD) {
        debug_printf("Memory footer stomped.\n");
    }
}

} // namespace fallout
