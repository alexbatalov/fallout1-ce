#ifndef FALLOUT_INT_SHARE1_H_
#define FALLOUT_INT_SHARE1_H_

namespace fallout {

char** getFileList(const char* pattern, int* fileNameListLengthPtr);
void freeFileList(char** fileList);

} // namespace fallout

#endif /* FALLOUT_INT_SHARE1_H_ */
