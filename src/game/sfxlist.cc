#include "game/sfxlist.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <adecode/adecode.h>

#include "platform_compat.h"
#include "plib/db/db.h"
#include "plib/gnw/debug.h"
#include "plib/gnw/memory.h"

namespace fallout {

typedef struct SoundEffectsListEntry {
    char* name;
    int dataSize;
    int fileSize;
} SoundEffectsListEntry;

static int sfxl_index(int tag, int* indexPtr);
static int sfxl_index_to_tag(int index, int* tagPtr);
static void sfxl_destroy();
static int sfxl_get_names();
static int sfxl_copy_names(char** fileNameList);
static int sfxl_get_sizes();
static int sfxl_sort_by_name();
static int sfxl_compare_by_name(const void* a1, const void* a2);
static unsigned int sfxl_ad_reader(void* stream, void* buf, unsigned int size);

// 0x507A8C
static bool sfxl_initialized = false;

// 0x507A90
static int sfxl_dlevel = INT_MAX;

// 0x507A94
static char* sfxl_effect_path = NULL;

// 0x507A98
static int sfxl_effect_path_len = 0;

// sndlist.lst
//
// 0x507A9C
static SoundEffectsListEntry* sfxl_list = NULL;

// The length of [sfxl_list] array.
//
// 0x507AA0
static int sfxl_files_total = 0;

// 0x664FEC
static int sfxl_compression;

// 0x497960
bool sfxl_tag_is_legal(int a1)
{
    return sfxl_index(a1, NULL) == SFXL_OK;
}

// 0x497974
int sfxl_init(const char* soundEffectsPath, int compression, int debugLevel)
{
    int err;

    sfxl_dlevel = debugLevel;
    sfxl_compression = compression;
    sfxl_files_total = 0;

    sfxl_effect_path = mem_strdup(soundEffectsPath);
    if (sfxl_effect_path == NULL) {
        return SFXL_ERR;
    }

    sfxl_effect_path_len = strlen(sfxl_effect_path);

    err = sfxl_get_names();
    if (err != SFXL_OK) {
        mem_free(sfxl_effect_path);
        return err;
    }

    err = sfxl_get_sizes();
    if (err != SFXL_OK) {
        sfxl_destroy();
        mem_free(sfxl_effect_path);
        return err;
    }

    // NOTE: Uninline.
    err = sfxl_sort_by_name();
    if (err != SFXL_OK) {
        sfxl_destroy();
        mem_free(sfxl_effect_path);
        return err;
    }

    sfxl_initialized = true;

    return SFXL_OK;
}

// 0x497A44
void sfxl_exit()
{
    if (sfxl_initialized) {
        sfxl_destroy();
        mem_free(sfxl_effect_path);
        sfxl_initialized = false;
    }
}

// 0x497A68
int sfxl_name_to_tag(char* name, int* tagPtr)
{
    SoundEffectsListEntry dummy;
    SoundEffectsListEntry* entry;
    int tag;

    if (compat_strnicmp(sfxl_effect_path, name, sfxl_effect_path_len) != 0) {
        return SFXL_ERR;
    }

    dummy.name = name + sfxl_effect_path_len;

    entry = (SoundEffectsListEntry*)bsearch(&dummy, sfxl_list, sfxl_files_total, sizeof(*sfxl_list), sfxl_compare_by_name);
    if (entry == NULL) {
        return SFXL_ERR;
    }

    if (sfxl_index_to_tag(entry - sfxl_list, &tag) != SFXL_OK) {
        return SFXL_ERR;
    }

    *tagPtr = tag;

    return SFXL_OK;
}

// 0x497B20
int sfxl_name(int tag, char** pathPtr)
{
    int index;
    int err = sfxl_index(tag, &index);
    if (err != SFXL_OK) {
        return err;
    }

    char* name = sfxl_list[index].name;

    char* path = (char*)mem_malloc(strlen(sfxl_effect_path) + strlen(name) + 1);
    if (path == NULL) {
        return SFXL_ERR;
    }

    strcpy(path, sfxl_effect_path);
    strcat(path, name);

    *pathPtr = path;

    return SFXL_OK;
}

// 0x497BE8
int sfxl_size_full(int tag, int* sizePtr)
{
    int index;
    int rc = sfxl_index(tag, &index);
    if (rc != SFXL_OK) {
        return rc;
    }

    SoundEffectsListEntry* entry = &(sfxl_list[index]);
    *sizePtr = entry->dataSize;

    return SFXL_OK;
}

// 0x497C18
int sfxl_size_cached(int tag, int* sizePtr)
{
    int index;
    int err = sfxl_index(tag, &index);
    if (err != SFXL_OK) {
        return err;
    }

    SoundEffectsListEntry* entry = &(sfxl_list[index]);
    *sizePtr = entry->fileSize;

    return SFXL_OK;
}

// 0x497C48
static int sfxl_index(int tag, int* indexPtr)
{
    if (tag <= 0) {
        return SFXL_ERR_TAG_INVALID;
    }

    if ((tag & 1) != 0) {
        return SFXL_ERR_TAG_INVALID;
    }

    int index = (tag / 2) - 1;
    if (index >= sfxl_files_total) {
        return SFXL_ERR_TAG_INVALID;
    }

    if (indexPtr != NULL) {
        *indexPtr = index;
    }

    return SFXL_OK;
}

// 0x497C80
static int sfxl_index_to_tag(int index, int* tagPtr)
{
    if (index >= sfxl_files_total) {
        return SFXL_ERR;
    }

    if (index < 0) {
        return SFXL_ERR;
    }

    *tagPtr = 2 * (index + 1);

    return SFXL_OK;
}

// 0x497CA4
static void sfxl_destroy()
{
    if (sfxl_files_total < 0) {
        return;
    }

    if (sfxl_list == NULL) {
        return;
    }

    for (int index = 0; index < sfxl_files_total; index++) {
        SoundEffectsListEntry* entry = &(sfxl_list[index]);
        if (entry->name != NULL) {
            mem_free(entry->name);
        }
    }

    mem_free(sfxl_list);
    sfxl_list = NULL;

    sfxl_files_total = 0;
}

// 0x497D00
static int sfxl_get_names()
{
    const char* extension;
    switch (sfxl_compression) {
    case 0:
        extension = "*.SND";
        break;
    case 1:
        extension = "*.ACM";
        break;
    default:
        return SFXL_ERR;
    }

    char* pattern = (char*)mem_malloc(strlen(sfxl_effect_path) + strlen(extension) + 1);
    if (pattern == NULL) {
        return SFXL_ERR;
    }

    strcpy(pattern, sfxl_effect_path);
    strcat(pattern, extension);

    char** fileNameList;
    sfxl_files_total = db_get_file_list(pattern, &fileNameList, NULL, 0);
    mem_free(pattern);

    if (sfxl_files_total > 10000) {
        db_free_file_list(&fileNameList, NULL);
        return SFXL_ERR;
    }

    if (sfxl_files_total <= 0) {
        return SFXL_ERR;
    }

    sfxl_list = (SoundEffectsListEntry*)mem_malloc(sizeof(*sfxl_list) * sfxl_files_total);
    if (sfxl_list == NULL) {
        db_free_file_list(&fileNameList, NULL);
        return SFXL_ERR;
    }

    memset(sfxl_list, 0, sizeof(*sfxl_list) * sfxl_files_total);

    int err = sfxl_copy_names(fileNameList);

    db_free_file_list(&fileNameList, NULL);

    if (err != SFXL_OK) {
        sfxl_destroy();
        return err;
    }

    return SFXL_OK;
}

// 0x497E68
static int sfxl_copy_names(char** fileNameList)
{
    for (int index = 0; index < sfxl_files_total; index++) {
        SoundEffectsListEntry* entry = &(sfxl_list[index]);
        entry->name = mem_strdup(*fileNameList++);
        if (entry->name == NULL) {
            sfxl_destroy();
            return SFXL_ERR;
        }
    }

    return SFXL_OK;
}

// 0x497EB8
static int sfxl_get_sizes()
{
    dir_entry de;

    char* path = (char*)mem_malloc(sfxl_effect_path_len + 13);
    if (path == NULL) {
        return SFXL_ERR;
    }

    strcpy(path, sfxl_effect_path);

    char* fileName = path + sfxl_effect_path_len;

    for (int index = 0; index < sfxl_files_total; index++) {
        SoundEffectsListEntry* entry = &(sfxl_list[index]);
        strcpy(fileName, entry->name);

        if (db_dir_entry(path, &de) != 0) {
            mem_free(path);
            return SFXL_ERR;
        }

        if (de.length <= 0) {
            mem_free(path);
            return SFXL_ERR;
        }

        entry->fileSize = de.length;

        switch (sfxl_compression) {
        case 0:
            entry->dataSize = de.length;
            break;
        case 1:
            if (1) {
                DB_FILE* stream = db_fopen(path, "rb");
                if (stream == NULL) {
                    mem_free(path);
                    return 1;
                }

                int channels;
                int sampleRate;
                int sampleCount;
                AudioDecoder* ad = Create_AudioDecoder(sfxl_ad_reader, stream, &channels, &sampleRate, &sampleCount);
                entry->dataSize = 2 * sampleCount;
                AudioDecoder_Close(ad);
                db_fclose(stream);
            }
            break;
        default:
            mem_free(path);
            return SFXL_ERR;
        }
    }

    mem_free(path);

    return SFXL_OK;
}

// 0x49806C
static int sfxl_sort_by_name()
{
    if (sfxl_files_total != 1) {
        qsort(sfxl_list, sfxl_files_total, sizeof(*sfxl_list), sfxl_compare_by_name);
    }
    return SFXL_OK;
}

// 0x498094
static int sfxl_compare_by_name(const void* a1, const void* a2)
{
    SoundEffectsListEntry* v1 = (SoundEffectsListEntry*)a1;
    SoundEffectsListEntry* v2 = (SoundEffectsListEntry*)a2;

    return compat_stricmp(v1->name, v2->name);
}

// 0x4980A0
static unsigned int sfxl_ad_reader(void* stream, void* buf, unsigned int size)
{
    return db_fread(buf, 1, size, (DB_FILE*)stream);
}

} // namespace fallout
