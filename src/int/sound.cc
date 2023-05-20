#include "int/sound.h"

#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

#include <algorithm>

#include <SDL.h>

#include "audio_engine.h"
#include "platform_compat.h"
#include "plib/gnw/debug.h"
#include "plib/gnw/memory.h"
#include "plib/gnw/winmain.h"

namespace fallout {

typedef enum SoundStatusFlags {
    SOUND_STATUS_DONE = 0x01,
    SOUND_STATUS_IS_PLAYING = 0x02,
    SOUND_STATUS_IS_FADING = 0x04,
    SOUND_STATUS_IS_PAUSED = 0x08,
} SoundStatusFlags;

typedef struct FadeSound {
    Sound* sound;
    int deltaVolume;
    int targetVolume;
    int initialVolume;
    int currentVolume;
    int field_14;
    struct FadeSound* prev;
    struct FadeSound* next;
} FadeSound;

static void* defaultMalloc(size_t size);
static void* defaultRealloc(void* ptr, size_t size);
static void defaultFree(void* ptr);
static long soundFileSize(int fileHandle);
static long soundTellData(int fileHandle);
static int soundWriteData(int fileHandle, const void* buf, unsigned int size);
static int soundReadData(int fileHandle, void* buf, unsigned int size);
static int soundOpenData(const char* filePath, int flags);
static long soundSeekData(int fileHandle, long offset, int origin);
static int soundCloseData(int fileHandle);
static char* defaultMangler(char* fname);
static void refreshSoundBuffers(Sound* sound);
static int preloadBuffers(Sound* sound);
static int addSoundData(Sound* sound, unsigned char* buf, int size);
static Uint32 doTimerEvent(Uint32 interval, void* param);
static void removeTimedEvent(SDL_TimerID* timerId);
static void removeFadeSound(FadeSound* fadeSound);
static void fadeSounds();
static int internalSoundFade(Sound* sound, int duration, int targetVolume, int a4);

// 0x507E04
static FadeSound* fadeHead = NULL;

// 0x507E08
static FadeSound* fadeFreeList = NULL;

// 0x507E10
static int defaultChannel = 2;

// 0x507E14
static SoundMallocFunc* mallocPtr = defaultMalloc;

// 0x507E18
static SoundReallocFunc* reallocPtr = defaultRealloc;

// 0x507E1C
static SoundFreeFunc* freePtr = defaultFree;

// 0x507E20
static SoundFileIO defaultStream = {
    soundOpenData,
    soundCloseData,
    soundReadData,
    soundWriteData,
    soundSeekData,
    soundTellData,
    soundFileSize,
    -1,
};

// 0x507E40
static SoundFileNameMangler* nameMangler = defaultMangler;

// 0x507E44
static const char* errorMsgs[SOUND_ERR_COUNT] = {
    "sound.c: No error",
    "sound.c: SOS driver not loaded",
    "sound.c: SOS invalid pointer",
    "sound.c: SOS detect initialized",
    "sound.c: SOS fail on file open",
    "sound.c: SOS memory fail",
    "sound.c: SOS invalid driver ID",
    "sound.c: SOS no driver found",
    "sound.c: SOS detection failure",
    "sound.c: SOS driver loaded",
    "sound.c: SOS invalid handle",
    "sound.c: SOS no handles",
    "sound.c: SOS paused",
    "sound.c: SOS not paused",
    "sound.c: SOS invalid data",
    "sound.c: SOS drv file fail",
    "sound.c: SOS invalid port",
    "sound.c: SOS invalid IRQ",
    "sound.c: SOS invalid DMA",
    "sound.c: SOS invalid DMA IRQ",
    "sound.c: no device",
    "sound.c: not initialized",
    "sound.c: no sound",
    "sound.c: function not supported",
    "sound.c: no buffers available",
    "sound.c: file not found",
    "sound.c: already playing",
    "sound.c: not playing",
    "sound.c: already paused",
    "sound.c: not paused",
    "sound.c: invalid handle",
    "sound.c: no memory available",
    "sound.c: unknown error",
};

// 0x6651A0
static int soundErrorno;

// 0x6651A4
static int masterVol;

// 0x6651AC
static int sampleRate;

// Number of sounds currently playing.
//
// 0x6651B0
static int numSounds;

// 0x6651B4
static int deviceInit;

// 0x6651B8
static int dataSize;

// 0x6651BC
static int numBuffers;

// 0x6651C0
static bool driverInit;

// 0x6651C4
static Sound* soundMgrList;

static SDL_TimerID gFadeSoundsTimerId = 0;

// 0x499C80
static void* defaultMalloc(size_t size)
{
    return malloc(size);
}

// 0x499C88
static void* defaultRealloc(void* ptr, size_t size)
{
    return realloc(ptr, size);
}

// 0x499C90
static void defaultFree(void* ptr)
{
    free(ptr);
}

// 0x499C98
void soundRegisterAlloc(SoundMallocFunc* mallocProc, SoundReallocFunc* reallocProc, SoundFreeFunc* freeProc)
{
    mallocPtr = mallocProc;
    reallocPtr = reallocProc;
    freePtr = freeProc;
}

// 0x499CAC
static long soundFileSize(int fileHandle)
{
    long pos;
    long size;

    pos = compat_tell(fileHandle);
    size = lseek(fileHandle, 0, SEEK_END);
    lseek(fileHandle, pos, SEEK_SET);

    return size;
}

// 0x499CE0
static long soundTellData(int fileHandle)
{
    return compat_tell(fileHandle);
}

// 0x499CE8
static int soundWriteData(int fileHandle, const void* buf, unsigned int size)
{
    return write(fileHandle, buf, size);
}

// 0x499CF0
static int soundReadData(int fileHandle, void* buf, unsigned int size)
{
    return read(fileHandle, buf, size);
}

// 0x499CF8
static int soundOpenData(const char* filePath, int flags)
{
    return open(filePath, flags);
}

// 0x499D04
static long soundSeekData(int fileHandle, long offset, int origin)
{
    return lseek(fileHandle, offset, origin);
}

// 0x499D0C
static int soundCloseData(int fileHandle)
{
    return close(fileHandle);
}

// 0x499D1C
static char* defaultMangler(char* fname)
{
    return fname;
}

// 0x499D20
const char* soundError(int err)
{
    if (err == -1) {
        err = soundErrorno;
    }

    if (err < 0 || err > SOUND_UNKNOWN_ERROR) {
        err = SOUND_UNKNOWN_ERROR;
    }

    return errorMsgs[err];
}

// 0x499D40
static void refreshSoundBuffers(Sound* sound)
{
    if (sound->soundFlags & 0x80) {
        return;
    }

    unsigned int readPos;
    unsigned int writePos;
    bool hr = audioEngineSoundBufferGetCurrentPosition(sound->soundBuffer, &readPos, &writePos);
    if (!hr) {
        return;
    }

    if (readPos < sound->lastPosition) {
        sound->numBytesRead += readPos + sound->numBuffers * sound->dataSize - sound->lastPosition;
    } else {
        sound->numBytesRead += readPos - sound->lastPosition;
    }

    if (sound->soundFlags & 0x0100) {
        if (sound->type & 0x20) {
            if (sound->soundFlags & 0x0200) {
                sound->soundFlags |= 0x80;
            }
        } else {
            if (sound->fileSize <= sound->numBytesRead) {
                sound->soundFlags |= 0x0280;
            }
        }
    }
    sound->lastPosition = readPos;

    if (sound->fileSize < sound->numBytesRead) {
        int v3;
        do {
            v3 = sound->numBytesRead - sound->fileSize;
            sound->numBytesRead = v3;
        } while (v3 > sound->fileSize);
    }

    int v6 = readPos / sound->dataSize;
    if (sound->lastUpdate == v6) {
        return;
    }

    int v53;
    if (sound->lastUpdate > v6) {
        v53 = v6 + sound->numBuffers - sound->lastUpdate;
    } else {
        v53 = v6 - sound->lastUpdate;
    }

    if (sound->dataSize * v53 >= sound->readLimit) {
        v53 = (sound->readLimit + sound->dataSize - 1) / sound->dataSize;
    }

    if (v53 < sound->minReadBuffer) {
        return;
    }

    void* audioPtr1;
    void* audioPtr2;
    unsigned int audioBytes1;
    unsigned int audioBytes2;
    hr = audioEngineSoundBufferLock(sound->soundBuffer, sound->dataSize * sound->lastUpdate, sound->dataSize * v53, &audioPtr1, &audioBytes1, &audioPtr2, &audioBytes2, 0);
    if (!hr) {
        return;
    }

    if (audioBytes1 + audioBytes2 != sound->dataSize * v53) {
        debug_printf("locked memory region not big enough, wanted %d (%d * %d), got %d (%d + %d)\n", sound->dataSize * v53, v53, sound->dataSize, audioBytes1 + audioBytes2, audioBytes1, audioBytes2);
        debug_printf("Resetting readBuffers from %d to %d\n", v53, (audioBytes1 + audioBytes2) / sound->dataSize);

        v53 = (audioBytes1 + audioBytes2) / sound->dataSize;
        if (v53 < sound->minReadBuffer) {
            debug_printf("No longer above read buffer size, returning\n");
            return;
        }
    }
    unsigned char* audioPtr = (unsigned char*)audioPtr1;
    int audioBytes = audioBytes1;
    while (--v53 != -1) {
        int bytesRead;
        if (sound->soundFlags & 0x0200) {
            bytesRead = sound->dataSize;
            memset(sound->data, 0, bytesRead);
        } else {
            int bytesToRead = sound->dataSize;
            if (sound->field_58 != -1) {
                int pos = sound->io.tell(sound->io.fd);
                if (bytesToRead + pos > sound->field_58) {
                    bytesToRead = sound->field_58 - pos;
                }
            }

            bytesRead = sound->io.read(sound->io.fd, sound->data, bytesToRead);
            if (bytesRead < sound->dataSize) {
                if (!(sound->soundFlags & 0x20) || (sound->soundFlags & 0x0100)) {
                    memset(sound->data + bytesRead, 0, sound->dataSize - bytesRead);
                    sound->soundFlags |= 0x0200;
                    bytesRead = sound->dataSize;
                } else {
                    while (bytesRead < sound->dataSize) {
                        if (sound->loops == -1) {
                            sound->io.seek(sound->io.fd, sound->field_54, SEEK_SET);
                            if (sound->callback != NULL) {
                                sound->callback(sound->callbackUserData, 0x0400);
                            }
                        } else {
                            if (sound->loops <= 0) {
                                sound->field_58 = -1;
                                sound->field_54 = 0;
                                sound->loops = 0;
                                sound->soundFlags &= ~0x20;
                                bytesRead += sound->io.read(sound->io.fd, sound->data + bytesRead, sound->dataSize - bytesRead);
                                break;
                            }

                            sound->loops--;
                            sound->io.seek(sound->io.fd, sound->field_54, SEEK_SET);

                            if (sound->callback != NULL) {
                                sound->callback(sound->callbackUserData, 0x400);
                            }
                        }

                        if (sound->field_58 == -1) {
                            bytesToRead = sound->dataSize - bytesRead;
                        } else {
                            int pos = sound->io.tell(sound->io.fd);
                            if (sound->dataSize + bytesRead + pos <= sound->field_58) {
                                bytesToRead = sound->dataSize - bytesRead;
                            } else {
                                bytesToRead = sound->field_58 - bytesRead - pos;
                            }
                        }

                        int v20 = sound->io.read(sound->io.fd, sound->data + bytesRead, bytesToRead);
                        bytesRead += v20;
                        if (v20 < bytesToRead) {
                            break;
                        }
                    }
                }
            }
        }

        if (bytesRead > audioBytes) {
            if (audioBytes != 0) {
                memcpy(audioPtr, sound->data, audioBytes);
            }

            if (audioPtr2 != NULL) {
                memcpy(audioPtr2, sound->data + audioBytes, bytesRead - audioBytes);
                audioPtr = (unsigned char*)audioPtr2 + bytesRead - audioBytes;
                audioBytes = audioBytes2 - bytesRead;
            } else {
                debug_printf("Hm, no second write pointer, but buffer not big enough, this shouldn't happen\n");
            }
        } else {
            memcpy(audioPtr, sound->data, bytesRead);
            audioPtr += bytesRead;
            audioBytes -= bytesRead;
        }
    }

    audioEngineSoundBufferUnlock(sound->soundBuffer, audioPtr1, audioBytes1, audioPtr2, audioBytes2);

    sound->lastUpdate = v6;

    return;
}

// 0x49A1E4
int soundInit(int a1, int num_buffers, int a3, int data_size, int sample_rate)
{
    if (!audioEngineInit()) {
        debug_printf("soundInit: Unable to init audio engine\n");

        soundErrorno = SOUND_SOS_DETECTION_FAILURE;
        return soundErrorno;
    }

    sampleRate = sample_rate;
    dataSize = data_size;
    numBuffers = num_buffers;
    driverInit = true;
    deviceInit = 1;

    soundSetMasterVolume(VOLUME_MAX);

    soundErrorno = SOUND_NO_ERROR;
    return 0;
}

// 0x49A5D8
void soundClose()
{
    while (soundMgrList != NULL) {
        Sound* next = soundMgrList->next;
        soundDelete(soundMgrList);
        soundMgrList = next;
    }

    if (gFadeSoundsTimerId != -1) {
        removeTimedEvent(&gFadeSoundsTimerId);
    }

    while (fadeFreeList != NULL) {
        FadeSound* next = fadeFreeList->next;
        freePtr(fadeFreeList);
        fadeFreeList = next;
    }

    audioEngineExit();

    soundErrorno = SOUND_NO_ERROR;
    driverInit = false;
}

// 0x49A688
Sound* soundAllocate(int type, int soundFlags)
{
    if (!driverInit) {
        soundErrorno = SOUND_NOT_INITIALIZED;
        return NULL;
    }

    Sound* sound = (Sound*)mallocPtr(sizeof(*sound));
    memset(sound, 0, sizeof(*sound));

    memcpy(&(sound->io), &defaultStream, sizeof(defaultStream));

    if (!(soundFlags & SOUND_FLAG_0x02)) {
        soundFlags |= SOUND_FLAG_0x02;
    }

    sound->bitsPerSample = (soundFlags & SOUND_16BIT) != 0 ? 16 : 8;
    sound->channels = 1;
    sound->rate = sampleRate;

    sound->soundFlags = soundFlags;
    sound->type = type;
    sound->dataSize = dataSize;
    sound->numBytesRead = 0;
    sound->soundBuffer = -1;
    sound->statusFlags = 0;
    sound->numBuffers = numBuffers;
    sound->readLimit = sound->dataSize * numBuffers;

    if ((type & SOUND_TYPE_INFINITE) != 0) {
        sound->loops = -1;
        sound->soundFlags |= SOUND_LOOPING;
    }

    sound->field_58 = -1;
    sound->minReadBuffer = 1;
    sound->volume = VOLUME_MAX;
    sound->prev = NULL;
    sound->field_54 = 0;
    sound->next = soundMgrList;

    if (soundMgrList != NULL) {
        soundMgrList->prev = sound;
    }

    soundMgrList = sound;

    return sound;
}

// 0x49A88C
static int preloadBuffers(Sound* sound)
{
    unsigned char* buf;
    int bytes_read;
    int result;
    int v15;
    unsigned char* v14;
    int size;

    size = sound->io.filelength(sound->io.fd);
    sound->fileSize = size;

    if ((sound->type & SOUND_TYPE_STREAMING) != 0) {
        if ((sound->soundFlags & SOUND_LOOPING) == 0) {
            sound->soundFlags |= SOUND_FLAG_0x100 | SOUND_LOOPING;
        }

        if (sound->numBuffers * sound->dataSize >= size) {
            if (size / sound->dataSize * sound->dataSize != size) {
                size = (size / sound->dataSize + 1) * sound->dataSize;
            }
        } else {
            size = sound->numBuffers * sound->dataSize;
        }
    } else {
        sound->type &= ~(SOUND_TYPE_MEMORY | SOUND_TYPE_STREAMING);
        sound->type |= SOUND_TYPE_MEMORY;
    }

    buf = (unsigned char*)mallocPtr(size);
    bytes_read = sound->io.read(sound->io.fd, buf, size);
    if (bytes_read != size) {
        if ((sound->soundFlags & SOUND_LOOPING) == 0 || (sound->soundFlags & SOUND_FLAG_0x100) != 0) {
            memset(buf + bytes_read, 0, size - bytes_read);
        } else {
            v14 = buf + bytes_read;
            v15 = bytes_read;
            while (size - v15 > bytes_read) {
                memcpy(v14, buf, bytes_read);
                v15 += bytes_read;
                v14 += bytes_read;
            }

            if (v15 < size) {
                memcpy(v14, buf, size - v15);
            }
        }
    }

    result = soundSetData(sound, buf, size);
    freePtr(buf);

    if ((sound->type & SOUND_TYPE_MEMORY) != 0) {
        sound->io.close(sound->io.fd);
        sound->io.fd = -1;
    } else {
        if (sound->data == NULL) {
            sound->data = (unsigned char*)mallocPtr(sound->dataSize);
        }
    }

    return result;
}

// 0x49AA1C
int soundLoad(Sound* sound, char* filePath)
{
    if (!driverInit) {
        soundErrorno = SOUND_NOT_INITIALIZED;
        return soundErrorno;
    }

    if (sound == NULL) {
        soundErrorno = SOUND_NO_SOUND;
        return soundErrorno;
    }

    sound->io.fd = sound->io.open(nameMangler(filePath), 0x0200);
    if (sound->io.fd == -1) {
        soundErrorno = SOUND_FILE_NOT_FOUND;
        return soundErrorno;
    }

    return preloadBuffers(sound);
}

// 0x49AA88
int soundRewind(Sound* sound)
{
    bool hr;

    if (!driverInit) {
        soundErrorno = SOUND_NOT_INITIALIZED;
        return soundErrorno;
    }

    if (sound == NULL || sound->soundBuffer == -1) {
        soundErrorno = SOUND_NO_SOUND;
        return soundErrorno;
    }

    if ((sound->type & SOUND_TYPE_STREAMING) != 0) {
        sound->io.seek(sound->io.fd, 0, SEEK_SET);
        sound->lastUpdate = 0;
        sound->lastPosition = 0;
        sound->numBytesRead = 0;
        sound->soundFlags &= 0xFD7F;
        hr = audioEngineSoundBufferSetCurrentPosition(sound->soundBuffer, 0);
        preloadBuffers(sound);
    } else {
        hr = audioEngineSoundBufferSetCurrentPosition(sound->soundBuffer, 0);
    }

    if (!hr) {
        soundErrorno = SOUND_UNKNOWN_ERROR;
        return soundErrorno;
    }

    sound->statusFlags &= ~SOUND_STATUS_DONE;

    soundErrorno = SOUND_NO_ERROR;
    return soundErrorno;
}

// 0x49AB4C
static int addSoundData(Sound* sound, unsigned char* buf, int size)
{
    bool hr;
    void* audioPtr1;
    unsigned int audioBytes1;
    void* audioPtr2;
    unsigned int audioBytes2;

    hr = audioEngineSoundBufferLock(sound->soundBuffer, 0, size, &audioPtr1, &audioBytes1, &audioPtr2, &audioBytes2, AUDIO_ENGINE_SOUND_BUFFER_LOCK_FROM_WRITE_POS);
    if (!hr) {
        soundErrorno = SOUND_UNKNOWN_ERROR;
        return soundErrorno;
    }

    memcpy(audioPtr1, buf, audioBytes1);

    if (audioPtr2 != NULL) {
        memcpy(audioPtr2, buf + audioBytes1, audioBytes2);
    }
    hr = audioEngineSoundBufferUnlock(sound->soundBuffer, audioPtr1, audioBytes1, audioPtr2, audioBytes2);
    if (!hr) {
        soundErrorno = SOUND_UNKNOWN_ERROR;
        return soundErrorno;
    }

    soundErrorno = SOUND_NO_ERROR;
    return soundErrorno;
}

// 0x49AC44
int soundSetData(Sound* sound, unsigned char* buf, int size)
{
    if (!driverInit) {
        soundErrorno = SOUND_NOT_INITIALIZED;
        return soundErrorno;
    }

    if (sound == NULL) {
        soundErrorno = SOUND_NO_SOUND;
        return soundErrorno;
    }

    if (sound->soundBuffer == -1) {
        sound->soundBuffer = audioEngineCreateSoundBuffer(size, sound->bitsPerSample, sound->channels, sound->rate);
        if (sound->soundBuffer == -1) {
            soundErrorno = SOUND_UNKNOWN_ERROR;
            return soundErrorno;
        }
    }

    return addSoundData(sound, buf, size);
}

// 0x49ACC0
int soundPlay(Sound* sound)
{
    bool hr;
    unsigned int readPos;
    unsigned int writePos;

    if (!driverInit) {
        soundErrorno = SOUND_NOT_INITIALIZED;
        return soundErrorno;
    }

    if (sound == NULL || sound->soundBuffer == -1) {
        soundErrorno = SOUND_NO_SOUND;
        return soundErrorno;
    }

    if ((sound->statusFlags & SOUND_STATUS_DONE) != 0) {
        soundRewind(sound);
    }

    soundVolume(sound, sound->volume);

    hr = audioEngineSoundBufferPlay(sound->soundBuffer, sound->soundFlags & SOUND_LOOPING ? AUDIO_ENGINE_SOUND_BUFFER_PLAY_LOOPING : 0);

    audioEngineSoundBufferGetCurrentPosition(sound->soundBuffer, &readPos, &writePos);
    sound->lastUpdate = readPos / sound->dataSize;

    if (!hr) {
        soundErrorno = SOUND_UNKNOWN_ERROR;
        return soundErrorno;
    }

    sound->statusFlags |= SOUND_STATUS_IS_PLAYING;

    ++numSounds;

    soundErrorno = SOUND_NO_ERROR;
    return soundErrorno;
}

// 0x49ADAC
int soundStop(Sound* sound)
{
    bool hr;

    if (!driverInit) {
        soundErrorno = SOUND_NOT_INITIALIZED;
        return soundErrorno;
    }

    if (sound == NULL || sound->soundBuffer == -1) {
        soundErrorno = SOUND_NO_SOUND;
        return soundErrorno;
    }

    if ((sound->statusFlags & SOUND_STATUS_IS_PLAYING) == 0) {
        soundErrorno = SOUND_NOT_PLAYING;
        return soundErrorno;
    }

    hr = audioEngineSoundBufferStop(sound->soundBuffer);
    if (!hr) {
        soundErrorno = SOUND_UNKNOWN_ERROR;
        return soundErrorno;
    }

    sound->statusFlags &= ~SOUND_STATUS_IS_PLAYING;
    numSounds--;

    soundErrorno = SOUND_NO_ERROR;
    return soundErrorno;
}

// 0x49AE60
int soundDelete(Sound* sample)
{
    if (!driverInit) {
        soundErrorno = SOUND_NOT_INITIALIZED;
        return soundErrorno;
    }

    if (sample == NULL) {
        soundErrorno = SOUND_NO_SOUND;
        return soundErrorno;
    }

    if (sample->io.fd != -1) {
        sample->io.close(sample->io.fd);
        sample->io.fd = -1;
    }

    soundMgrDelete(sample);

    soundErrorno = SOUND_NO_ERROR;
    return soundErrorno;
}

// 0x49AEC4
int numSoundsPlaying()
{
    return numSounds;
}

// 0x49AECC
int soundContinue(Sound* sound)
{
    bool hr;
    unsigned int status;

    if (!driverInit) {
        soundErrorno = SOUND_NOT_INITIALIZED;
        return soundErrorno;
    }

    if (sound == NULL) {
        soundErrorno = SOUND_NO_SOUND;
        return soundErrorno;
    }

    if (sound->soundBuffer == -1) {
        soundErrorno = SOUND_NO_SOUND;
        return soundErrorno;
    }

    if ((sound->statusFlags & SOUND_STATUS_IS_PLAYING) == 0) {
        soundErrorno = SOUND_NOT_PLAYING;
        return soundErrorno;
    }

    if ((sound->statusFlags & SOUND_STATUS_IS_PAUSED) != 0) {
        soundErrorno = SOUND_NOT_PLAYING;
        return soundErrorno;
    }

    if ((sound->statusFlags & SOUND_STATUS_DONE) != 0) {
        soundErrorno = SOUND_UNKNOWN_ERROR;
        return soundErrorno;
    }

    hr = audioEngineSoundBufferGetStatus(sound->soundBuffer, &status);
    if (!hr) {
        debug_printf("Error in soundContinue, %x\n", hr);

        soundErrorno = SOUND_UNKNOWN_ERROR;
        return soundErrorno;
    }

    if ((sound->soundFlags & SOUND_FLAG_0x80) == 0 && (status & (AUDIO_ENGINE_SOUND_BUFFER_STATUS_PLAYING | AUDIO_ENGINE_SOUND_BUFFER_STATUS_LOOPING)) != 0) {
        if ((sound->statusFlags & SOUND_STATUS_IS_PAUSED) == 0 && (sound->type & SOUND_TYPE_STREAMING) != 0) {
            refreshSoundBuffers(sound);
        }
    } else if ((sound->statusFlags & SOUND_STATUS_IS_PAUSED) == 0) {
        if (sound->callback != NULL) {
            sound->callback(sound->callbackUserData, 1);
            sound->callback = NULL;
        }

        if (sound->type & 0x04) {
            sound->callback = NULL;
            soundDelete(sound);
        } else {
            sound->statusFlags |= SOUND_STATUS_DONE;

            if (sound->statusFlags & SOUND_STATUS_IS_PLAYING) {
                --numSounds;
            }

            soundStop(sound);

            sound->statusFlags &= ~(SOUND_STATUS_DONE | SOUND_STATUS_IS_PLAYING);
        }
    }

    soundErrorno = SOUND_NO_ERROR;
    return soundErrorno;
}

// 0x49B008
bool soundPlaying(Sound* sound)
{
    if (!driverInit) {
        soundErrorno = SOUND_NOT_INITIALIZED;
        return false;
    }

    if (sound == NULL || sound->soundBuffer == -1) {
        soundErrorno = SOUND_NO_SOUND;
        return false;
    }

    return (sound->statusFlags & SOUND_STATUS_IS_PLAYING) != 0;
}

// 0x49B048
bool soundDone(Sound* sound)
{
    if (!driverInit) {
        soundErrorno = SOUND_NOT_INITIALIZED;
        return false;
    }

    if (sound == NULL || sound->soundBuffer == -1) {
        soundErrorno = SOUND_NO_SOUND;
        return false;
    }

    return (sound->statusFlags & SOUND_STATUS_DONE) != 0;
}

// 0x49B088
bool soundFading(Sound* sound)
{
    if (!driverInit) {
        soundErrorno = SOUND_NOT_INITIALIZED;
        return false;
    }

    if (sound == NULL || sound->soundBuffer == -1) {
        soundErrorno = SOUND_NO_SOUND;
        return false;
    }

    return (sound->statusFlags & SOUND_STATUS_IS_FADING) != 0;
}

// 0x49B0C8
bool soundPaused(Sound* sound)
{
    if (!driverInit) {
        soundErrorno = SOUND_NOT_INITIALIZED;
        return false;
    }

    if (sound == NULL || sound->soundBuffer == -1) {
        soundErrorno = SOUND_NO_SOUND;
        return false;
    }

    return (sound->statusFlags & SOUND_STATUS_IS_PAUSED) != 0;
}

// 0x49B108
int soundFlags(Sound* sound, int flags)
{
    if (!driverInit) {
        soundErrorno = SOUND_NOT_INITIALIZED;
        return 0;
    }

    if (sound == NULL || sound->soundBuffer == -1) {
        soundErrorno = SOUND_NO_SOUND;
        return 0;
    }

    return sound->soundFlags & flags;
}

// 0x49B148
int soundType(Sound* sound, int type)
{
    if (!driverInit) {
        soundErrorno = SOUND_NOT_INITIALIZED;
        return 0;
    }

    if (sound == NULL || sound->soundBuffer == -1) {
        soundErrorno = SOUND_NO_SOUND;
        return 0;
    }

    return sound->type & type;
}

// 0x49B188
int soundLength(Sound* sound)
{
    if (!driverInit) {
        soundErrorno = SOUND_NOT_INITIALIZED;
        return soundErrorno;
    }

    if (sound == NULL || sound->soundBuffer == -1) {
        soundErrorno = SOUND_NO_SOUND;
        return soundErrorno;
    }

    int bytesPerSec = sound->bitsPerSample / 8 * sound->rate;
    int v3 = sound->fileSize;
    int v4 = v3 % bytesPerSec;
    int result = v3 / bytesPerSec;
    if (v4 != 0) {
        result += 1;
    }

    return result;
}

// 0x49B284
int soundLoop(Sound* sound, int loops)
{
    if (!driverInit) {
        soundErrorno = SOUND_NOT_INITIALIZED;
        return soundErrorno;
    }

    if (sound == NULL) {
        soundErrorno = SOUND_NO_SOUND;
        return soundErrorno;
    }

    if (loops) {
        sound->soundFlags |= SOUND_LOOPING;
        sound->loops = loops;
    } else {
        sound->loops = 0;
        sound->field_58 = -1;
        sound->field_54 = 0;
        sound->soundFlags &= ~SOUND_LOOPING;
    }

    soundErrorno = SOUND_NO_ERROR;
    return soundErrorno;
}

// 0x49B2EC
int soundVolumeHMItoDirectSound(int volume)
{
    double normalizedVolume;

    if (volume > VOLUME_MAX) {
        volume = VOLUME_MAX;
    }

    // Normalize volume to SDL (0-128).
    normalizedVolume = (double)(volume - VOLUME_MIN) / (double)(VOLUME_MAX - VOLUME_MIN) * 128;

    return (int)normalizedVolume;
}

// 0x49B38C
int soundVolume(Sound* sound, int volume)
{
    int normalizedVolume;
    bool hr;

    if (!driverInit) {
        soundErrorno = SOUND_NOT_INITIALIZED;
        return soundErrorno;
    }

    if (sound == NULL) {
        soundErrorno = SOUND_NO_SOUND;
        return soundErrorno;
    }

    sound->volume = volume;

    if (sound->soundBuffer == -1) {
        soundErrorno = SOUND_NO_ERROR;
        return soundErrorno;
    }

    normalizedVolume = soundVolumeHMItoDirectSound(masterVol * volume / VOLUME_MAX);

    hr = audioEngineSoundBufferSetVolume(sound->soundBuffer, normalizedVolume);
    if (!hr) {
        soundErrorno = SOUND_UNKNOWN_ERROR;
        return soundErrorno;
    }

    soundErrorno = SOUND_NO_ERROR;
    return soundErrorno;
}

// 0x49B400
int soundGetVolume(Sound* sound)
{
    if (!deviceInit) {
        soundErrorno = SOUND_NOT_INITIALIZED;
        return soundErrorno;
    }

    if (sound == NULL || sound->soundBuffer == -1) {
        soundErrorno = SOUND_NO_SOUND;
        return soundErrorno;
    }

    return sound->volume;
}

// 0x49B570
int soundSetCallback(Sound* sound, SoundCallback* callback, void* userData)
{
    if (!driverInit) {
        soundErrorno = SOUND_NOT_INITIALIZED;
        return soundErrorno;
    }

    if (sound == NULL) {
        soundErrorno = SOUND_NO_SOUND;
        return soundErrorno;
    }

    sound->callback = callback;
    sound->callbackUserData = userData;

    soundErrorno = SOUND_NO_ERROR;
    return soundErrorno;
}

// 0x49B5AC
int soundSetChannel(Sound* sound, int channels)
{
    if (!driverInit) {
        soundErrorno = SOUND_NOT_INITIALIZED;
        return soundErrorno;
    }

    if (sound == NULL) {
        soundErrorno = SOUND_NO_SOUND;
        return soundErrorno;
    }

    if (channels == 3) {
        sound->channels = 2;
    }

    soundErrorno = SOUND_NO_ERROR;
    return soundErrorno;
}

// 0x49B630
int soundSetReadLimit(Sound* sound, int readLimit)
{
    if (!driverInit) {
        soundErrorno = SOUND_NOT_INITIALIZED;
        return soundErrorno;
    }

    if (sound == NULL) {
        soundErrorno = SOUND_NO_DEVICE;
        return soundErrorno;
    }

    sound->readLimit = readLimit;

    soundErrorno = SOUND_NO_ERROR;
    return soundErrorno;
}

// 0x49B664
int soundPause(Sound* sound)
{
    bool hr;
    unsigned int readPos;
    unsigned int writePos;

    if (!driverInit) {
        soundErrorno = SOUND_NOT_INITIALIZED;
        return soundErrorno;
    }

    if (sound == NULL) {
        soundErrorno = SOUND_NO_SOUND;
        return soundErrorno;
    }

    if (sound->soundBuffer == -1) {
        soundErrorno = SOUND_NO_SOUND;
        return soundErrorno;
    }

    if ((sound->statusFlags & SOUND_STATUS_IS_PLAYING) == 0) {
        soundErrorno = SOUND_NOT_PLAYING;
        return soundErrorno;
    }

    if ((sound->statusFlags & SOUND_STATUS_IS_PAUSED) != 0) {
        soundErrorno = SOUND_ALREADY_PAUSED;
        return soundErrorno;
    }

    hr = audioEngineSoundBufferGetCurrentPosition(sound->soundBuffer, &readPos, &writePos);
    if (!hr) {
        soundErrorno = SOUND_UNKNOWN_ERROR;
        return soundErrorno;
    }

    sound->pausePos = readPos;
    sound->statusFlags |= SOUND_STATUS_IS_PAUSED;

    return soundStop(sound);
}

// 0x49B770
int soundUnpause(Sound* sound)
{
    bool hr;

    if (!driverInit) {
        soundErrorno = SOUND_NOT_INITIALIZED;
        return soundErrorno;
    }

    if (sound == NULL || sound->soundBuffer == -1) {
        soundErrorno = SOUND_NO_SOUND;
        return soundErrorno;
    }

    if ((sound->statusFlags & SOUND_STATUS_IS_PLAYING) != 0) {
        soundErrorno = SOUND_NOT_PAUSED;
        return soundErrorno;
    }

    if ((sound->statusFlags & SOUND_STATUS_IS_PAUSED) == 0) {
        soundErrorno = SOUND_NOT_PAUSED;
        return soundErrorno;
    }

    hr = audioEngineSoundBufferSetCurrentPosition(sound->soundBuffer, sound->pausePos);
    if (!hr) {
        soundErrorno = SOUND_UNKNOWN_ERROR;
        return soundErrorno;
    }

    sound->statusFlags &= ~SOUND_STATUS_IS_PAUSED;
    sound->pausePos = 0;

    return soundPlay(sound);
}

// 0x49B87C
int soundSetFileIO(Sound* sound, SoundOpenProc* openProc, SoundCloseProc* closeProc, SoundReadProc* readProc, SoundWriteProc* writeProc, SoundSeekProc* seekProc, SoundTellProc* tellProc, SoundFileLengthProc* fileLengthProc)
{
    if (!driverInit) {
        soundErrorno = SOUND_NOT_INITIALIZED;
        return soundErrorno;
    }

    if (sound == NULL) {
        soundErrorno = SOUND_NO_SOUND;
        return soundErrorno;
    }

    if (openProc != NULL) {
        sound->io.open = openProc;
    }

    if (closeProc != NULL) {
        sound->io.close = closeProc;
    }

    if (readProc != NULL) {
        sound->io.read = readProc;
    }

    if (writeProc != NULL) {
        sound->io.write = writeProc;
    }

    if (seekProc != NULL) {
        sound->io.seek = seekProc;
    }

    if (tellProc != NULL) {
        sound->io.tell = tellProc;
    }

    if (fileLengthProc != NULL) {
        sound->io.filelength = fileLengthProc;
    }

    soundErrorno = SOUND_NO_ERROR;
    return soundErrorno;
}

// 0x49B8F8
void soundMgrDelete(Sound* sound)
{
    Sound* next;
    Sound* prev;

    if ((sound->statusFlags & SOUND_STATUS_IS_FADING) != 0) {
        FadeSound* curr = fadeHead;

        while (curr != NULL) {
            if (sound == curr->sound) {
                break;
            }

            curr = curr->next;
        }

        removeFadeSound(curr);
    }

    if (sound->soundBuffer != -1) {
        // NOTE: Uninline.
        if (!soundPlaying(sound)) {
            soundStop(sound);
        }

        if (sound->callback != NULL) {
            sound->callback(sound->callbackUserData, 1);
        }

        audioEngineSoundBufferRelease(sound->soundBuffer);
        sound->soundBuffer = -1;
    }

    if (sound->deleteCallback != NULL) {
        sound->deleteCallback(sound->deleteUserData);
    }

    if (sound->data != NULL) {
        freePtr(sound->data);
        sound->data = NULL;
    }

    next = sound->next;
    if (next != NULL) {
        next->prev = sound->prev;
    }

    prev = sound->prev;
    if (prev != NULL) {
        prev->next = sound->next;
    } else {
        soundMgrList = sound->next;
    }

    freePtr(sound);
}

// 0x49BAF8
int soundSetMasterVolume(int volume)
{
    if (volume < VOLUME_MIN || volume > VOLUME_MAX) {
        soundErrorno = SOUND_UNKNOWN_ERROR;
        return soundErrorno;
    }

    masterVol = volume;

    Sound* curr = soundMgrList;
    while (curr != NULL) {
        soundVolume(curr, curr->volume);
        curr = curr->next;
    }

    soundErrorno = SOUND_NO_ERROR;
    return soundErrorno;
}

// 0x49BB48
static Uint32 doTimerEvent(Uint32 interval, void* param)
{
    void (*fn)();

    if (param != NULL) {
        fn = (void (*)())param;
        fn();
    }

    return 40;
}

// 0x49BB94
static void removeTimedEvent(SDL_TimerID* timerId)
{
    if (*timerId != 0) {
        SDL_RemoveTimer(*timerId);
        *timerId = 0;
    }
}

// 0x49BBB4
int soundGetPosition(Sound* sound)
{
    if (!driverInit) {
        soundErrorno = SOUND_NOT_INITIALIZED;
        return soundErrorno;
    }

    if (sound == NULL) {
        soundErrorno = SOUND_NO_SOUND;
        return soundErrorno;
    }

    unsigned int readPos;
    unsigned int writePos;
    audioEngineSoundBufferGetCurrentPosition(sound->soundBuffer, &readPos, &writePos);

    if ((sound->type & SOUND_TYPE_STREAMING) != 0) {
        if (readPos < sound->lastPosition) {
            readPos += sound->numBytesRead + sound->numBuffers * sound->dataSize - sound->lastPosition;
        } else {
            readPos -= sound->lastPosition + sound->numBytesRead;
        }
    }

    return readPos;
}

// 0x49BC48
int soundSetPosition(Sound* sound, int pos)
{
    if (!driverInit) {
        soundErrorno = SOUND_NOT_INITIALIZED;
        return soundErrorno;
    }

    if (sound == NULL) {
        soundErrorno = SOUND_NO_SOUND;
        return soundErrorno;
    }

    if (sound->soundBuffer == -1) {
        soundErrorno = SOUND_NO_SOUND;
        return soundErrorno;
    }

    if ((sound->type & SOUND_TYPE_STREAMING) != 0) {
        int section = pos / sound->dataSize % sound->numBuffers;

        audioEngineSoundBufferSetCurrentPosition(sound->soundBuffer, section * sound->dataSize + pos % sound->dataSize);

        sound->io.seek(sound->io.fd, section * sound->dataSize, SEEK_SET);
        int bytes_read = sound->io.read(sound->io.fd, sound->data, sound->dataSize);
        if (bytes_read < sound->dataSize) {
            if (sound->type & 0x02) {
                sound->io.seek(sound->io.fd, 0, SEEK_SET);
                sound->io.read(sound->io.fd, sound->data + bytes_read, sound->dataSize - bytes_read);
            } else {
                memset(sound->data + bytes_read, 0, sound->dataSize - bytes_read);
            }
        }

        int nextSection = section + 1;
        sound->numBytesRead = pos;

        if (nextSection < sound->numBuffers) {
            sound->lastUpdate = nextSection;
        } else {
            sound->lastUpdate = 0;
        }

        soundContinue(sound);
    } else {
        audioEngineSoundBufferSetCurrentPosition(sound->soundBuffer, pos);
    }

    soundErrorno = SOUND_NO_ERROR;
    return soundErrorno;
}

// 0x49BDAC
static void removeFadeSound(FadeSound* fadeSound)
{
    FadeSound* prev;
    FadeSound* next;
    FadeSound* tmp;

    if (fadeSound == NULL) {
        return;
    }

    if (fadeSound->sound == NULL) {
        return;
    }

    if ((fadeSound->sound->statusFlags & SOUND_STATUS_IS_FADING) == 0) {
        return;
    }

    prev = fadeSound->prev;
    if (prev != NULL) {
        prev->next = fadeSound->next;
    } else {
        fadeHead = fadeSound->next;
    }

    next = fadeSound->next;
    if (next != NULL) {
        next->prev = fadeSound->prev;
    }

    fadeSound->sound->statusFlags &= ~SOUND_STATUS_IS_FADING;
    fadeSound->sound = NULL;

    tmp = fadeFreeList;
    fadeFreeList = fadeSound;
    fadeSound->next = tmp;
}

// 0x49BE2C
static void fadeSounds()
{
    FadeSound* ptr;

    ptr = fadeHead;
    while (ptr != NULL) {
        if ((ptr->currentVolume > ptr->targetVolume || ptr->currentVolume + ptr->deltaVolume < ptr->targetVolume) && (ptr->currentVolume < ptr->targetVolume || ptr->currentVolume + ptr->deltaVolume > ptr->targetVolume)) {
            ptr->currentVolume += ptr->deltaVolume;
            soundVolume(ptr->sound, ptr->currentVolume);
        } else {
            if (ptr->targetVolume == 0) {
                if (ptr->field_14) {
                    soundPause(ptr->sound);
                    soundVolume(ptr->sound, ptr->initialVolume);
                } else {
                    if (ptr->sound->type & 0x04) {
                        soundDelete(ptr->sound);
                    } else {
                        soundStop(ptr->sound);

                        ptr->initialVolume = ptr->targetVolume;
                        ptr->currentVolume = ptr->targetVolume;
                        ptr->deltaVolume = 0;

                        soundVolume(ptr->sound, ptr->targetVolume);
                    }
                }
            }

            removeFadeSound(ptr);
        }
    }

    if (fadeHead == NULL) {
        // NOTE: Uninline.
        removeTimedEvent(&gFadeSoundsTimerId);
    }
}

// 0x49BF04
static int internalSoundFade(Sound* sound, int duration, int targetVolume, int a4)
{
    FadeSound* ptr;

    if (!deviceInit) {
        soundErrorno = SOUND_NOT_INITIALIZED;
        return soundErrorno;
    }

    if (sound == NULL) {
        soundErrorno = SOUND_NO_SOUND;
        return soundErrorno;
    }

    ptr = NULL;
    if ((sound->statusFlags & SOUND_STATUS_IS_FADING) != 0) {
        ptr = fadeHead;
        while (ptr != NULL) {
            if (ptr->sound == sound) {
                break;
            }

            ptr = ptr->next;
        }
    }

    if (ptr == NULL) {
        if (fadeFreeList != NULL) {
            ptr = fadeFreeList;
            fadeFreeList = fadeFreeList->next;
        } else {
            ptr = (FadeSound*)mallocPtr(sizeof(FadeSound));
        }

        if (ptr != NULL) {
            if (fadeHead != NULL) {
                fadeHead->prev = ptr;
            }

            ptr->sound = sound;
            ptr->prev = NULL;
            ptr->next = fadeHead;
            fadeHead = ptr;
        }
    }

    if (ptr == NULL) {
        soundErrorno = SOUND_NO_MEMORY_AVAILABLE;
        return soundErrorno;
    }

    ptr->targetVolume = targetVolume;
    ptr->initialVolume = soundGetVolume(sound);
    ptr->currentVolume = ptr->initialVolume;
    ptr->field_14 = a4;
    // TODO: Check.
    ptr->deltaVolume = 8 * (125 * (targetVolume - ptr->initialVolume)) / (40 * duration);

    sound->statusFlags |= SOUND_STATUS_IS_FADING;

    bool shouldPlay;
    if (driverInit) {
        if (sound->soundBuffer != -1) {
            shouldPlay = (sound->statusFlags & SOUND_STATUS_IS_PLAYING) == 0;
        } else {
            soundErrorno = SOUND_NO_SOUND;
            shouldPlay = true;
        }
    } else {
        soundErrorno = SOUND_NOT_INITIALIZED;
        shouldPlay = true;
    }

    if (shouldPlay) {
        soundPlay(sound);
    }

    if (gFadeSoundsTimerId != 0) {
        soundErrorno = SOUND_NO_ERROR;
        return soundErrorno;
    }

    gFadeSoundsTimerId = SDL_AddTimer(40, doTimerEvent, (void*)fadeSounds);
    if (gFadeSoundsTimerId == 0) {
        soundErrorno = SOUND_UNKNOWN_ERROR;
        return soundErrorno;
    }

    soundErrorno = SOUND_NO_ERROR;
    return soundErrorno;
}

// 0x49C088
int soundFade(Sound* sound, int duration, int targetVolume)
{
    return internalSoundFade(sound, duration, targetVolume, 0);
}

// 0x49C0D0
void soundFlushAllSounds()
{
    while (soundMgrList != NULL) {
        soundDelete(soundMgrList);
    }
}

// 0x49C15C
void soundUpdate()
{
    Sound* curr = soundMgrList;
    while (curr != NULL) {
        // Sound can be deallocated in `soundContinue`.
        Sound* next = curr->next;
        soundContinue(curr);
        curr = next;
    }
}

// 0x49C17C
int soundSetDefaultFileIO(SoundOpenProc* openProc, SoundCloseProc* closeProc, SoundReadProc* readProc, SoundWriteProc* writeProc, SoundSeekProc* seekProc, SoundTellProc* tellProc, SoundFileLengthProc* fileLengthProc)
{
    if (openProc != NULL) {
        defaultStream.open = openProc;
    }

    if (closeProc != NULL) {
        defaultStream.close = closeProc;
    }

    if (readProc != NULL) {
        defaultStream.read = readProc;
    }

    if (writeProc != NULL) {
        defaultStream.write = writeProc;
    }

    if (seekProc != NULL) {
        defaultStream.seek = seekProc;
    }

    if (tellProc != NULL) {
        defaultStream.tell = tellProc;
    }

    if (fileLengthProc != NULL) {
        defaultStream.filelength = fileLengthProc;
    }

    soundErrorno = SOUND_NO_ERROR;
    return soundErrorno;
}

} // namespace fallout
