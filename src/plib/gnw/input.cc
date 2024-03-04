#include "plib/gnw/input.h"

#include <limits.h>
#include <stdio.h>

#include "audio_engine.h"
#include "platform_compat.h"
#include "plib/color/color.h"
#include "plib/gnw/button.h"
#include "plib/gnw/dxinput.h"
#include "plib/gnw/gnw.h"
#include "plib/gnw/grbuf.h"
#include "plib/gnw/intrface.h"
#include "plib/gnw/memory.h"
#include "plib/gnw/svga.h"
#include "plib/gnw/text.h"
#include "plib/gnw/touch.h"
#include "plib/gnw/vcr.h"
#include "plib/gnw/winmain.h"

namespace fallout {

typedef struct GNW95RepeatStruct {
    // Time when appropriate key was pressed down or -1 if it's up.
    unsigned int time;
    unsigned short count;
} GNW95RepeatStruct;

typedef struct inputdata {
    // This is either logical key or input event id, which can be either
    // character code pressed or some other numbers used throughout the
    // game interface.
    int input;
    int mx;
    int my;
} inputdata;

typedef struct funcdata {
    unsigned int flags;
    BackgroundProcess* f;
    struct funcdata* next;
} funcdata;

typedef funcdata* FuncPtr;

static int get_input_buffer();
static void pause_game();
static int default_pause_window();
static void buf_blit(unsigned char* src, unsigned int src_pitch, unsigned int a3, unsigned int x, unsigned int y, unsigned int width, unsigned int height, unsigned int dest_x, unsigned int dest_y);
static void GNW95_build_key_map();
static void GNW95_process_key(KeyboardData* data);

static void idleImpl();

// 0x539D6C
static IdleFunc* idle_func = NULL;

// 0x539D70
static FocusFunc* focus_func = NULL;

// 0x539D74
static unsigned int GNW95_repeat_rate = 80;

// 0x539D78
static unsigned int GNW95_repeat_delay = 500;

// A map of DIK_* constants normalized for QWERTY keyboard.
//
// 0x6713F0
static int GNW95_key_map[SDL_NUM_SCANCODES];

// Ring buffer of input events.
//
// Looks like this buffer does not support overwriting of values. Once the
// buffer is full it will not overwrite values until they are dequeued.
//
// 0x6714F0
static inputdata input_buffer[40];

// 0x6716D0
static GNW95RepeatStruct GNW95_key_time_stamps[SDL_NUM_SCANCODES];

// 0x671ED0
static int input_mx;

// 0x671ED4
static int input_my;

// 0x671ED8
static FuncPtr bk_list;

// 0x671EE0
static bool game_paused;

// 0x671EE4
static int screendump_key;

// 0x671EE8
static int using_msec_timer;

// 0x671EEC
static int pause_key;

// 0x671EF0
static ScreenDumpFunc* screendump_func;

// 0x671EF4
static int input_get;

// 0x671EF8
static unsigned char* screendump_buf;

// 0x671EFC
static PauseWinFunc* pause_win_func;

// 0x671F00
static int input_put;

// 0x671F04
static bool bk_disabled;

// 0x671F08
static unsigned int bk_process_time;

// 0x4B32C0
int GNW_input_init(int use_msec_timer)
{
    if (!dxinput_init()) {
        return -1;
    }

    if (GNW_kb_set() == -1) {
        return -1;
    }

    if (GNW_mouse_init() == -1) {
        return -1;
    }

    if (GNW95_input_init() == -1) {
        return -1;
    }

    GNW95_build_key_map();
    GNW95_clear_time_stamps();

    using_msec_timer = use_msec_timer;
    input_put = 0;
    input_get = -1;
    input_mx = -1;
    input_my = -1;
    bk_disabled = 0;
    game_paused = false;
    pause_key = KEY_ALT_P;
    pause_win_func = default_pause_window;
    screendump_func = default_screendump;
    bk_list = NULL;
    screendump_key = KEY_ALT_C;

    set_idle_func(idleImpl);

    return 0;
}

// 0x4B3390
void GNW_input_exit()
{
    // NOTE: Uninline.
    GNW95_input_exit();
    GNW_mouse_exit();
    GNW_kb_restore();
    dxinput_exit();

    FuncPtr curr = bk_list;
    while (curr != NULL) {
        FuncPtr next = curr->next;
        mem_free(curr);
        curr = next;
    }
}

// 0x4B33C8
int get_input()
{
    int v3;

    GNW95_process_message();

    if (!GNW95_isActive) {
        GNW95_lost_focus();
    }

    process_bk();

    v3 = get_input_buffer();
    if (v3 == -1 && mouse_get_buttons() & 0x33) {
        mouse_get_position(&input_mx, &input_my);
        return -2;
    } else {
        return GNW_check_menu_bars(v3);
    }

    return -1;
}

// 0x4B3418
void get_input_position(int* x, int* y)
{
    *x = input_mx;
    *y = input_my;
}

// 0x4B342C
void process_bk()
{
    int v1;

    GNW_do_bk_process();

    if (vcr_update() != 3) {
        mouse_info();
    }

    v1 = win_check_all_buttons();
    if (v1 != -1) {
        GNW_add_input_buffer(v1);
        return;
    }

    v1 = kb_getch();
    if (v1 != -1) {
        GNW_add_input_buffer(v1);
        return;
    }
}

// 0x4B3454
void GNW_add_input_buffer(int a1)
{
    if (a1 == -1) {
        return;
    }

    if (a1 == pause_key) {
        pause_game();
        return;
    }

    if (a1 == screendump_key) {
        dump_screen();
        return;
    }

    if (input_put == input_get) {
        return;
    }

    inputdata* inputEvent = &(input_buffer[input_put]);
    inputEvent->input = a1;

    mouse_get_position(&(inputEvent->mx), &(inputEvent->my));

    input_put++;

    if (input_put == 40) {
        input_put = 0;
        return;
    }

    if (input_get == -1) {
        input_get = 0;
    }
}

// 0x4B34E4
static int get_input_buffer()
{
    if (input_get == -1) {
        return -1;
    }

    inputdata* inputEvent = &(input_buffer[input_get]);
    int eventCode = inputEvent->input;
    input_mx = inputEvent->mx;
    input_my = inputEvent->my;

    input_get++;

    if (input_get == 40) {
        input_get = 0;
    }

    if (input_get == input_put) {
        input_get = -1;
        input_put = 0;
    }

    return eventCode;
}

// 0x4B354C
void flush_input_buffer()
{
    input_get = -1;
    input_put = 0;
}

// 0x4B3564
void GNW_do_bk_process()
{
    if (game_paused) {
        return;
    }

    if (bk_disabled) {
        return;
    }

    bk_process_time = get_time();

    FuncPtr curr = bk_list;
    FuncPtr* currPtr = &(bk_list);

    while (curr != NULL) {
        FuncPtr next = curr->next;
        if (curr->flags & 1) {
            *currPtr = next;

            mem_free(curr);
        } else {
            curr->f();
            currPtr = &(curr->next);
        }
        curr = next;
    }
}

// 0x4B35BC
void add_bk_process(BackgroundProcess* f)
{
    FuncPtr fp;

    fp = bk_list;
    while (fp != NULL) {
        if (fp->f == f) {
            if ((fp->flags & 0x01) != 0) {
                fp->flags &= ~0x01;
                return;
            }
        }
        fp = fp->next;
    }

    fp = (FuncPtr)mem_malloc(sizeof(*fp));
    fp->flags = 0;
    fp->f = f;
    fp->next = bk_list;
    bk_list = fp;
}

// 0x4B360C
void remove_bk_process(BackgroundProcess* f)
{
    FuncPtr fp;

    fp = bk_list;
    while (fp != NULL) {
        if (fp->f == f) {
            fp->flags |= 0x01;
            return;
        }
        fp = fp->next;
    }
}

// 0x4B362C
void enable_bk()
{
    bk_disabled = false;
}

// 0x4B3638
void disable_bk()
{
    bk_disabled = true;
}

// 0x4B3644
static void pause_game()
{
    if (!game_paused) {
        game_paused = true;

        int win = pause_win_func();

        while (get_input() != KEY_ESCAPE) {
        }

        game_paused = false;
        win_delete(win);
    }
}

// 0x4B3680
static int default_pause_window()
{
    int windowWidth = text_width("Paused") + 32;
    int windowHeight = 3 * text_height() + 16;

    int win = win_add((rectGetWidth(&scr_size) - windowWidth) / 2,
        (rectGetHeight(&scr_size) - windowHeight) / 2,
        windowWidth,
        windowHeight,
        256,
        WINDOW_MODAL | WINDOW_MOVE_ON_TOP);
    if (win == -1) {
        return -1;
    }

    win_border(win);

    unsigned char* windowBuffer = win_get_buf(win);
    text_to_buf(windowBuffer + 8 * windowWidth + 16,
        "Paused",
        windowWidth,
        windowWidth,
        colorTable[31744]);

    win_register_text_button(win,
        (windowWidth - text_width("Done") - 16) / 2,
        windowHeight - 8 - text_height() - 6,
        -1,
        -1,
        -1,
        KEY_ESCAPE,
        "Done",
        0);

    win_draw(win);

    return win;
}

// 0x4B377C
void register_pause(int new_pause_key, PauseWinFunc* new_pause_win_func)
{
    pause_key = new_pause_key;

    if (new_pause_win_func == NULL) {
        new_pause_win_func = default_pause_window;
    }

    pause_win_func = new_pause_win_func;
}

// 0x4B3794
void dump_screen()
{
    ScreenBlitFunc* old_scr_blit;
    ScreenBlitFunc* old_mouse_blit;
    ScreenTransBlitFunc* old_mouse_blit_trans;
    int width;
    int length;
    unsigned char* pal;

    width = scr_size.lrx - scr_size.ulx + 1;
    length = scr_size.lry - scr_size.uly + 1;
    screendump_buf = (unsigned char*)mem_malloc(width * length);
    if (screendump_buf == NULL) {
        return;
    }

    old_scr_blit = scr_blit;
    scr_blit = buf_blit;

    old_mouse_blit = mouse_blit;
    mouse_blit = buf_blit;

    old_mouse_blit_trans = mouse_blit_trans;
    mouse_blit_trans = NULL;

    win_refresh_all(&scr_size);

    mouse_blit_trans = old_mouse_blit_trans;
    mouse_blit = old_mouse_blit;
    scr_blit = old_scr_blit;

    pal = getSystemPalette();
    screendump_func(width, length, screendump_buf, pal);
    mem_free(screendump_buf);
}

// 0x4B3838
static void buf_blit(unsigned char* src, unsigned int srcPitch, unsigned int a3, unsigned int srcX, unsigned int srcY, unsigned int width, unsigned int height, unsigned int destX, unsigned int destY)
{
    int destWidth = scr_size.lrx - scr_size.ulx + 1;
    buf_to_buf(src + srcPitch * srcY + srcX, width, height, srcPitch, screendump_buf + destWidth * destY + destX, destWidth);
}

// 0x4B3890
int default_screendump(int width, int height, unsigned char* data, unsigned char* palette)
{
    char fileName[16];
    FILE* stream;
    int index;
    unsigned int intValue;
    unsigned short shortValue;

    for (index = 0; index < 100000; index++) {
        snprintf(fileName, sizeof(fileName), "scr%.5d.bmp", index);

        stream = compat_fopen(fileName, "rb");
        if (stream == NULL) {
            break;
        }

        fclose(stream);
    }

    if (index == 100000) {
        return -1;
    }

    stream = compat_fopen(fileName, "wb");
    if (stream == NULL) {
        return -1;
    }

    // bfType
    shortValue = 0x4D42;
    fwrite(&shortValue, sizeof(shortValue), 1, stream);

    // bfSize
    // 14 - sizeof(BITMAPFILEHEADER)
    // 40 - sizeof(BITMAPINFOHEADER)
    // 1024 - sizeof(RGBQUAD) * 256
    intValue = width * height + 14 + 40 + 1024;
    fwrite(&intValue, sizeof(intValue), 1, stream);

    // bfReserved1
    shortValue = 0;
    fwrite(&shortValue, sizeof(shortValue), 1, stream);

    // bfReserved2
    shortValue = 0;
    fwrite(&shortValue, sizeof(shortValue), 1, stream);

    // bfOffBits
    intValue = 14 + 40 + 1024;
    fwrite(&intValue, sizeof(intValue), 1, stream);

    // biSize
    intValue = 40;
    fwrite(&intValue, sizeof(intValue), 1, stream);

    // biWidth
    intValue = width;
    fwrite(&intValue, sizeof(intValue), 1, stream);

    // biHeight
    intValue = height;
    fwrite(&intValue, sizeof(intValue), 1, stream);

    // biPlanes
    shortValue = 1;
    fwrite(&shortValue, sizeof(shortValue), 1, stream);

    // biBitCount
    shortValue = 8;
    fwrite(&shortValue, sizeof(shortValue), 1, stream);

    // biCompression
    intValue = 0;
    fwrite(&intValue, sizeof(intValue), 1, stream);

    // biSizeImage
    intValue = 0;
    fwrite(&intValue, sizeof(intValue), 1, stream);

    // biXPelsPerMeter
    intValue = 0;
    fwrite(&intValue, sizeof(intValue), 1, stream);

    // biYPelsPerMeter
    intValue = 0;
    fwrite(&intValue, sizeof(intValue), 1, stream);

    // biClrUsed
    intValue = 0;
    fwrite(&intValue, sizeof(intValue), 1, stream);

    // biClrImportant
    intValue = 0;
    fwrite(&intValue, sizeof(intValue), 1, stream);

    for (int index = 0; index < 256; index++) {
        unsigned char rgbReserved = 0;
        unsigned char rgbRed = palette[index * 3] << 2;
        unsigned char rgbGreen = palette[index * 3 + 1] << 2;
        unsigned char rgbBlue = palette[index * 3 + 2] << 2;

        fwrite(&rgbBlue, sizeof(rgbBlue), 1, stream);
        fwrite(&rgbGreen, sizeof(rgbGreen), 1, stream);
        fwrite(&rgbRed, sizeof(rgbRed), 1, stream);
        fwrite(&rgbReserved, sizeof(rgbReserved), 1, stream);
    }

    for (int y = height - 1; y >= 0; y--) {
        unsigned char* dataPtr = data + y * width;
        fwrite(dataPtr, 1, width, stream);
    }

    fflush(stream);
    fclose(stream);

    return 0;
}

// 0x4B3BA0
void register_screendump(int new_screendump_key, ScreenDumpFunc* new_screendump_func)
{
    screendump_key = new_screendump_key;

    if (new_screendump_func == NULL) {
        new_screendump_func = default_screendump;
    }

    screendump_func = new_screendump_func;
}

// 0x4B3BB8
unsigned int get_time()
{
    return SDL_GetTicks();
}

// 0x4B3BC4
void pause_for_tocks(unsigned int delay)
{
    // NOTE: Uninline.
    unsigned int start = get_time();
    unsigned int end = get_time();

    // NOTE: Uninline.
    unsigned int diff = elapsed_tocks(end, start);
    while (diff < delay) {
        process_bk();

        end = get_time();

        // NOTE: Uninline.
        diff = elapsed_tocks(end, start);
    }
}

// 0x4B3C00
void block_for_tocks(unsigned int ms)
{
    unsigned int start = SDL_GetTicks();
    unsigned int diff;
    do {
        // NOTE: Uninline
        diff = elapsed_time(start);
    } while (diff < ms);
}

// 0x4B3C28
unsigned int elapsed_time(unsigned int start)
{
    unsigned int end = SDL_GetTicks();

    // NOTE: Uninline.
    return elapsed_tocks(end, start);
}

// 0x4B3C48
unsigned int elapsed_tocks(unsigned int end, unsigned int start)
{
    if (start > end) {
        return INT_MAX;
    } else {
        return end - start;
    }
}

// 0x4B3C58
unsigned int get_bk_time()
{
    return bk_process_time;
}

// 0x4B3C60
void set_repeat_rate(unsigned int rate)
{
    GNW95_repeat_rate = rate;
}

// 0x4B3C68
unsigned int get_repeat_rate()
{
    return GNW95_repeat_rate;
}

// 0x4B3C70
void set_repeat_delay(unsigned int delay)
{
    GNW95_repeat_delay = delay;
}

// 0x4B3C78
unsigned int get_repeat_delay()
{
    return GNW95_repeat_delay;
}

// 0x4B3C80
void set_focus_func(FocusFunc* new_focus_func)
{
    focus_func = new_focus_func;
}

// 0x4B3C88
FocusFunc* get_focus_func()
{
    return focus_func;
}

// 0x4B3C90
void set_idle_func(IdleFunc* new_idle_func)
{
    idle_func = new_idle_func;
}

// 0x4B3C98
IdleFunc* get_idle_func()
{
    return idle_func;
}

// 0x4B3CD8
static void GNW95_build_key_map()
{
    int* keys = GNW95_key_map;
    int k;

    keys[SDL_SCANCODE_ESCAPE] = SDL_SCANCODE_ESCAPE;
    keys[SDL_SCANCODE_1] = SDL_SCANCODE_1;
    keys[SDL_SCANCODE_2] = SDL_SCANCODE_2;
    keys[SDL_SCANCODE_3] = SDL_SCANCODE_3;
    keys[SDL_SCANCODE_4] = SDL_SCANCODE_4;
    keys[SDL_SCANCODE_5] = SDL_SCANCODE_5;
    keys[SDL_SCANCODE_6] = SDL_SCANCODE_6;
    keys[SDL_SCANCODE_7] = SDL_SCANCODE_7;
    keys[SDL_SCANCODE_8] = SDL_SCANCODE_8;
    keys[SDL_SCANCODE_9] = SDL_SCANCODE_9;
    keys[SDL_SCANCODE_0] = SDL_SCANCODE_0;

    switch (kb_layout) {
    case 0:
        k = SDL_SCANCODE_MINUS;
        break;
    case 1:
        k = SDL_SCANCODE_6;
        break;
    default:
        k = SDL_SCANCODE_SLASH;
        break;
    }
    keys[SDL_SCANCODE_MINUS] = k;

    switch (kb_layout) {
    case 1:
        k = SDL_SCANCODE_0;
        break;
    default:
        k = SDL_SCANCODE_EQUALS;
        break;
    }
    keys[SDL_SCANCODE_EQUALS] = k;

    keys[SDL_SCANCODE_BACKSPACE] = SDL_SCANCODE_BACKSPACE;
    keys[SDL_SCANCODE_TAB] = SDL_SCANCODE_TAB;

    switch (kb_layout) {
    case 1:
        k = SDL_SCANCODE_A;
        break;
    default:
        k = SDL_SCANCODE_Q;
        break;
    }
    keys[SDL_SCANCODE_Q] = k;

    switch (kb_layout) {
    case 1:
        k = SDL_SCANCODE_Z;
        break;
    default:
        k = SDL_SCANCODE_W;
        break;
    }
    keys[SDL_SCANCODE_W] = k;

    keys[SDL_SCANCODE_E] = SDL_SCANCODE_E;
    keys[SDL_SCANCODE_R] = SDL_SCANCODE_R;
    keys[SDL_SCANCODE_T] = SDL_SCANCODE_T;

    switch (kb_layout) {
    case 0:
    case 1:
    case 3:
    case 4:
        k = SDL_SCANCODE_Y;
        break;
    default:
        k = SDL_SCANCODE_Z;
        break;
    }
    keys[SDL_SCANCODE_Y] = k;

    keys[SDL_SCANCODE_U] = SDL_SCANCODE_U;
    keys[SDL_SCANCODE_I] = SDL_SCANCODE_I;
    keys[SDL_SCANCODE_O] = SDL_SCANCODE_O;
    keys[SDL_SCANCODE_P] = SDL_SCANCODE_P;

    switch (kb_layout) {
    case 0:
    case 3:
    case 4:
        k = SDL_SCANCODE_LEFTBRACKET;
        break;
    case 1:
        k = SDL_SCANCODE_5;
        break;
    default:
        k = SDL_SCANCODE_8;
        break;
    }
    keys[SDL_SCANCODE_LEFTBRACKET] = k;

    switch (kb_layout) {
    case 0:
    case 3:
    case 4:
        k = SDL_SCANCODE_RIGHTBRACKET;
        break;
    case 1:
        k = SDL_SCANCODE_MINUS;
        break;
    default:
        k = SDL_SCANCODE_9;
        break;
    }
    keys[SDL_SCANCODE_RIGHTBRACKET] = k;

    keys[SDL_SCANCODE_RETURN] = SDL_SCANCODE_RETURN;
    keys[SDL_SCANCODE_LCTRL] = SDL_SCANCODE_LCTRL;

    switch (kb_layout) {
    case 1:
        k = SDL_SCANCODE_Q;
        break;
    default:
        k = SDL_SCANCODE_A;
        break;
    }
    keys[SDL_SCANCODE_A] = k;

    keys[SDL_SCANCODE_S] = SDL_SCANCODE_S;
    keys[SDL_SCANCODE_D] = SDL_SCANCODE_D;
    keys[SDL_SCANCODE_F] = SDL_SCANCODE_F;
    keys[SDL_SCANCODE_G] = SDL_SCANCODE_G;
    keys[SDL_SCANCODE_H] = SDL_SCANCODE_H;
    keys[SDL_SCANCODE_J] = SDL_SCANCODE_J;
    keys[SDL_SCANCODE_K] = SDL_SCANCODE_K;
    keys[SDL_SCANCODE_L] = SDL_SCANCODE_L;

    switch (kb_layout) {
    case 0:
        k = SDL_SCANCODE_SEMICOLON;
        break;
    default:
        k = SDL_SCANCODE_COMMA;
        break;
    }
    keys[SDL_SCANCODE_SEMICOLON] = k;

    switch (kb_layout) {
    case 0:
        k = SDL_SCANCODE_APOSTROPHE;
        break;
    case 1:
        k = SDL_SCANCODE_4;
        break;
    default:
        k = SDL_SCANCODE_MINUS;
        break;
    }
    keys[SDL_SCANCODE_APOSTROPHE] = k;

    switch (kb_layout) {
    case 0:
        k = SDL_SCANCODE_GRAVE;
        break;
    case 1:
        k = SDL_SCANCODE_2;
        break;
    case 3:
    case 4:
        k = 0;
        break;
    default:
        k = SDL_SCANCODE_RIGHTBRACKET;
        break;
    }
    keys[SDL_SCANCODE_GRAVE] = k;

    keys[SDL_SCANCODE_LSHIFT] = SDL_SCANCODE_LSHIFT;

    switch (kb_layout) {
    case 0:
        k = SDL_SCANCODE_BACKSLASH;
        break;
    case 1:
        k = SDL_SCANCODE_8;
        break;
    case 3:
    case 4:
        k = SDL_SCANCODE_GRAVE;
        break;
    default:
        k = SDL_SCANCODE_Y;
        break;
    }
    keys[SDL_SCANCODE_BACKSLASH] = k;

    switch (kb_layout) {
    case 0:
    case 3:
    case 4:
        k = SDL_SCANCODE_Z;
        break;
    case 1:
        k = SDL_SCANCODE_W;
        break;
    default:
        k = SDL_SCANCODE_Y;
        break;
    }
    keys[SDL_SCANCODE_Z] = k;

    keys[SDL_SCANCODE_X] = SDL_SCANCODE_X;
    keys[SDL_SCANCODE_C] = SDL_SCANCODE_C;
    keys[SDL_SCANCODE_V] = SDL_SCANCODE_V;
    keys[SDL_SCANCODE_B] = SDL_SCANCODE_B;
    keys[SDL_SCANCODE_N] = SDL_SCANCODE_N;

    switch (kb_layout) {
    case 1:
        k = SDL_SCANCODE_SEMICOLON;
        break;
    default:
        k = SDL_SCANCODE_M;
        break;
    }
    keys[SDL_SCANCODE_M] = k;

    switch (kb_layout) {
    case 1:
        k = SDL_SCANCODE_M;
        break;
    default:
        k = SDL_SCANCODE_COMMA;
        break;
    }
    keys[SDL_SCANCODE_COMMA] = k;

    switch (kb_layout) {
    case 1:
        k = SDL_SCANCODE_COMMA;
        break;
    default:
        k = SDL_SCANCODE_PERIOD;
        break;
    }
    keys[SDL_SCANCODE_PERIOD] = k;

    switch (kb_layout) {
    case 0:
        k = SDL_SCANCODE_SLASH;
        break;
    case 1:
        k = SDL_SCANCODE_PERIOD;
        break;
    default:
        k = SDL_SCANCODE_7;
        break;
    }
    keys[SDL_SCANCODE_SLASH] = k;

    keys[SDL_SCANCODE_RSHIFT] = SDL_SCANCODE_RSHIFT;
    keys[SDL_SCANCODE_KP_MULTIPLY] = SDL_SCANCODE_KP_MULTIPLY;
    keys[SDL_SCANCODE_SPACE] = SDL_SCANCODE_SPACE;
    keys[SDL_SCANCODE_LALT] = SDL_SCANCODE_LALT;
    keys[SDL_SCANCODE_CAPSLOCK] = SDL_SCANCODE_CAPSLOCK;
    keys[SDL_SCANCODE_F1] = SDL_SCANCODE_F1;
    keys[SDL_SCANCODE_F2] = SDL_SCANCODE_F2;
    keys[SDL_SCANCODE_F3] = SDL_SCANCODE_F3;
    keys[SDL_SCANCODE_F4] = SDL_SCANCODE_F4;
    keys[SDL_SCANCODE_F5] = SDL_SCANCODE_F5;
    keys[SDL_SCANCODE_F6] = SDL_SCANCODE_F6;
    keys[SDL_SCANCODE_F7] = SDL_SCANCODE_F7;
    keys[SDL_SCANCODE_F8] = SDL_SCANCODE_F8;
    keys[SDL_SCANCODE_F9] = SDL_SCANCODE_F9;
    keys[SDL_SCANCODE_F10] = SDL_SCANCODE_F10;
    keys[SDL_SCANCODE_NUMLOCKCLEAR] = SDL_SCANCODE_NUMLOCKCLEAR;
    keys[SDL_SCANCODE_SCROLLLOCK] = SDL_SCANCODE_SCROLLLOCK;
    keys[SDL_SCANCODE_KP_7] = SDL_SCANCODE_KP_7;
    keys[SDL_SCANCODE_KP_9] = SDL_SCANCODE_KP_9;
    keys[SDL_SCANCODE_KP_8] = SDL_SCANCODE_KP_8;
    keys[SDL_SCANCODE_KP_MINUS] = SDL_SCANCODE_KP_MINUS;
    keys[SDL_SCANCODE_KP_4] = SDL_SCANCODE_KP_4;
    keys[SDL_SCANCODE_KP_5] = SDL_SCANCODE_KP_5;
    keys[SDL_SCANCODE_KP_6] = SDL_SCANCODE_KP_6;
    keys[SDL_SCANCODE_KP_PLUS] = SDL_SCANCODE_KP_PLUS;
    keys[SDL_SCANCODE_KP_1] = SDL_SCANCODE_KP_1;
    keys[SDL_SCANCODE_KP_2] = SDL_SCANCODE_KP_2;
    keys[SDL_SCANCODE_KP_3] = SDL_SCANCODE_KP_3;
    keys[SDL_SCANCODE_KP_0] = SDL_SCANCODE_KP_0;
    keys[SDL_SCANCODE_KP_DECIMAL] = SDL_SCANCODE_KP_DECIMAL;
    keys[SDL_SCANCODE_F11] = SDL_SCANCODE_F11;
    keys[SDL_SCANCODE_F12] = SDL_SCANCODE_F12;
    keys[SDL_SCANCODE_F13] = -1;
    keys[SDL_SCANCODE_F14] = -1;
    keys[SDL_SCANCODE_F15] = -1;
    // keys[DIK_KANA] = -1;
    // keys[DIK_CONVERT] = -1;
    // keys[DIK_NOCONVERT] = -1;
    // keys[DIK_YEN] = -1;
    keys[SDL_SCANCODE_KP_EQUALS] = -1;
    // keys[DIK_PREVTRACK] = -1;
    // keys[DIK_AT] = -1;
    // keys[DIK_COLON] = -1;
    // keys[DIK_UNDERLINE] = -1;
    // keys[DIK_KANJI] = -1;
    keys[SDL_SCANCODE_STOP] = -1;
    // keys[DIK_AX] = -1;
    // keys[DIK_UNLABELED] = -1;
    keys[SDL_SCANCODE_KP_ENTER] = SDL_SCANCODE_KP_ENTER;
    keys[SDL_SCANCODE_RCTRL] = SDL_SCANCODE_RCTRL;
    keys[SDL_SCANCODE_KP_COMMA] = -1;
    keys[SDL_SCANCODE_KP_DIVIDE] = SDL_SCANCODE_KP_DIVIDE;
    // keys[DIK_SYSRQ] = 84;
    keys[SDL_SCANCODE_RALT] = SDL_SCANCODE_RALT;
    keys[SDL_SCANCODE_HOME] = SDL_SCANCODE_HOME;
    keys[SDL_SCANCODE_UP] = SDL_SCANCODE_UP;
    keys[SDL_SCANCODE_PRIOR] = SDL_SCANCODE_PRIOR;
    keys[SDL_SCANCODE_LEFT] = SDL_SCANCODE_LEFT;
    keys[SDL_SCANCODE_RIGHT] = SDL_SCANCODE_RIGHT;
    keys[SDL_SCANCODE_END] = SDL_SCANCODE_END;
    keys[SDL_SCANCODE_DOWN] = SDL_SCANCODE_DOWN;
    keys[SDL_SCANCODE_PAGEDOWN] = SDL_SCANCODE_PAGEDOWN;
    keys[SDL_SCANCODE_INSERT] = SDL_SCANCODE_INSERT;
    keys[SDL_SCANCODE_DELETE] = SDL_SCANCODE_DELETE;
    keys[SDL_SCANCODE_LGUI] = -1;
    keys[SDL_SCANCODE_RGUI] = -1;
    keys[SDL_SCANCODE_APPLICATION] = -1;
}

// 0x4C9C20
int GNW95_input_init()
{
    return 0;
}

// 0x4B446C
void GNW95_input_exit()
{
}

// 0x4B4538
void GNW95_process_message()
{
    // We need to process event loop even if program is not active or keyboard
    // is disabled, because if we ignore it, we'll never be able to reactivate
    // it again.

    KeyboardData keyboardData;
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_MOUSEMOTION:
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
        case SDL_MOUSEWHEEL:
            handleMouseEvent(&e);
            break;
        case SDL_FINGERDOWN:
            touch_handle_start(&(e.tfinger));
            break;
        case SDL_FINGERMOTION:
            touch_handle_move(&(e.tfinger));
            break;
        case SDL_FINGERUP:
            touch_handle_end(&(e.tfinger));
            break;
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            if (!kb_is_disabled()) {
                keyboardData.key = e.key.keysym.scancode;
                keyboardData.down = (e.key.state & SDL_PRESSED) != 0;
                GNW95_process_key(&keyboardData);
            }
            break;
        case SDL_WINDOWEVENT:
            switch (e.window.event) {
            case SDL_WINDOWEVENT_EXPOSED:
                win_refresh_all(&scr_size);
                break;
            case SDL_WINDOWEVENT_SIZE_CHANGED:
                handleWindowSizeChanged();
                win_refresh_all(&scr_size);
                break;
            case SDL_WINDOWEVENT_FOCUS_GAINED:
                GNW95_isActive = true;
                win_refresh_all(&scr_size);
                audioEngineResume();
                break;
            case SDL_WINDOWEVENT_FOCUS_LOST:
                GNW95_isActive = false;
                audioEnginePause();
                break;
            }
            break;
        case SDL_QUIT:
            exit(EXIT_SUCCESS);
            break;
        }
    }

    touch_process_gesture();

    if (GNW95_isActive && !kb_is_disabled()) {
        // NOTE: Uninline
        unsigned int tick = get_time();

        for (int key = 0; key < SDL_NUM_SCANCODES; key++) {
            GNW95RepeatStruct* ptr = &(GNW95_key_time_stamps[key]);
            if (ptr->time != -1) {
                unsigned int elapsedTime = ptr->time > tick ? INT_MAX : tick - ptr->time;
                unsigned int delay = ptr->count == 0 ? GNW95_repeat_delay : GNW95_repeat_rate;
                if (elapsedTime > delay) {
                    keyboardData.key = key;
                    keyboardData.down = 1;
                    GNW95_process_key(&keyboardData);

                    ptr->time = tick;
                    ptr->count++;
                }
            }
        }
    }
}

