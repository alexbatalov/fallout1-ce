#ifndef FALLOUT_SOUND_DECODER_H_
#define FALLOUT_SOUND_DECODER_H_

#include <stddef.h>

typedef int(AudioDecoderReadFunc)(void* data, void* buffer, unsigned int size);

typedef struct _AudioDecoder AudioDecoder;

size_t AudioDecoder_Read(AudioDecoder* ad, void* buffer, size_t size);
void AudioDecoder_Close(AudioDecoder* ad);
AudioDecoder* Create_AudioDecoder(AudioDecoderReadFunc* reader, void* data, int* channels, int* sampleRate, int* sampleCount);

#endif /* FALLOUT_SOUND_DECODER_H_ */
