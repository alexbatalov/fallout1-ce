#include "int/audio.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <adecode/adecode.h>

#include "int/memdbg.h"
#include "int/sound.h"
#include "plib/db/db.h"
#include "plib/gnw/debug.h"

namespace fallout {

typedef enum AudioFlags {
    AUDIO_FILE_IN_USE = 0x01,
    AUDIO_FILE_COMPRESSED = 0x02,
} AudioFlags;

typedef struct Audio {
    int flags;
    DB_FILE* stream;
    AudioDecoder* audioDecoder;
    int fileSize;
    int sampleRate;
    int channels;
    int position;
} Audio;

static bool defaultCompressionFunc(char* filePath);
static unsigned int decodeRead(void* stream, void* buf, unsigned int size);

// 0x4FEC00
static AudioQueryCompressedFunc* queryCompressedFunc = defaultCompressionFunc;

// 0x56B860
static int numAudio;

// 0x56B864
static Audio* audio;

// 0x4198F0
static bool defaultCompressionFunc(char* filePath)
{
    char* pch = strrchr(filePath, '.');
    if (pch != NULL) {
        strcpy(pch + 1, "raw");
    }

    return false;
}

// 0x419910
static unsigned int decodeRead(void* stream, void* buffer, unsigned int size)
{
    return db_fread(buffer, 1, size, (DB_FILE*)stream);
}

// 0x41992C
int audioOpen(const char* fname, int flags)
{
    char path[80];
    snprintf(path, sizeof(path), fname);

    int compression;
    if (queryCompressedFunc(path)) {
        compression = 2;
    } else {
        compression = 0;
    }

    char mode[4];
    memset(mode, 0, 4);

    // NOTE: Original implementation is slightly different, it uses separate
    // variable to track index where to set 't' and 'b'.
    char* pm = mode;
    if (flags & 1) {
        *pm++ = 'w';
    } else if (flags & 2) {
        *pm++ = 'w';
        *pm++ = '+';
    } else {
        *pm++ = 'r';
    }

    if (flags & 0x100) {
        *pm++ = 't';
    } else if (flags & 0x200) {
        *pm++ = 'b';
    }

    DB_FILE* stream = db_fopen(path, mode);
    if (stream == NULL) {
        debug_printf("AudioOpen: Couldn't open %s for read\n", path);
        return -1;
    }

    int index;
    for (index = 0; index < numAudio; index++) {
        if ((audio[index].flags & AUDIO_FILE_IN_USE) == 0) {
            break;
        }
    }

    if (index == numAudio) {
        if (audio != NULL) {
            audio = (Audio*)myrealloc(audio, sizeof(*audio) * (numAudio + 1), __FILE__, __LINE__); // "..\int\audio.c", 216
        } else {
            audio = (Audio*)mymalloc(sizeof(*audio), __FILE__, __LINE__); // "..\int\audio.c", 218
        }
        numAudio++;
    }

    Audio* audioFile = &(audio[index]);
    audioFile->flags = AUDIO_FILE_IN_USE;
    audioFile->stream = stream;

    if (compression == 2) {
        audioFile->flags |= AUDIO_FILE_COMPRESSED;
        audioFile->audioDecoder = Create_AudioDecoder(decodeRead, audioFile->stream, &(audioFile->channels), &(audioFile->sampleRate), &(audioFile->fileSize));
        audioFile->fileSize *= 2;
    } else {
        audioFile->fileSize = db_filelength(stream);
    }

    audioFile->position = 0;

    return index + 1;
}

// 0x419B4C
int audioCloseFile(int fileHandle)
{
    Audio* audioFile = &(audio[fileHandle - 1]);
    db_fclose(audioFile->stream);

    if ((audioFile->flags & AUDIO_FILE_COMPRESSED) != 0) {
        AudioDecoder_Close(audioFile->audioDecoder);
    }

    memset(audioFile, 0, sizeof(Audio));

    return 0;
}

// 0x419BB4
int audioRead(int fileHandle, void* buffer, unsigned int size)
{
    Audio* audioFile = &(audio[fileHandle - 1]);

    int bytesRead;
    if ((audioFile->flags & AUDIO_FILE_COMPRESSED) != 0) {
        bytesRead = AudioDecoder_Read(audioFile->audioDecoder, buffer, size);
    } else {
        bytesRead = db_fread(buffer, 1, size, audioFile->stream);
    }

    audioFile->position += bytesRead;

    return bytesRead;
}

// 0x419C20
long audioSeek(int fileHandle, long offset, int origin)
{
    int pos;
    unsigned char* buf;
    int v10;

    Audio* audioFile = &(audio[fileHandle - 1]);

    switch (origin) {
    case SEEK_SET:
        pos = offset;
        break;
    case SEEK_CUR:
        pos = offset + audioFile->position;
        break;
    case SEEK_END:
        pos = offset + audioFile->fileSize;
        break;
    default:
        assert(false && "Should be unreachable");
    }

    if ((audioFile->flags & AUDIO_FILE_COMPRESSED) != 0) {
        if (pos < audioFile->position) {
            AudioDecoder_Close(audioFile->audioDecoder);
            db_fseek(audioFile->stream, 0, SEEK_SET);
            audioFile->audioDecoder = Create_AudioDecoder(decodeRead, audioFile->stream, &(audioFile->channels), &(audioFile->sampleRate), &(audioFile->fileSize));
            audioFile->position = 0;
            audioFile->fileSize *= 2;

            if (pos != 0) {
                buf = (unsigned char*)mymalloc(4096, __FILE__, __LINE__); // "..\int\audio.c", 361
                while (pos > 4096) {
                    pos -= 4096;
                    audioRead(fileHandle, buf, 4096);
                }

                if (pos != 0) {
                    audioRead(fileHandle, buf, pos);
                }

                myfree(buf, __FILE__, __LINE__); // // "..\int\audio.c", 367
            }
        } else {
            buf = (unsigned char*)mymalloc(1024, __FILE__, __LINE__); // "..\int\audio.c", 321
            v10 = audioFile->position - pos;
            while (v10 > 1024) {
                v10 -= 1024;
                audioRead(fileHandle, buf, 1024);
            }

            if (v10 != 0) {
                audioRead(fileHandle, buf, v10);
            }

            // TODO: Probably leaks memory.
        }

        return audioFile->position;
    } else {
        return db_fseek(audioFile->stream, offset, origin);
    }
}

// 0x419DCC
long audioFileSize(int fileHandle)
{
    Audio* audioFile = &(audio[fileHandle - 1]);
    return audioFile->fileSize;
}

// 0x419DE8
long audioTell(int fileHandle)
{
    Audio* audioFile = &(audio[fileHandle - 1]);
    return audioFile->position;
}

// 0x419E04
int audioWrite(int handle, const void* buf, unsigned int size)
{
    debug_printf("AudioWrite shouldn't be ever called\n");
    return 0;
}

// 0x419E14
int initAudio(AudioQueryCompressedFunc* isCompressedProc)
{
    queryCompressedFunc = isCompressedProc;
    audio = NULL;
    numAudio = 0;

    return soundSetDefaultFileIO(audioOpen, audioCloseFile, audioRead, audioWrite, audioSeek, audioTell, audioFileSize);
}

// 0x419E58
void audioClose()
{
    if (audio != NULL) {
        myfree(audio, __FILE__, __LINE__); // "..\int\audio.c", 406
    }

    numAudio = 0;
    audio = NULL;
}

} // namespace fallout
