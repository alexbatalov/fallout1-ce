#ifndef FALLOUT_INT_PCX_H_
#define FALLOUT_INT_PCX_H_

namespace fallout {

unsigned char* loadPCX(const char* path, int* widthPtr, int* heightPtr, unsigned char* palette);

} // namespace fallout

#endif /* FALLOUT_INT_PCX_H_ */