// 0x4B4638
void GNW95_clear_time_stamps()
{
    for (int index = 0; index < SDL_NUM_SCANCODES; index++) {
        GNW95_key_time_stamps[index].time = -1;
        GNW95_key_time_stamps[index].count = 0;
    }
}

// 0x4B465C
static void GNW95_process_key(KeyboardData* data)
{
    // Use originally pressed scancode, not qwerty-remapped one, for tracking
    // timestamps, see usage from |_GNW95_process_message|.
    int scanCode = data->key;

    data->key = GNW95_key_map[data->key];

    if (vcr_state == VCR_STATE_PLAYING) {
        if ((vcr_terminate_flags & VCR_TERMINATE_ON_KEY_PRESS) != 0) {
            vcr_terminated_condition = VCR_PLAYBACK_COMPLETION_REASON_TERMINATED;
            vcr_stop();
        }
    } else {
        if (data->down == 1) {
            GNW95_key_time_stamps[scanCode].time = get_time();
            GNW95_key_time_stamps[scanCode].count = 0;
        } else {
            GNW95_key_time_stamps[scanCode].time = -1;
        }

        // Ignore keys which were remapped to -1.
        if (data->key == -1) {
            return;
        }

        kb_simulate_key(data);
    }
}

// 0x4B4734
void GNW95_lost_focus()
{
    if (focus_func != NULL) {
        focus_func(0);
    }

    while (!GNW95_isActive) {
        GNW95_process_message();

        if (idle_func != NULL) {
            idle_func();
        }
    }

    if (focus_func != NULL) {
        focus_func(1);
    }
}

static void idleImpl()
{
    SDL_Delay(125);
}

void beginTextInput()
{
    SDL_StartTextInput();
}

void endTextInput()
{
    SDL_StopTextInput();
}

} // namespace fallout
