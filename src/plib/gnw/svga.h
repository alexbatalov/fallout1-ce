#ifndef FALLOUT_PLIB_GNW_SVGA_H_
#define FALLOUT_PLIB_GNW_SVGA_H_

#include <SDL.h>

#include "fps_limiter.h"
#include "plib/gnw/rect.h"
#include "plib/gnw/svga_types.h"

namespace fallout {

extern Rect scr_size;
extern ScreenBlitFunc* scr_blit;

extern SDL_Window* gSdlWindow;
extern SDL_Surface* gSdlSurface;
extern SDL_Renderer* gSdlRenderer;
extern SDL_Texture* gSdlTexture;
extern SDL_Surface* gSdlTextureSurface;
extern FpsLimiter sharedFpsLimiter;

void GNW95_SetPaletteEntries(unsigned char* a1, int a2, int a3);
void GNW95_SetPalette(unsigned char* palette);
void GNW95_ShowRect(unsigned char* src, unsigned int src_pitch, unsigned int a3, unsigned int src_x, unsigned int src_y, unsigned int src_width, unsigned int src_height, unsigned int dest_x, unsigned int dest_y);

bool svga_init(VideoOptions* video_options);
void svga_exit();
int screenGetWidth();
int screenGetHeight();
void handleWindowSizeChanged();
void renderPresent();

} // namespace fallout

#endif /* FALLOUT_PLIB_GNW_SVGA_H_ */
