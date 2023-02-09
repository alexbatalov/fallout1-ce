#include "game/bmpdlog.h"

#include <stdio.h>
#include <string.h>

#include <algorithm>

#include "game/art.h"
#include "game/editor.h"
#include "game/game.h"
#include "game/gsound.h"
#include "game/message.h"
#include "game/wordwrap.h"
#include "platform_compat.h"
#include "plib/color/color.h"
#include "plib/gnw/button.h"
#include "plib/gnw/debug.h"
#include "plib/gnw/gnw.h"
#include "plib/gnw/grbuf.h"
#include "plib/gnw/input.h"
#include "plib/gnw/svga.h"
#include "plib/gnw/text.h"

namespace fallout {

#define FILE_DIALOG_LINE_COUNT 12

#define FILE_DIALOG_DOUBLE_CLICK_DELAY 32

#define LOAD_FILE_DIALOG_DONE_BUTTON_X 58
#define LOAD_FILE_DIALOG_DONE_BUTTON_Y 187

#define LOAD_FILE_DIALOG_DONE_LABEL_X 79
#define LOAD_FILE_DIALOG_DONE_LABEL_Y 187

#define LOAD_FILE_DIALOG_CANCEL_BUTTON_X 163
#define LOAD_FILE_DIALOG_CANCEL_BUTTON_Y 187

#define LOAD_FILE_DIALOG_CANCEL_LABEL_X 182
#define LOAD_FILE_DIALOG_CANCEL_LABEL_Y 187

#define SAVE_FILE_DIALOG_DONE_BUTTON_X 58
#define SAVE_FILE_DIALOG_DONE_BUTTON_Y 214

#define SAVE_FILE_DIALOG_DONE_LABEL_X 79
#define SAVE_FILE_DIALOG_DONE_LABEL_Y 213

#define SAVE_FILE_DIALOG_CANCEL_BUTTON_X 163
#define SAVE_FILE_DIALOG_CANCEL_BUTTON_Y 214

#define SAVE_FILE_DIALOG_CANCEL_LABEL_X 182
#define SAVE_FILE_DIALOG_CANCEL_LABEL_Y 213

#define FILE_DIALOG_TITLE_X 49
#define FILE_DIALOG_TITLE_Y 16

#define FILE_DIALOG_SCROLL_BUTTON_X 36
#define FILE_DIALOG_SCROLL_BUTTON_Y 44

#define FILE_DIALOG_FILE_LIST_X 55
#define FILE_DIALOG_FILE_LIST_Y 49
#define FILE_DIALOG_FILE_LIST_WIDTH 190
#define FILE_DIALOG_FILE_LIST_HEIGHT 124

static void PrntFlist(unsigned char* buffer, char** fileList, int pageOffset, int fileListLength, int selectedIndex, int pitch);

// 0x4FEC0C
int dbox[DIALOG_TYPE_COUNT] = {
    218, // MEDIALOG.FRM - Medium generic dialog box
    217, // LGDIALOG.FRM - Large generic dialog box
};

// 0x4FEC14
int ytable[DIALOG_TYPE_COUNT] = {
    23,
    27,
};

// 0x4FEC1C
int xtable[DIALOG_TYPE_COUNT] = {
    29,
    29,
};

// 0x4FEC24
int doneY[DIALOG_TYPE_COUNT] = {
    81,
    98,
};

// 0x4FEC2C
int doneX[DIALOG_TYPE_COUNT] = {
    51,
    37,
};

// 0x4FEC34
int dblines[DIALOG_TYPE_COUNT] = {
    5,
    6,
};

// 0x4FEC44
int flgids[FILE_DIALOG_FRM_COUNT] = {
    224, // loadbox.frm - character editor
    8, // lilredup.frm - little red button up
    9, // lilreddn.frm - little red button down
    181, // dnarwoff.frm - character editor
    182, // dnarwon.frm - character editor
    199, // uparwoff.frm - character editor
    200, // uparwon.frm - character editor
};

// 0x4FEC60
int flgids2[FILE_DIALOG_FRM_COUNT] = {
    225, // savebox.frm - character editor
    8, // lilredup.frm - little red button up
    9, // lilreddn.frm - little red button down
    181, // dnarwoff.frm - character editor
    182, // dnarwon.frm - character editor
    199, // uparwoff.frm - character editor
    200, // uparwon.frm - character editor
};

// 0x41BE70
int dialog_out(const char* title, const char** body, int bodyLength, int x, int y, int titleColor, const char* a8, int bodyColor, int flags)
{
    MessageList messageList;
    MessageListItem messageListItem;
    int savedFont = text_curr();

    bool v86 = false;

    bool hasTwoButtons = false;
    if (a8 != NULL) {
        hasTwoButtons = true;
    }

    bool hasTitle = false;
    if (title != NULL) {
        hasTitle = true;
    }

    if ((flags & DIALOG_BOX_YES_NO) != 0) {
        hasTwoButtons = true;
        flags |= DIALOG_BOX_LARGE;
        flags &= ~DIALOG_BOX_0x20;
    }

    int maximumLineWidth = 0;
    if (hasTitle) {
        maximumLineWidth = text_width(title);
    }

    int linesCount = 0;
    for (int index = 0; index < bodyLength; index++) {
        // NOTE: Calls [text_width] twice because of [max] macro.
        maximumLineWidth = std::max(text_width(body[index]), maximumLineWidth);
        linesCount++;
    }

    int dialogType;
    if ((flags & DIALOG_BOX_LARGE) != 0 || hasTwoButtons) {
        dialogType = DIALOG_TYPE_LARGE;
    } else if ((flags & DIALOG_BOX_MEDIUM) != 0) {
        dialogType = DIALOG_TYPE_MEDIUM;
    } else {
        if (hasTitle) {
            linesCount++;
        }

        dialogType = maximumLineWidth > 168 || linesCount > 5
            ? DIALOG_TYPE_LARGE
            : DIALOG_TYPE_MEDIUM;
    }

    CacheEntry* backgroundHandle;
    int backgroundWidth;
    int backgroundHeight;
    int fid = art_id(OBJ_TYPE_INTERFACE, dbox[dialogType], 0, 0, 0);
    unsigned char* background = art_lock(fid, &backgroundHandle, &backgroundWidth, &backgroundHeight);
    if (background == NULL) {
        text_font(savedFont);
        return -1;
    }

    // Maintain original position in original resolution, otherwise center it.
    if (screenGetWidth() != 640) x += (screenGetWidth() - 640) / 2;
    if (screenGetHeight() != 480) y += (screenGetHeight() - 480) / 2;
    int win = win_add(x, y, backgroundWidth, backgroundHeight, 256, WINDOW_MODAL | WINDOW_MOVE_ON_TOP);
    if (win == -1) {
        art_ptr_unlock(backgroundHandle);
        text_font(savedFont);
        return -1;
    }

    unsigned char* windowBuf = win_get_buf(win);
    memcpy(windowBuf, background, backgroundWidth * backgroundHeight);

    CacheEntry* doneBoxHandle = NULL;
    unsigned char* doneBox = NULL;
    int doneBoxWidth;
    int doneBoxHeight;

    CacheEntry* downButtonHandle = NULL;
    unsigned char* downButton = NULL;
    int downButtonWidth;
    int downButtonHeight;

    CacheEntry* upButtonHandle = NULL;
    unsigned char* upButton = NULL;

    if ((flags & DIALOG_BOX_0x20) == 0) {
        int doneBoxFid = art_id(OBJ_TYPE_INTERFACE, 209, 0, 0, 0);
        doneBox = art_lock(doneBoxFid, &doneBoxHandle, &doneBoxWidth, &doneBoxHeight);
        if (doneBox == NULL) {
            art_ptr_unlock(backgroundHandle);
            text_font(savedFont);
            win_delete(win);
            return -1;
        }

        int downButtonFid = art_id(OBJ_TYPE_INTERFACE, 9, 0, 0, 0);
        downButton = art_lock(downButtonFid, &downButtonHandle, &downButtonWidth, &downButtonHeight);
        if (downButton == NULL) {
            art_ptr_unlock(doneBoxHandle);
            art_ptr_unlock(backgroundHandle);
            text_font(savedFont);
            win_delete(win);
            return -1;
        }

        int upButtonFid = art_id(OBJ_TYPE_INTERFACE, 8, 0, 0, 0);
        upButton = art_ptr_lock_data(upButtonFid, 0, 0, &upButtonHandle);
        if (upButton == NULL) {
            art_ptr_unlock(downButtonHandle);
            art_ptr_unlock(doneBoxHandle);
            art_ptr_unlock(backgroundHandle);
            text_font(savedFont);
            win_delete(win);
            return -1;
        }

        int v27 = hasTwoButtons ? doneX[dialogType] : (backgroundWidth - doneBoxWidth) / 2;
        buf_to_buf(doneBox, doneBoxWidth, doneBoxHeight, doneBoxWidth, windowBuf + backgroundWidth * doneY[dialogType] + v27, backgroundWidth);

        if (!message_init(&messageList)) {
            art_ptr_unlock(upButtonHandle);
            art_ptr_unlock(downButtonHandle);
            art_ptr_unlock(doneBoxHandle);
            art_ptr_unlock(backgroundHandle);
            text_font(savedFont);
            win_delete(win);
            return -1;
        }

        char path[COMPAT_MAX_PATH];
        snprintf(path, sizeof(path), "%s%s", msg_path, "DBOX.MSG");

        if (!message_load(&messageList, path)) {
            art_ptr_unlock(upButtonHandle);
            art_ptr_unlock(downButtonHandle);
            art_ptr_unlock(doneBoxHandle);
            art_ptr_unlock(backgroundHandle);
            text_font(savedFont);
            // FIXME: Window is not removed.
            return -1;
        }

        text_font(103);

        // 100 - DONE
        // 101 - YES
        messageListItem.num = (flags & DIALOG_BOX_YES_NO) == 0 ? 100 : 101;
        if (message_search(&messageList, &messageListItem)) {
            text_to_buf(windowBuf + backgroundWidth * (doneY[dialogType] + 3) + v27 + 35, messageListItem.text, backgroundWidth, backgroundWidth, colorTable[18979]);
        }

        int btn = win_register_button(win, v27 + 13, doneY[dialogType] + 4, downButtonWidth, downButtonHeight, -1, -1, -1, 500, upButton, downButton, NULL, BUTTON_FLAG_TRANSPARENT);
        if (btn != -1) {
            win_register_button_sound_func(btn, gsound_red_butt_press, gsound_red_butt_release);
        }

        v86 = true;
    }

    if (hasTwoButtons && dialogType == DIALOG_TYPE_LARGE) {
        if (v86) {
            if ((flags & DIALOG_BOX_YES_NO) != 0) {
                a8 = getmsg(&messageList, &messageListItem, 102);
            }

            text_font(103);

            trans_buf_to_buf(doneBox,
                doneBoxWidth,
                doneBoxHeight,
                doneBoxWidth,
                windowBuf + backgroundWidth * doneY[dialogType] + doneX[dialogType] + doneBoxWidth + 24,
                backgroundWidth);

            text_to_buf(windowBuf + backgroundWidth * (doneY[dialogType] + 3) + doneX[dialogType] + doneBoxWidth + 59,
                a8, backgroundWidth, backgroundWidth, colorTable[18979]);

            int btn = win_register_button(win,
                doneBoxWidth + doneX[dialogType] + 37,
                doneY[dialogType] + 4,
                downButtonWidth,
                downButtonHeight,
                -1, -1, -1, 501, upButton, downButton, 0, BUTTON_FLAG_TRANSPARENT);
            if (btn != -1) {
                win_register_button_sound_func(btn, gsound_red_butt_press, gsound_red_butt_release);
            }
        } else {
            int doneBoxFid = art_id(OBJ_TYPE_INTERFACE, 209, 0, 0, 0);
            unsigned char* doneBox = art_lock(doneBoxFid, &doneBoxHandle, &doneBoxWidth, &doneBoxHeight);
            if (doneBox == NULL) {
                art_ptr_unlock(backgroundHandle);
                text_font(savedFont);
                win_delete(win);
                return -1;
            }

            int downButtonFid = art_id(OBJ_TYPE_INTERFACE, 9, 0, 0, 0);
            unsigned char* downButton = art_lock(downButtonFid, &downButtonHandle, &downButtonWidth, &downButtonHeight);
            if (downButton == NULL) {
                art_ptr_unlock(doneBoxHandle);
                art_ptr_unlock(backgroundHandle);
                text_font(savedFont);
                win_delete(win);
                return -1;
            }

            int upButtonFid = art_id(OBJ_TYPE_INTERFACE, 8, 0, 0, 0);
            unsigned char* upButton = art_ptr_lock_data(upButtonFid, 0, 0, &upButtonHandle);
            if (upButton == NULL) {
                art_ptr_unlock(downButtonHandle);
                art_ptr_unlock(doneBoxHandle);
                art_ptr_unlock(backgroundHandle);
                text_font(savedFont);
                win_delete(win);
                return -1;
            }

            if (!message_init(&messageList)) {
                art_ptr_unlock(upButtonHandle);
                art_ptr_unlock(downButtonHandle);
                art_ptr_unlock(doneBoxHandle);
                art_ptr_unlock(backgroundHandle);
                text_font(savedFont);
                win_delete(win);
                return -1;
            }

            char path[COMPAT_MAX_PATH];
            snprintf(path, sizeof(path), "%s%s", msg_path, "DBOX.MSG");

            if (!message_load(&messageList, path)) {
                art_ptr_unlock(upButtonHandle);
                art_ptr_unlock(downButtonHandle);
                art_ptr_unlock(doneBoxHandle);
                art_ptr_unlock(backgroundHandle);
                text_font(savedFont);
                win_delete(win);
                return -1;
            }

            trans_buf_to_buf(doneBox,
                doneBoxWidth,
                doneBoxHeight,
                doneBoxWidth,
                windowBuf + backgroundWidth * doneY[dialogType] + doneX[dialogType],
                backgroundWidth);

            text_font(103);

            text_to_buf(windowBuf + backgroundWidth * (doneY[dialogType] + 3) + doneX[dialogType] + 35,
                a8, backgroundWidth, backgroundWidth, colorTable[18979]);

            int btn = win_register_button(win,
                doneX[dialogType] + 13,
                doneY[dialogType] + 4,
                downButtonWidth,
                downButtonHeight,
                -1,
                -1,
                -1,
                501,
                upButton,
                downButton,
                NULL,
                BUTTON_FLAG_TRANSPARENT);
            if (btn != -1) {
                win_register_button_sound_func(btn, gsound_red_butt_press, gsound_red_butt_release);
            }

            v86 = true;
        }
    }

    text_font(101);

    int v23 = ytable[dialogType];

    if ((flags & DIALOG_BOX_NO_VERTICAL_CENTERING) == 0) {
        int v41 = dblines[dialogType] * text_height() / 2 + v23;
        v23 = v41 - ((bodyLength + 1) * text_height() / 2);
    }

    if (hasTitle) {
        if ((flags & DIALOG_BOX_NO_HORIZONTAL_CENTERING) != 0) {
            text_to_buf(windowBuf + backgroundWidth * v23 + xtable[dialogType], title, backgroundWidth, backgroundWidth, titleColor);
        } else {
            int length = text_width(title);
            text_to_buf(windowBuf + backgroundWidth * v23 + (backgroundWidth - length) / 2, title, backgroundWidth, backgroundWidth, titleColor);
        }
        v23 += text_height();
    }

    for (int v94 = 0; v94 < bodyLength; v94++) {
        int len = text_width(body[v94]);
        if (len <= backgroundWidth - 26) {
            if ((flags & DIALOG_BOX_NO_HORIZONTAL_CENTERING) != 0) {
                text_to_buf(windowBuf + backgroundWidth * v23 + xtable[dialogType], body[v94], backgroundWidth, backgroundWidth, bodyColor);
            } else {
                int length = text_width(body[v94]);
                text_to_buf(windowBuf + backgroundWidth * v23 + (backgroundWidth - length) / 2, body[v94], backgroundWidth, backgroundWidth, bodyColor);
            }
            v23 += text_height();
        } else {
            short beginnings[WORD_WRAP_MAX_COUNT];
            short count;
            if (word_wrap(body[v94], backgroundWidth - 26, beginnings, &count) != 0) {
                debug_printf("\nError: dialog_out");
            }

            for (int v48 = 1; v48 < count; v48++) {
                int v51 = beginnings[v48] - beginnings[v48 - 1];
                if (v51 >= 260) {
                    v51 = 259;
                }

                char string[260];
                strncpy(string, body[v94] + beginnings[v48 - 1], v51);
                string[v51] = '\0';

                if ((flags & DIALOG_BOX_NO_HORIZONTAL_CENTERING) != 0) {
                    text_to_buf(windowBuf + backgroundWidth * v23 + xtable[dialogType], string, backgroundWidth, backgroundWidth, bodyColor);
                } else {
                    int length = text_width(string);
                    text_to_buf(windowBuf + backgroundWidth * v23 + (backgroundWidth - length) / 2, string, backgroundWidth, backgroundWidth, bodyColor);
                }
                v23 += text_height();
            }
        }
    }

    win_draw(win);

    int rc = -1;
    while (rc == -1) {
        sharedFpsLimiter.mark();

        int keyCode = get_input();

        if (keyCode == 500) {
            rc = 1;
        } else if (keyCode == KEY_RETURN) {
            gsound_play_sfx_file("ib1p1xx1");
            rc = 1;
        } else if (keyCode == KEY_ESCAPE || keyCode == 501) {
            rc = 0;
        } else {
            if ((flags & 0x10) != 0) {
                if (keyCode == KEY_UPPERCASE_Y || keyCode == KEY_LOWERCASE_Y) {
                    rc = 1;
                } else if (keyCode == KEY_UPPERCASE_N || keyCode == KEY_LOWERCASE_N) {
                    rc = 0;
                }
            }
        }

        if (game_user_wants_to_quit != 0) {
            rc = 1;
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    win_delete(win);
    art_ptr_unlock(backgroundHandle);
    text_font(savedFont);

    if (v86) {
        art_ptr_unlock(doneBoxHandle);
        art_ptr_unlock(downButtonHandle);
        art_ptr_unlock(upButtonHandle);
        message_exit(&messageList);
    }

    return rc;
}

// 0x41CC40
int file_dialog(char* title, char** fileList, char* dest, int fileListLength, int x, int y, int flags)
{
    int oldFont = text_curr();

    bool isScrollable = false;
    if (fileListLength > FILE_DIALOG_LINE_COUNT) {
        isScrollable = true;
    }

    int selectedFileIndex = 0;
    int pageOffset = 0;
    int maxPageOffset = fileListLength - (FILE_DIALOG_LINE_COUNT + 1);
    if (maxPageOffset < 0) {
        maxPageOffset = fileListLength - 1;
        if (maxPageOffset < 0) {
            maxPageOffset = 0;
        }
    }

    unsigned char* frmBuffers[FILE_DIALOG_FRM_COUNT];
    CacheEntry* frmHandles[FILE_DIALOG_FRM_COUNT];
    Size frmSizes[FILE_DIALOG_FRM_COUNT];

    for (int index = 0; index < FILE_DIALOG_FRM_COUNT; index++) {
        int fid = art_id(OBJ_TYPE_INTERFACE, flgids[index], 0, 0, 0);
        frmBuffers[index] = art_lock(fid, &(frmHandles[index]), &(frmSizes[index].width), &(frmSizes[index].height));
        if (frmBuffers[index] == NULL) {
            while (--index >= 0) {
                art_ptr_unlock(frmHandles[index]);
            }
            return -1;
        }
    }

    int backgroundWidth = frmSizes[FILE_DIALOG_FRM_BACKGROUND].width;
    int backgroundHeight = frmSizes[FILE_DIALOG_FRM_BACKGROUND].height;

    // Maintain original position in original resolution, otherwise center it.
    if (screenGetWidth() != 640) x += (screenGetWidth() - 640) / 2;
    if (screenGetHeight() != 480) y += (screenGetHeight() - 480) / 2;
    int win = win_add(x, y, backgroundWidth, backgroundHeight, 256, WINDOW_MODAL | WINDOW_MOVE_ON_TOP);
    if (win == -1) {
        for (int index = 0; index < FILE_DIALOG_FRM_COUNT; index++) {
            art_ptr_unlock(frmHandles[index]);
        }
        return -1;
    }

    unsigned char* windowBuffer = win_get_buf(win);
    memcpy(windowBuffer, frmBuffers[FILE_DIALOG_FRM_BACKGROUND], backgroundWidth * backgroundHeight);

    MessageList messageList;
    MessageListItem messageListItem;

    if (!message_init(&messageList)) {
        win_delete(win);

        for (int index = 0; index < FILE_DIALOG_FRM_COUNT; index++) {
            art_ptr_unlock(frmHandles[index]);
        }

        return -1;
    }

    char path[COMPAT_MAX_PATH];
    snprintf(path, sizeof(path), "%s%s", msg_path, "DBOX.MSG");

    if (!message_load(&messageList, path)) {
        win_delete(win);

        for (int index = 0; index < FILE_DIALOG_FRM_COUNT; index++) {
            art_ptr_unlock(frmHandles[index]);
        }

        return -1;
    }

    text_font(103);

    // DONE
    const char* done = getmsg(&messageList, &messageListItem, 100);
    text_to_buf(windowBuffer + LOAD_FILE_DIALOG_DONE_LABEL_Y * backgroundWidth + LOAD_FILE_DIALOG_DONE_LABEL_X, done, backgroundWidth, backgroundWidth, colorTable[18979]);

    // CANCEL
    const char* cancel = getmsg(&messageList, &messageListItem, 103);
    text_to_buf(windowBuffer + LOAD_FILE_DIALOG_CANCEL_LABEL_Y * backgroundWidth + LOAD_FILE_DIALOG_CANCEL_LABEL_X, cancel, backgroundWidth, backgroundWidth, colorTable[18979]);

    int doneBtn = win_register_button(win,
        LOAD_FILE_DIALOG_DONE_BUTTON_X,
        LOAD_FILE_DIALOG_DONE_BUTTON_Y,
        frmSizes[FILE_DIALOG_FRM_LITTLE_RED_BUTTON_PRESSED].width,
        frmSizes[FILE_DIALOG_FRM_LITTLE_RED_BUTTON_PRESSED].height,
        -1,
        -1,
        -1,
        500,
        frmBuffers[FILE_DIALOG_FRM_LITTLE_RED_BUTTON_NORMAL],
        frmBuffers[FILE_DIALOG_FRM_LITTLE_RED_BUTTON_PRESSED],
        NULL,
        BUTTON_FLAG_TRANSPARENT);
    if (doneBtn != -1) {
        win_register_button_sound_func(doneBtn, gsound_red_butt_press, gsound_red_butt_release);
    }

    int cancelBtn = win_register_button(win,
        LOAD_FILE_DIALOG_CANCEL_BUTTON_X,
        LOAD_FILE_DIALOG_CANCEL_BUTTON_Y,
        frmSizes[FILE_DIALOG_FRM_LITTLE_RED_BUTTON_PRESSED].width,
        frmSizes[FILE_DIALOG_FRM_LITTLE_RED_BUTTON_PRESSED].height,
        -1,
        -1,
        -1,
        501,
        frmBuffers[FILE_DIALOG_FRM_LITTLE_RED_BUTTON_NORMAL],
        frmBuffers[FILE_DIALOG_FRM_LITTLE_RED_BUTTON_PRESSED],
        NULL,
        BUTTON_FLAG_TRANSPARENT);
    if (cancelBtn != -1) {
        win_register_button_sound_func(cancelBtn, gsound_red_butt_press, gsound_red_butt_release);
    }

    int scrollUpBtn = win_register_button(win,
        FILE_DIALOG_SCROLL_BUTTON_X,
        FILE_DIALOG_SCROLL_BUTTON_Y,
        frmSizes[FILE_DIALOG_FRM_SCROLL_UP_ARROW_PRESSED].width,
        frmSizes[FILE_DIALOG_FRM_SCROLL_UP_ARROW_PRESSED].height,
        -1,
        505,
        506,
        505,
        frmBuffers[FILE_DIALOG_FRM_SCROLL_UP_ARROW_NORMAL],
        frmBuffers[FILE_DIALOG_FRM_SCROLL_UP_ARROW_PRESSED],
        NULL,
        BUTTON_FLAG_TRANSPARENT);
    if (scrollUpBtn != -1) {
        win_register_button_sound_func(cancelBtn, gsound_red_butt_press, gsound_red_butt_release);
    }

    int scrollDownButton = win_register_button(win,
        FILE_DIALOG_SCROLL_BUTTON_X,
        FILE_DIALOG_SCROLL_BUTTON_Y + frmSizes[FILE_DIALOG_FRM_SCROLL_UP_ARROW_PRESSED].height,
        frmSizes[FILE_DIALOG_FRM_SCROLL_DOWN_ARROW_PRESSED].width,
        frmSizes[FILE_DIALOG_FRM_SCROLL_DOWN_ARROW_PRESSED].height,
        -1,
        503,
        504,
        503,
        frmBuffers[FILE_DIALOG_FRM_SCROLL_DOWN_ARROW_NORMAL],
        frmBuffers[FILE_DIALOG_FRM_SCROLL_DOWN_ARROW_PRESSED],
        NULL,
        BUTTON_FLAG_TRANSPARENT);
    if (scrollUpBtn != -1) {
        win_register_button_sound_func(cancelBtn, gsound_red_butt_press, gsound_red_butt_release);
    }

    win_register_button(
        win,
        FILE_DIALOG_FILE_LIST_X,
        FILE_DIALOG_FILE_LIST_Y,
        FILE_DIALOG_FILE_LIST_WIDTH,
        FILE_DIALOG_FILE_LIST_HEIGHT,
        -1,
        -1,
        -1,
        502,
        NULL,
        NULL,
        NULL,
        0);

    if (title != NULL) {
        text_to_buf(windowBuffer + backgroundWidth * FILE_DIALOG_TITLE_Y + FILE_DIALOG_TITLE_X, title, backgroundWidth, backgroundWidth, colorTable[18979]);
    }

    text_font(101);

    PrntFlist(windowBuffer, fileList, pageOffset, fileListLength, selectedFileIndex, backgroundWidth);
    win_draw(win);

    int doubleClickSelectedFileIndex = -2;
    int doubleClickTimer = FILE_DIALOG_DOUBLE_CLICK_DELAY;

    int rc = -1;
    while (rc == -1) {
        sharedFpsLimiter.mark();

        unsigned int tick = get_time();
        int keyCode = get_input();
        int scrollDirection = FILE_DIALOG_SCROLL_DIRECTION_NONE;
        int scrollCounter = 0;
        bool isScrolling = false;

        convertMouseWheelToArrowKey(&keyCode);

        if (keyCode == 500) {
            if (fileListLength != 0) {
                strncpy(dest, fileList[selectedFileIndex + pageOffset], 16);
                rc = 0;
            } else {
                rc = 1;
            }
        } else if (keyCode == 501 || keyCode == KEY_ESCAPE) {
            rc = 1;
        } else if (keyCode == 502 && fileListLength != 0) {
            int mouseX;
            int mouseY;
            mouse_get_position(&mouseX, &mouseY);

            int selectedLine = (mouseY - y - FILE_DIALOG_FILE_LIST_Y) / text_height();
            if (selectedLine - 1 < 0) {
                selectedLine = 0;
            }

            if (isScrollable || selectedLine < fileListLength) {
                if (selectedLine >= FILE_DIALOG_LINE_COUNT) {
                    selectedLine = FILE_DIALOG_LINE_COUNT - 1;
                }
            } else {
                selectedLine = fileListLength - 1;
            }

            selectedFileIndex = selectedLine;
            if (selectedFileIndex == doubleClickSelectedFileIndex) {
                gsound_play_sfx_file("ib1p1xx1");
                strncpy(dest, fileList[selectedFileIndex + pageOffset], 16);
                rc = 0;
            }

            doubleClickSelectedFileIndex = selectedFileIndex;
            PrntFlist(windowBuffer, fileList, pageOffset, fileListLength, selectedFileIndex, backgroundWidth);
        } else if (keyCode == 506) {
            scrollDirection = FILE_DIALOG_SCROLL_DIRECTION_UP;
        } else if (keyCode == 504) {
            scrollDirection = FILE_DIALOG_SCROLL_DIRECTION_DOWN;
        } else {
            switch (keyCode) {
            case KEY_ARROW_UP:
                pageOffset--;
                if (pageOffset < 0) {
                    selectedFileIndex--;
                    if (selectedFileIndex < 0) {
                        selectedFileIndex = 0;
                    }
                    pageOffset = 0;
                }
                PrntFlist(windowBuffer, fileList, pageOffset, fileListLength, selectedFileIndex, backgroundWidth);
                doubleClickSelectedFileIndex = -2;
                break;
            case KEY_ARROW_DOWN:
                if (isScrollable) {
                    pageOffset++;
                    // FIXME: Should be >= maxPageOffset (as in save dialog).
                    // Otherwise out of bounds index is considered selected.
                    if (pageOffset > maxPageOffset) {
                        selectedFileIndex++;
                        // FIXME: Should be >= FILE_DIALOG_LINE_COUNT (as in
                        // save dialog). Otherwise out of bounds index is
                        // considered selected.
                        if (selectedFileIndex > FILE_DIALOG_LINE_COUNT) {
                            selectedFileIndex = FILE_DIALOG_LINE_COUNT - 1;
                        }
                        pageOffset = maxPageOffset;
                    }
                } else {
                    selectedFileIndex++;
                    if (selectedFileIndex > maxPageOffset) {
                        selectedFileIndex = maxPageOffset;
                    }
                }
                PrntFlist(windowBuffer, fileList, pageOffset, fileListLength, selectedFileIndex, backgroundWidth);
                doubleClickSelectedFileIndex = -2;
                break;
            case KEY_HOME:
                selectedFileIndex = 0;
                pageOffset = 0;
                PrntFlist(windowBuffer, fileList, pageOffset, fileListLength, selectedFileIndex, backgroundWidth);
                doubleClickSelectedFileIndex = -2;
                break;
            case KEY_END:
                if (isScrollable) {
                    selectedFileIndex = FILE_DIALOG_LINE_COUNT - 1;
                    pageOffset = maxPageOffset;
                } else {
                    selectedFileIndex = maxPageOffset;
                    pageOffset = 0;
                }
                PrntFlist(windowBuffer, fileList, pageOffset, fileListLength, selectedFileIndex, backgroundWidth);
                doubleClickSelectedFileIndex = -2;
                break;
            }
        }

        if (scrollDirection != FILE_DIALOG_SCROLL_DIRECTION_NONE) {
            unsigned int scrollDelay = 4;
            doubleClickSelectedFileIndex = -2;
            while (1) {
                unsigned int scrollTick = get_time();
                scrollCounter += 1;
                if ((!isScrolling && scrollCounter == 1) || (isScrolling && scrollCounter > 14.4)) {
                    isScrolling = true;

                    if (scrollCounter > 14.4) {
                        scrollDelay += 1;
                        if (scrollDelay > 24) {
                            scrollDelay = 24;
                        }
                    }

                    if (scrollDirection == FILE_DIALOG_SCROLL_DIRECTION_UP) {
                        pageOffset--;
                        if (pageOffset < 0) {
                            selectedFileIndex--;
                            if (selectedFileIndex < 0) {
                                selectedFileIndex = 0;
                            }
                            pageOffset = 0;
                        }
                    } else {
                        if (isScrollable) {
                            pageOffset++;
                            if (pageOffset > maxPageOffset) {
                                selectedFileIndex++;
                                if (selectedFileIndex >= FILE_DIALOG_LINE_COUNT) {
                                    selectedFileIndex = FILE_DIALOG_LINE_COUNT - 1;
                                }
                                pageOffset = maxPageOffset;
                            }
                        } else {
                            selectedFileIndex++;
                            if (selectedFileIndex > maxPageOffset) {
                                selectedFileIndex = maxPageOffset;
                            }
                        }
                    }

                    PrntFlist(windowBuffer, fileList, pageOffset, fileListLength, selectedFileIndex, backgroundWidth);
                    win_draw(win);
                }

                unsigned int delay = (scrollCounter > 14.4) ? 1000 / scrollDelay : 1000 / 24;
                while (elapsed_time(scrollTick) < delay) {
                }

                if (game_user_wants_to_quit != 0) {
                    rc = 1;
                    break;
                }

                int keyCode = get_input();
                if (keyCode == 505 || keyCode == 503) {
                    break;
                }

                renderPresent();
            }
        } else {
            win_draw(win);

            doubleClickTimer--;
            if (doubleClickTimer == 0) {
                doubleClickTimer = FILE_DIALOG_DOUBLE_CLICK_DELAY;
                doubleClickSelectedFileIndex = -2;
            }

            while (elapsed_time(tick) < (1000 / 24)) {
            }
        }

        if (game_user_wants_to_quit) {
            rc = 1;
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    win_delete(win);

    for (int index = 0; index < FILE_DIALOG_FRM_COUNT; index++) {
        art_ptr_unlock(frmHandles[index]);
    }

    message_exit(&messageList);
    text_font(oldFont);

    return rc;
}

// 0x41D7A4
int save_file_dialog(char* title, char** fileList, char* dest, int fileListLength, int x, int y, int flags)
{
    int oldFont = text_curr();

    bool isScrollable = false;
    if (fileListLength > FILE_DIALOG_LINE_COUNT) {
        isScrollable = true;
    }

    int selectedFileIndex = 0;
    int pageOffset = 0;
    int maxPageOffset = fileListLength - (FILE_DIALOG_LINE_COUNT + 1);
    if (maxPageOffset < 0) {
        maxPageOffset = fileListLength - 1;
        if (maxPageOffset < 0) {
            maxPageOffset = 0;
        }
    }

    unsigned char* frmBuffers[FILE_DIALOG_FRM_COUNT];
    CacheEntry* frmHandles[FILE_DIALOG_FRM_COUNT];
    Size frmSizes[FILE_DIALOG_FRM_COUNT];

    for (int index = 0; index < FILE_DIALOG_FRM_COUNT; index++) {
        int fid = art_id(OBJ_TYPE_INTERFACE, flgids2[index], 0, 0, 0);
        frmBuffers[index] = art_lock(fid, &(frmHandles[index]), &(frmSizes[index].width), &(frmSizes[index].height));
        if (frmBuffers[index] == NULL) {
            while (--index >= 0) {
                art_ptr_unlock(frmHandles[index]);
            }
            return -1;
        }
    }

    int backgroundWidth = frmSizes[FILE_DIALOG_FRM_BACKGROUND].width;
    int backgroundHeight = frmSizes[FILE_DIALOG_FRM_BACKGROUND].height;

    // Maintain original position in original resolution, otherwise center it.
    if (screenGetWidth() != 640) x += (screenGetWidth() - 640) / 2;
    if (screenGetHeight() != 480) y += (screenGetHeight() - 480) / 2;
    int win = win_add(x, y, backgroundWidth, backgroundHeight, 256, WINDOW_MODAL | WINDOW_MOVE_ON_TOP);
    if (win == -1) {
        for (int index = 0; index < FILE_DIALOG_FRM_COUNT; index++) {
            art_ptr_unlock(frmHandles[index]);
        }
        return -1;
    }

    unsigned char* windowBuffer = win_get_buf(win);
    memcpy(windowBuffer, frmBuffers[FILE_DIALOG_FRM_BACKGROUND], backgroundWidth * backgroundHeight);

    MessageList messageList;
    MessageListItem messageListItem;

    if (!message_init(&messageList)) {
        win_delete(win);

        for (int index = 0; index < FILE_DIALOG_FRM_COUNT; index++) {
            art_ptr_unlock(frmHandles[index]);
        }

        return -1;
    }

    char path[COMPAT_MAX_PATH];
    snprintf(path, sizeof(path), "%s%s", msg_path, "DBOX.MSG");

    if (!message_load(&messageList, path)) {
        win_delete(win);

        for (int index = 0; index < FILE_DIALOG_FRM_COUNT; index++) {
            art_ptr_unlock(frmHandles[index]);
        }

        return -1;
    }

    text_font(103);

    // DONE
    const char* done = getmsg(&messageList, &messageListItem, 100);
    text_to_buf(windowBuffer + backgroundWidth * SAVE_FILE_DIALOG_DONE_LABEL_Y + SAVE_FILE_DIALOG_DONE_LABEL_X, done, backgroundWidth, backgroundWidth, colorTable[18979]);

    // CANCEL
    const char* cancel = getmsg(&messageList, &messageListItem, 103);
    text_to_buf(windowBuffer + backgroundWidth * SAVE_FILE_DIALOG_CANCEL_LABEL_Y + SAVE_FILE_DIALOG_CANCEL_LABEL_X, cancel, backgroundWidth, backgroundWidth, colorTable[18979]);

    int doneBtn = win_register_button(win,
        SAVE_FILE_DIALOG_DONE_BUTTON_X,
        SAVE_FILE_DIALOG_DONE_BUTTON_Y,
        frmSizes[FILE_DIALOG_FRM_LITTLE_RED_BUTTON_PRESSED].width,
        frmSizes[FILE_DIALOG_FRM_LITTLE_RED_BUTTON_PRESSED].height,
        -1,
        -1,
        -1,
        500,
        frmBuffers[FILE_DIALOG_FRM_LITTLE_RED_BUTTON_NORMAL],
        frmBuffers[FILE_DIALOG_FRM_LITTLE_RED_BUTTON_PRESSED],
        NULL,
        BUTTON_FLAG_TRANSPARENT);
    if (doneBtn != -1) {
        win_register_button_sound_func(doneBtn, gsound_red_butt_press, gsound_red_butt_release);
    }

    int cancelBtn = win_register_button(win,
        SAVE_FILE_DIALOG_CANCEL_BUTTON_X,
        SAVE_FILE_DIALOG_CANCEL_BUTTON_Y,
        frmSizes[FILE_DIALOG_FRM_LITTLE_RED_BUTTON_PRESSED].width,
        frmSizes[FILE_DIALOG_FRM_LITTLE_RED_BUTTON_PRESSED].height,
        -1,
        -1,
        -1,
        501,
        frmBuffers[FILE_DIALOG_FRM_LITTLE_RED_BUTTON_NORMAL],
        frmBuffers[FILE_DIALOG_FRM_LITTLE_RED_BUTTON_PRESSED],
        NULL,
        BUTTON_FLAG_TRANSPARENT);
    if (cancelBtn != -1) {
        win_register_button_sound_func(cancelBtn, gsound_red_butt_press, gsound_red_butt_release);
    }

    int scrollUpBtn = win_register_button(win,
        FILE_DIALOG_SCROLL_BUTTON_X,
        FILE_DIALOG_SCROLL_BUTTON_Y,
        frmSizes[FILE_DIALOG_FRM_SCROLL_UP_ARROW_PRESSED].width,
        frmSizes[FILE_DIALOG_FRM_SCROLL_UP_ARROW_PRESSED].height,
        -1,
        505,
        506,
        505,
        frmBuffers[FILE_DIALOG_FRM_SCROLL_UP_ARROW_NORMAL],
        frmBuffers[FILE_DIALOG_FRM_SCROLL_UP_ARROW_PRESSED],
        NULL,
        BUTTON_FLAG_TRANSPARENT);
    if (scrollUpBtn != -1) {
        win_register_button_sound_func(cancelBtn, gsound_red_butt_press, gsound_red_butt_release);
    }

    int scrollDownButton = win_register_button(win,
        FILE_DIALOG_SCROLL_BUTTON_X,
        FILE_DIALOG_SCROLL_BUTTON_Y + frmSizes[FILE_DIALOG_FRM_SCROLL_UP_ARROW_PRESSED].height,
        frmSizes[FILE_DIALOG_FRM_SCROLL_DOWN_ARROW_PRESSED].width,
        frmSizes[FILE_DIALOG_FRM_SCROLL_DOWN_ARROW_PRESSED].height,
        -1,
        503,
        504,
        503,
        frmBuffers[FILE_DIALOG_FRM_SCROLL_DOWN_ARROW_NORMAL],
        frmBuffers[FILE_DIALOG_FRM_SCROLL_DOWN_ARROW_PRESSED],
        NULL,
        BUTTON_FLAG_TRANSPARENT);
    if (scrollUpBtn != -1) {
        win_register_button_sound_func(cancelBtn, gsound_red_butt_press, gsound_red_butt_release);
    }

    win_register_button(
        win,
        FILE_DIALOG_FILE_LIST_X,
        FILE_DIALOG_FILE_LIST_Y,
        FILE_DIALOG_FILE_LIST_WIDTH,
        FILE_DIALOG_FILE_LIST_HEIGHT,
        -1,
        -1,
        -1,
        502,
        NULL,
        NULL,
        NULL,
        0);

    if (title != NULL) {
        text_to_buf(windowBuffer + backgroundWidth * FILE_DIALOG_TITLE_Y + FILE_DIALOG_TITLE_X, title, backgroundWidth, backgroundWidth, colorTable[18979]);
    }

    text_font(101);

    int cursorHeight = text_height();
    int cursorWidth = text_width("_") - 4;
    PrntFlist(windowBuffer, fileList, pageOffset, fileListLength, selectedFileIndex, backgroundWidth);

    int fileNameLength = 0;
    char* pch = dest;
    while (*pch != '\0' && *pch != '.') {
        fileNameLength++;
        if (fileNameLength >= 12) {
            break;
        }
    }
    dest[fileNameLength] = '\0';

    char fileNameCopy[32];
    strncpy(fileNameCopy, dest, 32);

    int fileNameCopyLength = strlen(fileNameCopy);
    fileNameCopy[fileNameCopyLength + 1] = '\0';
    fileNameCopy[fileNameCopyLength] = ' ';

    unsigned char* fileNameBufferPtr = windowBuffer + backgroundWidth * 190 + 57;

    buf_fill(fileNameBufferPtr, text_width(fileNameCopy), cursorHeight, backgroundWidth, 100);
    text_to_buf(fileNameBufferPtr, fileNameCopy, backgroundWidth, backgroundWidth, colorTable[992]);

    win_draw(win);

    beginTextInput();

    int blinkingCounter = 3;
    bool blink = false;

    int doubleClickSelectedFileIndex = -2;
    int doubleClickTimer = FILE_DIALOG_DOUBLE_CLICK_DELAY;

    int rc = -1;
    while (rc == -1) {
        sharedFpsLimiter.mark();

        unsigned int tick = get_time();
        int keyCode = get_input();
        int scrollDirection = FILE_DIALOG_SCROLL_DIRECTION_NONE;
        int scrollCounter = 0;
        bool isScrolling = false;

        convertMouseWheelToArrowKey(&keyCode);

        if (keyCode == 500) {
            rc = 0;
        } else if (keyCode == KEY_RETURN) {
            gsound_play_sfx_file("ib1p1xx1");
            rc = 0;
        } else if (keyCode == 501 || keyCode == KEY_ESCAPE) {
            rc = 1;
        } else if ((keyCode == KEY_DELETE || keyCode == KEY_BACKSPACE) && fileNameCopyLength > 0) {
            buf_fill(fileNameBufferPtr, text_width(fileNameCopy), cursorHeight, backgroundWidth, 100);
            fileNameCopy[fileNameCopyLength - 1] = ' ';
            fileNameCopy[fileNameCopyLength] = '\0';
            text_to_buf(fileNameBufferPtr, fileNameCopy, backgroundWidth, backgroundWidth, colorTable[992]);
            fileNameCopyLength--;
            win_draw(win);
        } else if (keyCode < KEY_FIRST_INPUT_CHARACTER || keyCode > KEY_LAST_INPUT_CHARACTER || fileNameCopyLength >= 8) {
            if (keyCode == 502 && fileListLength != 0) {
                int mouseX;
                int mouseY;
                mouse_get_position(&mouseX, &mouseY);

                int selectedLine = (mouseY - y - FILE_DIALOG_FILE_LIST_Y) / text_height();
                if (selectedLine - 1 < 0) {
                    selectedLine = 0;
                }

                if (isScrollable || selectedLine < fileListLength) {
                    if (selectedLine >= FILE_DIALOG_LINE_COUNT) {
                        selectedLine = FILE_DIALOG_LINE_COUNT - 1;
                    }
                } else {
                    selectedLine = fileListLength - 1;
                }

                selectedFileIndex = selectedLine;
                if (selectedFileIndex == doubleClickSelectedFileIndex) {
                    gsound_play_sfx_file("ib1p1xx1");
                    strncpy(dest, fileList[selectedFileIndex + pageOffset], 16);

                    int index;
                    for (index = 0; index < 12; index++) {
                        if (dest[index] == '.' || dest[index] == '\0') {
                            break;
                        }
                    }

                    dest[index] = '\0';
                    rc = 2;
                } else {
                    doubleClickSelectedFileIndex = selectedFileIndex;
                    buf_fill(fileNameBufferPtr, text_width(fileNameCopy), cursorHeight, backgroundWidth, 100);
                    strncpy(fileNameCopy, fileList[selectedFileIndex + pageOffset], 16);

                    int index;
                    for (index = 0; index < 12; index++) {
                        if (fileNameCopy[index] == '.' || fileNameCopy[index] == '\0') {
                            break;
                        }
                    }

                    fileNameCopy[index] = '\0';
                    fileNameCopyLength = strlen(fileNameCopy);
                    fileNameCopy[fileNameCopyLength] = ' ';
                    fileNameCopy[fileNameCopyLength + 1] = '\0';

                    text_to_buf(fileNameBufferPtr, fileNameCopy, backgroundWidth, backgroundWidth, colorTable[992]);
                    PrntFlist(windowBuffer, fileList, pageOffset, fileListLength, selectedFileIndex, backgroundWidth);
                }
            } else if (keyCode == 506) {
                scrollDirection = FILE_DIALOG_SCROLL_DIRECTION_UP;
            } else if (keyCode == 504) {
                scrollDirection = FILE_DIALOG_SCROLL_DIRECTION_DOWN;
            } else {
                switch (keyCode) {
                case KEY_ARROW_UP:
                    pageOffset--;
                    if (pageOffset < 0) {
                        selectedFileIndex--;
                        if (selectedFileIndex < 0) {
                            selectedFileIndex = 0;
                        }
                        pageOffset = 0;
                    }
                    PrntFlist(windowBuffer, fileList, pageOffset, fileListLength, selectedFileIndex, backgroundWidth);
                    doubleClickSelectedFileIndex = -2;
                    break;
                case KEY_ARROW_DOWN:
                    if (isScrollable) {
                        pageOffset++;
                        if (pageOffset >= maxPageOffset) {
                            selectedFileIndex++;
                            if (selectedFileIndex >= FILE_DIALOG_LINE_COUNT) {
                                selectedFileIndex = FILE_DIALOG_LINE_COUNT - 1;
                            }
                            pageOffset = maxPageOffset;
                        }
                    } else {
                        selectedFileIndex++;
                        if (selectedFileIndex > maxPageOffset) {
                            selectedFileIndex = maxPageOffset;
                        }
                    }
                    PrntFlist(windowBuffer, fileList, pageOffset, fileListLength, selectedFileIndex, backgroundWidth);
                    doubleClickSelectedFileIndex = -2;
                    break;
                case KEY_HOME:
                    selectedFileIndex = 0;
                    pageOffset = 0;
                    PrntFlist(windowBuffer, fileList, pageOffset, fileListLength, selectedFileIndex, backgroundWidth);
                    doubleClickSelectedFileIndex = -2;
                    break;
                case KEY_END:
                    if (isScrollable) {
                        selectedFileIndex = 11;
                        pageOffset = maxPageOffset;
                    } else {
                        selectedFileIndex = maxPageOffset;
                        pageOffset = 0;
                    }
                    PrntFlist(windowBuffer, fileList, pageOffset, fileListLength, selectedFileIndex, backgroundWidth);
                    doubleClickSelectedFileIndex = -2;
                    break;
                }
            }
        } else if (isdoschar(keyCode)) {
            buf_fill(fileNameBufferPtr, text_width(fileNameCopy), cursorHeight, backgroundWidth, 100);

            fileNameCopy[fileNameCopyLength] = keyCode & 0xFF;
            fileNameCopy[fileNameCopyLength + 1] = ' ';
            fileNameCopy[fileNameCopyLength + 2] = '\0';
            text_to_buf(fileNameBufferPtr, fileNameCopy, backgroundWidth, backgroundWidth, colorTable[992]);
            fileNameCopyLength++;

            win_draw(win);
        }

        if (scrollDirection != FILE_DIALOG_SCROLL_DIRECTION_NONE) {
            unsigned int scrollDelay = 4;
            doubleClickSelectedFileIndex = -2;
            while (1) {
                unsigned int scrollTick = get_time();
                scrollCounter += 1;
                if ((!isScrolling && scrollCounter == 1) || (isScrolling && scrollCounter > 14.4)) {
                    isScrolling = true;

                    if (scrollCounter > 14.4) {
                        scrollDelay += 1;
                        if (scrollDelay > 24) {
                            scrollDelay = 24;
                        }
                    }

                    if (scrollDirection == FILE_DIALOG_SCROLL_DIRECTION_UP) {
                        pageOffset--;
                        if (pageOffset < 0) {
                            selectedFileIndex--;
                            if (selectedFileIndex < 0) {
                                selectedFileIndex = 0;
                            }
                            pageOffset = 0;
                        }
                    } else {
                        if (isScrollable) {
                            pageOffset++;
                            if (pageOffset > maxPageOffset) {
                                selectedFileIndex++;
                                if (selectedFileIndex >= FILE_DIALOG_LINE_COUNT) {
                                    selectedFileIndex = FILE_DIALOG_LINE_COUNT - 1;
                                }
                                pageOffset = maxPageOffset;
                            }
                        } else {
                            selectedFileIndex++;
                            if (selectedFileIndex > maxPageOffset) {
                                selectedFileIndex = maxPageOffset;
                            }
                        }
                    }

                    PrntFlist(windowBuffer, fileList, pageOffset, fileListLength, selectedFileIndex, backgroundWidth);
                    win_draw(win);
                }

                // NOTE: Original code is slightly different. For unknown reason
                // entire blinking stuff is placed into two different branches,
                // which only differs by amount of delay. Probably result of
                // using large blinking macro as there are no traces of inlined
                // function.
                blinkingCounter -= 1;
                if (blinkingCounter == 0) {
                    blinkingCounter = 3;

                    int color = blink ? 100 : colorTable[992];
                    blink = !blink;

                    buf_fill(fileNameBufferPtr + text_width(fileNameCopy) - cursorWidth, cursorWidth, cursorHeight - 2, backgroundWidth, color);
                }

                // FIXME: Missing windowRefresh makes blinking useless.

                unsigned int delay = (scrollCounter > 14.4) ? 1000 / scrollDelay : 1000 / 24;
                while (elapsed_time(scrollTick) < delay) {
                }

                if (game_user_wants_to_quit != 0) {
                    rc = 1;
                    break;
                }

                int key = get_input();
                if (key == 505 || key == 503) {
                    break;
                }

                renderPresent();
            }
        } else {
            blinkingCounter -= 1;
            if (blinkingCounter == 0) {
                blinkingCounter = 3;

                int color = blink ? 100 : colorTable[992];
                blink = !blink;

                buf_fill(fileNameBufferPtr + text_width(fileNameCopy) - cursorWidth, cursorWidth, cursorHeight - 2, backgroundWidth, color);
            }

            win_draw(win);

            doubleClickTimer--;
            if (doubleClickTimer == 0) {
                doubleClickTimer = FILE_DIALOG_DOUBLE_CLICK_DELAY;
                doubleClickSelectedFileIndex = -2;
            }

            while (elapsed_time(tick) < (1000 / 24)) {
            }
        }

        if (game_user_wants_to_quit != 0) {
            rc = 1;
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    endTextInput();

    if (rc == 0) {
        if (fileNameCopyLength != 0) {
            fileNameCopy[fileNameCopyLength] = '\0';
            strcpy(dest, fileNameCopy);
        } else {
            rc = 1;
        }
    } else {
        if (rc == 2) {
            rc = 0;
        }
    }

    win_delete(win);

    for (int index = 0; index < FILE_DIALOG_FRM_COUNT; index++) {
        art_ptr_unlock(frmHandles[index]);
    }

    message_exit(&messageList);
    text_font(oldFont);

    return rc;
}

// 0x41E8D8
static void PrntFlist(unsigned char* buffer, char** fileList, int pageOffset, int fileListLength, int selectedIndex, int pitch)
{
    int lineHeight = text_height();
    int y = FILE_DIALOG_FILE_LIST_Y;
    buf_fill(buffer + y * pitch + FILE_DIALOG_FILE_LIST_X, FILE_DIALOG_FILE_LIST_WIDTH, FILE_DIALOG_FILE_LIST_HEIGHT, pitch, 100);
    if (fileListLength != 0) {
        if (fileListLength - pageOffset > FILE_DIALOG_LINE_COUNT) {
            fileListLength = FILE_DIALOG_LINE_COUNT;
        }

        for (int index = 0; index < fileListLength; index++) {
            int color = index == selectedIndex ? colorTable[32747] : colorTable[992];
            text_to_buf(buffer + pitch * y + FILE_DIALOG_FILE_LIST_X, fileList[pageOffset + index], FILE_DIALOG_FILE_LIST_WIDTH, pitch, color);
            y += lineHeight;
        }
    }
}

} // namespace fallout
