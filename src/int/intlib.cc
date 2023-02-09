#include "int/intlib.h"

#include <stdio.h>

#include "int/datafile.h"
#include "int/dialog.h"
#include "int/memdbg.h"
#include "int/mousemgr.h"
#include "int/nevs.h"
#include "int/share1.h"
#include "int/sound.h"
#include "int/support/intextra.h"
#include "int/window.h"
#include "plib/color/color.h"
#include "plib/gnw/debug.h"
#include "plib/gnw/input.h"
#include "plib/gnw/intrface.h"
#include "plib/gnw/svga.h"
#include "plib/gnw/text.h"

namespace fallout {

#define INT_LIB_SOUNDS_CAPACITY 32
#define INT_LIB_KEY_HANDLERS_CAPACITY 256

typedef struct IntLibKeyHandlerEntry {
    Program* program;
    int proc;
} IntLibKeyHandlerEntry;

static void op_fillwin3x3(Program* program);
static void op_format(Program* program);
static void op_print(Program* program);
static void op_selectfilelist(Program* program);
static void op_tokenize(Program* program);
static void op_printrect(Program* program);
static void op_selectwin(Program* program);
static void op_display(Program* program);
static void op_displayraw(Program* program);
static void interpretFadePaletteBK(unsigned char* oldPalette, unsigned char* newPalette, int a3, float duration, int shouldProcessBk);
static void op_fadein(Program* program);
static void op_fadeout(Program* program);
static void op_movieflags(Program* program);
static void op_playmovie(Program* program);
static void op_playmovierect(Program* program);
static void op_stopmovie(Program* program);
static void op_addregionproc(Program* program);
static void op_addregionrightproc(Program* program);
static void op_createwin(Program* program);
static void op_resizewin(Program* program);
static void op_scalewin(Program* program);
static void op_deletewin(Program* program);
static void op_saystart(Program* program);
static void op_deleteregion(Program* program);
static void op_activateregion(Program* program);
static void op_checkregion(Program* program);
static void op_addregion(Program* program);
static void op_saystartpos(Program* program);
static void op_sayreplytitle(Program* program);
static void op_saygotoreply(Program* program);
static void op_sayoption(Program* program);
static void op_sayreply(Program* program);
static int checkDialog(Program* program);
static void op_sayend(Program* program);
static void op_saygetlastpos(Program* program);
static void op_sayquit(Program* program);
static void op_saymessagetimeout(Program* program);
static void op_saymessage(Program* program);
static void op_gotoxy(Program* program);
static void op_addbuttonflag(Program* program);
static void op_addregionflag(Program* program);
static void op_addbutton(Program* program);
static void op_addbuttontext(Program* program);
static void op_addbuttongfx(Program* program);
static void op_addbuttonproc(Program* program);
static void op_addbuttonrightproc(Program* program);
static void op_showwin(Program* program);
static void op_deletebutton(Program* program);
static void op_fillwin(Program* program);
static void op_fillrect(Program* program);
static void op_hidemouse(Program* program);
static void op_showmouse(Program* program);
static void op_mouseshape(Program* program);
static void op_setglobalmousefunc(Program* Program);
static void op_displaygfx(Program* program);
static void op_loadpalettetable(Program* program);
static void op_addNamedEvent(Program* program);
static void op_addNamedHandler(Program* program);
static void op_clearNamed(Program* program);
static void op_signalNamed(Program* program);
static void op_addkey(Program* program);
static void op_deletekey(Program* program);
static void op_refreshmouse(Program* program);
static void op_setfont(Program* program);
static void op_settextflags(Program* program);
static void op_settextcolor(Program* program);
static void op_sayoptioncolor(Program* program);
static void op_sayreplycolor(Program* program);
static void op_sethighlightcolor(Program* program);
static void op_sayreplywindow(Program* program);
static void op_sayreplyflags(Program* program);
static void op_sayoptionflags(Program* program);
static void op_sayoptionwindow(Program* program);
static void op_sayborder(Program* program);
static void op_sayscrollup(Program* program);
static void op_sayscrolldown(Program* program);
static void op_saysetspacing(Program* program);
static void op_sayrestart(Program* program);
static void soundCallbackInterpret(void* userData, int a2);
static int soundDeleteInterpret(int value);
static int soundPauseInterpret(int value);
static int soundRewindInterpret(int value);
static int soundUnpauseInterpret(int value);
static void op_soundplay(Program* program);
static void op_soundpause(Program* program);
static void op_soundresume(Program* program);
static void op_soundstop(Program* program);
static void op_soundrewind(Program* program);
static void op_sounddelete(Program* program);
static void op_setoneoptpause(Program* program);
static bool intLibDoInput(int key);

// 0x505620
static int TimeOut = 0;

// 0x59BB60
static Sound* interpretSounds[INT_LIB_SOUNDS_CAPACITY];

// 0x59BBE0
static unsigned char blackPal[256 * 3];

// 0x59BEE0
static IntLibKeyHandlerEntry inputProc[INT_LIB_KEY_HANDLERS_CAPACITY];

// 0x59C6E0
static bool currentlyFadedIn;

// 0x59C6E4
static int anyKeyOffset;

// 0x59C6E8
static int numCallbacks;

// 0x59C6EC
static Program* anyKeyProg;

// 0x59C6F0
static IntLibProgramDeleteCallback** callbacks;

// 0x59C6F4
static int sayStartingPosition;

// 0x456CC0
static void op_fillwin3x3(Program* program)
{
    char* fileName = programStackPopString(program);
    char* mangledFileName = interpretMangleName(fileName);

    int imageWidth;
    int imageHeight;
    unsigned char* imageData = loadDataFile(mangledFileName, &imageWidth, &imageHeight);
    if (imageData == NULL) {
        interpretError("cannot load 3x3 file '%s'", mangledFileName);
    }

    selectWindowID(program->windowId);

    fillBuf3x3(imageData,
        imageWidth,
        imageHeight,
        windowGetBuffer(),
        windowWidth(),
        windowHeight());

    myfree(imageData, __FILE__, __LINE__); // "..\\int\\INTLIB.C", 94
}

// 0x456D74
static void op_format(Program* program)
{
    int textAlignment = programStackPopInteger(program);
    int height = programStackPopInteger(program);
    int width = programStackPopInteger(program);
    int y = programStackPopInteger(program);
    int x = programStackPopInteger(program);
    char* string = programStackPopString(program);

    if (!windowFormatMessage(string, x, y, width, height, textAlignment)) {
        interpretError("Error formatting message\n");
    }
}

// 0x456EE8
static void op_print(Program* program)
{
    selectWindowID(program->windowId);

    ProgramValue value = programStackPopValue(program);
    char string[80];

    // SFALL: Fix broken Print() script function.
    // CE: Original code uses `interpretOutput` to handle printing. However
    // this function looks invalid or broken itself. Check `opSelect` - it sets
    // `outputFunc` to `windowOutput`, but `outputFunc` is never called. I'm not
    // sure if this fix can be moved into `interpretOutput` because it is also
    // used in procedure setup functions.
    //
    // The fix is slightly different, Sfall fixes strings only, ints and floats
    // are still passed to `interpretOutput`.
    switch (value.opcode & VALUE_TYPE_MASK) {
    case VALUE_TYPE_STRING:
        windowOutput(interpretGetString(program, value.opcode, value.integerValue));
        break;
    case VALUE_TYPE_FLOAT:
        snprintf(string, sizeof(string), "%.5f", value.floatValue);
        windowOutput(string);
        break;
    case VALUE_TYPE_INT:
        snprintf(string, sizeof(string), "%d", value.integerValue);
        windowOutput(string);
        break;
    }
}

// 0x456F80
static void op_selectfilelist(Program* program)
{
    program->flags |= PROGRAM_FLAG_0x20;

    char* pattern = programStackPopString(program);
    char* title = programStackPopString(program);

    int fileListLength;
    char** fileList = getFileList(interpretMangleName(pattern), &fileListLength);
    if (fileList != NULL && fileListLength != 0) {
        int selectedIndex = win_list_select(title,
            fileList,
            fileListLength,
            NULL,
            320 - text_width(title) / 2,
            200,
            colorTable[0x7FFF] | 0x10000);

        if (selectedIndex != -1) {
            programStackPushString(program, fileList[selectedIndex]);
        } else {
            programStackPushInteger(program, 0);
        }

        freeFileList(fileList);
    } else {
        programStackPushInteger(program, 0);
    }

    program->flags &= ~PROGRAM_FLAG_0x20;
}

// 0x4570DC
static void op_tokenize(Program* program)
{
    int ch = programStackPopInteger(program);

    ProgramValue prevValue = programStackPopValue(program);

    char* prev = NULL;
    if ((prevValue.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_INT) {
        if (prevValue.integerValue != 0) {
            interpretError("Error, invalid arg 2 to tokenize. (only accept 0 for int value)");
        }
    } else if ((prevValue.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
        prev = interpretGetString(program, prevValue.opcode, prevValue.integerValue);
    } else {
        interpretError("Error, invalid arg 2 to tokenize. (string)");
    }

    char* string = programStackPopString(program);
    char* temp = NULL;

    if (prev != NULL) {
        char* start = strstr(string, prev);
        if (start != NULL) {
            start += strlen(prev);
            while (*start != ch && *start != '\0') {
                start++;
            }
        }

        if (*start == ch) {
            int length = 0;
            char* end = start + 1;
            while (*end != ch && *end != '\0') {
                end++;
                length++;
            }

            temp = (char*)mycalloc(1, length + 1, __FILE__, __LINE__); // "..\\int\\INTLIB.C, 230
            strncpy(temp, start, length);
            programStackPushString(program, temp);
        } else {
            programStackPushInteger(program, 0);
        }
    } else {
        int length = 0;
        char* end = string;
        while (*end != ch && *end != '\0') {
            end++;
            length++;
        }

        if (string != NULL) {
            temp = (char*)mycalloc(1, length + 1, __FILE__, __LINE__); // "..\\int\\INTLIB.C", 248
            strncpy(temp, string, length);
            programStackPushString(program, temp);
        } else {
            programStackPushInteger(program, 0);
        }
    }

    if (temp != NULL) {
        myfree(temp, __FILE__, __LINE__); // "..\\int\\INTLIB.C" , 260
    }
}

// 0x457308
static void op_printrect(Program* program)
{
    selectWindowID(program->windowId);

    int v1 = programStackPopInteger(program);
    if (v1 > 2) {
        interpretError("Invalid arg 3 given to printrect, expecting int");
    }

    int v2 = programStackPopInteger(program);

    ProgramValue value = programStackPopValue(program);
    char string[80];
    switch (value.opcode & VALUE_TYPE_MASK) {
    case VALUE_TYPE_STRING:
        snprintf(string, sizeof(string), "%s", interpretGetString(program, value.opcode, value.integerValue));
        break;
    case VALUE_TYPE_FLOAT:
        snprintf(string, sizeof(string), "%.5f", value.floatValue);
        break;
    case VALUE_TYPE_INT:
        snprintf(string, sizeof(string), "%d", value.integerValue);
        break;
    }

    if (!windowPrintRect(string, v2, v1)) {
        interpretError("Error in printrect");
    }
}

// 0x457430
static void op_selectwin(Program* program)
{
    const char* windowName = programStackPopString(program);
    int win = pushWindow(windowName);
    if (win == -1) {
        interpretError("Error selecing window %s\n", windowName);
    }

    program->windowId = win;

    interpretOutputFunc(windowOutput);
}

// 0x4574B4
static void op_display(Program* program)
{
    char* fileName = programStackPopString(program);

    selectWindowID(program->windowId);

    char* mangledFileName = interpretMangleName(fileName);
    displayFile(mangledFileName);
}

// 0x457514
static void op_displayraw(Program* program)
{
    char* fileName = programStackPopString(program);

    selectWindowID(program->windowId);

    char* mangledFileName = interpretMangleName(fileName);
    displayFileRaw(mangledFileName);
}

// 0x457574
static void interpretFadePaletteBK(unsigned char* oldPalette, unsigned char* newPalette, int a3, float duration, int shouldProcessBk)
{
    unsigned int time;
    unsigned int previousTime;
    unsigned int delta;
    int step;
    int steps;
    int index;
    unsigned char palette[256 * 3];

    time = get_time();
    previousTime = time;
    steps = (int)duration;
    step = 0;
    delta = 0;

    if (duration != 0.0) {
        while (step < steps) {
            if (delta != 0) {
                for (index = 0; index < 768; index++) {
                    palette[index] = oldPalette[index] - (oldPalette[index] - newPalette[index]) * step / steps;
                }

                setSystemPalette(palette);
                renderPresent();

                previousTime = time;
                step += delta;
            }

            if (shouldProcessBk) {
                process_bk();
            }

            time = get_time();
            delta = time - previousTime;
        }
    }

    setSystemPalette(newPalette);
    renderPresent();
}

// 0x457678
void interpretFadePalette(unsigned char* oldPalette, unsigned char* newPalette, int a3, float duration)
{
    interpretFadePaletteBK(oldPalette, newPalette, a3, duration, 1);
}

// 0x457688
int intlibGetFadeIn()
{
    return currentlyFadedIn;
}

// 0x457690
void interpretFadeOut(float duration)
{
    int cursorWasHidden;

    cursorWasHidden = mouse_hidden();
    mouse_hide();

    interpretFadePaletteBK(getSystemPalette(), blackPal, 64, duration, 1);

    if (!cursorWasHidden) {
        mouse_show();
    }
}

// 0x4576C8
void interpretFadeIn(float duration)
{
    interpretFadePaletteBK(blackPal, cmap, 64, duration, 1);
}

// 0x4576EC
void interpretFadeOutNoBK(float duration)
{
    int cursorWasHidden;

    cursorWasHidden = mouse_hidden();
    mouse_hide();

    interpretFadePaletteBK(getSystemPalette(), blackPal, 64, duration, 0);

    if (!cursorWasHidden) {
        mouse_show();
    }
}

// 0x457724
void interpretFadeInNoBK(float duration)
{
    interpretFadePaletteBK(blackPal, cmap, 64, duration, 0);
}

// 0x457748
static void op_fadein(Program* program)
{
    int data = programStackPopInteger(program);

    program->flags |= PROGRAM_FLAG_0x20;

    setSystemPalette(blackPal);

    // NOTE: Uninline.
    interpretFadeIn((float)data);

    currentlyFadedIn = true;

    program->flags &= ~PROGRAM_FLAG_0x20;
}

// 0x4577E0
static void op_fadeout(Program* program)
{
    int data = programStackPopInteger(program);

    program->flags |= PROGRAM_FLAG_0x20;

    // NOTE: Uninline.
    interpretFadeOut((float)data);

    currentlyFadedIn = false;

    program->flags &= ~PROGRAM_FLAG_0x20;
}

// 0x457884
int checkMovie(Program* program)
{
    if (dialogGetDialogDepth() > 0) {
        return 1;
    }

    return windowMoviePlaying();
}

// 0x457898
static void op_movieflags(Program* program)
{
    int data = programStackPopInteger(program);

    if (!windowSetMovieFlags(data)) {
        interpretError("Error setting movie flags\n");
    }
}

// 0x4578C0
static void op_playmovie(Program* program)
{
    // 0x59C6F8
    static char name[100];

    char* movieFileName = programStackPopString(program);

    strcpy(name, movieFileName);

    if (strrchr(name, '.') == NULL) {
        strcat(name, ".mve");
    }

    selectWindowID(program->windowId);

    program->flags |= PROGRAM_IS_WAITING;
    program->checkWaitFunc = checkMovie;

    char* mangledFileName = interpretMangleName(name);
    if (!windowPlayMovie(mangledFileName)) {
        interpretError("Error playing movie");
    }
}

// 0x45799C
static void op_playmovierect(Program* program)
{
    // 0x59C75C
    static char name[100];

    int height = programStackPopInteger(program);
    int width = programStackPopInteger(program);
    int y = programStackPopInteger(program);
    int x = programStackPopInteger(program);
    char* movieFileName = programStackPopString(program);

    strcpy(name, movieFileName);

    if (strrchr(name, '.') == NULL) {
        strcat(name, ".mve");
    }

    selectWindowID(program->windowId);

    program->checkWaitFunc = checkMovie;
    program->flags |= PROGRAM_IS_WAITING;

    char* mangledFileName = interpretMangleName(name);
    if (!windowPlayMovieRect(mangledFileName, x, y, width, height)) {
        interpretError("Error playing movie");
    }
}

// 0x457ADC
static void op_stopmovie(Program* program)
{
    windowStopMovie();
    program->flags |= PROGRAM_FLAG_0x40;
}

// 0x457AF0
static void op_deleteregion(Program* program)
{
    ProgramValue value = programStackPopValue(program);

    selectWindowID(program->windowId);

    const char* regionName = value.integerValue != -1 ? interpretGetString(program, value.opcode, value.integerValue) : NULL;
    windowDeleteRegion(regionName);
}

// 0x457B6C
static void op_activateregion(Program* program)
{
    int v1 = programStackPopInteger(program);
    char* regionName = programStackPopString(program);

    windowActivateRegion(regionName, v1);
}

// 0x457BAC
static void op_checkregion(Program* program)
{
    const char* regionName = programStackPopString(program);

    bool regionExists = windowCheckRegionExists(regionName);
    programStackPushInteger(program, regionExists);
}

// 0x457C0C
static void op_addregion(Program* program)
{
    int args = programStackPopInteger(program);

    if (args < 2) {
        interpretError("addregion call without enough points!");
    }

    selectWindowID(program->windowId);

    windowStartRegion(args / 2);

    while (args >= 2) {
        int y = programStackPopInteger(program);
        int x = programStackPopInteger(program);

        y = (y * windowGetYres() + 479) / 480;
        x = (x * windowGetXres() + 639) / 640;
        args -= 2;

        windowAddRegionPoint(x, y, true);
    }

    if (args == 0) {
        interpretError("Unnamed regions not allowed\n");
        windowEndRegion();
    } else {
        const char* regionName = programStackPopString(program);
        windowAddRegionName(regionName);
        windowEndRegion();
    }
}

// 0x457D90
static void op_addregionproc(Program* program)
{
    int v1 = programStackPopInteger(program);
    int v2 = programStackPopInteger(program);
    int v3 = programStackPopInteger(program);
    int v4 = programStackPopInteger(program);
    const char* regionName = programStackPopString(program);

    selectWindowID(program->windowId);

    if (!windowAddRegionProc(regionName, program, v4, v3, v2, v1)) {
        interpretError("Error setting procedures to region %s\n", regionName);
    }
}

// 0x457EDC
static void op_addregionrightproc(Program* program)
{
    int v1 = programStackPopInteger(program);
    int v2 = programStackPopInteger(program);
    const char* regionName = programStackPopString(program);
    selectWindowID(program->windowId);

    if (!windowAddRegionRightProc(regionName, program, v2, v1)) {
        interpretError("ErrorError setting right button procedures to region %s\n", regionName);
    }
}

// 0x457FB4
static void op_createwin(Program* program)
{
    int height = programStackPopInteger(program);
    int width = programStackPopInteger(program);
    int y = programStackPopInteger(program);
    int x = programStackPopInteger(program);
    char* windowName = programStackPopString(program);

    x = (x * windowGetXres() + 639) / 640;
    y = (y * windowGetYres() + 479) / 480;
    width = (width * windowGetXres() + 639) / 640;
    height = (height * windowGetYres() + 479) / 480;

    if (createWindow(windowName, x, y, width, height, colorTable[0], 0) == -1) {
        interpretError("Couldn't create window.");
    }
}

// 0x4580B4
static void op_resizewin(Program* program)
{
    int height = programStackPopInteger(program);
    int width = programStackPopInteger(program);
    int y = programStackPopInteger(program);
    int x = programStackPopInteger(program);
    char* windowName = programStackPopString(program);

    x = (x * windowGetXres() + 639) / 640;
    y = (y * windowGetYres() + 479) / 480;
    width = (width * windowGetXres() + 639) / 640;
    height = (height * windowGetYres() + 479) / 480;

    if (resizeWindow(windowName, x, y, width, height) == -1) {
        interpretError("Couldn't resize window.");
    }
}

// 0x4581A8
static void op_scalewin(Program* program)
{
    int height = programStackPopInteger(program);
    int width = programStackPopInteger(program);
    int y = programStackPopInteger(program);
    int x = programStackPopInteger(program);
    char* windowName = programStackPopString(program);

    x = (x * windowGetXres() + 639) / 640;
    y = (y * windowGetYres() + 479) / 480;
    width = (width * windowGetXres() + 639) / 640;
    height = (height * windowGetYres() + 479) / 480;

    if (scaleWindow(windowName, x, y, width, height) == -1) {
        interpretError("Couldn't scale window.");
    }
}

// 0x45829C
static void op_deletewin(Program* program)
{
    char* windowName = programStackPopString(program);

    if (!deleteWindow(windowName)) {
        interpretError("Error deleting window %s\n", windowName);
    }

    program->windowId = popWindow();
}

// 0x4582E8
static void op_saystart(Program* program)
{
    sayStartingPosition = 0;

    program->flags |= PROGRAM_FLAG_0x20;
    int rc = dialogStart(program);
    program->flags &= ~PROGRAM_FLAG_0x20;

    if (rc != 0) {
        interpretError("Error starting dialog.");
    }
}

// 0x458334
static void op_saystartpos(Program* program)
{
    sayStartingPosition = programStackPopInteger(program);

    program->flags |= PROGRAM_FLAG_0x20;
    int rc = dialogStart(program);
    program->flags &= ~PROGRAM_FLAG_0x20;

    if (rc != 0) {
        interpretError("Error starting dialog.");
    }
}

// 0x45838C
static void op_sayreplytitle(Program* program)
{
    ProgramValue value = programStackPopValue(program);

    char* string = NULL;
    if ((value.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
        string = interpretGetString(program, value.opcode, value.integerValue);
    }

    if (dialogTitle(string) != 0) {
        interpretError("Error setting title.");
    }
}

// 0x4583E0
static void op_saygotoreply(Program* program)
{
    ProgramValue value = programStackPopValue(program);

    char* string = NULL;
    if ((value.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
        string = interpretGetString(program, value.opcode, value.integerValue);
    }

    if (dialogGotoReply(string) != 0) {
        interpretError("Error during goto, couldn't find reply target %s", string);
    }
}

// 0x458438
static void op_sayoption(Program* program)
{
    program->flags |= PROGRAM_FLAG_0x20;

    ProgramValue v3 = programStackPopValue(program);
    ProgramValue v2 = programStackPopValue(program);

    const char* v1;
    if ((v2.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
        v1 = interpretGetString(program, v2.opcode, v2.integerValue);
    } else {
        v1 = NULL;
    }
    if ((v3.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
        const char* v2 = interpretGetString(program, v3.opcode, v3.integerValue);
        if (dialogOption(v1, v2) != 0) {
            program->flags &= ~PROGRAM_FLAG_0x20;
            interpretError("Error setting option.");
        }
    } else if ((v3.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_INT) {
        if (dialogOptionProc(v1, v3.integerValue) != 0) {
            program->flags &= ~PROGRAM_FLAG_0x20;
            interpretError("Error setting option.");
        }
    } else {
        interpretError("Invalid arg 2 to sayOption");
    }

    program->flags &= ~PROGRAM_FLAG_0x20;
}

// 0x458524
static void op_sayreply(Program* program)
{
    program->flags |= PROGRAM_FLAG_0x20;

    ProgramValue v3 = programStackPopValue(program);
    ProgramValue v4 = programStackPopValue(program);

    const char* v1;
    if ((v4.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
        v1 = interpretGetString(program, v4.opcode, v4.integerValue);
    } else {
        v1 = NULL;
    }

    const char* v2;
    if ((v3.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
        v2 = interpretGetString(program, v3.opcode, v3.integerValue);
    } else {
        v2 = NULL;
    }

    if (dialogReply(v1, v2) != 0) {
        program->flags &= ~PROGRAM_FLAG_0x20;
        interpretError("Error setting option.");
    }

    program->flags &= ~PROGRAM_FLAG_0x20;
}

// 0x4585DC
static int checkDialog(Program* program)
{
    program->flags |= PROGRAM_FLAG_0x40;
    return dialogGetDialogDepth() != -1;
}

// 0x4585F4
static void op_sayend(Program* program)
{
    program->flags |= PROGRAM_FLAG_0x20;
    int rc = dialogGo(sayStartingPosition);
    program->flags &= ~PROGRAM_FLAG_0x20;

    if (rc == -2) {
        program->checkWaitFunc = checkDialog;
        program->flags |= PROGRAM_IS_WAITING;
    }
}

// 0x45863C
static void op_saygetlastpos(Program* program)
{
    int value = dialogGetExitPoint();
    programStackPushInteger(program, value);
}

// 0x458660
static void op_sayquit(Program* program)
{
    if (dialogQuit() != 0) {
        interpretError("Error quitting option.");
    }
}

// 0x458678
int getTimeOut()
{
    return TimeOut;
}

// 0x458680
void setTimeOut(int value)
{
    TimeOut = value;
}

// 0x458688
static void op_saymessagetimeout(Program* program)
{
    ProgramValue value = programStackPopValue(program);

    // TODO: What the hell is this?
    if ((value.opcode & VALUE_TYPE_MASK) == 0x4000) {
        interpretError("sayMsgTimeout:  invalid var type passed.");
    }

    TimeOut = value.integerValue;
}

// 0x4586C4
static void op_saymessage(Program* program)
{
    program->flags |= PROGRAM_FLAG_0x20;

    ProgramValue v3 = programStackPopValue(program);
    ProgramValue v4 = programStackPopValue(program);

    const char* v1;
    if ((v4.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
        v1 = interpretGetString(program, v4.opcode, v4.integerValue);
    } else {
        v1 = NULL;
    }

    const char* v2;
    if ((v3.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
        v2 = interpretGetString(program, v3.opcode, v3.integerValue);
    } else {
        v2 = NULL;
    }

    if (dialogMessage(v1, v2, TimeOut) != 0) {
        program->flags &= ~PROGRAM_FLAG_0x20;
        interpretError("Error setting option.");
    }

    program->flags &= ~PROGRAM_FLAG_0x20;
}

// 0x458780
static void op_gotoxy(Program* program)
{
    int y = programStackPopInteger(program);
    int x = programStackPopInteger(program);

    selectWindowID(program->windowId);

    windowGotoXY(x, y);
}

// 0x4587FC
static void op_addbuttonflag(Program* program)
{
    int flag = programStackPopInteger(program);
    const char* buttonName = programStackPopString(program);
    if (!windowSetButtonFlag(buttonName, flag)) {
        // NOTE: Original code calls interpretGetString one more time with the
        // same params.
        interpretError("Error setting flag on button %s", buttonName);
    }
}

// 0x45889C
static void op_addregionflag(Program* program)
{
    int flag = programStackPopInteger(program);
    const char* regionName = programStackPopString(program);
    if (!windowSetRegionFlag(regionName, flag)) {
        // NOTE: Original code calls interpretGetString one more time with the
        // same params.
        interpretError("Error setting flag on region %s", regionName);
    }
}

// 0x45893C
static void op_addbutton(Program* program)
{
    int height = programStackPopInteger(program);
    int width = programStackPopInteger(program);
    int y = programStackPopInteger(program);
    int x = programStackPopInteger(program);
    char* buttonName = programStackPopString(program);

    selectWindowID(program->windowId);

    height = (height * windowGetYres() + 479) / 480;
    width = (width * windowGetXres() + 639) / 640;
    y = (y * windowGetYres() + 479) / 480;
    x = (x * windowGetXres() + 639) / 640;

    windowAddButton(buttonName, x, y, width, height, 0);
}

// 0x458ACC
static void op_addbuttontext(Program* program)
{
    const char* text = programStackPopString(program);
    const char* buttonName = programStackPopString(program);

    if (!windowAddButtonText(buttonName, text)) {
        interpretError("Error setting text to button %s\n", buttonName);
    }
}

// 0x458B90
static void op_addbuttongfx(Program* program)
{
    ProgramValue v1 = programStackPopValue(program);
    ProgramValue v2 = programStackPopValue(program);
    ProgramValue v3 = programStackPopValue(program);
    char* buttonName = programStackPopString(program);

    if (((v3.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING || ((v3.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_INT && v3.integerValue == 0))
        || ((v2.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING || ((v2.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_INT && v2.integerValue == 0))
        || ((v1.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING || ((v1.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_INT && v1.integerValue == 0))) {
        char* pressedFileName = interpretMangleName(interpretGetString(program, v3.opcode, v3.integerValue));
        char* normalFileName = interpretMangleName(interpretGetString(program, v2.opcode, v2.integerValue));
        char* hoverFileName = interpretMangleName(interpretGetString(program, v1.opcode, v1.integerValue));

        selectWindowID(program->windowId);

        if (!windowAddButtonGfx(buttonName, pressedFileName, normalFileName, hoverFileName)) {
            interpretError("Error setting graphics to button %s\n", buttonName);
        }
    } else {
        interpretError("Invalid filename given to addbuttongfx");
    }
}

// 0x458D28
static void op_addbuttonproc(Program* program)
{
    int v1 = programStackPopInteger(program);
    int v2 = programStackPopInteger(program);
    int v3 = programStackPopInteger(program);
    int v4 = programStackPopInteger(program);
    const char* buttonName = programStackPopString(program);
    selectWindowID(program->windowId);

    if (!windowAddButtonProc(buttonName, program, v4, v3, v2, v1)) {
        interpretError("Error setting procedures to button %s\n", buttonName);
    }
}

// 0x458E74
static void op_addbuttonrightproc(Program* program)
{
    int v1 = programStackPopInteger(program);
    int v2 = programStackPopInteger(program);
    const char* regionName = programStackPopString(program);
    selectWindowID(program->windowId);

    if (!windowAddRegionRightProc(regionName, program, v2, v1)) {
        interpretError("Error setting right button procedures to button %s\n", regionName);
    }
}

// 0x458F4C
static void op_showwin(Program* program)
{
    selectWindowID(program->windowId);
    windowDraw();
}

// 0x458F5C
static void op_deletebutton(Program* program)
{
    ProgramValue value = programStackPopValue(program);

    switch (value.opcode & VALUE_TYPE_MASK) {
    case VALUE_TYPE_STRING:
        break;
    case VALUE_TYPE_INT:
        if (value.integerValue == -1) {
            break;
        }
        // FALLTHROUGH
    default:
        interpretError("Invalid type given to delete button");
    }

    selectWindowID(program->windowId);

    if ((value.opcode & 0xF7FF) == VALUE_TYPE_INT) {
        if (windowDeleteButton(NULL)) {
            return;
        }
    } else {
        const char* buttonName = interpretGetString(program, value.opcode, value.integerValue);
        if (windowDeleteButton(buttonName)) {
            return;
        }
    }

    interpretError("Error deleting button");
}

// 0x458FF8
static void op_fillwin(Program* program)
{
    ProgramValue b = programStackPopValue(program);
    ProgramValue g = programStackPopValue(program);
    ProgramValue r = programStackPopValue(program);

    if ((r.opcode & VALUE_TYPE_MASK) != VALUE_TYPE_FLOAT) {
        if ((r.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_INT) {
            if (r.integerValue == 1) {
                r.floatValue = 1.0;
            } else if (r.integerValue != 0) {
                interpretError("Invalid red value given to fillwin");
            }
        }
    }

    if ((g.opcode & VALUE_TYPE_MASK) != VALUE_TYPE_FLOAT) {
        if ((g.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_INT) {
            if (g.integerValue == 1) {
                g.floatValue = 1.0;
            } else if (g.integerValue != 0) {
                interpretError("Invalid green value given to fillwin");
            }
        }
    }

    if ((b.opcode & VALUE_TYPE_MASK) != VALUE_TYPE_FLOAT) {
        if ((b.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_INT) {
            if (b.integerValue == 1) {
                b.floatValue = 1.0;
            } else if (b.integerValue != 0) {
                interpretError("Invalid blue value given to fillwin");
            }
        }
    }

    selectWindowID(program->windowId);

    windowFill(r.floatValue, g.floatValue, b.floatValue);
}

// 0x459108
static void op_fillrect(Program* program)
{
    ProgramValue b = programStackPopValue(program);
    ProgramValue g = programStackPopValue(program);
    ProgramValue r = programStackPopValue(program);
    int height = programStackPopInteger(program);
    int width = programStackPopInteger(program);
    int y = programStackPopInteger(program);
    int x = programStackPopInteger(program);

    if ((r.opcode & VALUE_TYPE_MASK) != VALUE_TYPE_FLOAT) {
        if ((r.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_INT) {
            if (r.integerValue == 1) {
                r.floatValue = 1.0;
            } else if (r.integerValue != 0) {
                interpretError("Invalid red value given to fillrect");
            }
        }
    }

    if ((g.opcode & VALUE_TYPE_MASK) != VALUE_TYPE_FLOAT) {
        if ((g.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_INT) {
            if (g.integerValue == 1) {
                g.floatValue = 1.0;
            } else if (g.integerValue != 0) {
                interpretError("Invalid green value given to fillrect");
            }
        }
    }

    if ((b.opcode & VALUE_TYPE_MASK) != VALUE_TYPE_FLOAT) {
        if ((b.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_INT) {
            if (b.integerValue == 1) {
                b.floatValue = 1.0;
            } else if (b.integerValue != 0) {
                interpretError("Invalid blue value given to fillrect");
            }
        }
    }

    selectWindowID(program->windowId);

    windowFillRect(x, y, width, height, r.floatValue, g.floatValue, b.floatValue);
}

// 0x459300
static void op_hidemouse(Program* program)
{
    mouse_hide();
}

// 0x459308
static void op_showmouse(Program* program)
{
    mouse_show();
}

// 0x459310
static void op_mouseshape(Program* program)
{
    int v1 = programStackPopInteger(program);
    int v2 = programStackPopInteger(program);
    char* fileName = programStackPopString(program);

    if (!mouseSetMouseShape(fileName, v2, v1)) {
        interpretError("Error loading mouse shape.");
    }
}

// 0x4593D8
static void op_setglobalmousefunc(Program* Program)
{
    interpretError("setglobalmousefunc not defined");
}

// 0x4593E8
static void op_displaygfx(Program* program)
{
    int height = programStackPopInteger(program);
    int width = programStackPopInteger(program);
    int y = programStackPopInteger(program);
    int x = programStackPopInteger(program);
    char* fileName = programStackPopString(program);

    char* mangledFileName = interpretMangleName(fileName);
    windowDisplay(mangledFileName, x, y, width, height);
}

// 0x459464
static void op_loadpalettetable(Program* program)
{
    char* path = programStackPopString(program);
    if (!loadColorTable(path)) {
        interpretError(colorError());
    }
}

// 0x4594C0
static void op_addNamedEvent(Program* program)
{
    int proc = programStackPopInteger(program);
    const char* name = programStackPopString(program);
    nevs_addevent(name, program, proc, NEVS_TYPE_EVENT);
}

// 0x459524
static void op_addNamedHandler(Program* program)
{
    int proc = programStackPopInteger(program);
    const char* name = programStackPopString(program);
    nevs_addevent(name, program, proc, NEVS_TYPE_HANDLER);
}

// 0x45958C
static void op_clearNamed(Program* program)
{
    char* string = programStackPopString(program);
    nevs_clearevent(string);
}

// 0x4595D8
static void op_signalNamed(Program* program)
{
    char* str = programStackPopString(program);
    nevs_signal(str);
}

// 0x459624
static void op_addkey(Program* program)
{
    int proc = programStackPopInteger(program);
    int key = programStackPopInteger(program);

    if (key == -1) {
        anyKeyOffset = proc;
        anyKeyProg = program;
    } else {
        if (key > INT_LIB_KEY_HANDLERS_CAPACITY - 1) {
            interpretError("Key out of range");
        }

        inputProc[key].program = program;
        inputProc[key].proc = proc;
    }
}

// 0x4596C4
static void op_deletekey(Program* program)
{
    int key = programStackPopInteger(program);

    if (key == -1) {
        anyKeyOffset = 0;
        anyKeyProg = NULL;
    } else {
        if (key > INT_LIB_KEY_HANDLERS_CAPACITY - 1) {
            interpretError("Key out of range");
        }

        inputProc[key].program = NULL;
        inputProc[key].proc = 0;
    }
}

// 0x459738
static void op_refreshmouse(Program* program)
{
    int data = programStackPopInteger(program);

    if (!windowRefreshRegions()) {
        executeProc(program, data);
    }
}

// 0x459784
static void op_setfont(Program* program)
{
    int data = programStackPopInteger(program);

    if (!windowSetFont(data)) {
        interpretError("Error setting font");
    }
}

// 0x4597D0
static void op_settextflags(Program* program)
{
    int data = programStackPopInteger(program);

    if (!windowSetTextFlags(data)) {
        interpretError("Error setting text flags");
    }
}

// 0x45981C
static void op_settextcolor(Program* program)
{
    ProgramValue value[3];

    // NOTE: Original code does not use loops.
    for (int arg = 0; arg < 3; arg++) {
        value[arg] = programStackPopValue(program);
    }

    for (int arg = 0; arg < 3; arg++) {
        if ((value[arg].opcode & VALUE_TYPE_MASK) != VALUE_TYPE_FLOAT
            && (value[arg].opcode & VALUE_TYPE_MASK) == VALUE_TYPE_INT
            && value[arg].integerValue != 0) {
            interpretError("Invalid type given to settextcolor");
        }
    }

    float r = value[2].floatValue;
    float g = value[1].floatValue;
    float b = value[0].floatValue;

    if (!windowSetTextColor(r, g, b)) {
        interpretError("Error setting text color");
    }
}

// 0x459920
static void op_sayoptioncolor(Program* program)
{
    ProgramValue value[3];

    // NOTE: Original code does not use loops.
    for (int arg = 0; arg < 3; arg++) {
        value[arg] = programStackPopValue(program);
    }

    for (int arg = 0; arg < 3; arg++) {
        if ((value[arg].opcode & VALUE_TYPE_MASK) != VALUE_TYPE_FLOAT
            && (value[arg].opcode & VALUE_TYPE_MASK) == VALUE_TYPE_INT
            && value[arg].integerValue != 0) {
            interpretError("Invalid type given to sayoptioncolor");
        }
    }

    float r = value[2].floatValue;
    float g = value[1].floatValue;
    float b = value[0].floatValue;

    if (dialogSetOptionColor(r, g, b)) {
        interpretError("Error setting option color");
    }
}

// 0x459A24
static void op_sayreplycolor(Program* program)
{
    ProgramValue value[3];

    // NOTE: Original code does not use loops.
    for (int arg = 0; arg < 3; arg++) {
        value[arg] = programStackPopValue(program);
    }

    for (int arg = 0; arg < 3; arg++) {
        if ((value[arg].opcode & VALUE_TYPE_MASK) != VALUE_TYPE_FLOAT
            && (value[arg].opcode & VALUE_TYPE_MASK) == VALUE_TYPE_INT
            && value[arg].integerValue != 0) {
            interpretError("Invalid type given to sayreplycolor");
        }
    }

    float r = value[2].floatValue;
    float g = value[1].floatValue;
    float b = value[0].floatValue;

    if (dialogSetReplyColor(r, g, b) != 0) {
        interpretError("Error setting reply color");
    }
}

// 0x459B28
static void op_sethighlightcolor(Program* program)
{
    ProgramValue value[3];

    // NOTE: Original code does not use loops.
    for (int arg = 0; arg < 3; arg++) {
        value[arg] = programStackPopValue(program);
    }

    for (int arg = 0; arg < 3; arg++) {
        if ((value[arg].opcode & VALUE_TYPE_MASK) != VALUE_TYPE_FLOAT
            && (value[arg].opcode & VALUE_TYPE_MASK) == VALUE_TYPE_INT
            && value[arg].integerValue != 0) {
            interpretError("Invalid type given to sayreplycolor");
        }
    }

    float r = value[2].floatValue;
    float g = value[1].floatValue;
    float b = value[0].floatValue;

    if (!windowSetHighlightColor(r, g, b)) {
        interpretError("Error setting text highlight color");
    }
}

// 0x459C2C
static void op_sayreplywindow(Program* program)
{
    ProgramValue v2 = programStackPopValue(program);
    int height = programStackPopInteger(program);
    int width = programStackPopInteger(program);
    int y = programStackPopInteger(program);
    int x = programStackPopInteger(program);

    char* v1;
    if ((v2.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
        v1 = interpretGetString(program, v2.opcode, v2.integerValue);
        v1 = interpretMangleName(v1);
        v1 = mystrdup(v1, __FILE__, __LINE__); // "..\\int\\INTLIB.C", 1510
    } else if ((v2.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_INT && v2.integerValue == 0) {
        v1 = NULL;
    } else {
        interpretError("Invalid arg 5 given to sayreplywindow");
    }

    if (dialogSetReplyWindow(x, y, width, height, v1) != 0) {
        interpretError("Error setting reply window");
    }
}

// 0x459D08
static void op_sayreplyflags(Program* program)
{
    int data = programStackPopInteger(program);

    if (!dialogSetReplyFlags(data)) {
        interpretError("Error setting reply flags");
    }
}

// 0x459D54
static void op_sayoptionflags(Program* program)
{
    int data = programStackPopInteger(program);

    if (!dialogSetOptionFlags(data)) {
        interpretError("Error setting option flags");
    }
}

// 0x459DA0
static void op_sayoptionwindow(Program* program)
{
    ProgramValue v2 = programStackPopValue(program);
    int height = programStackPopInteger(program);
    int width = programStackPopInteger(program);
    int y = programStackPopInteger(program);
    int x = programStackPopInteger(program);

    char* v1;
    if ((v2.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
        v1 = interpretGetString(program, v2.opcode, v2.integerValue);
        v1 = interpretMangleName(v1);
        v1 = mystrdup(v1, __FILE__, __LINE__); // "..\\int\\INTLIB.C", 1556
    } else if ((v2.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_INT && v2.integerValue == 0) {
        v1 = NULL;
    } else {
        interpretError("Invalid arg 5 given to sayoptionwindow");
    }

    if (dialogSetOptionWindow(x, y, width, height, v1) != 0) {
        interpretError("Error setting option window");
    }
}

// 0x459E7C
static void op_sayborder(Program* program)
{
    int y = programStackPopInteger(program);
    int x = programStackPopInteger(program);

    if (dialogSetBorder(x, y) != 0) {
        interpretError("Error setting dialog border");
    }
}

// 0x459F00
static void op_sayscrollup(Program* program)
{
    ProgramValue v6 = programStackPopValue(program);
    ProgramValue v7 = programStackPopValue(program);
    ProgramValue v8 = programStackPopValue(program);
    ProgramValue v9 = programStackPopValue(program);
    int y = programStackPopInteger(program);
    int x = programStackPopInteger(program);

    char* v1 = NULL;
    char* v2 = NULL;
    char* v3 = NULL;
    char* v4 = NULL;
    int v5 = 0;

    if ((v6.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_INT) {
        if (v6.integerValue != -1 && v6.integerValue != 0) {
            interpretError("Invalid arg 4 given to sayscrollup");
        }

        if (v6.integerValue == -1) {
            v5 = 1;
        }
    } else {
        if ((v6.opcode & VALUE_TYPE_MASK) != VALUE_TYPE_STRING) {
            interpretError("Invalid arg 4 given to sayscrollup");
        }
    }

    if ((v7.opcode & VALUE_TYPE_MASK) != VALUE_TYPE_STRING && (v7.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_INT && v7.integerValue != 0) {
        interpretError("Invalid arg 3 given to sayscrollup");
    }

    if ((v8.opcode & VALUE_TYPE_MASK) != VALUE_TYPE_STRING && (v8.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_INT && v8.integerValue != 0) {
        interpretError("Invalid arg 2 given to sayscrollup");
    }

    if ((v9.opcode & VALUE_TYPE_MASK) != VALUE_TYPE_STRING && (v9.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_INT && v9.integerValue != 0) {
        interpretError("Invalid arg 1 given to sayscrollup");
    }

    if ((v9.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
        v1 = interpretGetString(program, v9.opcode, v9.integerValue);
        v1 = interpretMangleName(v1);
        v1 = mystrdup(v1, __FILE__, __LINE__); // "..\\int\\INTLIB.C", 1611
    }

    if ((v8.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
        v2 = interpretGetString(program, v8.opcode, v8.integerValue);
        v2 = interpretMangleName(v2);
        v2 = mystrdup(v2, __FILE__, __LINE__); // "..\\int\\INTLIB.C", 1613
    }

    if ((v7.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
        v3 = interpretGetString(program, v7.opcode, v7.integerValue);
        v3 = interpretMangleName(v3);
        v3 = mystrdup(v3, __FILE__, __LINE__); // "..\\int\\INTLIB.C", 1615
    }

    if ((v6.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
        v4 = interpretGetString(program, v6.opcode, v6.integerValue);
        v4 = interpretMangleName(v4);
        v4 = mystrdup(v4, __FILE__, __LINE__); // "..\\int\\INTLIB.C", 1617
    }

    if (dialogSetScrollUp(x, y, v1, v2, v3, v4, v5) != 0) {
        interpretError("Error setting scroll up");
    }
}

// 0x45A1A0
static void op_sayscrolldown(Program* program)
{
    ProgramValue v6 = programStackPopValue(program);
    ProgramValue v7 = programStackPopValue(program);
    ProgramValue v8 = programStackPopValue(program);
    ProgramValue v9 = programStackPopValue(program);
    int y = programStackPopInteger(program);
    int x = programStackPopInteger(program);

    char* v1 = NULL;
    char* v2 = NULL;
    char* v3 = NULL;
    char* v4 = NULL;
    int v5 = 0;

    if ((v6.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_INT) {
        if (v6.integerValue != -1 && v6.integerValue != 0) {
            // FIXME: Wrong function name, should be sayscrolldown.
            interpretError("Invalid arg 4 given to sayscrollup");
        }

        if (v6.integerValue == -1) {
            v5 = 1;
        }
    } else {
        if ((v6.opcode & VALUE_TYPE_MASK) != VALUE_TYPE_STRING) {
            // FIXME: Wrong function name, should be sayscrolldown.
            interpretError("Invalid arg 4 given to sayscrollup");
        }
    }

    if ((v7.opcode & VALUE_TYPE_MASK) != VALUE_TYPE_STRING && (v7.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_INT && v7.integerValue != 0) {
        interpretError("Invalid arg 3 given to sayscrolldown");
    }

    if ((v8.opcode & VALUE_TYPE_MASK) != VALUE_TYPE_STRING && (v8.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_INT && v8.integerValue != 0) {
        interpretError("Invalid arg 2 given to sayscrolldown");
    }

    if ((v9.opcode & VALUE_TYPE_MASK) != VALUE_TYPE_STRING && (v9.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_INT && v9.integerValue != 0) {
        interpretError("Invalid arg 1 given to sayscrolldown");
    }

    if ((v9.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
        v1 = interpretGetString(program, v9.opcode, v9.integerValue);
        v1 = interpretMangleName(v1);
        v1 = mystrdup(v1, __FILE__, __LINE__); // "..\\int\\INTLIB.C", 1652
    }

    if ((v8.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
        v2 = interpretGetString(program, v8.opcode, v8.integerValue);
        v2 = interpretMangleName(v2);
        v2 = mystrdup(v2, __FILE__, __LINE__); // "..\\int\\INTLIB.C", 1654
    }

    if ((v7.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
        v3 = interpretGetString(program, v7.opcode, v7.integerValue);
        v3 = interpretMangleName(v3);
        v3 = mystrdup(v3, __FILE__, __LINE__); // "..\\int\\INTLIB.C", 1656
    }

    if ((v6.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
        v4 = interpretGetString(program, v6.opcode, v6.integerValue);
        v4 = interpretMangleName(v4);
        v4 = mystrdup(v4, __FILE__, __LINE__); // "..\\int\\INTLIB.C", 1658
    }

    if (dialogSetScrollDown(x, y, v1, v2, v3, v4, v5) != 0) {
        interpretError("Error setting scroll down");
    }
}

// 0x45A440
static void op_saysetspacing(Program* program)
{
    int data = programStackPopInteger(program);

    if (dialogSetSpacing(data) != 0) {
        interpretError("Error setting option spacing");
    }
}

// 0x45A48C
static void op_sayrestart(Program* program)
{
    if (dialogRestart() != 0) {
        interpretError("Error restarting option");
    }
}

// 0x45A4A4
static void soundCallbackInterpret(void* userData, int a2)
{
    if (a2 == 1) {
        Sound** sound = (Sound**)userData;
        *sound = NULL;
    }
}

// 0x45A4B0
static int soundDeleteInterpret(int value)
{
    if (value == -1) {
        return 1;
    }

    if ((value & 0xA0000000) == 0) {
        return 0;
    }

    int index = value & ~0xA0000000;
    Sound* sound = interpretSounds[index];
    if (sound == NULL) {
        return 0;
    }

    if (soundPlaying(sound)) {
        soundStop(sound);
    }

    soundDelete(sound);

    interpretSounds[index] = NULL;

    return 1;
}

// NOTE: Inlined.
//
// 0x45A528
void soundCloseInterpret()
{
    int index;

    for (index = 0; index < INT_LIB_SOUNDS_CAPACITY; index++) {
        if (interpretSounds[index] != NULL) {
            soundDeleteInterpret(index | 0xA0000000);
        }
    }
}

// 0x45A550
int soundStartInterpret(char* fileName, int mode)
{
    int v3 = 1;
    int v5 = 0;

    if (mode & 0x01) {
        // looping
        v5 |= 0x20;
    } else {
        v3 = 5;
    }

    if (mode & 0x02) {
        v5 |= 0x08;
    } else {
        v5 |= 0x10;
    }

    if (mode & 0x0100) {
        // memory
        v3 &= ~0x03;
        v3 |= 0x01;
    }

    if (mode & 0x0200) {
        // streamed
        v3 &= ~0x03;
        v3 |= 0x02;
    }

    int index;
    for (index = 0; index < INT_LIB_SOUNDS_CAPACITY; index++) {
        if (interpretSounds[index] == NULL) {
            break;
        }
    }

    if (index == INT_LIB_SOUNDS_CAPACITY) {
        return -1;
    }

    Sound* sound = interpretSounds[index] = soundAllocate(v3, v5);
    if (sound == NULL) {
        return -1;
    }

    soundSetCallback(sound, soundCallbackInterpret, &(interpretSounds[index]));

    if (mode & 0x01) {
        soundLoop(sound, 0xFFFF);
    }

    if (mode & 0x1000) {
        // mono
        soundSetChannel(sound, 2);
    }

    if (mode & 0x2000) {
        // stereo
        soundSetChannel(sound, 3);
    }

    int rc = soundLoad(sound, fileName);
    if (rc != SOUND_NO_ERROR) {
        goto err;
    }

    rc = soundPlay(sound);

    // TODO: Maybe wrong.
    switch (rc) {
    case SOUND_NO_DEVICE:
        debug_printf("soundPlay error: %s\n", "SOUND_NO_DEVICE");
        goto err;
    case SOUND_NOT_INITIALIZED:
        debug_printf("soundPlay error: %s\n", "SOUND_NOT_INITIALIZED");
        goto err;
    case SOUND_NO_SOUND:
        debug_printf("soundPlay error: %s\n", "SOUND_NO_SOUND");
        goto err;
    case SOUND_FUNCTION_NOT_SUPPORTED:
        debug_printf("soundPlay error: %s\n", "SOUND_FUNC_NOT_SUPPORTED");
        goto err;
    case SOUND_NO_BUFFERS_AVAILABLE:
        debug_printf("soundPlay error: %s\n", "SOUND_NO_BUFFERS_AVAILABLE");
        goto err;
    case SOUND_FILE_NOT_FOUND:
        debug_printf("soundPlay error: %s\n", "SOUND_FILE_NOT_FOUND");
        goto err;
    case SOUND_ALREADY_PLAYING:
        debug_printf("soundPlay error: %s\n", "SOUND_ALREADY_PLAYING");
        goto err;
    case SOUND_NOT_PLAYING:
        debug_printf("soundPlay error: %s\n", "SOUND_NOT_PLAYING");
        goto err;
    case SOUND_ALREADY_PAUSED:
        debug_printf("soundPlay error: %s\n", "SOUND_ALREADY_PAUSED");
        goto err;
    case SOUND_NOT_PAUSED:
        debug_printf("soundPlay error: %s\n", "SOUND_NOT_PAUSED");
        goto err;
    case SOUND_INVALID_HANDLE:
        debug_printf("soundPlay error: %s\n", "SOUND_INVALID_HANDLE");
        goto err;
    case SOUND_NO_MEMORY_AVAILABLE:
        debug_printf("soundPlay error: %s\n", "SOUND_NO_MEMORY");
        goto err;
    case SOUND_UNKNOWN_ERROR:
        debug_printf("soundPlay error: %s\n", "SOUND_ERROR");
        goto err;
    }

    return index | 0xA0000000;

err:

    soundDelete(sound);
    interpretSounds[index] = NULL;
    return -1;
}

// 0x45A99C
static int soundPauseInterpret(int value)
{
    if (value == -1) {
        return 1;
    }

    if ((value & 0xA0000000) == 0) {
        return 0;
    }

    int index = value & ~0xA0000000;
    Sound* sound = interpretSounds[index];
    if (sound == NULL) {
        return 0;
    }

    int rc;
    if (soundType(sound, 0x01)) {
        rc = soundStop(sound);
    } else {
        rc = soundPause(sound);
    }
    return rc == SOUND_NO_ERROR;
}

// 0x45AA08
static int soundRewindInterpret(int value)
{
    if (value == -1) {
        return 1;
    }

    if ((value & 0xA0000000) == 0) {
        return 0;
    }

    int index = value & ~0xA0000000;
    Sound* sound = interpretSounds[index];
    if (sound == NULL) {
        return 0;
    }

    if (!soundPlaying(sound)) {
        return 1;
    }

    soundStop(sound);

    return soundPlay(sound) == SOUND_NO_ERROR;
}

// 0x45AA6C
static int soundUnpauseInterpret(int value)
{
    if (value == -1) {
        return 1;
    }

    if ((value & 0xA0000000) == 0) {
        return 0;
    }

    int index = value & ~0xA0000000;
    Sound* sound = interpretSounds[index];
    if (sound == NULL) {
        return 0;
    }

    int rc;
    if (soundType(sound, 0x01)) {
        rc = soundPlay(sound);
    } else {
        rc = soundUnpause(sound);
    }
    return rc == SOUND_NO_ERROR;
}

// 0x45AAD8
static void op_soundplay(Program* program)
{
    int flags = programStackPopInteger(program);
    char* fileName = programStackPopString(program);

    char* mangledFileName = interpretMangleName(fileName);
    int rc = soundStartInterpret(mangledFileName, flags);

    programStackPushInteger(program, rc);
}

// 0x45AB6C
static void op_soundpause(Program* program)
{
    int data = programStackPopInteger(program);
    soundPauseInterpret(data);
}

// 0x45ABA8
static void op_soundresume(Program* program)
{
    int data = programStackPopInteger(program);
    soundUnpauseInterpret(data);
}

// 0x45ABE4
static void op_soundstop(Program* program)
{
    int data = programStackPopInteger(program);
    soundPauseInterpret(data);
}

// 0x45AC20
static void op_soundrewind(Program* program)
{
    int data = programStackPopInteger(program);
    soundRewindInterpret(data);
}

// 0x45AC5C
static void op_sounddelete(Program* program)
{
    int data = programStackPopInteger(program);
    soundDeleteInterpret(data);
}

// 0x45AC98
static void op_setoneoptpause(Program* program)
{
    int data = programStackPopInteger(program);

    if (data) {
        if ((dialogGetMediaFlag() & 8) == 0) {
            return;
        }
    } else {
        if ((dialogGetMediaFlag() & 8) != 0) {
            return;
        }
    }

    dialogToggleMediaFlag(8);
}

// 0x45ACF0
void updateIntLib()
{
    nevs_update();
    updateIntExtra();
}

// 0x45ACFC
void intlibClose()
{
    dialogClose();
    intExtraClose();

    // NOTE: Uninline.
    soundCloseInterpret();

    nevs_close();

    if (callbacks != NULL) {
        myfree(callbacks, __FILE__, __LINE__); // "..\\int\\INTLIB.C", 1976
        callbacks = NULL;
        numCallbacks = 0;
    }
}

// 0x45AD60
static bool intLibDoInput(int key)
{
    if (key < 0 || key >= INT_LIB_KEY_HANDLERS_CAPACITY) {
        return false;
    }

    if (anyKeyProg != NULL) {
        if (anyKeyOffset != 0) {
            executeProc(anyKeyProg, anyKeyOffset);
        }
        return true;
    }

    IntLibKeyHandlerEntry* entry = &(inputProc[key]);
    if (entry->program == NULL) {
        return false;
    }

    if (entry->proc != 0) {
        executeProc(entry->program, entry->proc);
    }

    return true;
}

// 0x45ADCC
void initIntlib()
{
    windowAddInputFunc(intLibDoInput);

    interpretAddFunc(0x806A, op_fillwin3x3);
    interpretAddFunc(0x808C, op_deletebutton);
    interpretAddFunc(0x8086, op_addbutton);
    interpretAddFunc(0x8088, op_addbuttonflag);
    interpretAddFunc(0x8087, op_addbuttontext);
    interpretAddFunc(0x8089, op_addbuttongfx);
    interpretAddFunc(0x808A, op_addbuttonproc);
    interpretAddFunc(0x808B, op_addbuttonrightproc);
    interpretAddFunc(0x8067, op_showwin);
    interpretAddFunc(0x8068, op_fillwin);
    interpretAddFunc(0x8069, op_fillrect);
    interpretAddFunc(0x8072, op_print);
    interpretAddFunc(0x8073, op_format);
    interpretAddFunc(0x8074, op_printrect);
    interpretAddFunc(0x8075, op_setfont);
    interpretAddFunc(0x8076, op_settextflags);
    interpretAddFunc(0x8077, op_settextcolor);
    interpretAddFunc(0x8078, op_sethighlightcolor);
    interpretAddFunc(0x8064, op_selectwin);
    interpretAddFunc(0x806B, op_display);
    interpretAddFunc(0x806D, op_displayraw);
    interpretAddFunc(0x806C, op_displaygfx);
    interpretAddFunc(0x806F, op_fadein);
    interpretAddFunc(0x8070, op_fadeout);
    interpretAddFunc(0x807A, op_playmovie);
    interpretAddFunc(0x807B, op_movieflags);
    interpretAddFunc(0x807C, op_playmovierect);
    interpretAddFunc(0x8079, op_stopmovie);
    interpretAddFunc(0x807F, op_addregion);
    interpretAddFunc(0x8080, op_addregionflag);
    interpretAddFunc(0x8081, op_addregionproc);
    interpretAddFunc(0x8082, op_addregionrightproc);
    interpretAddFunc(0x8083, op_deleteregion);
    interpretAddFunc(0x8084, op_activateregion);
    interpretAddFunc(0x8085, op_checkregion);
    interpretAddFunc(0x8062, op_createwin);
    interpretAddFunc(0x8063, op_deletewin);
    interpretAddFunc(0x8065, op_resizewin);
    interpretAddFunc(0x8066, op_scalewin);
    interpretAddFunc(0x804E, op_saystart);
    interpretAddFunc(0x804F, op_saystartpos);
    interpretAddFunc(0x8050, op_sayreplytitle);
    interpretAddFunc(0x8051, op_saygotoreply);
    interpretAddFunc(0x8053, op_sayoption);
    interpretAddFunc(0x8052, op_sayreply);
    interpretAddFunc(0x804D, op_sayend);
    interpretAddFunc(0x804C, op_sayquit);
    interpretAddFunc(0x8054, op_saymessage);
    interpretAddFunc(0x8055, op_sayreplywindow);
    interpretAddFunc(0x8056, op_sayoptionwindow);
    interpretAddFunc(0x805F, op_sayreplyflags);
    interpretAddFunc(0x8060, op_sayoptionflags);
    interpretAddFunc(0x8057, op_sayborder);
    interpretAddFunc(0x8058, op_sayscrollup);
    interpretAddFunc(0x8059, op_sayscrolldown);
    interpretAddFunc(0x805A, op_saysetspacing);
    interpretAddFunc(0x805B, op_sayoptioncolor);
    interpretAddFunc(0x805C, op_sayreplycolor);
    interpretAddFunc(0x805D, op_sayrestart);
    interpretAddFunc(0x805E, op_saygetlastpos);
    interpretAddFunc(0x8061, op_saymessagetimeout);
    interpretAddFunc(0x8071, op_gotoxy);
    interpretAddFunc(0x808D, op_hidemouse);
    interpretAddFunc(0x808E, op_showmouse);
    interpretAddFunc(0x8090, op_refreshmouse);
    interpretAddFunc(0x808F, op_mouseshape);
    interpretAddFunc(0x8091, op_setglobalmousefunc);
    interpretAddFunc(0x806E, op_loadpalettetable);
    interpretAddFunc(0x8092, op_addNamedEvent);
    interpretAddFunc(0x8093, op_addNamedHandler);
    interpretAddFunc(0x8094, op_clearNamed);
    interpretAddFunc(0x8095, op_signalNamed);
    interpretAddFunc(0x8096, op_addkey);
    interpretAddFunc(0x8097, op_deletekey);
    interpretAddFunc(0x8098, op_soundplay);
    interpretAddFunc(0x8099, op_soundpause);
    interpretAddFunc(0x809A, op_soundresume);
    interpretAddFunc(0x809B, op_soundstop);
    interpretAddFunc(0x809C, op_soundrewind);
    interpretAddFunc(0x809D, op_sounddelete);
    interpretAddFunc(0x809E, op_setoneoptpause);
    interpretAddFunc(0x809F, op_selectfilelist);
    interpretAddFunc(0x80A0, op_tokenize);

    nevs_initonce();
    initIntExtra();
    initDialog();
}

// 0x45B2C8
void interpretRegisterProgramDeleteCallback(IntLibProgramDeleteCallback* callback)
{
    int index;
    for (index = 0; index < numCallbacks; index++) {
        if (callbacks[index] == NULL) {
            break;
        }
    }

    if (index == numCallbacks) {
        if (callbacks != NULL) {
            callbacks = (IntLibProgramDeleteCallback**)myrealloc(callbacks, sizeof(*callbacks) * (numCallbacks + 1), __FILE__, __LINE__); // ..\\int\\INTLIB.C, 2110
        } else {
            callbacks = (IntLibProgramDeleteCallback**)mymalloc(sizeof(*callbacks), __FILE__, __LINE__); // ..\\int\\INTLIB.C, 2112
        }
        numCallbacks++;
    }

    callbacks[index] = callback;
}

// 0x45B39C
void removeProgramReferences(Program* program)
{
    for (int index = 0; index < INT_LIB_KEY_HANDLERS_CAPACITY; index++) {
        if (program == inputProc[index].program) {
            inputProc[index].program = NULL;
        }
    }

    intExtraRemoveProgramReferences(program);

    for (int index = 0; index < numCallbacks; index++) {
        IntLibProgramDeleteCallback* callback = callbacks[index];
        if (callback != NULL) {
            callback(program);
        }
    }
}

} // namespace fallout
