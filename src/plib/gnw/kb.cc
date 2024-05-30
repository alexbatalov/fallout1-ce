#include "plib/gnw/kb.h"

#include "plib/gnw/dxinput.h"
#include "plib/gnw/input.h"
#include "plib/gnw/vcr.h"

namespace fallout {

typedef struct key_ansi_t {
    short keys;
    short normal;
    short shift;
    short left_alt;
    short right_alt;
    short ctrl;
} key_ansi_t;

typedef struct key_data_t {
    // NOTE: `mapper2.exe` says it's type is `char`. However when it is too
    // many casts to `unsigned char` is needed to make sure it can be used as
    // offset (otherwise chars above 0x7F will be treated as negative values).
    unsigned char scan_code;
    unsigned short modifiers;
} key_data_t;

typedef int(AsciiConvert)();

static int kb_next_ascii_English_US();
static int kb_next_ascii_French();
static int kb_next_ascii_German();
static int kb_next_ascii_Italian();
static int kb_next_ascii_Spanish();
static int kb_next_ascii();
static void kb_map_ascii_English_US();
static void kb_map_ascii_French();
static void kb_map_ascii_German();
static void kb_map_ascii_Italian();
static void kb_map_ascii_Spanish();
static void kb_init_lock_status();
static void kb_toggle_caps();
static void kb_toggle_num();
static void kb_toggle_scroll();
static int kb_buffer_put(key_data_t* key_data);
static int kb_buffer_get(key_data_t* key_data);
static int kb_buffer_peek(int index, key_data_t** keyboardEventPtr);

// 0x539E00
static unsigned char kb_installed = 0;

// 0x539E04
static bool kb_disabled = false;

// 0x539E08
static bool kb_numpad_disabled = false;

// 0x539E0C
static bool kb_numlock_disabled = false;

// 0x539E10
static int kb_put = 0;

// 0x539E14
static int kb_get = 0;

// 0x539E18
static unsigned short extended_code = 0;

// 0x539E1A
static unsigned char kb_lock_flags = 0;

// 0x539E1C
static AsciiConvert* kb_scan_to_ascii = kb_next_ascii_English_US;

// Ring buffer of keyboard events.
//
// 0x6722A0
static key_data_t kb_buffer[64];

// A map of logical key configurations for physical scan codes [DIK_*].
//
// 0x6723A0
static key_ansi_t ascii_table[256];

// A state of physical keys [DIK_*] currently pressed.
//
// 0 - key is not pressed.
// 1 - key pressed.
//
// 0x672FA0
unsigned char keys[SDL_NUM_SCANCODES];

// 0x6730A0
static unsigned int kb_idle_start_time;

// 0x6730A4
static key_data_t temp;

// 0x6730A8
int kb_layout;

// The number of keys currently pressed.
//
// 0x6730AC
unsigned char keynumpress;

// 0x4B6430
int GNW_kb_set()
{
    if (kb_installed) {
        return -1;
    }

    kb_installed = 1;

    // NOTE: Uninline.
    kb_clear();

    kb_init_lock_status();
    kb_set_layout(KEYBOARD_LAYOUT_QWERTY);

    kb_idle_start_time = get_time();

    return 0;
}

// 0x4B64A0
void GNW_kb_restore()
{
    if (kb_installed) {
        kb_installed = 0;
    }
}

// 0x4B64B4
void kb_wait()
{
    if (kb_installed) {
        // NOTE: Uninline.
        kb_clear();

        do {
            GNW95_process_message();
        } while (keynumpress == 0);

        // NOTE: Uninline.
        kb_clear();
    }
}

// 0x4B6548
void kb_clear()
{
    int i;

    if (kb_installed) {
        keynumpress = 0;

        for (i = 0; i < 256; i++) {
            keys[i] = 0;
        }

        kb_put = 0;
        kb_get = 0;
    }

    dxinput_flush_keyboard_buffer();
    GNW95_clear_time_stamps();
}

// 0x4B6588
int kb_getch()
{
    int rc = -1;

    if (kb_installed != 0) {
        rc = kb_scan_to_ascii();
    }

    return rc;
}

// 0x4B65A0
void kb_disable()
{
    kb_disabled = true;
}

// 0x4B65AC
void kb_enable()
{
    kb_disabled = false;
}

// 0x4B65B8
bool kb_is_disabled()
{
    return kb_disabled;
}

// 0x4B65C0
void kb_disable_numpad()
{
    kb_numpad_disabled = true;
}

// 0x4B65CC
void kb_enable_numpad()
{
    kb_numpad_disabled = false;
}

// 0x4B65D8
bool kb_numpad_is_disabled()
{
    return kb_numpad_disabled;
}

// 0x4B65E0
void kb_disable_numlock()
{
    kb_numlock_disabled = true;
}

// 0x4B65EC
void kb_enable_numlock()
{
    kb_numlock_disabled = false;
}

// 0x4B65F8
bool kb_numlock_is_disabled()
{
    return kb_numlock_disabled;
}

// 0x4B6614
void kb_set_layout(int layout)
{
    int old_layout = kb_layout;
    kb_layout = layout;

    switch (layout) {
    case KEYBOARD_LAYOUT_QWERTY:
        kb_scan_to_ascii = kb_next_ascii_English_US;
        kb_map_ascii_English_US();
        break;
    // case KEYBOARD_LAYOUT_FRENCH:
    //     kb_scan_to_ascii = kb_next_ascii_French;
    //     kb_map_ascii_French();
    //     break;
    // case KEYBOARD_LAYOUT_GERMAN:
    //     kb_scan_to_ascii = kb_next_ascii_German;
    //     kb_map_ascii_German();
    //     break;
    // case KEYBOARD_LAYOUT_ITALIAN:
    //     kb_scan_to_ascii = kb_next_ascii_Italian;
    //     kb_map_ascii_Italian();
    //     break;
    // case KEYBOARD_LAYOUT_SPANISH:
    //     kb_scan_to_ascii = kb_next_ascii_Spanish;
    //     kb_map_ascii_Spanish();
    //     break;
    default:
        kb_layout = old_layout;
        break;
    }
}

// 0x4B668C
int kb_get_layout()
{
    return kb_layout;
}

// 0x4B6694
int kb_ascii_to_scan(int ascii)
{
    int k;

    for (k = 0; k < 256; k++) {
        if (ascii_table[k].normal == k
            || ascii_table[k].shift == k
            || ascii_table[k].left_alt == k
            || ascii_table[k].right_alt == k
            || ascii_table[k].ctrl == k) {
            return k;
        }
    }

    return -1;
}

// 0x4B66F0
unsigned int kb_elapsed_time()
{
    return elapsed_time(kb_idle_start_time);
}

// 0x4B66FC
void kb_reset_elapsed_time()
{
    kb_idle_start_time = get_time();
}

// 0x4B6708
void kb_simulate_key(KeyboardData* data)
{
    if (vcr_state == 0) {
        if (vcr_buffer_index != VCR_BUFFER_CAPACITY - 1) {
            VcrEntry* vcrEntry = &(vcr_buffer[vcr_buffer_index]);
            vcrEntry->type = VCR_ENTRY_TYPE_KEYBOARD_EVENT;
            vcrEntry->keyboardEvent.key = data->key & 0xFFFF;
            vcrEntry->time = vcr_time;
            vcrEntry->counter = vcr_counter;

            vcr_buffer_index++;
        }
    }

    kb_idle_start_time = get_bk_time();

    int key = data->key;
    int keyState = data->down == 1 ? KEY_STATE_DOWN : KEY_STATE_UP;

    int physicalKey = key;

    if (keyState != KEY_STATE_UP && keys[physicalKey] != KEY_STATE_UP) {
        keyState = KEY_STATE_REPEAT;
    }

    if (keys[physicalKey] != keyState) {
        keys[physicalKey] = keyState;
        if (keyState == KEY_STATE_DOWN) {
            keynumpress++;
        } else if (keyState == KEY_STATE_UP) {
            keynumpress--;
        }
    }

    if (keyState != KEY_STATE_UP) {
        temp.scan_code = physicalKey & 0xFF;
        temp.modifiers = 0;

        if (physicalKey == SDL_SCANCODE_CAPSLOCK) {
            if (keys[SDL_SCANCODE_LCTRL] == KEY_STATE_UP && keys[SDL_SCANCODE_RCTRL] == KEY_STATE_UP) {
                // NOTE: Uninline.
                kb_toggle_caps();
            }
        } else if (physicalKey == SDL_SCANCODE_NUMLOCKCLEAR) {
            if (keys[SDL_SCANCODE_LCTRL] == KEY_STATE_UP && keys[SDL_SCANCODE_RCTRL] == KEY_STATE_UP) {
                // NOTE: Uninline.
                kb_toggle_num();
            }
        } else if (physicalKey == SDL_SCANCODE_SCROLLLOCK) {
            if (keys[SDL_SCANCODE_LCTRL] == KEY_STATE_UP && keys[SDL_SCANCODE_RCTRL] == KEY_STATE_UP) {
                // NOTE: Uninline.
                kb_toggle_scroll();
            }
        } else if ((physicalKey == SDL_SCANCODE_LSHIFT || physicalKey == SDL_SCANCODE_RSHIFT) && (kb_lock_flags & MODIFIER_KEY_STATE_CAPS_LOCK) != 0 && kb_layout != 0) {
            if (keys[SDL_SCANCODE_LCTRL] == KEY_STATE_UP && keys[SDL_SCANCODE_RCTRL] == KEY_STATE_UP) {
                // NOTE: Uninline.
                kb_toggle_caps();
            }
        }

        if (kb_lock_flags != 0) {
            if ((kb_lock_flags & MODIFIER_KEY_STATE_NUM_LOCK) != 0 && !kb_numlock_disabled) {
                temp.modifiers |= KEYBOARD_EVENT_MODIFIER_NUM_LOCK;
            }

            if ((kb_lock_flags & MODIFIER_KEY_STATE_CAPS_LOCK) != 0) {
                temp.modifiers |= KEYBOARD_EVENT_MODIFIER_CAPS_LOCK;
            }

            if ((kb_lock_flags & MODIFIER_KEY_STATE_SCROLL_LOCK) != 0) {
                temp.modifiers |= KEYBOARD_EVENT_MODIFIER_SCROLL_LOCK;
            }
        }

        if (keys[SDL_SCANCODE_LSHIFT] != KEY_STATE_UP) {
            temp.modifiers |= KEYBOARD_EVENT_MODIFIER_LEFT_SHIFT;
        }

        if (keys[SDL_SCANCODE_RSHIFT] != KEY_STATE_UP) {
            temp.modifiers |= KEYBOARD_EVENT_MODIFIER_RIGHT_SHIFT;
        }

        if (keys[SDL_SCANCODE_LALT] != KEY_STATE_UP) {
            temp.modifiers |= KEYBOARD_EVENT_MODIFIER_LEFT_ALT;
        }

        if (keys[SDL_SCANCODE_RALT] != KEY_STATE_UP) {
            temp.modifiers |= KEYBOARD_EVENT_MODIFIER_RIGHT_ALT;
        }

        if (keys[SDL_SCANCODE_LCTRL] != KEY_STATE_UP) {
            temp.modifiers |= KEYBOARD_EVENT_MODIFIER_LEFT_CONTROL;
        }

        if (keys[SDL_SCANCODE_RCTRL] != KEY_STATE_UP) {
            temp.modifiers |= KEYBOARD_EVENT_MODIFIER_RIGHT_CONTROL;
        }

        // NOTE: Uninline.
        kb_buffer_put(&temp);
    }
}

// 0x4B6AC8
static int kb_next_ascii_English_US()
{
    key_data_t* keyboardEvent;
    if (kb_buffer_peek(0, &keyboardEvent) != 0) {
        return -1;
    }

    if ((keyboardEvent->modifiers & KEYBOARD_EVENT_MODIFIER_CAPS_LOCK) != 0) {
        unsigned char a = (kb_layout != KEYBOARD_LAYOUT_FRENCH ? SDL_SCANCODE_A : SDL_SCANCODE_Q);
        unsigned char m = (kb_layout != KEYBOARD_LAYOUT_FRENCH ? SDL_SCANCODE_M : SDL_SCANCODE_SEMICOLON);
        unsigned char q = (kb_layout != KEYBOARD_LAYOUT_FRENCH ? SDL_SCANCODE_Q : SDL_SCANCODE_A);
        unsigned char w = (kb_layout != KEYBOARD_LAYOUT_FRENCH ? SDL_SCANCODE_W : SDL_SCANCODE_Z);

        unsigned char y;
        switch (kb_layout) {
        case KEYBOARD_LAYOUT_QWERTY:
        case KEYBOARD_LAYOUT_FRENCH:
        case KEYBOARD_LAYOUT_ITALIAN:
        case KEYBOARD_LAYOUT_SPANISH:
            y = SDL_SCANCODE_Y;
            break;
        default:
            // GERMAN
            y = SDL_SCANCODE_Z;
            break;
        }

        unsigned char z;
        switch (kb_layout) {
        case KEYBOARD_LAYOUT_QWERTY:
        case KEYBOARD_LAYOUT_ITALIAN:
        case KEYBOARD_LAYOUT_SPANISH:
            z = SDL_SCANCODE_Z;
            break;
        case KEYBOARD_LAYOUT_FRENCH:
            z = SDL_SCANCODE_W;
            break;
        default:
            // GERMAN
            z = SDL_SCANCODE_Y;
            break;
        }

        unsigned char scanCode = keyboardEvent->scan_code;
        if (scanCode == a
            || scanCode == SDL_SCANCODE_B
            || scanCode == SDL_SCANCODE_C
            || scanCode == SDL_SCANCODE_D
            || scanCode == SDL_SCANCODE_E
            || scanCode == SDL_SCANCODE_F
            || scanCode == SDL_SCANCODE_G
            || scanCode == SDL_SCANCODE_H
            || scanCode == SDL_SCANCODE_I
            || scanCode == SDL_SCANCODE_J
            || scanCode == SDL_SCANCODE_K
            || scanCode == SDL_SCANCODE_L
            || scanCode == m
            || scanCode == SDL_SCANCODE_N
            || scanCode == SDL_SCANCODE_O
            || scanCode == SDL_SCANCODE_P
            || scanCode == q
            || scanCode == SDL_SCANCODE_R
            || scanCode == SDL_SCANCODE_S
            || scanCode == SDL_SCANCODE_T
            || scanCode == SDL_SCANCODE_U
            || scanCode == SDL_SCANCODE_V
            || scanCode == w
            || scanCode == SDL_SCANCODE_X
            || scanCode == y
            || scanCode == z) {
            if (keyboardEvent->modifiers & KEYBOARD_EVENT_MODIFIER_ANY_SHIFT) {
                keyboardEvent->modifiers &= ~KEYBOARD_EVENT_MODIFIER_ANY_SHIFT;
            } else {
                keyboardEvent->modifiers |= KEYBOARD_EVENT_MODIFIER_LEFT_SHIFT;
            }
        }
    }

    return kb_next_ascii();
}

// 0x4B6D94
static int kb_next_ascii_French()
{
    // TODO: Incomplete.

    return -1;
}

// 0x4B7124
static int kb_next_ascii_German()
{
    // TODO: Incomplete.

    return -1;
}

// 0x4B75EC
static int kb_next_ascii_Italian()
{
    // TODO: Incomplete.

    return -1;
}

// 0x4B78B8
static int kb_next_ascii_Spanish()
{
    // TODO: Incomplete.

    return -1;
}

// 0x4B8224
static int kb_next_ascii()
{
    key_data_t* keyboardEvent;
    if (kb_buffer_peek(0, &keyboardEvent) != 0) {
        return -1;
    }

    switch (keyboardEvent->scan_code) {
    case SDL_SCANCODE_KP_DIVIDE:
    case SDL_SCANCODE_KP_MULTIPLY:
    case SDL_SCANCODE_KP_MINUS:
    case SDL_SCANCODE_KP_PLUS:
    case SDL_SCANCODE_KP_ENTER:
        if (kb_numpad_disabled) {
            // NOTE: Uninline.
            kb_buffer_get(NULL);
            return -1;
        }
        break;
    case SDL_SCANCODE_KP_0:
    case SDL_SCANCODE_KP_1:
    case SDL_SCANCODE_KP_2:
    case SDL_SCANCODE_KP_3:
    case SDL_SCANCODE_KP_4:
    case SDL_SCANCODE_KP_5:
    case SDL_SCANCODE_KP_6:
    case SDL_SCANCODE_KP_7:
    case SDL_SCANCODE_KP_8:
    case SDL_SCANCODE_KP_9:
        if (kb_numpad_disabled) {
            // NOTE: Uninline.
            kb_buffer_get(NULL);
            return -1;
        }

        if ((keyboardEvent->modifiers & KEYBOARD_EVENT_MODIFIER_ANY_ALT) == 0 && (keyboardEvent->modifiers & KEYBOARD_EVENT_MODIFIER_NUM_LOCK) != 0) {
            if ((keyboardEvent->modifiers & KEYBOARD_EVENT_MODIFIER_ANY_SHIFT) != 0) {
                keyboardEvent->modifiers &= ~KEYBOARD_EVENT_MODIFIER_ANY_SHIFT;
            } else {
                keyboardEvent->modifiers |= KEYBOARD_EVENT_MODIFIER_LEFT_SHIFT;
            }
        }

        break;
    }

    int logicalKey = -1;

    key_ansi_t* logicalKeyDescription = &(ascii_table[keyboardEvent->scan_code]);
    if ((keyboardEvent->modifiers & KEYBOARD_EVENT_MODIFIER_ANY_CONTROL) != 0) {
        logicalKey = logicalKeyDescription->ctrl;
    } else if ((keyboardEvent->modifiers & KEYBOARD_EVENT_MODIFIER_RIGHT_ALT) != 0) {
        logicalKey = logicalKeyDescription->right_alt;
    } else if ((keyboardEvent->modifiers & KEYBOARD_EVENT_MODIFIER_LEFT_ALT) != 0) {
        logicalKey = logicalKeyDescription->left_alt;
    } else if ((keyboardEvent->modifiers & KEYBOARD_EVENT_MODIFIER_ANY_SHIFT) != 0) {
        logicalKey = logicalKeyDescription->shift;
    } else {
        logicalKey = logicalKeyDescription->normal;
    }

    // NOTE: Uninline.
    kb_buffer_get(NULL);

    return logicalKey;
}

// 0x4B83E0
static void kb_map_ascii_English_US()
{
    int k;

    for (k = 0; k < 256; k++) {
        ascii_table[k].keys = -1;
        ascii_table[k].normal = -1;
        ascii_table[k].shift = -1;
        ascii_table[k].left_alt = -1;
        ascii_table[k].right_alt = -1;
        ascii_table[k].ctrl = -1;
    }

    ascii_table[SDL_SCANCODE_ESCAPE].normal = KEY_ESCAPE;
    ascii_table[SDL_SCANCODE_ESCAPE].shift = KEY_ESCAPE;
    ascii_table[SDL_SCANCODE_ESCAPE].left_alt = KEY_ESCAPE;
    ascii_table[SDL_SCANCODE_ESCAPE].right_alt = KEY_ESCAPE;
    ascii_table[SDL_SCANCODE_ESCAPE].ctrl = KEY_ESCAPE;

    ascii_table[SDL_SCANCODE_F1].normal = KEY_F1;
    ascii_table[SDL_SCANCODE_F1].shift = KEY_SHIFT_F1;
    ascii_table[SDL_SCANCODE_F1].left_alt = KEY_ALT_F1;
    ascii_table[SDL_SCANCODE_F1].right_alt = KEY_ALT_F1;
    ascii_table[SDL_SCANCODE_F1].ctrl = KEY_CTRL_F1;

    ascii_table[SDL_SCANCODE_F2].normal = KEY_F2;
    ascii_table[SDL_SCANCODE_F2].shift = KEY_SHIFT_F2;
    ascii_table[SDL_SCANCODE_F2].left_alt = KEY_ALT_F2;
    ascii_table[SDL_SCANCODE_F2].right_alt = KEY_ALT_F2;
    ascii_table[SDL_SCANCODE_F2].ctrl = KEY_CTRL_F2;

    ascii_table[SDL_SCANCODE_F3].normal = KEY_F3;
    ascii_table[SDL_SCANCODE_F3].shift = KEY_SHIFT_F3;
    ascii_table[SDL_SCANCODE_F3].left_alt = KEY_ALT_F3;
    ascii_table[SDL_SCANCODE_F3].right_alt = KEY_ALT_F3;
    ascii_table[SDL_SCANCODE_F3].ctrl = KEY_CTRL_F3;

    ascii_table[SDL_SCANCODE_F4].normal = KEY_F4;
    ascii_table[SDL_SCANCODE_F4].shift = KEY_SHIFT_F4;
    ascii_table[SDL_SCANCODE_F4].left_alt = KEY_ALT_F4;
    ascii_table[SDL_SCANCODE_F4].right_alt = KEY_ALT_F4;
    ascii_table[SDL_SCANCODE_F4].ctrl = KEY_CTRL_F4;

    ascii_table[SDL_SCANCODE_F5].normal = KEY_F5;
    ascii_table[SDL_SCANCODE_F5].shift = KEY_SHIFT_F5;
    ascii_table[SDL_SCANCODE_F5].left_alt = KEY_ALT_F5;
    ascii_table[SDL_SCANCODE_F5].right_alt = KEY_ALT_F5;
    ascii_table[SDL_SCANCODE_F5].ctrl = KEY_CTRL_F5;

    ascii_table[SDL_SCANCODE_F6].normal = KEY_F6;
    ascii_table[SDL_SCANCODE_F6].shift = KEY_SHIFT_F6;
    ascii_table[SDL_SCANCODE_F6].left_alt = KEY_ALT_F6;
    ascii_table[SDL_SCANCODE_F6].right_alt = KEY_ALT_F6;
    ascii_table[SDL_SCANCODE_F6].ctrl = KEY_CTRL_F6;

    ascii_table[SDL_SCANCODE_F7].normal = KEY_F7;
    ascii_table[SDL_SCANCODE_F7].shift = KEY_SHIFT_F7;
    ascii_table[SDL_SCANCODE_F7].left_alt = KEY_ALT_F7;
    ascii_table[SDL_SCANCODE_F7].right_alt = KEY_ALT_F7;
    ascii_table[SDL_SCANCODE_F7].ctrl = KEY_CTRL_F7;

    ascii_table[SDL_SCANCODE_F8].normal = KEY_F8;
    ascii_table[SDL_SCANCODE_F8].shift = KEY_SHIFT_F8;
    ascii_table[SDL_SCANCODE_F8].left_alt = KEY_ALT_F8;
    ascii_table[SDL_SCANCODE_F8].right_alt = KEY_ALT_F8;
    ascii_table[SDL_SCANCODE_F8].ctrl = KEY_CTRL_F8;

    ascii_table[SDL_SCANCODE_F9].normal = KEY_F9;
    ascii_table[SDL_SCANCODE_F9].shift = KEY_SHIFT_F9;
    ascii_table[SDL_SCANCODE_F9].left_alt = KEY_ALT_F9;
    ascii_table[SDL_SCANCODE_F9].right_alt = KEY_ALT_F9;
    ascii_table[SDL_SCANCODE_F9].ctrl = KEY_CTRL_F9;

    ascii_table[SDL_SCANCODE_F10].normal = KEY_F10;
    ascii_table[SDL_SCANCODE_F10].shift = KEY_SHIFT_F10;
    ascii_table[SDL_SCANCODE_F10].left_alt = KEY_ALT_F10;
    ascii_table[SDL_SCANCODE_F10].right_alt = KEY_ALT_F10;
    ascii_table[SDL_SCANCODE_F10].ctrl = KEY_CTRL_F10;

    ascii_table[SDL_SCANCODE_F11].normal = KEY_F11;
    ascii_table[SDL_SCANCODE_F11].shift = KEY_SHIFT_F11;
    ascii_table[SDL_SCANCODE_F11].left_alt = KEY_ALT_F11;
    ascii_table[SDL_SCANCODE_F11].right_alt = KEY_ALT_F11;
    ascii_table[SDL_SCANCODE_F11].ctrl = KEY_CTRL_F11;

    ascii_table[SDL_SCANCODE_F12].normal = KEY_F12;
    ascii_table[SDL_SCANCODE_F12].shift = KEY_SHIFT_F12;
    ascii_table[SDL_SCANCODE_F12].left_alt = KEY_ALT_F12;
    ascii_table[SDL_SCANCODE_F12].right_alt = KEY_ALT_F12;
    ascii_table[SDL_SCANCODE_F12].ctrl = KEY_CTRL_F12;

    switch (kb_layout) {
    case KEYBOARD_LAYOUT_QWERTY:
        k = SDL_SCANCODE_GRAVE;
        break;
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_2;
        break;
    case KEYBOARD_LAYOUT_ITALIAN:
    case KEYBOARD_LAYOUT_SPANISH:
        k = 0;
        break;
    default:
        k = SDL_SCANCODE_RIGHTBRACKET;
        break;
    }

    ascii_table[k].normal = KEY_GRAVE;
    ascii_table[k].shift = KEY_TILDE;
    ascii_table[k].left_alt = -1;
    ascii_table[k].right_alt = -1;
    ascii_table[k].ctrl = -1;

    ascii_table[SDL_SCANCODE_1].normal = KEY_1;
    ascii_table[SDL_SCANCODE_1].shift = KEY_EXCLAMATION;
    ascii_table[SDL_SCANCODE_1].left_alt = -1;
    ascii_table[SDL_SCANCODE_1].right_alt = -1;
    ascii_table[SDL_SCANCODE_1].ctrl = -1;

    ascii_table[SDL_SCANCODE_2].normal = KEY_2;
    ascii_table[SDL_SCANCODE_2].shift = KEY_AT;
    ascii_table[SDL_SCANCODE_2].left_alt = -1;
    ascii_table[SDL_SCANCODE_2].right_alt = -1;
    ascii_table[SDL_SCANCODE_2].ctrl = -1;

    ascii_table[SDL_SCANCODE_3].normal = KEY_3;
    ascii_table[SDL_SCANCODE_3].shift = KEY_NUMBER_SIGN;
    ascii_table[SDL_SCANCODE_3].left_alt = -1;
    ascii_table[SDL_SCANCODE_3].right_alt = -1;
    ascii_table[SDL_SCANCODE_3].ctrl = -1;

    ascii_table[SDL_SCANCODE_4].normal = KEY_4;
    ascii_table[SDL_SCANCODE_4].shift = KEY_DOLLAR;
    ascii_table[SDL_SCANCODE_4].left_alt = -1;
    ascii_table[SDL_SCANCODE_4].right_alt = -1;
    ascii_table[SDL_SCANCODE_4].ctrl = -1;

    ascii_table[SDL_SCANCODE_5].normal = KEY_5;
    ascii_table[SDL_SCANCODE_5].shift = KEY_PERCENT;
    ascii_table[SDL_SCANCODE_5].left_alt = -1;
    ascii_table[SDL_SCANCODE_5].right_alt = -1;
    ascii_table[SDL_SCANCODE_5].ctrl = -1;

    ascii_table[SDL_SCANCODE_6].normal = KEY_6;
    ascii_table[SDL_SCANCODE_6].shift = KEY_CARET;
    ascii_table[SDL_SCANCODE_6].left_alt = -1;
    ascii_table[SDL_SCANCODE_6].right_alt = -1;
    ascii_table[SDL_SCANCODE_6].ctrl = -1;

    ascii_table[SDL_SCANCODE_7].normal = KEY_7;
    ascii_table[SDL_SCANCODE_7].shift = KEY_AMPERSAND;
    ascii_table[SDL_SCANCODE_7].left_alt = -1;
    ascii_table[SDL_SCANCODE_7].right_alt = -1;
    ascii_table[SDL_SCANCODE_7].ctrl = -1;

    ascii_table[SDL_SCANCODE_8].normal = KEY_8;
    ascii_table[SDL_SCANCODE_8].shift = KEY_ASTERISK;
    ascii_table[SDL_SCANCODE_8].left_alt = -1;
    ascii_table[SDL_SCANCODE_8].right_alt = -1;
    ascii_table[SDL_SCANCODE_8].ctrl = -1;

    ascii_table[SDL_SCANCODE_9].normal = KEY_9;
    ascii_table[SDL_SCANCODE_9].shift = KEY_PAREN_LEFT;
    ascii_table[SDL_SCANCODE_9].left_alt = -1;
    ascii_table[SDL_SCANCODE_9].right_alt = -1;
    ascii_table[SDL_SCANCODE_9].ctrl = -1;

    ascii_table[SDL_SCANCODE_0].normal = KEY_0;
    ascii_table[SDL_SCANCODE_0].shift = KEY_PAREN_RIGHT;
    ascii_table[SDL_SCANCODE_0].left_alt = -1;
    ascii_table[SDL_SCANCODE_0].right_alt = -1;
    ascii_table[SDL_SCANCODE_0].ctrl = -1;

    switch (kb_layout) {
    case KEYBOARD_LAYOUT_QWERTY:
        k = SDL_SCANCODE_MINUS;
        break;
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_6;
        break;
    default:
        k = SDL_SCANCODE_SLASH;
        break;
    }

    ascii_table[k].normal = KEY_MINUS;
    ascii_table[k].shift = KEY_UNDERSCORE;
    ascii_table[k].left_alt = -1;
    ascii_table[k].right_alt = -1;
    ascii_table[k].ctrl = -1;

    switch (kb_layout) {
    case KEYBOARD_LAYOUT_QWERTY:
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_EQUALS;
        break;
    default:
        k = SDL_SCANCODE_0;
        break;
    }

    ascii_table[k].normal = KEY_EQUAL;
    ascii_table[k].shift = KEY_PLUS;
    ascii_table[k].left_alt = -1;
    ascii_table[k].right_alt = -1;
    ascii_table[k].ctrl = -1;

    ascii_table[SDL_SCANCODE_BACKSPACE].normal = KEY_BACKSPACE;
    ascii_table[SDL_SCANCODE_BACKSPACE].shift = KEY_BACKSPACE;
    ascii_table[SDL_SCANCODE_BACKSPACE].left_alt = KEY_BACKSPACE;
    ascii_table[SDL_SCANCODE_BACKSPACE].right_alt = KEY_BACKSPACE;
    ascii_table[SDL_SCANCODE_BACKSPACE].ctrl = KEY_DEL;

    ascii_table[SDL_SCANCODE_TAB].normal = KEY_TAB;
    ascii_table[SDL_SCANCODE_TAB].shift = KEY_TAB;
    ascii_table[SDL_SCANCODE_TAB].left_alt = KEY_TAB;
    ascii_table[SDL_SCANCODE_TAB].right_alt = KEY_TAB;
    ascii_table[SDL_SCANCODE_TAB].ctrl = KEY_TAB;

    switch (kb_layout) {
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_A;
        break;
    default:
        k = SDL_SCANCODE_Q;
        break;
    }

    ascii_table[k].normal = KEY_LOWERCASE_Q;
    ascii_table[k].shift = KEY_UPPERCASE_Q;
    ascii_table[k].left_alt = KEY_ALT_Q;
    ascii_table[k].right_alt = KEY_ALT_Q;
    ascii_table[k].ctrl = KEY_CTRL_Q;

    switch (kb_layout) {
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_Z;
        break;
    default:
        k = SDL_SCANCODE_W;
        break;
    }

    ascii_table[k].normal = KEY_LOWERCASE_W;
    ascii_table[k].shift = KEY_UPPERCASE_W;
    ascii_table[k].left_alt = KEY_ALT_W;
    ascii_table[k].right_alt = KEY_ALT_W;
    ascii_table[k].ctrl = KEY_CTRL_W;

    ascii_table[SDL_SCANCODE_E].normal = KEY_LOWERCASE_E;
    ascii_table[SDL_SCANCODE_E].shift = KEY_UPPERCASE_E;
    ascii_table[SDL_SCANCODE_E].left_alt = KEY_ALT_E;
    ascii_table[SDL_SCANCODE_E].right_alt = KEY_ALT_E;
    ascii_table[SDL_SCANCODE_E].ctrl = KEY_CTRL_E;

    ascii_table[SDL_SCANCODE_R].normal = KEY_LOWERCASE_R;
    ascii_table[SDL_SCANCODE_R].shift = KEY_UPPERCASE_R;
    ascii_table[SDL_SCANCODE_R].left_alt = KEY_ALT_R;
    ascii_table[SDL_SCANCODE_R].right_alt = KEY_ALT_R;
    ascii_table[SDL_SCANCODE_R].ctrl = KEY_CTRL_R;

    ascii_table[SDL_SCANCODE_T].normal = KEY_LOWERCASE_T;
    ascii_table[SDL_SCANCODE_T].shift = KEY_UPPERCASE_T;
    ascii_table[SDL_SCANCODE_T].left_alt = KEY_ALT_T;
    ascii_table[SDL_SCANCODE_T].right_alt = KEY_ALT_T;
    ascii_table[SDL_SCANCODE_T].ctrl = KEY_CTRL_T;

    switch (kb_layout) {
    case KEYBOARD_LAYOUT_QWERTY:
    case KEYBOARD_LAYOUT_FRENCH:
    case KEYBOARD_LAYOUT_ITALIAN:
    case KEYBOARD_LAYOUT_SPANISH:
        k = SDL_SCANCODE_Y;
        break;
    default:
        k = SDL_SCANCODE_Z;
        break;
    }

    ascii_table[k].normal = KEY_LOWERCASE_Y;
    ascii_table[k].shift = KEY_UPPERCASE_Y;
    ascii_table[k].left_alt = KEY_ALT_Y;
    ascii_table[k].right_alt = KEY_ALT_Y;
    ascii_table[k].ctrl = KEY_CTRL_Y;

    ascii_table[SDL_SCANCODE_U].normal = KEY_LOWERCASE_U;
    ascii_table[SDL_SCANCODE_U].shift = KEY_UPPERCASE_U;
    ascii_table[SDL_SCANCODE_U].left_alt = KEY_ALT_U;
    ascii_table[SDL_SCANCODE_U].right_alt = KEY_ALT_U;
    ascii_table[SDL_SCANCODE_U].ctrl = KEY_CTRL_U;

    ascii_table[SDL_SCANCODE_I].normal = KEY_LOWERCASE_I;
    ascii_table[SDL_SCANCODE_I].shift = KEY_UPPERCASE_I;
    ascii_table[SDL_SCANCODE_I].left_alt = KEY_ALT_I;
    ascii_table[SDL_SCANCODE_I].right_alt = KEY_ALT_I;
    ascii_table[SDL_SCANCODE_I].ctrl = KEY_CTRL_I;

    ascii_table[SDL_SCANCODE_O].normal = KEY_LOWERCASE_O;
    ascii_table[SDL_SCANCODE_O].shift = KEY_UPPERCASE_O;
    ascii_table[SDL_SCANCODE_O].left_alt = KEY_ALT_O;
    ascii_table[SDL_SCANCODE_O].right_alt = KEY_ALT_O;
    ascii_table[SDL_SCANCODE_O].ctrl = KEY_CTRL_O;

    ascii_table[SDL_SCANCODE_P].normal = KEY_LOWERCASE_P;
    ascii_table[SDL_SCANCODE_P].shift = KEY_UPPERCASE_P;
    ascii_table[SDL_SCANCODE_P].left_alt = KEY_ALT_P;
    ascii_table[SDL_SCANCODE_P].right_alt = KEY_ALT_P;
    ascii_table[SDL_SCANCODE_P].ctrl = KEY_CTRL_P;

    switch (kb_layout) {
    case KEYBOARD_LAYOUT_QWERTY:
    case KEYBOARD_LAYOUT_ITALIAN:
    case KEYBOARD_LAYOUT_SPANISH:
        k = SDL_SCANCODE_LEFTBRACKET;
        break;
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_5;
        break;
    default:
        k = SDL_SCANCODE_8;
        break;
    }

    ascii_table[k].normal = KEY_BRACKET_LEFT;
    ascii_table[k].shift = KEY_BRACE_LEFT;
    ascii_table[k].left_alt = -1;
    ascii_table[k].right_alt = -1;
    ascii_table[k].ctrl = -1;

    switch (kb_layout) {
    case KEYBOARD_LAYOUT_QWERTY:
    case KEYBOARD_LAYOUT_ITALIAN:
    case KEYBOARD_LAYOUT_SPANISH:
        k = SDL_SCANCODE_RIGHTBRACKET;
        break;
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_MINUS;
        break;
    default:
        k = SDL_SCANCODE_9;
        break;
    }

    ascii_table[k].normal = KEY_BRACKET_RIGHT;
    ascii_table[k].shift = KEY_BRACE_RIGHT;
    ascii_table[k].left_alt = -1;
    ascii_table[k].right_alt = -1;
    ascii_table[k].ctrl = -1;

    switch (kb_layout) {
    case KEYBOARD_LAYOUT_QWERTY:
        k = SDL_SCANCODE_BACKSLASH;
        break;
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_8;
        break;
    case KEYBOARD_LAYOUT_ITALIAN:
    case KEYBOARD_LAYOUT_SPANISH:
        k = SDL_SCANCODE_GRAVE;
        break;
    default:
        k = SDL_SCANCODE_MINUS;
        break;
    }

    ascii_table[k].normal = KEY_BACKSLASH;
    ascii_table[k].shift = KEY_BAR;
    ascii_table[k].left_alt = -1;
    ascii_table[k].right_alt = -1;
    ascii_table[k].ctrl = KEY_CTRL_BACKSLASH;

    ascii_table[SDL_SCANCODE_CAPSLOCK].normal = -1;
    ascii_table[SDL_SCANCODE_CAPSLOCK].shift = -1;
    ascii_table[SDL_SCANCODE_CAPSLOCK].left_alt = -1;
    ascii_table[SDL_SCANCODE_CAPSLOCK].right_alt = -1;
    ascii_table[SDL_SCANCODE_CAPSLOCK].ctrl = -1;

    switch (kb_layout) {
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_Q;
        break;
    default:
        k = SDL_SCANCODE_A;
        break;
    }

    ascii_table[k].normal = KEY_LOWERCASE_A;
    ascii_table[k].shift = KEY_UPPERCASE_A;
    ascii_table[k].left_alt = KEY_ALT_A;
    ascii_table[k].right_alt = KEY_ALT_A;
    ascii_table[k].ctrl = KEY_CTRL_A;

    ascii_table[SDL_SCANCODE_S].normal = KEY_LOWERCASE_S;
    ascii_table[SDL_SCANCODE_S].shift = KEY_UPPERCASE_S;
    ascii_table[SDL_SCANCODE_S].left_alt = KEY_ALT_S;
    ascii_table[SDL_SCANCODE_S].right_alt = KEY_ALT_S;
    ascii_table[SDL_SCANCODE_S].ctrl = KEY_CTRL_S;

    ascii_table[SDL_SCANCODE_D].normal = KEY_LOWERCASE_D;
    ascii_table[SDL_SCANCODE_D].shift = KEY_UPPERCASE_D;
    ascii_table[SDL_SCANCODE_D].left_alt = KEY_ALT_D;
    ascii_table[SDL_SCANCODE_D].right_alt = KEY_ALT_D;
    ascii_table[SDL_SCANCODE_D].ctrl = KEY_CTRL_D;

    ascii_table[SDL_SCANCODE_F].normal = KEY_LOWERCASE_F;
    ascii_table[SDL_SCANCODE_F].shift = KEY_UPPERCASE_F;
    ascii_table[SDL_SCANCODE_F].left_alt = KEY_ALT_F;
    ascii_table[SDL_SCANCODE_F].right_alt = KEY_ALT_F;
    ascii_table[SDL_SCANCODE_F].ctrl = KEY_CTRL_F;

    ascii_table[SDL_SCANCODE_G].normal = KEY_LOWERCASE_G;
    ascii_table[SDL_SCANCODE_G].shift = KEY_UPPERCASE_G;
    ascii_table[SDL_SCANCODE_G].left_alt = KEY_ALT_G;
    ascii_table[SDL_SCANCODE_G].right_alt = KEY_ALT_G;
    ascii_table[SDL_SCANCODE_G].ctrl = KEY_CTRL_G;

    ascii_table[SDL_SCANCODE_H].normal = KEY_LOWERCASE_H;
    ascii_table[SDL_SCANCODE_H].shift = KEY_UPPERCASE_H;
    ascii_table[SDL_SCANCODE_H].left_alt = KEY_ALT_H;
    ascii_table[SDL_SCANCODE_H].right_alt = KEY_ALT_H;
    ascii_table[SDL_SCANCODE_H].ctrl = KEY_CTRL_H;

    ascii_table[SDL_SCANCODE_J].normal = KEY_LOWERCASE_J;
    ascii_table[SDL_SCANCODE_J].shift = KEY_UPPERCASE_J;
    ascii_table[SDL_SCANCODE_J].left_alt = KEY_ALT_J;
    ascii_table[SDL_SCANCODE_J].right_alt = KEY_ALT_J;
    ascii_table[SDL_SCANCODE_J].ctrl = KEY_CTRL_J;

    ascii_table[SDL_SCANCODE_K].normal = KEY_LOWERCASE_K;
    ascii_table[SDL_SCANCODE_K].shift = KEY_UPPERCASE_K;
    ascii_table[SDL_SCANCODE_K].left_alt = KEY_ALT_K;
    ascii_table[SDL_SCANCODE_K].right_alt = KEY_ALT_K;
    ascii_table[SDL_SCANCODE_K].ctrl = KEY_CTRL_K;

    ascii_table[SDL_SCANCODE_L].normal = KEY_LOWERCASE_L;
    ascii_table[SDL_SCANCODE_L].shift = KEY_UPPERCASE_L;
    ascii_table[SDL_SCANCODE_L].left_alt = KEY_ALT_L;
    ascii_table[SDL_SCANCODE_L].right_alt = KEY_ALT_L;
    ascii_table[SDL_SCANCODE_L].ctrl = KEY_CTRL_L;

    switch (kb_layout) {
    case KEYBOARD_LAYOUT_QWERTY:
        k = SDL_SCANCODE_SEMICOLON;
        break;
    default:
        k = SDL_SCANCODE_COMMA;
        break;
    }

    ascii_table[k].normal = KEY_SEMICOLON;
    ascii_table[k].shift = KEY_COLON;
    ascii_table[k].left_alt = -1;
    ascii_table[k].right_alt = -1;
    ascii_table[k].ctrl = -1;

    switch (kb_layout) {
    case KEYBOARD_LAYOUT_QWERTY:
        k = SDL_SCANCODE_APOSTROPHE;
        break;
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_3;
        break;
    default:
        k = SDL_SCANCODE_2;
        break;
    }

    ascii_table[k].normal = KEY_SINGLE_QUOTE;
    ascii_table[k].shift = KEY_QUOTE;
    ascii_table[k].left_alt = -1;
    ascii_table[k].right_alt = -1;
    ascii_table[k].ctrl = -1;

    ascii_table[SDL_SCANCODE_RETURN].normal = KEY_RETURN;
    ascii_table[SDL_SCANCODE_RETURN].shift = KEY_RETURN;
    ascii_table[SDL_SCANCODE_RETURN].left_alt = KEY_RETURN;
    ascii_table[SDL_SCANCODE_RETURN].right_alt = KEY_RETURN;
    ascii_table[SDL_SCANCODE_RETURN].ctrl = KEY_CTRL_J;

    ascii_table[SDL_SCANCODE_LSHIFT].normal = -1;
    ascii_table[SDL_SCANCODE_LSHIFT].shift = -1;
    ascii_table[SDL_SCANCODE_LSHIFT].left_alt = -1;
    ascii_table[SDL_SCANCODE_LSHIFT].right_alt = -1;
    ascii_table[SDL_SCANCODE_LSHIFT].ctrl = -1;

    switch (kb_layout) {
    case KEYBOARD_LAYOUT_QWERTY:
    case KEYBOARD_LAYOUT_ITALIAN:
    case KEYBOARD_LAYOUT_SPANISH:
        k = SDL_SCANCODE_Z;
        break;
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_W;
        break;
    default:
        k = SDL_SCANCODE_Y;
        break;
    }

    ascii_table[k].normal = KEY_LOWERCASE_Z;
    ascii_table[k].shift = KEY_UPPERCASE_Z;
    ascii_table[k].left_alt = KEY_ALT_Z;
    ascii_table[k].right_alt = KEY_ALT_Z;
    ascii_table[k].ctrl = KEY_CTRL_Z;

    ascii_table[SDL_SCANCODE_X].normal = KEY_LOWERCASE_X;
    ascii_table[SDL_SCANCODE_X].shift = KEY_UPPERCASE_X;
    ascii_table[SDL_SCANCODE_X].left_alt = KEY_ALT_X;
    ascii_table[SDL_SCANCODE_X].right_alt = KEY_ALT_X;
    ascii_table[SDL_SCANCODE_X].ctrl = KEY_CTRL_X;

    ascii_table[SDL_SCANCODE_C].normal = KEY_LOWERCASE_C;
    ascii_table[SDL_SCANCODE_C].shift = KEY_UPPERCASE_C;
    ascii_table[SDL_SCANCODE_C].left_alt = KEY_ALT_C;
    ascii_table[SDL_SCANCODE_C].right_alt = KEY_ALT_C;
    ascii_table[SDL_SCANCODE_C].ctrl = KEY_CTRL_C;

    ascii_table[SDL_SCANCODE_V].normal = KEY_LOWERCASE_V;
    ascii_table[SDL_SCANCODE_V].shift = KEY_UPPERCASE_V;
    ascii_table[SDL_SCANCODE_V].left_alt = KEY_ALT_V;
    ascii_table[SDL_SCANCODE_V].right_alt = KEY_ALT_V;
    ascii_table[SDL_SCANCODE_V].ctrl = KEY_CTRL_V;

    ascii_table[SDL_SCANCODE_B].normal = KEY_LOWERCASE_B;
    ascii_table[SDL_SCANCODE_B].shift = KEY_UPPERCASE_B;
    ascii_table[SDL_SCANCODE_B].left_alt = KEY_ALT_B;
    ascii_table[SDL_SCANCODE_B].right_alt = KEY_ALT_B;
    ascii_table[SDL_SCANCODE_B].ctrl = KEY_CTRL_B;

    ascii_table[SDL_SCANCODE_N].normal = KEY_LOWERCASE_N;
    ascii_table[SDL_SCANCODE_N].shift = KEY_UPPERCASE_N;
    ascii_table[SDL_SCANCODE_N].left_alt = KEY_ALT_N;
    ascii_table[SDL_SCANCODE_N].right_alt = KEY_ALT_N;
    ascii_table[SDL_SCANCODE_N].ctrl = KEY_CTRL_N;

    switch (kb_layout) {
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_SEMICOLON;
        break;
    default:
        k = SDL_SCANCODE_M;
        break;
    }

    ascii_table[k].normal = KEY_LOWERCASE_M;
    ascii_table[k].shift = KEY_UPPERCASE_M;
    ascii_table[k].left_alt = KEY_ALT_M;
    ascii_table[k].right_alt = KEY_ALT_M;
    ascii_table[k].ctrl = KEY_CTRL_M;

    switch (kb_layout) {
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_M;
        break;
    default:
        k = SDL_SCANCODE_COMMA;
        break;
    }

    ascii_table[k].normal = KEY_COMMA;
    ascii_table[k].shift = KEY_LESS;
    ascii_table[k].left_alt = -1;
    ascii_table[k].right_alt = -1;
    ascii_table[k].ctrl = -1;

    switch (kb_layout) {
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_COMMA;
        break;
    default:
        k = SDL_SCANCODE_PERIOD;
        break;
    }

    ascii_table[k].normal = KEY_DOT;
    ascii_table[k].shift = KEY_GREATER;
    ascii_table[k].left_alt = -1;
    ascii_table[k].right_alt = -1;
    ascii_table[k].ctrl = -1;

    switch (kb_layout) {
    case KEYBOARD_LAYOUT_QWERTY:
        k = SDL_SCANCODE_SLASH;
        break;
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_PERIOD;
        break;
    default:
        k = SDL_SCANCODE_7;
        break;
    }

    ascii_table[k].normal = KEY_SLASH;
    ascii_table[k].shift = KEY_QUESTION;
    ascii_table[k].left_alt = -1;
    ascii_table[k].right_alt = -1;
    ascii_table[k].ctrl = -1;

    ascii_table[SDL_SCANCODE_RSHIFT].normal = -1;
    ascii_table[SDL_SCANCODE_RSHIFT].shift = -1;
    ascii_table[SDL_SCANCODE_RSHIFT].left_alt = -1;
    ascii_table[SDL_SCANCODE_RSHIFT].right_alt = -1;
    ascii_table[SDL_SCANCODE_RSHIFT].ctrl = -1;

    ascii_table[SDL_SCANCODE_LCTRL].normal = -1;
    ascii_table[SDL_SCANCODE_LCTRL].shift = -1;
    ascii_table[SDL_SCANCODE_LCTRL].left_alt = -1;
    ascii_table[SDL_SCANCODE_LCTRL].right_alt = -1;
    ascii_table[SDL_SCANCODE_LCTRL].ctrl = -1;

    ascii_table[SDL_SCANCODE_LALT].normal = -1;
    ascii_table[SDL_SCANCODE_LALT].shift = -1;
    ascii_table[SDL_SCANCODE_LALT].left_alt = -1;
    ascii_table[SDL_SCANCODE_LALT].right_alt = -1;
    ascii_table[SDL_SCANCODE_LALT].ctrl = -1;

    ascii_table[SDL_SCANCODE_SPACE].normal = KEY_SPACE;
    ascii_table[SDL_SCANCODE_SPACE].shift = KEY_SPACE;
    ascii_table[SDL_SCANCODE_SPACE].left_alt = KEY_SPACE;
    ascii_table[SDL_SCANCODE_SPACE].right_alt = KEY_SPACE;
    ascii_table[SDL_SCANCODE_SPACE].ctrl = KEY_SPACE;

    ascii_table[SDL_SCANCODE_RALT].normal = -1;
    ascii_table[SDL_SCANCODE_RALT].shift = -1;
    ascii_table[SDL_SCANCODE_RALT].left_alt = -1;
    ascii_table[SDL_SCANCODE_RALT].right_alt = -1;
    ascii_table[SDL_SCANCODE_RALT].ctrl = -1;

    ascii_table[SDL_SCANCODE_RCTRL].normal = -1;
    ascii_table[SDL_SCANCODE_RCTRL].shift = -1;
    ascii_table[SDL_SCANCODE_RCTRL].left_alt = -1;
    ascii_table[SDL_SCANCODE_RCTRL].right_alt = -1;
    ascii_table[SDL_SCANCODE_RCTRL].ctrl = -1;

    ascii_table[SDL_SCANCODE_INSERT].normal = KEY_INSERT;
    ascii_table[SDL_SCANCODE_INSERT].shift = KEY_INSERT;
    ascii_table[SDL_SCANCODE_INSERT].left_alt = KEY_ALT_INSERT;
    ascii_table[SDL_SCANCODE_INSERT].right_alt = KEY_ALT_INSERT;
    ascii_table[SDL_SCANCODE_INSERT].ctrl = KEY_CTRL_INSERT;

    ascii_table[SDL_SCANCODE_HOME].normal = KEY_HOME;
    ascii_table[SDL_SCANCODE_HOME].shift = KEY_HOME;
    ascii_table[SDL_SCANCODE_HOME].left_alt = KEY_ALT_HOME;
    ascii_table[SDL_SCANCODE_HOME].right_alt = KEY_ALT_HOME;
    ascii_table[SDL_SCANCODE_HOME].ctrl = KEY_CTRL_HOME;

    ascii_table[SDL_SCANCODE_PRIOR].normal = KEY_PAGE_UP;
    ascii_table[SDL_SCANCODE_PRIOR].shift = KEY_PAGE_UP;
    ascii_table[SDL_SCANCODE_PRIOR].left_alt = KEY_ALT_PAGE_UP;
    ascii_table[SDL_SCANCODE_PRIOR].right_alt = KEY_ALT_PAGE_UP;
    ascii_table[SDL_SCANCODE_PRIOR].ctrl = KEY_CTRL_PAGE_UP;

    ascii_table[SDL_SCANCODE_PAGEUP].normal = KEY_PAGE_UP;
    ascii_table[SDL_SCANCODE_PAGEUP].shift = KEY_PAGE_UP;
    ascii_table[SDL_SCANCODE_PAGEUP].left_alt = KEY_ALT_PAGE_UP;
    ascii_table[SDL_SCANCODE_PAGEUP].right_alt = KEY_ALT_PAGE_UP;
    ascii_table[SDL_SCANCODE_PAGEUP].ctrl = KEY_CTRL_PAGE_UP;

    ascii_table[SDL_SCANCODE_DELETE].normal = KEY_DELETE;
    ascii_table[SDL_SCANCODE_DELETE].shift = KEY_DELETE;
    ascii_table[SDL_SCANCODE_DELETE].left_alt = KEY_ALT_DELETE;
    ascii_table[SDL_SCANCODE_DELETE].right_alt = KEY_ALT_DELETE;
    ascii_table[SDL_SCANCODE_DELETE].ctrl = KEY_CTRL_DELETE;

    ascii_table[SDL_SCANCODE_END].normal = KEY_END;
    ascii_table[SDL_SCANCODE_END].shift = KEY_END;
    ascii_table[SDL_SCANCODE_END].left_alt = KEY_ALT_END;
    ascii_table[SDL_SCANCODE_END].right_alt = KEY_ALT_END;
    ascii_table[SDL_SCANCODE_END].ctrl = KEY_CTRL_END;

    ascii_table[SDL_SCANCODE_PAGEDOWN].normal = KEY_PAGE_DOWN;
    ascii_table[SDL_SCANCODE_PAGEDOWN].shift = KEY_PAGE_DOWN;
    ascii_table[SDL_SCANCODE_PAGEDOWN].left_alt = KEY_ALT_PAGE_DOWN;
    ascii_table[SDL_SCANCODE_PAGEDOWN].right_alt = KEY_ALT_PAGE_DOWN;
    ascii_table[SDL_SCANCODE_PAGEDOWN].ctrl = KEY_CTRL_PAGE_DOWN;

    ascii_table[SDL_SCANCODE_UP].normal = KEY_ARROW_UP;
    ascii_table[SDL_SCANCODE_UP].shift = KEY_ARROW_UP;
    ascii_table[SDL_SCANCODE_UP].left_alt = KEY_ALT_ARROW_UP;
    ascii_table[SDL_SCANCODE_UP].right_alt = KEY_ALT_ARROW_UP;
    ascii_table[SDL_SCANCODE_UP].ctrl = KEY_CTRL_ARROW_UP;

    ascii_table[SDL_SCANCODE_DOWN].normal = KEY_ARROW_DOWN;
    ascii_table[SDL_SCANCODE_DOWN].shift = KEY_ARROW_DOWN;
    ascii_table[SDL_SCANCODE_DOWN].left_alt = KEY_ALT_ARROW_DOWN;
    ascii_table[SDL_SCANCODE_DOWN].right_alt = KEY_ALT_ARROW_DOWN;
    ascii_table[SDL_SCANCODE_DOWN].ctrl = KEY_CTRL_ARROW_DOWN;

    ascii_table[SDL_SCANCODE_LEFT].normal = KEY_ARROW_LEFT;
    ascii_table[SDL_SCANCODE_LEFT].shift = KEY_ARROW_LEFT;
    ascii_table[SDL_SCANCODE_LEFT].left_alt = KEY_ALT_ARROW_LEFT;
    ascii_table[SDL_SCANCODE_LEFT].right_alt = KEY_ALT_ARROW_LEFT;
    ascii_table[SDL_SCANCODE_LEFT].ctrl = KEY_CTRL_ARROW_LEFT;

    ascii_table[SDL_SCANCODE_RIGHT].normal = KEY_ARROW_RIGHT;
    ascii_table[SDL_SCANCODE_RIGHT].shift = KEY_ARROW_RIGHT;
    ascii_table[SDL_SCANCODE_RIGHT].left_alt = KEY_ALT_ARROW_RIGHT;
    ascii_table[SDL_SCANCODE_RIGHT].right_alt = KEY_ALT_ARROW_RIGHT;
    ascii_table[SDL_SCANCODE_RIGHT].ctrl = KEY_CTRL_ARROW_RIGHT;

    ascii_table[SDL_SCANCODE_NUMLOCKCLEAR].normal = -1;
    ascii_table[SDL_SCANCODE_NUMLOCKCLEAR].shift = -1;
    ascii_table[SDL_SCANCODE_NUMLOCKCLEAR].left_alt = -1;
    ascii_table[SDL_SCANCODE_NUMLOCKCLEAR].right_alt = -1;
    ascii_table[SDL_SCANCODE_NUMLOCKCLEAR].ctrl = -1;

    ascii_table[SDL_SCANCODE_KP_DIVIDE].normal = KEY_SLASH;
    ascii_table[SDL_SCANCODE_KP_DIVIDE].shift = KEY_SLASH;
    ascii_table[SDL_SCANCODE_KP_DIVIDE].left_alt = -1;
    ascii_table[SDL_SCANCODE_KP_DIVIDE].right_alt = -1;
    ascii_table[SDL_SCANCODE_KP_DIVIDE].ctrl = 3;

    ascii_table[SDL_SCANCODE_KP_MULTIPLY].normal = KEY_ASTERISK;
    ascii_table[SDL_SCANCODE_KP_MULTIPLY].shift = KEY_ASTERISK;
    ascii_table[SDL_SCANCODE_KP_MULTIPLY].left_alt = -1;
    ascii_table[SDL_SCANCODE_KP_MULTIPLY].right_alt = -1;
    ascii_table[SDL_SCANCODE_KP_MULTIPLY].ctrl = -1;

    ascii_table[SDL_SCANCODE_KP_MINUS].normal = KEY_MINUS;
    ascii_table[SDL_SCANCODE_KP_MINUS].shift = KEY_MINUS;
    ascii_table[SDL_SCANCODE_KP_MINUS].left_alt = -1;
    ascii_table[SDL_SCANCODE_KP_MINUS].right_alt = -1;
    ascii_table[SDL_SCANCODE_KP_MINUS].ctrl = -1;

    ascii_table[SDL_SCANCODE_KP_7].normal = KEY_HOME;
    ascii_table[SDL_SCANCODE_KP_7].shift = KEY_7;
    ascii_table[SDL_SCANCODE_KP_7].left_alt = KEY_ALT_HOME;
    ascii_table[SDL_SCANCODE_KP_7].right_alt = KEY_ALT_HOME;
    ascii_table[SDL_SCANCODE_KP_7].ctrl = KEY_CTRL_HOME;

    ascii_table[SDL_SCANCODE_KP_8].normal = KEY_ARROW_UP;
    ascii_table[SDL_SCANCODE_KP_8].shift = KEY_8;
    ascii_table[SDL_SCANCODE_KP_8].left_alt = KEY_ALT_ARROW_UP;
    ascii_table[SDL_SCANCODE_KP_8].right_alt = KEY_ALT_ARROW_UP;
    ascii_table[SDL_SCANCODE_KP_8].ctrl = KEY_CTRL_ARROW_UP;

    ascii_table[SDL_SCANCODE_KP_9].normal = KEY_PAGE_UP;
    ascii_table[SDL_SCANCODE_KP_9].shift = KEY_9;
    ascii_table[SDL_SCANCODE_KP_9].left_alt = KEY_ALT_PAGE_UP;
    ascii_table[SDL_SCANCODE_KP_9].right_alt = KEY_ALT_PAGE_UP;
    ascii_table[SDL_SCANCODE_KP_9].ctrl = KEY_CTRL_PAGE_UP;

    ascii_table[SDL_SCANCODE_KP_PLUS].normal = KEY_PLUS;
    ascii_table[SDL_SCANCODE_KP_PLUS].shift = KEY_PLUS;
    ascii_table[SDL_SCANCODE_KP_PLUS].left_alt = -1;
    ascii_table[SDL_SCANCODE_KP_PLUS].right_alt = -1;
    ascii_table[SDL_SCANCODE_KP_PLUS].ctrl = -1;

    ascii_table[SDL_SCANCODE_KP_4].normal = KEY_ARROW_LEFT;
    ascii_table[SDL_SCANCODE_KP_4].shift = KEY_4;
    ascii_table[SDL_SCANCODE_KP_4].left_alt = KEY_ALT_ARROW_LEFT;
    ascii_table[SDL_SCANCODE_KP_4].right_alt = KEY_ALT_ARROW_LEFT;
    ascii_table[SDL_SCANCODE_KP_4].ctrl = KEY_CTRL_ARROW_LEFT;

    ascii_table[SDL_SCANCODE_KP_5].normal = KEY_NUMBERPAD_5;
    ascii_table[SDL_SCANCODE_KP_5].shift = KEY_5;
    ascii_table[SDL_SCANCODE_KP_5].left_alt = KEY_ALT_NUMBERPAD_5;
    ascii_table[SDL_SCANCODE_KP_5].right_alt = KEY_ALT_NUMBERPAD_5;
    ascii_table[SDL_SCANCODE_KP_5].ctrl = KEY_CTRL_NUMBERPAD_5;

    ascii_table[SDL_SCANCODE_KP_6].normal = KEY_ARROW_RIGHT;
    ascii_table[SDL_SCANCODE_KP_6].shift = KEY_6;
    ascii_table[SDL_SCANCODE_KP_6].left_alt = KEY_ALT_ARROW_RIGHT;
    ascii_table[SDL_SCANCODE_KP_6].right_alt = KEY_ALT_ARROW_RIGHT;
    ascii_table[SDL_SCANCODE_KP_6].ctrl = KEY_CTRL_ARROW_RIGHT;

    ascii_table[SDL_SCANCODE_KP_1].normal = KEY_END;
    ascii_table[SDL_SCANCODE_KP_1].shift = KEY_1;
    ascii_table[SDL_SCANCODE_KP_1].left_alt = KEY_ALT_END;
    ascii_table[SDL_SCANCODE_KP_1].right_alt = KEY_ALT_END;
    ascii_table[SDL_SCANCODE_KP_1].ctrl = KEY_CTRL_END;

    ascii_table[SDL_SCANCODE_KP_2].normal = KEY_ARROW_DOWN;
    ascii_table[SDL_SCANCODE_KP_2].shift = KEY_2;
    ascii_table[SDL_SCANCODE_KP_2].left_alt = KEY_ALT_ARROW_DOWN;
    ascii_table[SDL_SCANCODE_KP_2].right_alt = KEY_ALT_ARROW_DOWN;
    ascii_table[SDL_SCANCODE_KP_2].ctrl = KEY_CTRL_ARROW_DOWN;

    ascii_table[SDL_SCANCODE_KP_3].normal = KEY_PAGE_DOWN;
    ascii_table[SDL_SCANCODE_KP_3].shift = KEY_3;
    ascii_table[SDL_SCANCODE_KP_3].left_alt = KEY_ALT_PAGE_DOWN;
    ascii_table[SDL_SCANCODE_KP_3].right_alt = KEY_ALT_PAGE_DOWN;
    ascii_table[SDL_SCANCODE_KP_3].ctrl = KEY_CTRL_PAGE_DOWN;

    ascii_table[SDL_SCANCODE_KP_ENTER].normal = KEY_RETURN;
    ascii_table[SDL_SCANCODE_KP_ENTER].shift = KEY_RETURN;
    ascii_table[SDL_SCANCODE_KP_ENTER].left_alt = -1;
    ascii_table[SDL_SCANCODE_KP_ENTER].right_alt = -1;
    ascii_table[SDL_SCANCODE_KP_ENTER].ctrl = -1;

    ascii_table[SDL_SCANCODE_KP_0].normal = KEY_INSERT;
    ascii_table[SDL_SCANCODE_KP_0].shift = KEY_0;
    ascii_table[SDL_SCANCODE_KP_0].left_alt = KEY_ALT_INSERT;
    ascii_table[SDL_SCANCODE_KP_0].right_alt = KEY_ALT_INSERT;
    ascii_table[SDL_SCANCODE_KP_0].ctrl = KEY_CTRL_INSERT;

    ascii_table[SDL_SCANCODE_KP_DECIMAL].normal = KEY_DELETE;
    ascii_table[SDL_SCANCODE_KP_DECIMAL].shift = KEY_DOT;
    ascii_table[SDL_SCANCODE_KP_DECIMAL].left_alt = -1;
    ascii_table[SDL_SCANCODE_KP_DECIMAL].right_alt = KEY_ALT_DELETE;
    ascii_table[SDL_SCANCODE_KP_DECIMAL].ctrl = KEY_CTRL_DELETE;
}

// 0x4BABD8
static void kb_map_ascii_French()
{
    int k;

    kb_map_ascii_English_US();

    ascii_table[SDL_SCANCODE_GRAVE].normal = KEY_178;
    ascii_table[SDL_SCANCODE_GRAVE].shift = -1;
    ascii_table[SDL_SCANCODE_GRAVE].left_alt = -1;
    ascii_table[SDL_SCANCODE_GRAVE].right_alt = -1;
    ascii_table[SDL_SCANCODE_GRAVE].ctrl = -1;

    ascii_table[SDL_SCANCODE_1].normal = KEY_AMPERSAND;
    ascii_table[SDL_SCANCODE_1].shift = KEY_1;
    ascii_table[SDL_SCANCODE_1].left_alt = -1;
    ascii_table[SDL_SCANCODE_1].right_alt = -1;
    ascii_table[SDL_SCANCODE_1].ctrl = -1;

    ascii_table[SDL_SCANCODE_2].normal = KEY_233;
    ascii_table[SDL_SCANCODE_2].shift = KEY_2;
    ascii_table[SDL_SCANCODE_2].left_alt = -1;
    ascii_table[SDL_SCANCODE_2].right_alt = KEY_152;
    ascii_table[SDL_SCANCODE_2].ctrl = -1;

    ascii_table[SDL_SCANCODE_3].normal = KEY_QUOTE;
    ascii_table[SDL_SCANCODE_3].shift = KEY_3;
    ascii_table[SDL_SCANCODE_3].left_alt = -1;
    ascii_table[SDL_SCANCODE_3].right_alt = KEY_NUMBER_SIGN;
    ascii_table[SDL_SCANCODE_3].ctrl = -1;

    ascii_table[SDL_SCANCODE_4].normal = KEY_SINGLE_QUOTE;
    ascii_table[SDL_SCANCODE_4].shift = KEY_4;
    ascii_table[SDL_SCANCODE_4].left_alt = -1;
    ascii_table[SDL_SCANCODE_4].right_alt = KEY_BRACE_LEFT;
    ascii_table[SDL_SCANCODE_4].ctrl = -1;

    ascii_table[SDL_SCANCODE_5].normal = KEY_PAREN_LEFT;
    ascii_table[SDL_SCANCODE_5].shift = KEY_5;
    ascii_table[SDL_SCANCODE_5].left_alt = -1;
    ascii_table[SDL_SCANCODE_5].right_alt = KEY_BRACKET_LEFT;
    ascii_table[SDL_SCANCODE_5].ctrl = -1;

    ascii_table[SDL_SCANCODE_6].normal = KEY_150;
    ascii_table[SDL_SCANCODE_6].shift = KEY_6;
    ascii_table[SDL_SCANCODE_6].left_alt = -1;
    ascii_table[SDL_SCANCODE_6].right_alt = KEY_166;
    ascii_table[SDL_SCANCODE_6].ctrl = -1;

    ascii_table[SDL_SCANCODE_7].normal = KEY_232;
    ascii_table[SDL_SCANCODE_7].shift = KEY_7;
    ascii_table[SDL_SCANCODE_7].left_alt = -1;
    ascii_table[SDL_SCANCODE_7].right_alt = KEY_GRAVE;
    ascii_table[SDL_SCANCODE_7].ctrl = -1;

    ascii_table[SDL_SCANCODE_8].normal = KEY_UNDERSCORE;
    ascii_table[SDL_SCANCODE_8].shift = KEY_8;
    ascii_table[SDL_SCANCODE_8].left_alt = -1;
    ascii_table[SDL_SCANCODE_8].right_alt = KEY_BACKSLASH;
    ascii_table[SDL_SCANCODE_8].ctrl = -1;

    ascii_table[SDL_SCANCODE_9].normal = KEY_231;
    ascii_table[SDL_SCANCODE_9].shift = KEY_9;
    ascii_table[SDL_SCANCODE_9].left_alt = -1;
    ascii_table[SDL_SCANCODE_9].right_alt = KEY_136;
    ascii_table[SDL_SCANCODE_9].ctrl = -1;

    ascii_table[SDL_SCANCODE_0].normal = KEY_224;
    ascii_table[SDL_SCANCODE_0].shift = KEY_0;
    ascii_table[SDL_SCANCODE_0].left_alt = -1;
    ascii_table[SDL_SCANCODE_0].right_alt = KEY_AT;
    ascii_table[SDL_SCANCODE_0].ctrl = -1;

    ascii_table[SDL_SCANCODE_MINUS].normal = KEY_PAREN_RIGHT;
    ascii_table[SDL_SCANCODE_MINUS].shift = KEY_176;
    ascii_table[SDL_SCANCODE_MINUS].left_alt = -1;
    ascii_table[SDL_SCANCODE_MINUS].right_alt = KEY_BRACKET_RIGHT;
    ascii_table[SDL_SCANCODE_MINUS].ctrl = -1;

    switch (kb_layout) {
    case KEYBOARD_LAYOUT_QWERTY:
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_EQUALS;
        break;
    default:
        k = SDL_SCANCODE_0;
        break;
    }

    ascii_table[k].normal = KEY_EQUAL;
    ascii_table[k].shift = KEY_PLUS;
    ascii_table[k].left_alt = -1;
    ascii_table[k].right_alt = KEY_BRACE_RIGHT;
    ascii_table[k].ctrl = -1;

    ascii_table[SDL_SCANCODE_LEFTBRACKET].normal = KEY_136;
    ascii_table[SDL_SCANCODE_LEFTBRACKET].shift = KEY_168;
    ascii_table[SDL_SCANCODE_LEFTBRACKET].left_alt = -1;
    ascii_table[SDL_SCANCODE_LEFTBRACKET].right_alt = -1;
    ascii_table[SDL_SCANCODE_LEFTBRACKET].ctrl = -1;

    ascii_table[SDL_SCANCODE_RIGHTBRACKET].normal = KEY_DOLLAR;
    ascii_table[SDL_SCANCODE_RIGHTBRACKET].shift = KEY_163;
    ascii_table[SDL_SCANCODE_RIGHTBRACKET].left_alt = -1;
    ascii_table[SDL_SCANCODE_RIGHTBRACKET].right_alt = KEY_164;
    ascii_table[SDL_SCANCODE_RIGHTBRACKET].ctrl = -1;

    ascii_table[SDL_SCANCODE_APOSTROPHE].normal = KEY_249;
    ascii_table[SDL_SCANCODE_APOSTROPHE].shift = KEY_PERCENT;
    ascii_table[SDL_SCANCODE_APOSTROPHE].left_alt = -1;
    ascii_table[SDL_SCANCODE_APOSTROPHE].right_alt = -1;
    ascii_table[SDL_SCANCODE_APOSTROPHE].ctrl = -1;

    ascii_table[SDL_SCANCODE_BACKSLASH].normal = KEY_ASTERISK;
    ascii_table[SDL_SCANCODE_BACKSLASH].shift = KEY_181;
    ascii_table[SDL_SCANCODE_BACKSLASH].left_alt = -1;
    ascii_table[SDL_SCANCODE_BACKSLASH].right_alt = -1;
    ascii_table[SDL_SCANCODE_BACKSLASH].ctrl = -1;

    // ascii_table[DIK_OEM_102].normal = KEY_LESS;
    // ascii_table[DIK_OEM_102].shift = KEY_GREATER;
    // ascii_table[DIK_OEM_102].left_alt = -1;
    // ascii_table[DIK_OEM_102].right_alt = -1;
    // ascii_table[DIK_OEM_102].ctrl = -1;

    switch (kb_layout) {
    case KEYBOARD_LAYOUT_QWERTY:
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_M;
        break;
    default:
        k = SDL_SCANCODE_COMMA;
        break;
    }

    ascii_table[k].normal = KEY_COMMA;
    ascii_table[k].shift = KEY_QUESTION;
    ascii_table[k].left_alt = -1;
    ascii_table[k].right_alt = -1;
    ascii_table[k].ctrl = -1;

    switch (kb_layout) {
    case KEYBOARD_LAYOUT_QWERTY:
        k = SDL_SCANCODE_SEMICOLON;
        break;
    default:
        k = SDL_SCANCODE_COMMA;
        break;
    }

    ascii_table[k].normal = KEY_SEMICOLON;
    ascii_table[k].shift = KEY_DOT;
    ascii_table[k].left_alt = -1;
    ascii_table[k].right_alt = -1;
    ascii_table[k].ctrl = -1;

    switch (kb_layout) {
    case KEYBOARD_LAYOUT_QWERTY:
        // FIXME: Probably error, maps semicolon to colon on QWERTY keyboards.
        // Semicolon is already mapped above, so I bet it should be SDL_SCANCODE_COLON.
        k = SDL_SCANCODE_SEMICOLON;
        break;
    default:
        k = SDL_SCANCODE_PERIOD;
        break;
    }

    ascii_table[k].normal = KEY_COLON;
    ascii_table[k].shift = KEY_SLASH;
    ascii_table[k].left_alt = -1;
    ascii_table[k].right_alt = -1;
    ascii_table[k].ctrl = -1;

    ascii_table[SDL_SCANCODE_SLASH].normal = KEY_EXCLAMATION;
    ascii_table[SDL_SCANCODE_SLASH].shift = KEY_167;
    ascii_table[SDL_SCANCODE_SLASH].left_alt = -1;
    ascii_table[SDL_SCANCODE_SLASH].right_alt = -1;
    ascii_table[SDL_SCANCODE_SLASH].ctrl = -1;
}

// 0x4BB42C
static void kb_map_ascii_German()
{
    int k;

    kb_map_ascii_English_US();

    ascii_table[SDL_SCANCODE_GRAVE].normal = KEY_136;
    ascii_table[SDL_SCANCODE_GRAVE].shift = KEY_186;
    ascii_table[SDL_SCANCODE_GRAVE].left_alt = -1;
    ascii_table[SDL_SCANCODE_GRAVE].right_alt = -1;
    ascii_table[SDL_SCANCODE_GRAVE].ctrl = -1;

    ascii_table[SDL_SCANCODE_2].normal = KEY_2;
    ascii_table[SDL_SCANCODE_2].shift = KEY_QUOTE;
    ascii_table[SDL_SCANCODE_2].left_alt = -1;
    ascii_table[SDL_SCANCODE_2].right_alt = KEY_178;
    ascii_table[SDL_SCANCODE_2].ctrl = -1;

    ascii_table[SDL_SCANCODE_3].normal = KEY_3;
    ascii_table[SDL_SCANCODE_3].shift = KEY_167;
    ascii_table[SDL_SCANCODE_3].left_alt = -1;
    ascii_table[SDL_SCANCODE_3].right_alt = KEY_179;
    ascii_table[SDL_SCANCODE_3].ctrl = -1;

    ascii_table[SDL_SCANCODE_6].normal = KEY_6;
    ascii_table[SDL_SCANCODE_6].shift = KEY_AMPERSAND;
    ascii_table[SDL_SCANCODE_6].left_alt = -1;
    ascii_table[SDL_SCANCODE_6].right_alt = -1;
    ascii_table[SDL_SCANCODE_6].ctrl = -1;

    ascii_table[SDL_SCANCODE_7].normal = KEY_7;
    ascii_table[SDL_SCANCODE_7].shift = KEY_166;
    ascii_table[SDL_SCANCODE_7].left_alt = -1;
    ascii_table[SDL_SCANCODE_7].right_alt = KEY_BRACE_LEFT;
    ascii_table[SDL_SCANCODE_7].ctrl = -1;

    ascii_table[SDL_SCANCODE_8].normal = KEY_8;
    ascii_table[SDL_SCANCODE_8].shift = KEY_PAREN_LEFT;
    ascii_table[SDL_SCANCODE_8].left_alt = -1;
    ascii_table[SDL_SCANCODE_8].right_alt = KEY_BRACKET_LEFT;
    ascii_table[SDL_SCANCODE_8].ctrl = -1;

    ascii_table[SDL_SCANCODE_9].normal = KEY_9;
    ascii_table[SDL_SCANCODE_9].shift = KEY_PAREN_RIGHT;
    ascii_table[SDL_SCANCODE_9].left_alt = -1;
    ascii_table[SDL_SCANCODE_9].right_alt = KEY_BRACKET_RIGHT;
    ascii_table[SDL_SCANCODE_9].ctrl = -1;

    ascii_table[SDL_SCANCODE_0].normal = KEY_0;
    ascii_table[SDL_SCANCODE_0].shift = KEY_EQUAL;
    ascii_table[SDL_SCANCODE_0].left_alt = -1;
    ascii_table[SDL_SCANCODE_0].right_alt = KEY_BRACE_RIGHT;
    ascii_table[SDL_SCANCODE_0].ctrl = -1;

    ascii_table[SDL_SCANCODE_MINUS].normal = KEY_223;
    ascii_table[SDL_SCANCODE_MINUS].shift = KEY_QUESTION;
    ascii_table[SDL_SCANCODE_MINUS].left_alt = -1;
    ascii_table[SDL_SCANCODE_MINUS].right_alt = KEY_BACKSLASH;
    ascii_table[SDL_SCANCODE_MINUS].ctrl = -1;

    ascii_table[SDL_SCANCODE_EQUALS].normal = KEY_180;
    ascii_table[SDL_SCANCODE_EQUALS].shift = KEY_GRAVE;
    ascii_table[SDL_SCANCODE_EQUALS].left_alt = -1;
    ascii_table[SDL_SCANCODE_EQUALS].right_alt = -1;
    ascii_table[SDL_SCANCODE_EQUALS].ctrl = -1;

    switch (kb_layout) {
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_A;
        break;
    default:
        k = SDL_SCANCODE_Q;
        break;
    }

    ascii_table[k].normal = KEY_LOWERCASE_Q;
    ascii_table[k].shift = KEY_UPPERCASE_Q;
    ascii_table[k].left_alt = KEY_ALT_Q;
    ascii_table[k].right_alt = KEY_AT;
    ascii_table[k].ctrl = KEY_CTRL_Q;

    ascii_table[SDL_SCANCODE_LEFTBRACKET].normal = KEY_252;
    ascii_table[SDL_SCANCODE_LEFTBRACKET].shift = KEY_220;
    ascii_table[SDL_SCANCODE_LEFTBRACKET].left_alt = -1;
    ascii_table[SDL_SCANCODE_LEFTBRACKET].right_alt = -1;
    ascii_table[SDL_SCANCODE_LEFTBRACKET].ctrl = -1;

    switch (kb_layout) {
    case KEYBOARD_LAYOUT_QWERTY:
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_EQUALS;
        break;
    default:
        k = SDL_SCANCODE_RIGHTBRACKET;
        break;
    }

    ascii_table[k].normal = KEY_PLUS;
    ascii_table[k].shift = KEY_ASTERISK;
    ascii_table[k].left_alt = -1;
    ascii_table[k].right_alt = KEY_152;
    ascii_table[k].ctrl = -1;

    ascii_table[SDL_SCANCODE_SEMICOLON].normal = KEY_246;
    ascii_table[SDL_SCANCODE_SEMICOLON].shift = KEY_214;
    ascii_table[SDL_SCANCODE_SEMICOLON].left_alt = -1;
    ascii_table[SDL_SCANCODE_SEMICOLON].right_alt = -1;
    ascii_table[SDL_SCANCODE_SEMICOLON].ctrl = -1;

    ascii_table[SDL_SCANCODE_APOSTROPHE].normal = KEY_228;
    ascii_table[SDL_SCANCODE_APOSTROPHE].shift = KEY_196;
    ascii_table[SDL_SCANCODE_APOSTROPHE].left_alt = -1;
    ascii_table[SDL_SCANCODE_APOSTROPHE].right_alt = -1;
    ascii_table[SDL_SCANCODE_APOSTROPHE].ctrl = -1;

    ascii_table[SDL_SCANCODE_BACKSLASH].normal = KEY_NUMBER_SIGN;
    ascii_table[SDL_SCANCODE_BACKSLASH].shift = KEY_SINGLE_QUOTE;
    ascii_table[SDL_SCANCODE_BACKSLASH].left_alt = -1;
    ascii_table[SDL_SCANCODE_BACKSLASH].right_alt = -1;
    ascii_table[SDL_SCANCODE_BACKSLASH].ctrl = -1;

    // ascii_table[DIK_OEM_102].normal = KEY_LESS;
    // ascii_table[DIK_OEM_102].shift = KEY_GREATER;
    // ascii_table[DIK_OEM_102].left_alt = -1;
    // ascii_table[DIK_OEM_102].right_alt = KEY_166;
    // ascii_table[DIK_OEM_102].ctrl = -1;

    switch (kb_layout) {
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_SEMICOLON;
        break;
    default:
        k = SDL_SCANCODE_M;
        break;
    }

    ascii_table[k].normal = KEY_LOWERCASE_M;
    ascii_table[k].shift = KEY_UPPERCASE_M;
    ascii_table[k].left_alt = KEY_ALT_M;
    ascii_table[k].right_alt = KEY_181;
    ascii_table[k].ctrl = KEY_CTRL_M;

    switch (kb_layout) {
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_M;
        break;
    default:
        k = SDL_SCANCODE_COMMA;
        break;
    }

    ascii_table[k].normal = KEY_COMMA;
    ascii_table[k].shift = KEY_SEMICOLON;
    ascii_table[k].left_alt = -1;
    ascii_table[k].right_alt = -1;
    ascii_table[k].ctrl = -1;

    switch (kb_layout) {
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_COMMA;
        break;
    default:
        k = SDL_SCANCODE_PERIOD;
        break;
    }

    ascii_table[k].normal = KEY_DOT;
    ascii_table[k].shift = KEY_COLON;
    ascii_table[k].left_alt = -1;
    ascii_table[k].right_alt = -1;
    ascii_table[k].ctrl = -1;

    switch (kb_layout) {
    case KEYBOARD_LAYOUT_QWERTY:
        k = SDL_SCANCODE_MINUS;
        break;
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_6;
        break;
    default:
        k = SDL_SCANCODE_SLASH;
        break;
    }

    ascii_table[k].normal = KEY_150;
    ascii_table[k].shift = KEY_151;
    ascii_table[k].left_alt = -1;
    ascii_table[k].right_alt = -1;
    ascii_table[k].ctrl = -1;

    ascii_table[SDL_SCANCODE_KP_DIVIDE].normal = KEY_247;
    ascii_table[SDL_SCANCODE_KP_DIVIDE].shift = KEY_247;
    ascii_table[SDL_SCANCODE_KP_DIVIDE].left_alt = -1;
    ascii_table[SDL_SCANCODE_KP_DIVIDE].right_alt = -1;
    ascii_table[SDL_SCANCODE_KP_DIVIDE].ctrl = -1;

    ascii_table[SDL_SCANCODE_KP_MULTIPLY].normal = KEY_215;
    ascii_table[SDL_SCANCODE_KP_MULTIPLY].shift = KEY_215;
    ascii_table[SDL_SCANCODE_KP_MULTIPLY].left_alt = -1;
    ascii_table[SDL_SCANCODE_KP_MULTIPLY].right_alt = -1;
    ascii_table[SDL_SCANCODE_KP_MULTIPLY].ctrl = -1;

    ascii_table[SDL_SCANCODE_KP_DECIMAL].normal = KEY_DELETE;
    ascii_table[SDL_SCANCODE_KP_DECIMAL].shift = KEY_COMMA;
    ascii_table[SDL_SCANCODE_KP_DECIMAL].left_alt = -1;
    ascii_table[SDL_SCANCODE_KP_DECIMAL].right_alt = KEY_ALT_DELETE;
    ascii_table[SDL_SCANCODE_KP_DECIMAL].ctrl = KEY_CTRL_DELETE;
}

// 0x4BBF30
static void kb_map_ascii_Italian()
{
    int k;

    kb_map_ascii_English_US();

    ascii_table[SDL_SCANCODE_GRAVE].normal = KEY_BACKSLASH;
    ascii_table[SDL_SCANCODE_GRAVE].shift = KEY_BAR;
    ascii_table[SDL_SCANCODE_GRAVE].left_alt = -1;
    ascii_table[SDL_SCANCODE_GRAVE].right_alt = -1;
    ascii_table[SDL_SCANCODE_GRAVE].ctrl = -1;

    // ascii_table[DIK_OEM_102].normal = KEY_LESS;
    // ascii_table[DIK_OEM_102].shift = KEY_GREATER;
    // ascii_table[DIK_OEM_102].left_alt = -1;
    // ascii_table[DIK_OEM_102].right_alt = -1;
    // ascii_table[DIK_OEM_102].ctrl = -1;

    ascii_table[SDL_SCANCODE_1].normal = KEY_1;
    ascii_table[SDL_SCANCODE_1].shift = KEY_EXCLAMATION;
    ascii_table[SDL_SCANCODE_1].left_alt = -1;
    ascii_table[SDL_SCANCODE_1].right_alt = -1;
    ascii_table[SDL_SCANCODE_1].ctrl = -1;

    ascii_table[SDL_SCANCODE_2].normal = KEY_2;
    ascii_table[SDL_SCANCODE_2].shift = KEY_QUOTE;
    ascii_table[SDL_SCANCODE_2].left_alt = -1;
    ascii_table[SDL_SCANCODE_2].right_alt = -1;
    ascii_table[SDL_SCANCODE_2].ctrl = -1;

    ascii_table[SDL_SCANCODE_3].normal = KEY_3;
    ascii_table[SDL_SCANCODE_3].shift = KEY_163;
    ascii_table[SDL_SCANCODE_3].left_alt = -1;
    ascii_table[SDL_SCANCODE_3].right_alt = -1;
    ascii_table[SDL_SCANCODE_3].ctrl = -1;

    ascii_table[SDL_SCANCODE_6].normal = KEY_6;
    ascii_table[SDL_SCANCODE_6].shift = KEY_AMPERSAND;
    ascii_table[SDL_SCANCODE_6].left_alt = -1;
    ascii_table[SDL_SCANCODE_6].right_alt = -1;
    ascii_table[SDL_SCANCODE_6].ctrl = -1;

    ascii_table[SDL_SCANCODE_7].normal = KEY_7;
    ascii_table[SDL_SCANCODE_7].shift = KEY_SLASH;
    ascii_table[SDL_SCANCODE_7].left_alt = -1;
    ascii_table[SDL_SCANCODE_7].right_alt = -1;
    ascii_table[SDL_SCANCODE_7].ctrl = -1;

    ascii_table[SDL_SCANCODE_8].normal = KEY_8;
    ascii_table[SDL_SCANCODE_8].shift = KEY_PAREN_LEFT;
    ascii_table[SDL_SCANCODE_8].left_alt = -1;
    ascii_table[SDL_SCANCODE_8].right_alt = -1;
    ascii_table[SDL_SCANCODE_8].ctrl = -1;

    ascii_table[SDL_SCANCODE_9].normal = KEY_9;
    ascii_table[SDL_SCANCODE_9].shift = KEY_PAREN_RIGHT;
    ascii_table[SDL_SCANCODE_9].left_alt = -1;
    ascii_table[SDL_SCANCODE_9].right_alt = -1;
    ascii_table[SDL_SCANCODE_9].ctrl = -1;

    ascii_table[SDL_SCANCODE_0].normal = KEY_0;
    ascii_table[SDL_SCANCODE_0].shift = KEY_EQUAL;
    ascii_table[SDL_SCANCODE_0].left_alt = -1;
    ascii_table[SDL_SCANCODE_0].right_alt = -1;
    ascii_table[SDL_SCANCODE_0].ctrl = -1;

    ascii_table[SDL_SCANCODE_MINUS].normal = KEY_SINGLE_QUOTE;
    ascii_table[SDL_SCANCODE_MINUS].shift = KEY_QUESTION;
    ascii_table[SDL_SCANCODE_MINUS].left_alt = -1;
    ascii_table[SDL_SCANCODE_MINUS].right_alt = -1;
    ascii_table[SDL_SCANCODE_MINUS].ctrl = -1;

    ascii_table[SDL_SCANCODE_LEFTBRACKET].normal = KEY_232;
    ascii_table[SDL_SCANCODE_LEFTBRACKET].shift = KEY_233;
    ascii_table[SDL_SCANCODE_LEFTBRACKET].left_alt = -1;
    ascii_table[SDL_SCANCODE_LEFTBRACKET].right_alt = KEY_BRACKET_LEFT;
    ascii_table[SDL_SCANCODE_LEFTBRACKET].ctrl = -1;

    ascii_table[SDL_SCANCODE_RIGHTBRACKET].normal = KEY_PLUS;
    ascii_table[SDL_SCANCODE_RIGHTBRACKET].shift = KEY_ASTERISK;
    ascii_table[SDL_SCANCODE_RIGHTBRACKET].left_alt = -1;
    ascii_table[SDL_SCANCODE_RIGHTBRACKET].right_alt = KEY_BRACKET_RIGHT;
    ascii_table[SDL_SCANCODE_RIGHTBRACKET].ctrl = -1;

    ascii_table[SDL_SCANCODE_BACKSLASH].normal = KEY_249;
    ascii_table[SDL_SCANCODE_BACKSLASH].shift = KEY_167;
    ascii_table[SDL_SCANCODE_BACKSLASH].left_alt = -1;
    ascii_table[SDL_SCANCODE_BACKSLASH].right_alt = KEY_BRACKET_RIGHT;
    ascii_table[SDL_SCANCODE_BACKSLASH].ctrl = -1;

    switch (kb_layout) {
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_M;
        break;
    default:
        k = SDL_SCANCODE_COMMA;
        break;
    }

    ascii_table[k].normal = KEY_COMMA;
    ascii_table[k].shift = KEY_SEMICOLON;
    ascii_table[k].left_alt = -1;
    ascii_table[k].right_alt = -1;
    ascii_table[k].ctrl = -1;

    switch (kb_layout) {
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_COMMA;
        break;
    default:
        k = SDL_SCANCODE_PERIOD;
        break;
    }

    ascii_table[k].normal = KEY_DOT;
    ascii_table[k].shift = KEY_COLON;
    ascii_table[k].left_alt = -1;
    ascii_table[k].right_alt = -1;
    ascii_table[k].ctrl = -1;

    switch (kb_layout) {
    case KEYBOARD_LAYOUT_QWERTY:
        k = SDL_SCANCODE_MINUS;
        break;
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_6;
        break;
    default:
        k = SDL_SCANCODE_SLASH;
        break;
    }

    ascii_table[k].normal = KEY_MINUS;
    ascii_table[k].shift = KEY_UNDERSCORE;
    ascii_table[k].left_alt = -1;
    ascii_table[k].right_alt = -1;
    ascii_table[k].ctrl = -1;
}

// 0x4BC5FC
static void kb_map_ascii_Spanish()
{
    int k;

    kb_map_ascii_English_US();

    ascii_table[SDL_SCANCODE_1].normal = KEY_1;
    ascii_table[SDL_SCANCODE_1].shift = KEY_EXCLAMATION;
    ascii_table[SDL_SCANCODE_1].left_alt = -1;
    ascii_table[SDL_SCANCODE_1].right_alt = KEY_BAR;
    ascii_table[SDL_SCANCODE_1].ctrl = -1;

    ascii_table[SDL_SCANCODE_2].normal = KEY_2;
    ascii_table[SDL_SCANCODE_2].shift = KEY_QUOTE;
    ascii_table[SDL_SCANCODE_2].left_alt = -1;
    ascii_table[SDL_SCANCODE_2].right_alt = KEY_AT;
    ascii_table[SDL_SCANCODE_2].ctrl = -1;

    ascii_table[SDL_SCANCODE_3].normal = KEY_3;
    ascii_table[SDL_SCANCODE_3].shift = KEY_149;
    ascii_table[SDL_SCANCODE_3].left_alt = -1;
    ascii_table[SDL_SCANCODE_3].right_alt = KEY_NUMBER_SIGN;
    ascii_table[SDL_SCANCODE_3].ctrl = -1;

    ascii_table[SDL_SCANCODE_6].normal = KEY_6;
    ascii_table[SDL_SCANCODE_6].shift = KEY_AMPERSAND;
    ascii_table[SDL_SCANCODE_6].left_alt = -1;
    ascii_table[SDL_SCANCODE_6].right_alt = KEY_172;
    ascii_table[SDL_SCANCODE_6].ctrl = -1;

    ascii_table[SDL_SCANCODE_7].normal = KEY_7;
    ascii_table[SDL_SCANCODE_7].shift = KEY_SLASH;
    ascii_table[SDL_SCANCODE_7].left_alt = -1;
    ascii_table[SDL_SCANCODE_7].right_alt = -1;
    ascii_table[SDL_SCANCODE_7].ctrl = -1;

    ascii_table[SDL_SCANCODE_8].normal = KEY_8;
    ascii_table[SDL_SCANCODE_8].shift = KEY_PAREN_LEFT;
    ascii_table[SDL_SCANCODE_8].left_alt = -1;
    ascii_table[SDL_SCANCODE_8].right_alt = -1;
    ascii_table[SDL_SCANCODE_8].ctrl = -1;

    ascii_table[SDL_SCANCODE_9].normal = KEY_9;
    ascii_table[SDL_SCANCODE_9].shift = KEY_PAREN_RIGHT;
    ascii_table[SDL_SCANCODE_9].left_alt = -1;
    ascii_table[SDL_SCANCODE_9].right_alt = -1;
    ascii_table[SDL_SCANCODE_9].ctrl = -1;

    ascii_table[SDL_SCANCODE_0].normal = KEY_0;
    ascii_table[SDL_SCANCODE_0].shift = KEY_EQUAL;
    ascii_table[SDL_SCANCODE_0].left_alt = -1;
    ascii_table[SDL_SCANCODE_0].right_alt = -1;
    ascii_table[SDL_SCANCODE_0].ctrl = -1;

    ascii_table[SDL_SCANCODE_MINUS].normal = KEY_146;
    ascii_table[SDL_SCANCODE_MINUS].shift = KEY_QUESTION;
    ascii_table[SDL_SCANCODE_MINUS].left_alt = -1;
    ascii_table[SDL_SCANCODE_MINUS].right_alt = -1;
    ascii_table[SDL_SCANCODE_MINUS].ctrl = -1;

    ascii_table[SDL_SCANCODE_EQUALS].normal = KEY_161;
    ascii_table[SDL_SCANCODE_EQUALS].shift = KEY_191;
    ascii_table[SDL_SCANCODE_EQUALS].left_alt = -1;
    ascii_table[SDL_SCANCODE_EQUALS].right_alt = -1;
    ascii_table[SDL_SCANCODE_EQUALS].ctrl = -1;

    ascii_table[SDL_SCANCODE_GRAVE].normal = KEY_176;
    ascii_table[SDL_SCANCODE_GRAVE].shift = KEY_170;
    ascii_table[SDL_SCANCODE_GRAVE].left_alt = -1;
    ascii_table[SDL_SCANCODE_GRAVE].right_alt = KEY_BACKSLASH;
    ascii_table[SDL_SCANCODE_GRAVE].ctrl = -1;

    ascii_table[SDL_SCANCODE_LEFTBRACKET].normal = KEY_GRAVE;
    ascii_table[SDL_SCANCODE_LEFTBRACKET].shift = KEY_CARET;
    ascii_table[SDL_SCANCODE_LEFTBRACKET].left_alt = -1;
    ascii_table[SDL_SCANCODE_LEFTBRACKET].right_alt = KEY_BRACKET_LEFT;
    ascii_table[SDL_SCANCODE_LEFTBRACKET].ctrl = -1;

    ascii_table[SDL_SCANCODE_RIGHTBRACKET].normal = KEY_PLUS;
    ascii_table[SDL_SCANCODE_RIGHTBRACKET].shift = KEY_ASTERISK;
    ascii_table[SDL_SCANCODE_RIGHTBRACKET].left_alt = -1;
    ascii_table[SDL_SCANCODE_RIGHTBRACKET].right_alt = KEY_BRACKET_RIGHT;
    ascii_table[SDL_SCANCODE_RIGHTBRACKET].ctrl = -1;

    // ascii_table[DIK_OEM_102].normal = KEY_LESS;
    // ascii_table[DIK_OEM_102].shift = KEY_GREATER;
    // ascii_table[DIK_OEM_102].left_alt = -1;
    // ascii_table[DIK_OEM_102].right_alt = -1;
    // ascii_table[DIK_OEM_102].ctrl = -1;

    ascii_table[SDL_SCANCODE_SEMICOLON].normal = KEY_241;
    ascii_table[SDL_SCANCODE_SEMICOLON].shift = KEY_209;
    ascii_table[SDL_SCANCODE_SEMICOLON].left_alt = -1;
    ascii_table[SDL_SCANCODE_SEMICOLON].right_alt = -1;
    ascii_table[SDL_SCANCODE_SEMICOLON].ctrl = -1;

    ascii_table[SDL_SCANCODE_APOSTROPHE].normal = KEY_168;
    ascii_table[SDL_SCANCODE_APOSTROPHE].shift = KEY_180;
    ascii_table[SDL_SCANCODE_APOSTROPHE].left_alt = -1;
    ascii_table[SDL_SCANCODE_APOSTROPHE].right_alt = KEY_BRACE_LEFT;
    ascii_table[SDL_SCANCODE_APOSTROPHE].ctrl = -1;

    ascii_table[SDL_SCANCODE_BACKSLASH].normal = KEY_231;
    ascii_table[SDL_SCANCODE_BACKSLASH].shift = KEY_199;
    ascii_table[SDL_SCANCODE_BACKSLASH].left_alt = -1;
    ascii_table[SDL_SCANCODE_BACKSLASH].right_alt = KEY_BRACE_RIGHT;
    ascii_table[SDL_SCANCODE_BACKSLASH].ctrl = -1;

    switch (kb_layout) {
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_M;
        break;
    default:
        k = SDL_SCANCODE_COMMA;
        break;
    }

    ascii_table[k].normal = KEY_COMMA;
    ascii_table[k].shift = KEY_SEMICOLON;
    ascii_table[k].left_alt = -1;
    ascii_table[k].right_alt = -1;
    ascii_table[k].ctrl = -1;

    switch (kb_layout) {
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_COMMA;
        break;
    default:
        k = SDL_SCANCODE_PERIOD;
        break;
    }

    ascii_table[k].normal = KEY_DOT;
    ascii_table[k].shift = KEY_COLON;
    ascii_table[k].left_alt = -1;
    ascii_table[k].right_alt = -1;
    ascii_table[k].ctrl = -1;

    switch (kb_layout) {
    case KEYBOARD_LAYOUT_QWERTY:
        k = SDL_SCANCODE_MINUS;
        break;
    case KEYBOARD_LAYOUT_FRENCH:
        k = SDL_SCANCODE_6;
        break;
    default:
        k = SDL_SCANCODE_SLASH;
        break;
    }

    ascii_table[k].normal = KEY_MINUS;
    ascii_table[k].shift = KEY_UNDERSCORE;
    ascii_table[k].left_alt = -1;
    ascii_table[k].right_alt = -1;
    ascii_table[k].ctrl = -1;
}

// 0x4BCCD0
static void kb_init_lock_status()
{
    if ((SDL_GetModState() & KMOD_CAPS) != 0) {
        kb_lock_flags |= MODIFIER_KEY_STATE_CAPS_LOCK;
    }

    if ((SDL_GetModState() & KMOD_NUM) != 0) {
        kb_lock_flags |= MODIFIER_KEY_STATE_NUM_LOCK;
    }

#if SDL_VERSION_ATLEAST(2, 0, 18)
    if ((SDL_GetModState() & KMOD_SCROLL) != 0) {
        kb_lock_flags |= MODIFIER_KEY_STATE_SCROLL_LOCK;
    }
#endif
}

// 0x4BCD18
static void kb_toggle_caps()
{
    if ((kb_lock_flags & MODIFIER_KEY_STATE_CAPS_LOCK) != 0) {
        kb_lock_flags &= ~MODIFIER_KEY_STATE_CAPS_LOCK;
    } else {
        kb_lock_flags |= MODIFIER_KEY_STATE_CAPS_LOCK;
    }
}

// 0x4BCD34
static void kb_toggle_num()
{
    if ((kb_lock_flags & MODIFIER_KEY_STATE_NUM_LOCK) != 0) {
        kb_lock_flags &= ~MODIFIER_KEY_STATE_NUM_LOCK;
    } else {
        kb_lock_flags |= MODIFIER_KEY_STATE_NUM_LOCK;
    }
}

// 0x4BCD50
static void kb_toggle_scroll()
{
    if ((kb_lock_flags & MODIFIER_KEY_STATE_SCROLL_LOCK) != 0) {
        kb_lock_flags &= ~MODIFIER_KEY_STATE_SCROLL_LOCK;
    } else {
        kb_lock_flags |= MODIFIER_KEY_STATE_SCROLL_LOCK;
    }
}

// 0x4BCD6C
static int kb_buffer_put(key_data_t* key_data)
{
    int rc = -1;

    if (((kb_put + 1) & (KEY_QUEUE_SIZE - 1)) != kb_get) {
        kb_buffer[kb_put] = *key_data;

        kb_put++;
        kb_put &= KEY_QUEUE_SIZE - 1;

        rc = 0;
    }

    return rc;
}

// 0x4BCDAC
static int kb_buffer_get(key_data_t* key_data)
{
    int rc = -1;

    if (kb_get != kb_put) {
        if (key_data != NULL) {
            *key_data = kb_buffer[kb_get];
        }

        kb_get++;
        kb_get &= KEY_QUEUE_SIZE - 1;

        rc = 0;
    }

    return rc;
}

// Get pointer to pending key event from the queue but do not consume it.
//
// 0x4BCDEC
static int kb_buffer_peek(int index, key_data_t** keyboardEventPtr)
{
    int rc = -1;

    if (kb_get != kb_put) {
        int end;
        if (kb_put <= kb_get) {
            end = kb_put + KEY_QUEUE_SIZE - kb_get - 1;
        } else {
            end = kb_put - kb_get - 1;
        }

        if (index <= end) {
            int eventIndex = (kb_get + index) & (KEY_QUEUE_SIZE - 1);
            *keyboardEventPtr = &(kb_buffer[eventIndex]);
            rc = 0;
        }
    }

    return rc;
}

} // namespace fallout
