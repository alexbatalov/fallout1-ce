#include "game/sfxcache.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <adecode/adecode.h>

#include "game/cache.h"
#include "game/gconfig.h"
#include "game/sfxlist.h"
#include "plib/db/db.h"
#include "plib/gnw/memory.h"

namespace fallout {

#define SOUND_EFFECTS_CACHE_MIN_SIZE 0x40000

typedef struct SoundEffect {
    // NOTE: This field is only 1 byte, likely unsigned char. It always uses
    // cmp for checking implying it's not bitwise flags. Therefore it's better
    // to express it as boolean.
    bool used;
    CacheEntry* cacheHandle;
    int tag;
    int dataSize;
    int fileSize;
    // TODO: Make size_t.
    int position;
    int dataPosition;
    unsigned char* data;
} SoundEffect;

static int sfxc_effect_size(int tag, int* sizePtr);
static int sfxc_effect_load(int tag, int* sizePtr, unsigned char* data);
static void sfxc_effect_free(void* ptr);
static int sfxc_handle_list_create();
static void sfxc_handle_list_destroy();
static int sfxc_handle_create(int* handlePtr, int id, void* data, CacheEntry* cacheHandle);
static void sfxc_handle_destroy(int handle);
static bool sfxc_handle_is_legal(int a1);
static bool sfxc_mode_is_legal(int mode);
static int sfxc_decode(int handle, void* buf, unsigned int size);
static unsigned int sfxc_ad_reader(void* stream, void* buf, unsigned int size);

// 0x507A70
static int sfxc_dlevel = INT_MAX;

// 0x507A74
static char* sfxc_effect_path = NULL;

// 0x507A78
static SoundEffect* sfxc_handle_list = NULL;

// 0x507A7C
static int sfxc_files_open = 0;

// 0x507A80
static Cache* sfxc_pcache = NULL;

// 0x507A84
static bool sfxc_initialized = false;

// 0x507A88
static int sfxc_cmpr = 1;

// 0x497140
int sfxc_init(int cacheSize, const char* effectsPath)
{
    if (!config_get_value(&game_config, GAME_CONFIG_SOUND_KEY, GAME_CONFIG_DEBUG_SFXC_KEY, &sfxc_dlevel)) {
        sfxc_dlevel = 1;
    }

    if (cacheSize <= SOUND_EFFECTS_CACHE_MIN_SIZE) {
        return -1;
    }

    if (effectsPath == NULL) {
        effectsPath = "";
    }

    sfxc_effect_path = mem_strdup(effectsPath);
    if (sfxc_effect_path == NULL) {
        return -1;
    }

    if (sfxl_init(sfxc_effect_path, sfxc_cmpr, sfxc_dlevel) != SFXL_OK) {
        mem_free(sfxc_effect_path);
        return -1;
    }

    if (sfxc_handle_list_create() != 0) {
        sfxl_exit();
        mem_free(sfxc_effect_path);
        return -1;
    }

    sfxc_pcache = (Cache*)mem_malloc(sizeof(*sfxc_pcache));
    if (sfxc_pcache == NULL) {
        sfxc_handle_list_destroy();
        sfxl_exit();
        mem_free(sfxc_effect_path);
        return -1;
    }

    if (!cache_init(sfxc_pcache, sfxc_effect_size, sfxc_effect_load, sfxc_effect_free, cacheSize)) {
        mem_free(sfxc_pcache);
        sfxc_handle_list_destroy();
        sfxl_exit();
        mem_free(sfxc_effect_path);
        return -1;
    }

    sfxc_initialized = true;

    return 0;
}

// 0x49727C
void sfxc_exit()
{
    if (sfxc_initialized) {
        cache_exit(sfxc_pcache);
        mem_free(sfxc_pcache);
        sfxc_pcache = NULL;

        sfxc_handle_list_destroy();

        sfxl_exit();

        mem_free(sfxc_effect_path);

        sfxc_initialized = false;
    }
}

// 0x4972C0
int sfxc_is_initialized()
{
    return sfxc_initialized;
}

// 0x4972C8
void sfxc_flush()
{
    if (sfxc_initialized) {
        cache_flush(sfxc_pcache);
    }
}

// 0x4972DC
int sfxc_cached_open(const char* fname, int mode)
{
    if (sfxc_files_open >= SOUND_EFFECTS_MAX_COUNT) {
        return -1;
    }

    char* copy = mem_strdup(fname);
    if (copy == NULL) {
        return -1;
    }

    int tag;
    int err = sfxl_name_to_tag(copy, &tag);

    mem_free(copy);

    if (err != SFXL_OK) {
        return -1;
    }

    void* data;
    CacheEntry* cacheHandle;
    if (!cache_lock(sfxc_pcache, tag, &data, &cacheHandle)) {
        return -1;
    }

    int handle;
    if (sfxc_handle_create(&handle, tag, data, cacheHandle) != 0) {
        cache_unlock(sfxc_pcache, cacheHandle);
        return -1;
    }

    return handle;
}

// 0x4973A0
int sfxc_cached_close(int handle)
{
    if (!sfxc_handle_is_legal(handle)) {
        return -1;
    }

    SoundEffect* soundEffect = &(sfxc_handle_list[handle]);
    if (!cache_unlock(sfxc_pcache, soundEffect->cacheHandle)) {
        return -1;
    }

    // NOTE: Uninline.
    sfxc_handle_destroy(handle);

    return 0;
}

// 0x4973F4
int sfxc_cached_read(int handle, void* buf, unsigned int size)
{
    if (!sfxc_handle_is_legal(handle)) {
        return -1;
    }

    if (size == 0) {
        return 0;
    }

    SoundEffect* soundEffect = &(sfxc_handle_list[handle]);
    if (soundEffect->dataSize - soundEffect->position <= 0) {
        return 0;
    }

    size_t bytesToRead;
    // NOTE: Original code uses signed comparison.
    if ((int)size < (soundEffect->dataSize - soundEffect->position)) {
        bytesToRead = size;
    } else {
        bytesToRead = soundEffect->dataSize - soundEffect->position;
    }

    switch (sfxc_cmpr) {
    case 0:
        memcpy(buf, soundEffect->data + soundEffect->position, bytesToRead);
        break;
    case 1:
        if (sfxc_decode(handle, buf, bytesToRead) != 0) {
            return -1;
        }
        break;
    default:
        return -1;
    }

    soundEffect->position += bytesToRead;

    return bytesToRead;
}

// 0x4974D0
int sfxc_cached_write(int handle, const void* buf, unsigned int size)
{
    return -1;
}

// 0x4974D8
long sfxc_cached_seek(int handle, long offset, int origin)
{
    if (!sfxc_handle_is_legal(handle)) {
        return -1;
    }

    SoundEffect* soundEffect = &(sfxc_handle_list[handle]);

    int position;
    switch (origin) {
    case SEEK_SET:
        position = 0;
        break;
    case SEEK_CUR:
        position = soundEffect->position;
        break;
    case SEEK_END:
        position = soundEffect->dataSize;
        break;
    default:
        assert(false && "Should be unreachable");
    }

    long normalizedOffset = abs(offset);

    if (offset >= 0) {
        long remainingSize = soundEffect->dataSize - soundEffect->position;
        if (normalizedOffset > remainingSize) {
            normalizedOffset = remainingSize;
        }
        offset = position + normalizedOffset;
    } else {
        if (normalizedOffset > position) {
            return -1;
        }

        offset = position - normalizedOffset;
    }

    soundEffect->position = offset;

    return offset;
}

// 0x497574
long sfxc_cached_tell(int handle)
{
    if (!sfxc_handle_is_legal(handle)) {
        return -1;
    }

    SoundEffect* soundEffect = &(sfxc_handle_list[handle]);
    return soundEffect->position;
}

// 0x497598
long sfxc_cached_file_size(int handle)
{
    if (!sfxc_handle_is_legal(handle)) {
        return 0;
    }

    SoundEffect* soundEffect = &(sfxc_handle_list[handle]);
    return soundEffect->dataSize;
}

// 0x4975B4
static int sfxc_effect_size(int tag, int* sizePtr)
{
    int size;
    if (sfxl_size_cached(tag, &size) == -1) {
        return -1;
    }

    *sizePtr = size;

    return 0;
}

// 0x4975DC
static int sfxc_effect_load(int tag, int* sizePtr, unsigned char* data)
{
    if (!sfxl_tag_is_legal(tag)) {
        return -1;
    }

    int size;
    sfxl_size_cached(tag, &size);

    char* name;
    sfxl_name(tag, &name);

    if (db_read_to_buf(name, data)) {
        mem_free(name);
        return -1;
    }

    mem_free(name);

    *sizePtr = size;

    return 0;
}

// 0x49764C
static void sfxc_effect_free(void* ptr)
{
    mem_free(ptr);
}

// 0x497654
static int sfxc_handle_list_create()
{
    sfxc_handle_list = (SoundEffect*)mem_malloc(sizeof(*sfxc_handle_list) * SOUND_EFFECTS_MAX_COUNT);
    if (sfxc_handle_list == NULL) {
        return -1;
    }

    for (int index = 0; index < SOUND_EFFECTS_MAX_COUNT; index++) {
        SoundEffect* soundEffect = &(sfxc_handle_list[index]);
        soundEffect->used = false;
    }

    sfxc_files_open = 0;

    return 0;
}

// 0x497698
static void sfxc_handle_list_destroy()
{
    if (sfxc_files_open) {
        for (int index = 0; index < SOUND_EFFECTS_MAX_COUNT; index++) {
            SoundEffect* soundEffect = &(sfxc_handle_list[index]);
            if (!soundEffect->used) {
                sfxc_cached_close(index);
            }
        }
    }

    mem_free(sfxc_handle_list);
}

// 0x4976D0
static int sfxc_handle_create(int* handlePtr, int tag, void* data, CacheEntry* cacheHandle)
{
    if (sfxc_files_open >= SOUND_EFFECTS_MAX_COUNT) {
        return -1;
    }

    SoundEffect* soundEffect;
    int index;
    for (index = 0; index < SOUND_EFFECTS_MAX_COUNT; index++) {
        soundEffect = &(sfxc_handle_list[index]);
        if (!soundEffect->used) {
            break;
        }
    }

    if (index == SOUND_EFFECTS_MAX_COUNT) {
        return -1;
    }

    soundEffect->used = true;
    soundEffect->cacheHandle = cacheHandle;
    soundEffect->tag = tag;

    sfxl_size_full(tag, &(soundEffect->dataSize));
    sfxl_size_cached(tag, &(soundEffect->fileSize));

    soundEffect->position = 0;
    soundEffect->dataPosition = 0;

    soundEffect->data = (unsigned char*)data;

    *handlePtr = index;

    return 0;
}

// 0x497784
static void sfxc_handle_destroy(int handle)
{
    // NOTE: There is an overflow when handle == SOUND_EFFECTS_MAX_COUNT, but
    // thanks to [sfxc_handle_is_legal] handle will always be less than
    // [SOUND_EFFECTS_MAX_COUNT].
    if (handle <= SOUND_EFFECTS_MAX_COUNT) {
        sfxc_handle_list[handle].used = false;
    }
}

// 0x49779C
static bool sfxc_handle_is_legal(int handle)
{
    if (handle >= SOUND_EFFECTS_MAX_COUNT) {
        return false;
    }

    SoundEffect* soundEffect = &sfxc_handle_list[handle];

    if (!soundEffect->used) {
        return false;
    }

    if (soundEffect->dataSize < soundEffect->position) {
        return false;
    }

    return sfxl_tag_is_legal(soundEffect->tag);
}

// 0x4977DC
static bool sfxc_mode_is_legal(int mode)
{
    if ((mode & 0x01) != 0) return false;
    if ((mode & 0x02) != 0) return false;
    if ((mode & 0x10) != 0) return false;

    return true;
}

// 0x4977F8
static int sfxc_decode(int handle, void* buf, unsigned int size)
{
    if (!sfxc_handle_is_legal(handle)) {
        return -1;
    }

    SoundEffect* soundEffect = &(sfxc_handle_list[handle]);
    soundEffect->dataPosition = 0;

    int channels;
    int sampleRate;
    int sampleCount;
    AudioDecoder* ad = Create_AudioDecoder(sfxc_ad_reader, &handle, &channels, &sampleRate, &sampleCount);

    if (soundEffect->position != 0) {
        void* temp = mem_malloc(soundEffect->position);
        if (temp == NULL) {
            AudioDecoder_Close(ad);
            return -1;
        }

        size_t bytesRead = AudioDecoder_Read(ad, temp, soundEffect->position);
        mem_free(temp);

        if (bytesRead != soundEffect->position) {
            AudioDecoder_Close(ad);
            return -1;
        }
    }

    size_t bytesRead = AudioDecoder_Read(ad, buf, size);
    AudioDecoder_Close(ad);

    if (bytesRead != size) {
        return -1;
    }

    return 0;
}

// 0x4978F0
static unsigned int sfxc_ad_reader(void* stream, void* buf, unsigned int size)
{
    if (size == 0) {
        return 0;
    }

    int handle = *reinterpret_cast<int*>(stream);
    SoundEffect* soundEffect = &(sfxc_handle_list[handle]);

    unsigned int bytesToRead = soundEffect->fileSize - soundEffect->dataPosition;
    if (size <= bytesToRead) {
        bytesToRead = size;
    }

    memcpy(buf, soundEffect->data + soundEffect->dataPosition, bytesToRead);

    soundEffect->dataPosition += bytesToRead;

    return bytesToRead;
}

} // namespace fallout
