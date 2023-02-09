#ifndef FALLOUT_INT_AUDIOF_H_
#define FALLOUT_INT_AUDIOF_H_

namespace fallout {

typedef bool(AudioFileQueryCompressedFunc)(char* filePath);

int audiofOpen(const char* fname, int flags);
int audiofCloseFile(int a1);
int audiofRead(int a1, void* buf, unsigned int size);
long audiofSeek(int handle, long offset, int origin);
long audiofFileSize(int a1);
long audiofTell(int a1);
int audiofWrite(int handle, const void* buf, unsigned int size);
int initAudiof(AudioFileQueryCompressedFunc* isCompressedProc);
void audiofClose();

} // namespace fallout

#endif /* FALLOUT_INT_AUDIOF_H_ */
