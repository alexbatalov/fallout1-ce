#include "int/audiof.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "int/memdbg.h"
#include "int/sound.h"
#include "platform_compat.h"
#include "plib/gnw/debug.h"
#include "sound_decoder.h"

namespace fallout {

typedef enum AudioFileFlags {
    AUDIO_FILE_IN_USE = 0x01,
    AUDIO_FILE_COMPRESSED = 0x02,
} AudioFileFlags;

typedef struct AudioFile {
    int flags;
    FILE* stream;
    AudioDecoder* audioDecoder;
    int fileSize;
    int sampleRate;
    int channels;
    int position;
} AudioFile;

static bool defaultCompressionFunc(char* filePath);
static int decodeRead(void* stream, void* buffer, unsigned int size);

// 0x4FEC04
static AudioFileQueryCompressedFunc* queryCompressedFunc = defaultCompressionFunc;

// 0x56B870
static AudioFile* audiof;

// 0x56B874
static int numAudiof;

// 0x419E90
static bool defaultCompressionFunc(char* filePath)
{
    char* pch = strrchr(filePath, '.');
    if (pch != NULL) {
        strcpy(pch + 1, "raw");
    }

    return false;
}

// 0x419EB0
static int decodeRead(void* stream, void* buffer, unsigned int size)
{
    return fread(buffer, 1, size, (FILE*)stream);
}

// 0x419ECC
int audiofOpen(const char* fname, int flags)
{
    char path[COMPAT_MAX_PATH];
    strcpy(path, fname);

    int compression;
    if (queryCompressedFunc(path)) {
        compression = 2;
    } else {
        compression = 0;
    }

    char mode[4];
    memset(mode, '\0', 4);

    // NOTE: Original implementation is slightly different, it uses separate
    // variable to track index where to set 't' and 'b'.
    char* pm = mode;
    if (flags & 0x01) {
        *pm++ = 'w';
    } else if (flags & 0x02) {
        *pm++ = 'w';
        *pm++ = '+';
    } else {
        *pm++ = 'r';
    }

    if (flags & 0x0100) {
        *pm++ = 't';
    } else if (flags & 0x0200) {
        *pm++ = 'b';
    }

    FILE* stream = compat_fopen(path, mode);
    if (stream == NULL) {
        return -1;
    }

    int index;
    for (index = 0; index < numAudiof; index++) {
        if ((audiof[index].flags & AUDIO_FILE_IN_USE) == 0) {
            break;
        }
    }

    if (index == numAudiof) {
        if (audiof != NULL) {
            audiof = (AudioFile*)myrealloc(audiof, sizeof(*audiof) * (numAudiof + 1), __FILE__, __LINE__); // "..\int\audiof.c", 206
        } else {
            audiof = (AudioFile*)mymalloc(sizeof(*audiof), __FILE__, __LINE__); // "..\int\audiof.c", 208
        }
        numAudiof++;
    }

    AudioFile* audioFile = &(audiof[index]);
    audioFile->flags = AUDIO_FILE_IN_USE;
    audioFile->stream = stream;

    if (compression == 2) {
        audioFile->flags |= AUDIO_FILE_COMPRESSED;
        audioFile->audioDecoder = Create_AudioDecoder(decodeRead, audioFile->stream, &(audioFile->channels), &(audioFile->sampleRate), &(audioFile->fileSize));
        audioFile->fileSize *= 2;
    } else {
        audioFile->fileSize = getFileSize(stream);
    }

    audioFile->position = 0;

    return index + 1;
}

// 0x41A0E0
int audiofCloseFile(int fileHandle)
{
    AudioFile* audioFile = &(audiof[fileHandle - 1]);
    fclose((FILE*)audioFile->stream);

    if ((audioFile->flags & AUDIO_FILE_COMPRESSED) != 0) {
        AudioDecoder_Close(audioFile->audioDecoder);
    }

    // Reset audio file (which also resets it's use flag).
    memset(audioFile, 0, sizeof(*audioFile));

    return 0;
}

// 0x41A148
int audiofRead(int fileHandle, void* buffer, unsigned int size)
{

    AudioFile* ptr = &(audiof[fileHandle - 1]);

    int bytesRead;
    if ((ptr->flags & AUDIO_FILE_COMPRESSED) != 0) {
        bytesRead = AudioDecoder_Read(ptr->audioDecoder, buffer, size);
    } else {
        bytesRead = fread(buffer, 1, size, ptr->stream);
    }

    ptr->position += bytesRead;

    return bytesRead;
}

// 0x41A1B4
long audiofSeek(int fileHandle, long offset, int origin)
{
    void* buf;
    int remaining;
    int a4;

    AudioFile* audioFile = &(audiof[fileHandle - 1]);

    switch (origin) {
    case SEEK_SET:
        a4 = offset;
        break;
    case SEEK_CUR:
        a4 = audioFile->fileSize + offset;
        break;
    case SEEK_END:
        a4 = audioFile->position + offset;
        break;
    default:
        assert(false && "Should be unreachable");
    }

    if ((audioFile->flags & AUDIO_FILE_COMPRESSED) != 0) {
        if (a4 <= audioFile->position) {
            AudioDecoder_Close(audioFile->audioDecoder);
            fseek(audioFile->stream, 0, SEEK_SET);
            audioFile->audioDecoder = Create_AudioDecoder(decodeRead, audioFile->stream, &(audioFile->channels), &(audioFile->sampleRate), &(audioFile->fileSize));
            audioFile->fileSize *= 2;
            audioFile->position = 0;

            if (a4) {
                buf = mymalloc(4096, __FILE__, __LINE__); // "..\int\audiof.c", 363
                while (a4 > 4096) {
                    audiofRead(fileHandle, buf, 4096);
                    a4 -= 4096;
                }
                if (a4 != 0) {
                    audiofRead(fileHandle, buf, a4);
                }
                myfree(buf, __FILE__, __LINE__); // "..\int\audiof.c", 369
            }
        } else {
            buf = mymalloc(0x400, __FILE__, __LINE__); // "..\int\audiof.c", 315
            remaining = audioFile->position - a4;
            while (remaining > 1024) {
                audiofRead(fileHandle, buf, 1024);
                remaining -= 1024;
            }
            if (remaining != 0) {
                audiofRead(fileHandle, buf, remaining);
            }
            // TODO: Obiously leaks memory.
        }
        return audioFile->position;
    }

    return fseek(audioFile->stream, offset, origin);
}

// 0x41A360
long audiofFileSize(int fileHandle)
{
    AudioFile* audioFile = &(audiof[fileHandle - 1]);
    return audioFile->fileSize;
}

// 0x41A37C
long audiofTell(int fileHandle)
{
    AudioFile* audioFile = &(audiof[fileHandle - 1]);
    return audioFile->position;
}

// 0x41A398
int audiofWrite(int fileHandle, const void* buffer, unsigned int size)
{
    debug_printf("AudiofWrite shouldn't be ever called\n");
    return 0;
}

// 0x41A3A8
int initAudiof(AudioFileQueryCompressedFunc* isCompressedProc)
{
    queryCompressedFunc = isCompressedProc;
    audiof = NULL;
    numAudiof = 0;

    return soundSetDefaultFileIO(audiofOpen, audiofCloseFile, audiofRead, audiofWrite, audiofSeek, audiofTell, audiofFileSize);
}

// 0x41A3EC
void audiofClose()
{
    if (audiof != NULL) {
        myfree(audiof, __FILE__, __LINE__); // "..\int\audiof.c", 404
    }

    numAudiof = 0;
    audiof = NULL;
}

} // namespace fallout
