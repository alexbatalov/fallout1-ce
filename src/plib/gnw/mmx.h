#ifndef FALLOUT_PLIB_GNW_MMX_H_
#define FALLOUT_PLIB_GNW_MMX_H_

namespace fallout {

bool mmxTest();
void srcCopy(unsigned char* dest, int destPitch, unsigned char* src, int srcPitch, int width, int height);
void transSrcCopy(unsigned char* dest, int destPitch, unsigned char* src, int srcPitch, int width, int height);

} // namespace fallout

#endif /* FALLOUT_PLIB_GNW_MMX_H_ */
