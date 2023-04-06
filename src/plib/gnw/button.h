#ifndef FALLOUT_PLIB_GNW_BUTTON_H_
#define FALLOUT_PLIB_GNW_BUTTON_H_

#include "plib/gnw/gnw_types.h"

namespace fallout {

int win_register_button(int win, int x, int y, int width, int height, int mouseEnterEventCode, int mouseExitEventCode, int mouseDownEventCode, int mouseUpEventCode, unsigned char* up, unsigned char* dn, unsigned char* hover, int flags);
int win_register_text_button(int win, int x, int y, int mouseEnterEventCode, int mouseExitEventCode, int mouseDownEventCode, int mouseUpEventCode, const char* title, int flags);
int win_register_button_disable(int btn, unsigned char* up, unsigned char* down, unsigned char* hover);
int win_register_button_image(int btn, unsigned char* up, unsigned char* down, unsigned char* hover, bool draw);
int win_register_button_func(int btn, ButtonCallback* mouseEnterProc, ButtonCallback* mouseExitProc, ButtonCallback* mouseDownProc, ButtonCallback* mouseUpProc);
int win_register_right_button(int btn, int rightMouseDownEventCode, int rightMouseUpEventCode, ButtonCallback* rightMouseDownProc, ButtonCallback* rightMouseUpProc);
int win_register_button_sound_func(int btn, ButtonCallback* pressSoundFunc, ButtonCallback* releaseSoundFunc);
int win_register_button_mask(int btn, unsigned char* mask);
bool win_button_down(int btn);
int GNW_check_buttons(Window* window, int* keyCodePtr);
int win_button_winID(int btn);
int win_last_button_winID();
int win_delete_button(int btn);
void GNW_delete_button(Button* ptr);
void win_delete_button_win(int btn, int inputEvent);
int button_new_id();
int win_enable_button(int btn);
int win_disable_button(int btn);
int win_set_button_rest_state(int btn, bool checked, int flags);
int win_group_check_buttons(int buttonCount, int* btns, int maxChecked, RadioButtonCallback* func);
int win_group_radio_buttons(int buttonCount, int* btns);
void GNW_button_refresh(Window* window, Rect* rect);
int win_button_press_and_release(int btn);

} // namespace fallout

#endif /* FALLOUT_PLIB_GNW_BUTTON_H_ */
