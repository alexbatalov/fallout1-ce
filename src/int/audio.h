#ifndef FALLOUT_INT_AUDIO_H_
#define FALLOUT_INT_AUDIO_H_

namespace fallout {

typedef bool(AudioQueryCompressedFunc)(char* filePath);

int audioOpen(const char* fname, int mode);
int audioCloseFile(int fileHandle);
int audioRead(int fileHandle, void* buffer, unsigned int size);
long audioSeek(int fileHandle, long offset, int origin);
long audioFileSize(int fileHandle);
long audioTell(int fileHandle);
int audioWrite(int handle, const void* buf, unsigned int size);
int initAudio(AudioQueryCompressedFunc* isCompressedProc);
void audioClose();

} // namespace fallout

#endif /* FALLOUT_INT_AUDIO_H_ */
