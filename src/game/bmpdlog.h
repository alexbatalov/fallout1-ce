#ifndef FALLOUT_GAME_BMPDLOG_H_
#define FALLOUT_GAME_BMPDLOG_H_

namespace fallout {

typedef enum DialogBoxOptions {
    DIALOG_BOX_LARGE = 0x01,
    DIALOG_BOX_MEDIUM = 0x02,
    DIALOG_BOX_NO_HORIZONTAL_CENTERING = 0x04,
    DIALOG_BOX_NO_VERTICAL_CENTERING = 0x08,
    DIALOG_BOX_YES_NO = 0x10,
    DIALOG_BOX_0x20 = 0x20,
} DialogBoxOptions;

typedef enum DialogType {
    DIALOG_TYPE_MEDIUM,
    DIALOG_TYPE_LARGE,
    DIALOG_TYPE_COUNT,
} DialogType;

typedef enum FileDialogFrm {
    FILE_DIALOG_FRM_BACKGROUND,
    FILE_DIALOG_FRM_LITTLE_RED_BUTTON_NORMAL,
    FILE_DIALOG_FRM_LITTLE_RED_BUTTON_PRESSED,
    FILE_DIALOG_FRM_SCROLL_DOWN_ARROW_NORMAL,
    FILE_DIALOG_FRM_SCROLL_DOWN_ARROW_PRESSED,
    FILE_DIALOG_FRM_SCROLL_UP_ARROW_NORMAL,
    FILE_DIALOG_FRM_SCROLL_UP_ARROW_PRESSED,
    FILE_DIALOG_FRM_COUNT,
} FileDialogFrm;

typedef enum FileDialogScrollDirection {
    FILE_DIALOG_SCROLL_DIRECTION_NONE,
    FILE_DIALOG_SCROLL_DIRECTION_UP,
    FILE_DIALOG_SCROLL_DIRECTION_DOWN,
} FileDialogScrollDirection;

extern int dbox[DIALOG_TYPE_COUNT];
extern int ytable[DIALOG_TYPE_COUNT];
extern int xtable[DIALOG_TYPE_COUNT];
extern int doneY[DIALOG_TYPE_COUNT];
extern int doneX[DIALOG_TYPE_COUNT];
extern int dblines[DIALOG_TYPE_COUNT];
extern int flgids[FILE_DIALOG_FRM_COUNT];
extern int flgids2[FILE_DIALOG_FRM_COUNT];

int dialog_out(const char* title, const char** body, int bodyLength, int x, int y, int titleColor, const char* a8, int bodyColor, int flags);
int file_dialog(char* title, char** fileList, char* dest, int fileListLength, int x, int y, int flags);
int save_file_dialog(char* title, char** fileList, char* dest, int fileListLength, int x, int y, int flags);

} // namespace fallout

#endif /* FALLOUT_GAME_BMPDLOG_H_ */
