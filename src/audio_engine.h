#ifndef FALLOUT_AUDIO_ENGINE_H_
#define FALLOUT_AUDIO_ENGINE_H_

namespace fallout {

#define AUDIO_ENGINE_SOUND_BUFFER_LOCK_FROM_WRITE_POS 0x00000001
#define AUDIO_ENGINE_SOUND_BUFFER_LOCK_ENTIRE_BUFFER 0x00000002

#define AUDIO_ENGINE_SOUND_BUFFER_PLAY_LOOPING 0x00000001

#define AUDIO_ENGINE_SOUND_BUFFER_STATUS_PLAYING 0x00000001
#define AUDIO_ENGINE_SOUND_BUFFER_STATUS_LOOPING 0x00000004

bool audioEngineInit();
void audioEngineExit();
void audioEnginePause();
void audioEngineResume();
int audioEngineCreateSoundBuffer(unsigned int size, int bitsPerSample, int channels, int rate);
bool audioEngineSoundBufferRelease(int soundBufferIndex);
bool audioEngineSoundBufferSetVolume(int soundBufferIndex, int volume);
bool audioEngineSoundBufferGetVolume(int soundBufferIndex, int* volumePtr);
bool audioEngineSoundBufferSetPan(int soundBufferIndex, int pan);
bool audioEngineSoundBufferPlay(int soundBufferIndex, unsigned int flags);
bool audioEngineSoundBufferStop(int soundBufferIndex);
bool audioEngineSoundBufferGetCurrentPosition(int soundBufferIndex, unsigned int* readPosPtr, unsigned int* writePosPtr);
bool audioEngineSoundBufferSetCurrentPosition(int soundBufferIndex, unsigned int pos);
bool audioEngineSoundBufferLock(int soundBufferIndex, unsigned int writePos, unsigned int writeBytes, void** audioPtr1, unsigned int* audioBytes1, void** audioPtr2, unsigned int* audioBytes2, unsigned int flags);
bool audioEngineSoundBufferUnlock(int soundBufferIndex, void* audioPtr1, unsigned int audioBytes1, void* audioPtr2, unsigned int audioBytes2);
bool audioEngineSoundBufferGetStatus(int soundBufferIndex, unsigned int* status);

} // namespace fallout

#endif /* FALLOUT_AUDIO_ENGINE_H_ */
