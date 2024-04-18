#include "plib/gnw/mouse.h"

#include "plib/color/color.h"
#include "plib/gnw/dxinput.h"
#include "plib/gnw/gnw.h"
#include "plib/gnw/input.h"
#include "plib/gnw/memory.h"
#include "plib/gnw/svga.h"
#include "plib/gnw/touch.h"
#include "plib/gnw/vcr.h"

namespace fallout {

static void mouse_colorize();
static void mouse_anim();
static void mouse_clip();

// The default mouse cursor buffer.
//
// Initially it contains color codes, which will be replaced at startup
// according to loaded palette.
//
// Available color codes:
// - 0: transparent
// - 1: white
// - 15:  black
//
// 0x539D80
static unsigned char or_mask[MOUSE_DEFAULT_CURSOR_SIZE] = {
    // clang-format off
    1,  1,  1,  1,  1,  1,  1, 0,
    1, 15, 15, 15, 15, 15,  1, 0,
    1, 15, 15, 15, 15,  1,  1, 0,
    1, 15, 15, 15, 15,  1,  1, 0,
    1, 15, 15, 15, 15, 15,  1, 1,
    1, 15,  1,  1, 15, 15, 15, 1,
    1,  1,  1,  1,  1, 15, 15, 1,
    0,  0,  0,  0,  1,  1,  1, 1,
    // clang-format on
};

// 0x539DC0
static int mouse_idling = 0;

// 0x539DC4
static unsigned char* mouse_buf = NULL;

// 0x539DC8
static unsigned char* mouse_shape = NULL;

// 0x539DCC
static unsigned char* mouse_fptr = NULL;

// 0x539DD0
static double mouse_sensitivity = 1.0;

// 0x539DDC
static int last_buttons = 0;

// 0x671F18
static bool mouse_is_hidden;

// 0x671F1C
static int raw_x;

// 0x671F20
static int mouse_length;

// 0x671F24
static int raw_y;

// 0x671F28
static int raw_buttons;

// 0x671F2C
static int mouse_y;

// 0x671F30
static int mouse_x;

// 0x671F34
static bool mouse_disabled;

// 0x671F38
static int mouse_buttons;

// 0x671F10
static unsigned int mouse_speed;

// 0x671F3C
static int mouse_curr_frame;

// 0x671F40
static bool have_mouse;

// 0x671F44
static int mouse_full;

// 0x671F48
static int mouse_width;

// 0x671F4C
static int mouse_num_frames;

// 0x671F50
static int mouse_hoty;

// 0x671F54
static int mouse_hotx;

// 0x671F14
static unsigned int mouse_idle_start_time;

// 0x671F58
ScreenTransBlitFunc* mouse_blit_trans;

// 0x671F5C
ScreenBlitFunc* mouse_blit;

// 0x671F60
static char mouse_trans;

static int gMouseWheelX = 0;
static int gMouseWheelY = 0;

static int gWindowScale = 1;

// 0x4B4780
int GNW_mouse_init()
{
    have_mouse = false;
    mouse_disabled = false;

    mouse_is_hidden = true;

    mouse_colorize();

    if (mouse_set_shape(NULL, 0, 0, 0, 0, 0, 0) == -1) {
        return -1;
    }

    if (!dxinput_acquire_mouse()) {
        return -1;
    }

    have_mouse = true;
    mouse_x = scr_size.lrx / 2;
    mouse_y = scr_size.lry / 2;
    raw_x = scr_size.lrx / 2;
    raw_y = scr_size.lry / 2;
    mouse_idle_start_time = get_time();

    return 0;
}

// 0x4B4818
void GNW_mouse_exit()
{
    dxinput_unacquire_mouse();

    if (mouse_buf != NULL) {
        mem_free(mouse_buf);
        mouse_buf = NULL;
    }

    if (mouse_fptr != NULL) {
        remove_bk_process(mouse_anim);
        mouse_fptr = NULL;
    }
}

// 0x4B485C
static void mouse_colorize()
{
    for (int index = 0; index < 64; index++) {
        switch (or_mask[index]) {
        case 0:
            or_mask[index] = colorTable[0];
            break;
        case 1:
            or_mask[index] = colorTable[8456];
            break;
        case 15:
            or_mask[index] = colorTable[32767];
            break;
        }
    }
}

// 0x4B48A4
void mouse_get_shape(unsigned char** buf, int* width, int* length, int* full, int* hotx, int* hoty, char* trans)
{
    *buf = mouse_shape;
    *width = mouse_width;
    *length = mouse_length;
    *full = mouse_full;
    *hotx = mouse_hotx;
    *hoty = mouse_hoty;
    *trans = mouse_trans;
}

// 0x4B48EC
int mouse_set_shape(unsigned char* buf, int width, int length, int full, int hotx, int hoty, char trans)
{
    Rect rect;
    unsigned char* v9;
    int v11, v12;
    int v7, v8;

    v7 = hotx;
    v8 = hoty;
    v9 = buf;

    if (buf == NULL) {
        // NOTE: Original code looks tail recursion optimization.
        return mouse_set_shape(or_mask, MOUSE_DEFAULT_CURSOR_WIDTH, MOUSE_DEFAULT_CURSOR_HEIGHT, MOUSE_DEFAULT_CURSOR_WIDTH, 1, 1, colorTable[0]);
    }

    bool cursorWasHidden = mouse_is_hidden;
    if (!mouse_is_hidden && have_mouse) {
        mouse_is_hidden = true;
        mouse_get_rect(&rect);
        win_refresh_all(&rect);
    }

    if (width != mouse_width || length != mouse_length) {
        unsigned char* buf = (unsigned char*)mem_malloc(width * length);
        if (buf == NULL) {
            if (!cursorWasHidden) {
                mouse_show();
            }
            return -1;
        }

        if (mouse_buf != NULL) {
            mem_free(mouse_buf);
        }

        mouse_buf = buf;
    }

    mouse_width = width;
    mouse_length = length;
    mouse_full = full;
    mouse_shape = v9;
    mouse_trans = trans;

    if (mouse_fptr) {
        remove_bk_process(mouse_anim);
        mouse_fptr = NULL;
    }

    v11 = mouse_hotx - v7;
    mouse_hotx = v7;

    mouse_x += v11;

    v12 = mouse_hoty - v8;
    mouse_hoty = v8;

    mouse_y += v12;

    mouse_clip();

    if (!cursorWasHidden) {
        mouse_show();
    }

    raw_x = mouse_x;
    raw_y = mouse_y;

    return 0;
}

// 0x4B4A4C
int mouse_get_anim(unsigned char** frames, int* num_frames, int* width, int* length, int* hotx, int* hoty, char* trans, int* speed)
{
    if (mouse_fptr == NULL) {
        return -1;
    }

    *frames = mouse_fptr;
    *num_frames = mouse_num_frames;
    *width = mouse_width;
    *length = mouse_length;
    *hotx = mouse_hotx;
    *hoty = mouse_hoty;
    *trans = mouse_trans;
    *speed = mouse_speed;

    return 0;
}

// 0x4B4AAC
int mouse_set_anim_frames(unsigned char* frames, int num_frames, int start_frame, int width, int length, int hotx, int hoty, char trans, int speed)
{
    if (mouse_set_shape(frames + start_frame * width * length, width, length, width, hotx, hoty, trans) == -1) {
        return -1;
    }

    mouse_fptr = frames;
    mouse_num_frames = num_frames;
    mouse_curr_frame = start_frame;
    mouse_speed = speed;

    add_bk_process(mouse_anim);

    return 0;
}

// 0x4B4B10
static void mouse_anim()
{
    // 0x539DD8
    static unsigned int ticker = 0;

    if (elapsed_time(ticker) >= mouse_speed) {
        ticker = get_time();

        if (++mouse_curr_frame == mouse_num_frames) {
            mouse_curr_frame = 0;
        }

        mouse_shape = mouse_width * mouse_curr_frame * mouse_length + mouse_fptr;

        if (!mouse_is_hidden) {
            mouse_show();
        }
    }
}

// 0x4B4B88
void mouse_show()
{
    int i;
    unsigned char* v2;
    int v7, v8;
    int v9, v10;
    int v4;
    unsigned char v6;
    int v3;

    v2 = mouse_buf;
    if (have_mouse) {
        if (!mouse_blit_trans || !mouse_is_hidden) {
            win_get_mouse_buf(mouse_buf);
            v2 = mouse_buf;
            v3 = 0;

            for (i = 0; i < mouse_length; i++) {
                for (v4 = 0; v4 < mouse_width; v4++) {
                    v6 = mouse_shape[i * mouse_full + v4];
                    if (v6 != mouse_trans) {
                        v2[v3] = v6;
                    }
                    v3++;
                }
            }
        }

        if (mouse_x >= scr_size.ulx) {
            if (mouse_width + mouse_x - 1 <= scr_size.lrx) {
                v8 = mouse_width;
                v7 = 0;
            } else {
                v7 = 0;
                v8 = scr_size.lrx - mouse_x + 1;
            }
        } else {
            v7 = scr_size.ulx - mouse_x;
            v8 = mouse_width - (scr_size.ulx - mouse_x);
        }

        if (mouse_y >= scr_size.uly) {
            if (mouse_length + mouse_y - 1 <= scr_size.lry) {
                v9 = 0;
                v10 = mouse_length;
            } else {
                v9 = 0;
                v10 = scr_size.lry - mouse_y + 1;
            }
        } else {
            v9 = scr_size.uly - mouse_y;
            v10 = mouse_length - (scr_size.uly - mouse_y);
        }

        mouse_buf = v2;
        if (mouse_blit_trans && mouse_is_hidden) {
            mouse_blit_trans(mouse_shape, mouse_full, mouse_length, v7, v9, v8, v10, v7 + mouse_x, v9 + mouse_y, mouse_trans);
        } else {
            mouse_blit(mouse_buf, mouse_width, mouse_length, v7, v9, v8, v10, v7 + mouse_x, v9 + mouse_y);
        }

        v2 = mouse_buf;
        mouse_is_hidden = false;
    }
    mouse_buf = v2;
}

// 0x4B4D70
void mouse_hide()
{
    Rect rect;

    if (have_mouse) {
        if (!mouse_is_hidden) {
            rect.ulx = mouse_x;
            rect.uly = mouse_y;
            rect.lrx = mouse_x + mouse_width - 1;
            rect.lry = mouse_y + mouse_length - 1;

            mouse_is_hidden = true;
            win_refresh_all(&rect);
        }
    }
}

// 0x4B4DD8
void mouse_info()
{
    if (!have_mouse) {
        return;
    }

    if (mouse_is_hidden) {
        return;
    }

    if (mouse_disabled) {
        return;
    }

    Gesture gesture;
    if (touch_get_gesture(&gesture)) {
        static int prevx;
        static int prevy;

        switch (gesture.type) {
        case kTap:
            if (gesture.numberOfTouches == 1) {
                mouse_simulate_input(0, 0, MOUSE_STATE_LEFT_BUTTON_DOWN);
            } else if (gesture.numberOfTouches == 2) {
                mouse_simulate_input(0, 0, MOUSE_STATE_RIGHT_BUTTON_DOWN);
            }
            break;
        case kLongPress:
        case kPan:
            if (gesture.state == kBegan) {
                prevx = gesture.x;
                prevy = gesture.y;
            }

            if (gesture.type == kLongPress) {
                if (gesture.numberOfTouches == 1) {
                    mouse_simulate_input(gesture.x - prevx, gesture.y - prevy, MOUSE_STATE_LEFT_BUTTON_DOWN);
                } else if (gesture.numberOfTouches == 2) {
                    mouse_simulate_input(gesture.x - prevx, gesture.y - prevy, MOUSE_STATE_RIGHT_BUTTON_DOWN);
                }
            } else if (gesture.type == kPan) {
                if (gesture.numberOfTouches == 1) {
                    mouse_simulate_input(gesture.x - prevx, gesture.y - prevy, 0);
                } else if (gesture.numberOfTouches == 2) {
                    gMouseWheelX = (prevx - gesture.x) / 2;
                    gMouseWheelY = (gesture.y - prevy) / 2;

                    if (gMouseWheelX != 0 || gMouseWheelY != 0) {
                        mouse_buttons |= MOUSE_EVENT_WHEEL;
                        raw_buttons |= MOUSE_EVENT_WHEEL;
                    }
                }
            }

            prevx = gesture.x;
            prevy = gesture.y;
            break;
        }

        return;
    }

    int x;
    int y;
    int buttons = 0;

    MouseData mouseData;
    if (dxinput_get_mouse_state(&mouseData)) {
        x = mouseData.x;
        y = mouseData.y;

        if (mouseData.buttons[0] == 1) {
            buttons |= MOUSE_STATE_LEFT_BUTTON_DOWN;
        }

        if (mouseData.buttons[1] == 1) {
            buttons |= MOUSE_STATE_RIGHT_BUTTON_DOWN;
        }
    } else {
        x = 0;
        y = 0;
    }

    // Adjust for mouse senstivity.
    x = (int)(x * mouse_sensitivity);
    y = (int)(y * mouse_sensitivity);

    if (vcr_state == VCR_STATE_PLAYING) {
        if (((vcr_terminate_flags & VCR_TERMINATE_ON_MOUSE_PRESS) != 0 && buttons != 0)
            || ((vcr_terminate_flags & VCR_TERMINATE_ON_MOUSE_MOVE) != 0 && (x != 0 || y != 0))) {
            vcr_terminated_condition = VCR_PLAYBACK_COMPLETION_REASON_TERMINATED;
            vcr_stop();
            return;
        }
        x = 0;
        y = 0;
        buttons = last_buttons;
    }

    mouse_simulate_input(x, y, buttons);

    // TODO: Move to `_mouse_simulate_input`.
    // TODO: Record wheel event in VCR.
    gMouseWheelX = mouseData.wheelX;
    gMouseWheelY = mouseData.wheelY;

    if (gMouseWheelX != 0 || gMouseWheelY != 0) {
        mouse_buttons |= MOUSE_EVENT_WHEEL;
        raw_buttons |= MOUSE_EVENT_WHEEL;
    }
}

// 0x4B4ECC
void mouse_simulate_input(int delta_x, int delta_y, int buttons)
{
    // 0x671F64
    static unsigned int right_time;

    // 0x671F68
    static unsigned int left_time;

    // 0x671F6C
    static int old;

    if (!have_mouse || mouse_is_hidden) {
        return;
    }

    if (delta_x || delta_y || buttons != last_buttons) {
        if (vcr_state == 0) {
            if (vcr_buffer_index == VCR_BUFFER_CAPACITY - 1) {
                vcr_dump_buffer();
            }

            VcrEntry* vcrEntry = &(vcr_buffer[vcr_buffer_index]);
            vcrEntry->type = VCR_ENTRY_TYPE_MOUSE_EVENT;
            vcrEntry->time = vcr_time;
            vcrEntry->counter = vcr_counter;
            vcrEntry->mouseEvent.dx = delta_x;
            vcrEntry->mouseEvent.dy = delta_y;
            vcrEntry->mouseEvent.buttons = buttons;

            vcr_buffer_index++;
        }
    } else {
        if (last_buttons == 0) {
            if (!mouse_idling) {
                mouse_idle_start_time = get_time();
                mouse_idling = 1;
            }

            last_buttons = 0;
            raw_buttons = 0;
            mouse_buttons = 0;

            return;
        }
    }

    mouse_idling = 0;
    last_buttons = buttons;
    old = mouse_buttons;
    mouse_buttons = 0;

    if ((old & MOUSE_EVENT_LEFT_BUTTON_DOWN_REPEAT) != 0) {
        if ((buttons & 0x01) != 0) {
            mouse_buttons |= MOUSE_EVENT_LEFT_BUTTON_REPEAT;

            if (elapsed_time(left_time) > BUTTON_REPEAT_TIME) {
                mouse_buttons |= MOUSE_EVENT_LEFT_BUTTON_DOWN;
                left_time = get_time();
            }
        } else {
            mouse_buttons |= MOUSE_EVENT_LEFT_BUTTON_UP;
        }
    } else {
        if ((buttons & 0x01) != 0) {
            mouse_buttons |= MOUSE_EVENT_LEFT_BUTTON_DOWN;
            left_time = get_time();
        }
    }

    if ((old & MOUSE_EVENT_RIGHT_BUTTON_DOWN_REPEAT) != 0) {
        if ((buttons & 0x02) != 0) {
            mouse_buttons |= MOUSE_EVENT_RIGHT_BUTTON_REPEAT;
            if (elapsed_time(right_time) > BUTTON_REPEAT_TIME) {
                mouse_buttons |= MOUSE_EVENT_RIGHT_BUTTON_DOWN;
                right_time = get_time();
            }
        } else {
            mouse_buttons |= MOUSE_EVENT_RIGHT_BUTTON_UP;
        }
    } else {
        if (buttons & 0x02) {
            mouse_buttons |= MOUSE_EVENT_RIGHT_BUTTON_DOWN;
            right_time = get_time();
        }
    }

    raw_buttons = mouse_buttons;

    if (delta_x != 0 || delta_y != 0) {
        Rect mouseRect;
        mouseRect.ulx = mouse_x;
        mouseRect.uly = mouse_y;
        mouseRect.lrx = mouse_width + mouse_x - 1;
        mouseRect.lry = mouse_length + mouse_y - 1;

        mouse_x += delta_x;
        mouse_y += delta_y;
        mouse_clip();

        win_refresh_all(&mouseRect);

        mouse_show();

        raw_x = mouse_x;
        raw_y = mouse_y;
    }
}

// 0x4B5154
bool mouse_in(int left, int top, int right, int bottom)
{
    if (!have_mouse) {
        return false;
    }

    return mouse_length + mouse_y > top
        && right >= mouse_x
        && mouse_width + mouse_x > left
        && bottom >= mouse_y;
}

// 0x4B51C0
bool mouse_click_in(int left, int top, int right, int bottom)
{
    if (!have_mouse) {
        return false;
    }

    return mouse_hoty + mouse_y >= top
        && mouse_hotx + mouse_x <= right
        && mouse_hotx + mouse_x >= left
        && mouse_hoty + mouse_y <= bottom;
}

// 0x4B522C
void mouse_get_rect(Rect* rect)
{
    rect->ulx = mouse_x;
    rect->uly = mouse_y;
    rect->lrx = mouse_width + mouse_x - 1;
    rect->lry = mouse_length + mouse_y - 1;
}

// 0x4B5268
void mouse_get_position(int* x, int* y)
{
    *x = mouse_hotx + mouse_x;
    *y = mouse_hoty + mouse_y;
}

// 0x4B528C
void mouse_set_position(int x, int y)
{
    mouse_x = x - mouse_hotx;
    mouse_y = y - mouse_hoty;
    raw_y = y - mouse_hoty;
    raw_x = x - mouse_hotx;
    mouse_clip();
}

// 0x4B52C0
static void mouse_clip()
{
    if (mouse_hotx + mouse_x < scr_size.ulx) {
        mouse_x = scr_size.ulx - mouse_hotx;
    } else if (mouse_hotx + mouse_x > scr_size.lrx) {
        mouse_x = scr_size.lrx - mouse_hotx;
    }

    if (mouse_hoty + mouse_y < scr_size.uly) {
        mouse_y = scr_size.uly - mouse_hoty;
    } else if (mouse_hoty + mouse_y > scr_size.lry) {
        mouse_y = scr_size.lry - mouse_hoty;
    }
}

// 0x4B5328
int mouse_get_buttons()
{
    return mouse_buttons;
}

// 0x4B5330
bool mouse_hidden()
{
    return mouse_is_hidden;
}

// 0x4B5338
void mouse_get_hotspot(int* hotx, int* hoty)
{
    *hotx = mouse_hotx;
    *hoty = mouse_hoty;
}

// 0x4B534C
void mouse_set_hotspot(int hotx, int hoty)
{
    bool mh;

    mh = mouse_is_hidden;
    if (!mouse_is_hidden) {
        mouse_hide();
    }

    mouse_x += mouse_hotx - hotx;
    mouse_y += mouse_hoty - hoty;
    mouse_hotx = hotx;
    mouse_hoty = hoty;

    if (mh) {
        mouse_show();
    }
}

// 0x4B53DC
bool mouse_query_exist()
{
    return have_mouse;
}

// 0x4B53E4
void mouse_get_raw_state(int* x, int* y, int* buttons)
{
    MouseData mouseData;
    if (!dxinput_get_mouse_state(&mouseData)) {
        mouseData.x = 0;
        mouseData.y = 0;
        mouseData.buttons[0] = (mouse_buttons & MOUSE_EVENT_LEFT_BUTTON_DOWN) != 0;
        mouseData.buttons[1] = (mouse_buttons & MOUSE_EVENT_RIGHT_BUTTON_DOWN) != 0;
    }

    raw_buttons = 0;
    raw_x += mouseData.x;
    raw_y += mouseData.y;

    if (mouseData.buttons[0] != 0) {
        raw_buttons |= MOUSE_EVENT_LEFT_BUTTON_DOWN;
    }

    if (mouseData.buttons[1] != 0) {
        raw_buttons |= MOUSE_EVENT_RIGHT_BUTTON_DOWN;
    }

    *x = raw_x;
    *y = raw_y;
    *buttons = raw_buttons;
}

// 0x4B54A4
void mouse_disable()
{
    mouse_disabled = true;
}

// 0x4B54B0
void mouse_enable()
{
    mouse_disabled = false;
}

// 0x4B54BC
bool mouse_is_disabled()
{
    return mouse_disabled;
}

// 0x4B54C4
void mouse_set_sensitivity(double value)
{
    if (value > 0 && value < 2.0) {
        mouse_sensitivity = value / gWindowScale;
    }
}

// 0x4B54F4
double mouse_get_sensitivity()
{
    return mouse_sensitivity;
}

// 0x4B54FC
unsigned int mouse_elapsed_time()
{
    if (mouse_idling) {
        if (have_mouse && !mouse_is_hidden && !mouse_disabled) {
            return elapsed_time(mouse_idle_start_time);
        }
        mouse_idling = false;
    }
    return 0;
}

// 0x4B553C
void mouse_reset_elapsed_time()
{
    if (mouse_idling) {
        mouse_idling = false;
    }
}

void mouseGetPositionInWindow(int win, int* x, int* y)
{
    mouse_get_position(x, y);

    Window* window = GNW_find(win);
    if (window != NULL) {
        *x -= window->rect.ulx;
        *y -= window->rect.uly;
    }
}

bool mouseHitTestInWindow(int win, int left, int top, int right, int bottom)
{
    Window* window = GNW_find(win);
    if (window != NULL) {
        left += window->rect.ulx;
        top += window->rect.uly;
        right += window->rect.ulx;
        bottom += window->rect.uly;
    }

    return mouse_click_in(left, top, right, bottom);
}

void mouseGetWheel(int* x, int* y)
{
    *x = gMouseWheelX;
    *y = gMouseWheelY;
}

void convertMouseWheelToArrowKey(int* keyCodePtr)
{
    if (*keyCodePtr == -1) {
        if ((mouse_get_buttons() & MOUSE_EVENT_WHEEL) != 0) {
            int wheelX;
            int wheelY;
            mouseGetWheel(&wheelX, &wheelY);

            if (wheelY > 0) {
                *keyCodePtr = KEY_ARROW_UP;
            } else if (wheelY < 0) {
                *keyCodePtr = KEY_ARROW_DOWN;
            }
        }
    }
}

void mouseSetWindowScale(int scale)
{
    gWindowScale = scale;
}

} // namespace fallout
