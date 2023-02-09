#ifndef FALLOUT_INT_MOVIE_H_
#define FALLOUT_INT_MOVIE_H_

#include "plib/gnw/rect.h"

namespace fallout {

typedef enum MovieFlags {
    MOVIE_FLAG_0x01 = 0x01,
    MOVIE_FLAG_0x02 = 0x02,
    MOVIE_FLAG_0x04 = 0x04,
    MOVIE_FLAG_0x08 = 0x08,
} MovieFlags;

typedef enum MovieExtendedFlags {
    MOVIE_EXTENDED_FLAG_0x01 = 0x01,
    MOVIE_EXTENDED_FLAG_0x02 = 0x02,
    MOVIE_EXTENDED_FLAG_0x04 = 0x04,
    MOVIE_EXTENDED_FLAG_0x08 = 0x08,
    MOVIE_EXTENDED_FLAG_0x10 = 0x10,
} MovieExtendedFlags;

typedef char*(MovieSubtitleFunc)(char* movieFilePath);
typedef void(MoviePaletteFunc)(unsigned char* palette, int start, int end);
typedef void(MovieUpdateCallbackProc)(int frame);
typedef void(MovieFrameGrabProc)(unsigned char* data, int width, int height, int pitch);
typedef void(MovieCaptureFrameProc)(unsigned char* data, int width, int height, int pitch, int movieX, int movieY, int movieWidth, int movieHeight);
typedef void(MoviePreDrawFunc)(int win, Rect* rect);
typedef void(MovieStartFunc)(int win);
typedef void(MovieEndFunc)(int win, int x, int y, int width, int height);
typedef int(MovieFailedOpenFunc)(char* path);

void movieSetPreDrawFunc(MoviePreDrawFunc* func);
void movieSetFailedOpenFunc(MovieFailedOpenFunc* func);
void movieSetFunc(MovieStartFunc* startFunc, MovieEndFunc* endFunc);
void movieSetFrameGrabFunc(MovieFrameGrabProc* func);
void movieSetCaptureFrameFunc(MovieCaptureFrameProc* func);
void initMovie();
void movieClose();
void movieStop();
int movieSetFlags(int a1);
void movieSetSubtitleFont(int font);
void movieSetSubtitleColor(float r, float g, float b);
void movieSetPaletteFunc(MoviePaletteFunc* func);
void movieSetCallback(MovieUpdateCallbackProc* func);
int movieRun(int win, char* filePath);
int movieRunRect(int win, char* filePath, int a3, int a4, int a5, int a6);
void movieSetSubtitleFunc(MovieSubtitleFunc* proc);
void movieSetVolume(int volume);
void movieUpdate();
int moviePlaying();

} // namespace fallout

#endif /* FALLOUT_INT_MOVIE_H_ */
