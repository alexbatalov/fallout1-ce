#include "plib/gnw/mmx.h"

#include <string.h>

#include "plib/gnw/svga.h"

namespace fallout {

// Return `true` if CPU supports MMX.
//
// 0x4CD640
bool mmxTest()
{
    return SDL_HasMMX() == SDL_TRUE;
}

// 0x4CDB50
void srcCopy(unsigned char* dest, int destPitch, unsigned char* src, int srcPitch, int width, int height)
{
    for (int y = 0; y < height; y++) {
        memcpy(dest, src, width);
        dest += destPitch;
        src += srcPitch;
    }
}

// 0x4CDC75
void transSrcCopy(unsigned char* dest, int destPitch, unsigned char* src, int srcPitch, int width, int height)
{
    int destSkip = destPitch - width;
    int srcSkip = srcPitch - width;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            unsigned char c = *src++;
            if (c != 0) {
                *dest = c;
            }
            dest++;
        }
        src += srcSkip;
        dest += destSkip;
    }
}

} // namespace fallout
