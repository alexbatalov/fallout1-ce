#ifndef FALLOUT_PLIB_GNW_GNW_TYPES_H_
#define FALLOUT_PLIB_GNW_GNW_TYPES_H_

#include "plib/gnw/rect.h"

namespace fallout {

// The maximum number of buttons in one button group.
#define BUTTON_GROUP_BUTTON_LIST_CAPACITY 64

typedef enum WindowFlags {
    // Use system window flags which are set during game startup and does not
    // change afterwards.
    WINDOW_USE_DEFAULTS = 0x1,
    WINDOW_DONT_MOVE_TOP = 0x2,
    WINDOW_MOVE_ON_TOP = 0x4,
    WINDOW_HIDDEN = 0x8,
    // Sfall calls this Exclusive.
    WINDOW_MODAL = 0x10,
    WINDOW_TRANSPARENT = 0x20,
    WINDOW_FLAG_0x40 = 0x40,
    // Draggable?
    WINDOW_FLAG_0x80 = 0x80,
    WINDOW_MANAGED = 0x100,
} WindowFlags;

typedef enum ButtonFlags {
    BUTTON_FLAG_0x01 = 0x01,
    BUTTON_FLAG_0x02 = 0x02,
    BUTTON_FLAG_0x04 = 0x04,
    BUTTON_FLAG_DISABLED = 0x08,
    BUTTON_FLAG_0x10 = 0x10,
    BUTTON_FLAG_TRANSPARENT = 0x20,
    BUTTON_FLAG_0x40 = 0x40,
    BUTTON_FLAG_GRAPHIC = 0x010000,
    BUTTON_FLAG_CHECKED = 0x020000,
    BUTTON_FLAG_RADIO = 0x040000,
    BUTTON_FLAG_RIGHT_MOUSE_BUTTON_CONFIGURED = 0x080000,
} ButtonFlags;

typedef struct Button Button;
typedef struct ButtonGroup ButtonGroup;

typedef void WindowBlitProc(unsigned char* src, int width, int height, int srcPitch, unsigned char* dest, int destPitch);
typedef void ButtonCallback(int btn, int keyCode);
typedef void RadioButtonCallback(int btn);

typedef struct MenuPulldown {
    Rect rect;
    int keyCode;
    int itemsLength;
    char** items;
    int foregroundColor;
    int backgroundColor;
} MenuPulldown;

typedef struct MenuBar {
    int win;
    Rect rect;
    int pulldownsLength;
    MenuPulldown pulldowns[15];
    int foregroundColor;
    int backgroundColor;
} MenuBar;

typedef struct Window {
    int id;
    int flags;
    Rect rect;
    int width;
    int height;
    int color;
    int tx;
    int ty;
    unsigned char* buffer;
    Button* buttonListHead;
    Button* hoveredButton;
    Button* clickedButton;
    MenuBar* menuBar;
    WindowBlitProc* blitProc;
} Window;

typedef struct Button {
    int id;
    int flags;
    Rect rect;
    int mouseEnterEventCode;
    int mouseExitEventCode;
    int lefMouseDownEventCode;
    int leftMouseUpEventCode;
    int rightMouseDownEventCode;
    int rightMouseUpEventCode;
    unsigned char* normalImage;
    unsigned char* pressedImage;
    unsigned char* hoverImage;
    unsigned char* disabledNormalImage;
    unsigned char* disabledPressedImage;
    unsigned char* disabledHoverImage;
    unsigned char* currentImage;
    unsigned char* mask;
    ButtonCallback* mouseEnterProc;
    ButtonCallback* mouseExitProc;
    ButtonCallback* leftMouseDownProc;
    ButtonCallback* leftMouseUpProc;
    ButtonCallback* rightMouseDownProc;
    ButtonCallback* rightMouseUpProc;
    ButtonCallback* pressSoundFunc;
    ButtonCallback* releaseSoundFunc;
    ButtonGroup* buttonGroup;
    Button* prev;
    Button* next;
} Button;

typedef struct ButtonGroup {
    int maxChecked;
    int currChecked;
    RadioButtonCallback* func;
    int buttonsLength;
    Button* buttons[BUTTON_GROUP_BUTTON_LIST_CAPACITY];
} ButtonGroup;

} // namespace fallout

#endif /* FALLOUT_PLIB_GNW_GNW_TYPES_H_ */
