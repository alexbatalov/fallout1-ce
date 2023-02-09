#ifndef FALLOUT_INT_DATAFILE_H_
#define FALLOUT_INT_DATAFILE_H_

namespace fallout {

typedef unsigned char*(DatafileLoader)(char* path, unsigned char* palette, int* widthPtr, int* heightPtr);
typedef char*(DatafileNameMangler)(char* path);

void datafileSetFilenameFunc(DatafileNameMangler* mangler);
void setBitmapLoadFunc(DatafileLoader* loader);
void datafileConvertData(unsigned char* data, unsigned char* palette, int width, int height);
void datafileConvertDataVGA(unsigned char* data, unsigned char* palette, int width, int height);
unsigned char* loadRawDataFile(char* path, int* widthPtr, int* heightPtr);
unsigned char* loadDataFile(char* path, int* widthPtr, int* heightPtr);
unsigned char* load256Palette(char* path);
void trimBuffer(unsigned char* data, int* widthPtr, int* heightPtr);
unsigned char* datafileGetPalette();
unsigned char* datafileLoadBlock(char* path, int* sizePtr);

} // namespace fallout

#endif /* FALLOUT_INT_DATAFILE_H_ */
