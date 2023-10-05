#ifndef FALLOUT_INT_WINDOW_H_
#define FALLOUT_INT_WINDOW_H_

#include "int/intrpret.h"
#include "int/region.h"
#include "int/widget.h"
#include "plib/gnw/gnw.h"
#include "plib/gnw/rect.h"
#include "plib/gnw/svga_types.h"

namespace fallout {

#define MANAGED_WINDOW_COUNT (16)

typedef bool(WindowInputHandler)(int key);
typedef void(WindowDeleteCallback)(int windowIndex, const char* windowName);
typedef void(DisplayInWindowCallback)(int windowIndex, const char* windowName, unsigned char* data, int width, int height);
typedef void(ManagedButtonMouseEventCallback)(void* userData, int eventType);
typedef void(ManagedWindowCreateCallback)(int windowIndex, const char* windowName, int* flagsPtr);
typedef void(ManagedWindowSelectFunc)(int windowIndex, const char* windowName);

typedef enum TextAlignment {
    TEXT_ALIGNMENT_LEFT,
    TEXT_ALIGNMENT_RIGHT,
    TEXT_ALIGNMENT_CENTER,
} TextAlignment;

int windowGetFont();
int windowSetFont(int a1);
void windowResetTextAttributes();
int windowGetTextFlags();
int windowSetTextFlags(int a1);
unsigned char windowGetTextColor();
unsigned char windowGetHighlightColor();
int windowSetTextColor(float r, float g, float b);
int windowSetHighlightColor(float r, float g, float b);
bool windowCheckRegion(int windowIndex, int mouseX, int mouseY, int mouseEvent);
bool windowRefreshRegions();
void windowAddInputFunc(WindowInputHandler* handler);
bool windowActivateRegion(const char* regionName, int a2);
int getInput();
int windowHide();
int windowShow();
int windowDraw();
int windowDrawRect(int left, int top, int right, int bottom);
int windowDrawRectID(int windowId, int left, int top, int right, int bottom);
int windowWidth();
int windowHeight();
int windowSX();
int windowSY();
int pointInWindow(int x, int y);
int windowGetRect(Rect* rect);
int windowGetID();
int windowGetGNWID();
int windowGetSpecificGNWID(int windowIndex);
bool deleteWindow(const char* windowName);
int resizeWindow(const char* windowName, int x, int y, int width, int height);
int scaleWindow(const char* windowName, int x, int y, int width, int height);
int createWindow(const char* windowName, int x, int y, int width, int height, int a6, int flags);
int windowOutput(char* string);
bool windowGotoXY(int x, int y);
bool selectWindowID(int index);
int selectWindow(const char* windowName);
int windowGetDefined(const char* name);
unsigned char* windowGetBuffer();
char* windowGetName();
int pushWindow(const char* windowName);
int popWindow();
void windowPrintBuf(int win, char* string, int stringLength, int width, int maxY, int x, int y, int flags, int textAlignment);
char** windowWordWrap(char* string, int maxLength, int a3, int* substringListLengthPtr);
void windowFreeWordList(char** substringList, int substringListLength);
void windowWrapLineWithSpacing(int win, char* string, int width, int height, int x, int y, int flags, int textAlignment, int a9);
void windowWrapLine(int win, char* string, int width, int height, int x, int y, int flags, int textAlignment);
bool windowPrintRect(char* string, int a2, int textAlignment);
bool windowFormatMessage(char* string, int x, int y, int width, int height, int textAlignment);
int windowFormatMessageColor(char* string, int x, int y, int width, int height, int textAlignment, int flags);
bool windowPrint(char* string, int a2, int x, int y, int a5);
int windowPrintFont(char* string, int a2, int x, int y, int a5, int font);
void displayInWindow(unsigned char* data, int width, int height, int pitch);
void displayFile(char* fileName);
void displayFileRaw(char* fileName);
int windowDisplayRaw(char* fileName);
bool windowDisplay(char* fileName, int x, int y, int width, int height);
int windowDisplayScaled(char* fileName, int x, int y, int width, int height);
bool windowDisplayBuf(unsigned char* src, int srcWidth, int srcHeight, int destX, int destY, int destWidth, int destHeight);
int windowDisplayTransBuf(unsigned char* src, int srcWidth, int srcHeight, int destX, int destY, int destWidth, int destHeight);
int windowDisplayBufScaled(unsigned char* src, int srcWidth, int srcHeight, int destX, int destY, int destWidth, int destHeight);
int windowGetXres();
int windowGetYres();
void initWindow(VideoOptions* video_options, int flags);
void windowSetWindowFuncs(ManagedWindowCreateCallback* createCallback, ManagedWindowSelectFunc* selectCallback, WindowDeleteCallback* deleteCallback, DisplayInWindowCallback* displayCallback);
void windowClose();
bool windowDeleteButton(const char* buttonName);
void windowEnableButton(const char* buttonName, int enabled);
int windowGetButtonID(const char* buttonName);
bool windowSetButtonFlag(const char* buttonName, int value);
void windowRegisterButtonSoundFunc(ButtonCallback* soundPressFunc, ButtonCallback* soundReleaseFunc, ButtonCallback* soundDisableFunc);
bool windowAddButton(const char* buttonName, int x, int y, int width, int height, int flags);
bool windowAddButtonGfx(const char* buttonName, char* a2, char* a3, char* a4);
int windowAddButtonMask(const char* buttonName, unsigned char* buffer);
int windowAddButtonBuf(const char* buttonName, unsigned char* normal, unsigned char* pressed, unsigned char* hover, int width, int height, int pitch);
bool windowAddButtonProc(const char* buttonName, Program* program, int mouseEnterProc, int mouseExitProc, int mouseDownProc, int mouseUpProc);
bool windowAddButtonRightProc(const char* buttonName, Program* program, int rightMouseDownProc, int rightMouseUpProc);
bool windowAddButtonCfunc(const char* buttonName, ManagedButtonMouseEventCallback* callback, void* userData);
bool windowAddButtonRightCfunc(const char* buttonName, ManagedButtonMouseEventCallback* callback, void* userData);
bool windowAddButtonText(const char* buttonName, const char* text);
bool windowAddButtonTextWithOffsets(const char* buttonName, const char* text, int pressedImageOffsetX, int pressedImageOffsetY, int normalImageOffsetX, int normalImageOffsetY);
bool windowFill(float r, float g, float b);
bool windowFillRect(int x, int y, int width, int height, float r, float g, float b);
void windowEndRegion();
void* windowRegionGetUserData(const char* windowRegionName);
void windowRegionSetUserData(const char* windowRegionName, void* userData);
bool windowCheckRegionExists(const char* regionName);
bool windowStartRegion(int initialCapacity);
bool windowAddRegionPoint(int x, int y, bool a3);
int windowAddRegionRect(int a1, int a2, int a3, int a4, int a5);
int windowAddRegionCfunc(const char* regionName, RegionMouseEventCallback* callback, void* userData);
int windowAddRegionRightCfunc(const char* regionName, RegionMouseEventCallback* callback, void* userData);
bool windowAddRegionProc(const char* regionName, Program* program, int a3, int a4, int a5, int a6);
bool windowAddRegionRightProc(const char* regionName, Program* program, int a3, int a4);
bool windowSetRegionFlag(const char* regionName, int value);
bool windowAddRegionName(const char* regionName);
bool windowDeleteRegion(const char* regionName);
void updateWindows();
int windowMoviePlaying();
bool windowSetMovieFlags(int flags);
bool windowPlayMovie(char* filePath);
bool windowPlayMovieRect(char* filePath, int a2, int a3, int a4, int a5);
void windowStopMovie();
void drawScaled(unsigned char* dest, int destWidth, int destHeight, int destPitch, unsigned char* src, int srcWidth, int srcHeight, int srcPitch);
void drawScaledBuf(unsigned char* dest, int destWidth, int destHeight, unsigned char* src, int srcWidth, int srcHeight);
void alphaBltBuf(unsigned char* src, int srcWidth, int srcHeight, int srcPitch, unsigned char* a5, unsigned char* a6, unsigned char* dest, int destPitch);
void fillBuf3x3(unsigned char* src, int srcWidth, int srcHeight, unsigned char* dest, int destWidth, int destHeight);
int windowEnableCheckRegion();
int windowDisableCheckRegion();
int windowSetHoldTime(int value);
int windowAddTextRegion(int x, int y, int width, int font, int textAlignment, int textFlags, int backgroundColor);
int windowPrintTextRegion(int textRegionId, char* string);
int windowUpdateTextRegion(int textRegionId);
int windowDeleteTextRegion(int textRegionId);
int windowTextRegionStyle(int textRegionId, int font, int textAlignment, int textFlags, int backgroundColor);
int windowAddTextInputRegion(int textRegionId, char* text, int a3, int a4);
int windowDeleteTextInputRegion(int textInputRegionId);
int windowSetTextInputDeleteFunc(int textInputRegionId, TextInputRegionDeleteFunc* deleteFunc, void* userData);

} // namespace fallout

#endif /* FALLOUT_INT_WINDOW_H_ */
