#ifndef FALLOUT_PLIB_GNW_MOUSE_H_
#define FALLOUT_PLIB_GNW_MOUSE_H_

#include "plib/gnw/rect.h"
#include "plib/gnw/svga_types.h"

namespace fallout {

#define MOUSE_DEFAULT_CURSOR_WIDTH 8
#define MOUSE_DEFAULT_CURSOR_HEIGHT 8
#define MOUSE_DEFAULT_CURSOR_SIZE (MOUSE_DEFAULT_CURSOR_WIDTH * MOUSE_DEFAULT_CURSOR_HEIGHT)

#define MOUSE_STATE_LEFT_BUTTON_DOWN 0x01
#define MOUSE_STATE_RIGHT_BUTTON_DOWN 0x02

#define MOUSE_EVENT_LEFT_BUTTON_DOWN 0x01
#define MOUSE_EVENT_RIGHT_BUTTON_DOWN 0x02
#define MOUSE_EVENT_LEFT_BUTTON_REPEAT 0x04
#define MOUSE_EVENT_RIGHT_BUTTON_REPEAT 0x08
#define MOUSE_EVENT_LEFT_BUTTON_UP 0x10
#define MOUSE_EVENT_RIGHT_BUTTON_UP 0x20
#define MOUSE_EVENT_ANY_BUTTON_DOWN (MOUSE_EVENT_LEFT_BUTTON_DOWN | MOUSE_EVENT_RIGHT_BUTTON_DOWN)
#define MOUSE_EVENT_ANY_BUTTON_REPEAT (MOUSE_EVENT_LEFT_BUTTON_REPEAT | MOUSE_EVENT_RIGHT_BUTTON_REPEAT)
#define MOUSE_EVENT_ANY_BUTTON_UP (MOUSE_EVENT_LEFT_BUTTON_UP | MOUSE_EVENT_RIGHT_BUTTON_UP)
#define MOUSE_EVENT_LEFT_BUTTON_DOWN_REPEAT (MOUSE_EVENT_LEFT_BUTTON_DOWN | MOUSE_EVENT_LEFT_BUTTON_REPEAT)
#define MOUSE_EVENT_RIGHT_BUTTON_DOWN_REPEAT (MOUSE_EVENT_RIGHT_BUTTON_DOWN | MOUSE_EVENT_RIGHT_BUTTON_REPEAT)
#define MOUSE_EVENT_WHEEL 0x40

#define BUTTON_REPEAT_TIME 250

extern ScreenTransBlitFunc* mouse_blit_trans;
extern ScreenBlitFunc* mouse_blit;

int GNW_mouse_init();
void GNW_mouse_exit();
void mouse_get_shape(unsigned char** buf, int* width, int* length, int* full, int* hotx, int* hoty, char* trans);
int mouse_set_shape(unsigned char* buf, int width, int length, int full, int hotx, int hoty, char trans);
int mouse_get_anim(unsigned char** frames, int* num_frames, int* width, int* length, int* hotx, int* hoty, char* trans, int* speed);
int mouse_set_anim_frames(unsigned char* frames, int num_frames, int start_frame, int width, int length, int hotx, int hoty, char trans, int speed);
void mouse_show();
void mouse_hide();
void mouse_info();
void mouse_simulate_input(int delta_x, int delta_y, int buttons);
bool mouse_in(int left, int top, int right, int bottom);
bool mouse_click_in(int left, int top, int right, int bottom);
void mouse_get_rect(Rect* rect);
void mouse_get_position(int* x, int* y);
void mouse_set_position(int x, int y);
int mouse_get_buttons();
bool mouse_hidden();
void mouse_get_hotspot(int* hotx, int* hoty);
void mouse_set_hotspot(int hotx, int hoty);
bool mouse_query_exist();
void mouse_get_raw_state(int* x, int* y, int* buttons);
void mouse_disable();
void mouse_enable();
bool mouse_is_disabled();
void mouse_set_sensitivity(double value);
double mouse_get_sensitivity();
unsigned int mouse_elapsed_time();
void mouse_reset_elapsed_time();

void mouseGetPositionInWindow(int win, int* x, int* y);
bool mouseHitTestInWindow(int win, int left, int top, int right, int bottom);
void mouseGetWheel(int* x, int* y);
void convertMouseWheelToArrowKey(int* keyCodePtr);
void mouseSetWindowScale(int scale);

} // namespace fallout

#endif /* FALLOUT_PLIB_GNW_MOUSE_H_ */
