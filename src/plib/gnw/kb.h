#ifndef FALLOUT_PLIB_GNW_KB_H_
#define FALLOUT_PLIB_GNW_KB_H_

#include <SDL.h>

#include "plib/gnw/dxinput.h"

namespace fallout {

#define KEY_STATE_UP 0
#define KEY_STATE_DOWN 1
#define KEY_STATE_REPEAT 2

#define MODIFIER_KEY_STATE_NUM_LOCK 0x01
#define MODIFIER_KEY_STATE_CAPS_LOCK 0x02
#define MODIFIER_KEY_STATE_SCROLL_LOCK 0x04

#define KEYBOARD_EVENT_MODIFIER_CAPS_LOCK 0x0001
#define KEYBOARD_EVENT_MODIFIER_NUM_LOCK 0x0002
#define KEYBOARD_EVENT_MODIFIER_SCROLL_LOCK 0x0004
#define KEYBOARD_EVENT_MODIFIER_LEFT_SHIFT 0x0008
#define KEYBOARD_EVENT_MODIFIER_RIGHT_SHIFT 0x0010
#define KEYBOARD_EVENT_MODIFIER_LEFT_ALT 0x0020
#define KEYBOARD_EVENT_MODIFIER_RIGHT_ALT 0x0040
#define KEYBOARD_EVENT_MODIFIER_LEFT_CONTROL 0x0080
#define KEYBOARD_EVENT_MODIFIER_RIGHT_CONTROL 0x0100
#define KEYBOARD_EVENT_MODIFIER_ANY_SHIFT (KEYBOARD_EVENT_MODIFIER_LEFT_SHIFT | KEYBOARD_EVENT_MODIFIER_RIGHT_SHIFT)
#define KEYBOARD_EVENT_MODIFIER_ANY_ALT (KEYBOARD_EVENT_MODIFIER_LEFT_ALT | KEYBOARD_EVENT_MODIFIER_RIGHT_ALT)
#define KEYBOARD_EVENT_MODIFIER_ANY_CONTROL (KEYBOARD_EVENT_MODIFIER_LEFT_CONTROL | KEYBOARD_EVENT_MODIFIER_RIGHT_CONTROL)

#define KEY_QUEUE_SIZE 64

typedef enum KeyboardLayout {
    KEYBOARD_LAYOUT_QWERTY,
    KEYBOARD_LAYOUT_FRENCH,
    KEYBOARD_LAYOUT_GERMAN,
    KEYBOARD_LAYOUT_ITALIAN,
    KEYBOARD_LAYOUT_SPANISH,
} KeyboardLayout;

typedef enum Key {
    KEY_ESCAPE = '\x1b',
    KEY_TAB = '\x09',
    KEY_BACKSPACE = '\x08',
    KEY_RETURN = '\r',

    KEY_SPACE = ' ',
    KEY_EXCLAMATION = '!',
    KEY_QUOTE = '"',
    KEY_NUMBER_SIGN = '#',
    KEY_DOLLAR = '$',
    KEY_PERCENT = '%',
    KEY_AMPERSAND = '&',
    KEY_SINGLE_QUOTE = '\'',
    KEY_PAREN_LEFT = '(',
    KEY_PAREN_RIGHT = ')',
    KEY_ASTERISK = '*',
    KEY_PLUS = '+',
    KEY_COMMA = ',',
    KEY_MINUS = '-',
    KEY_DOT = '.',
    KEY_SLASH = '/',
    KEY_0 = '0',
    KEY_1 = '1',
    KEY_2 = '2',
    KEY_3 = '3',
    KEY_4 = '4',
    KEY_5 = '5',
    KEY_6 = '6',
    KEY_7 = '7',
    KEY_8 = '8',
    KEY_9 = '9',
    KEY_COLON = ':',
    KEY_SEMICOLON = ';',
    KEY_LESS = '<',
    KEY_EQUAL = '=',
    KEY_GREATER = '>',
    KEY_QUESTION = '?',
    KEY_AT = '@',
    KEY_UPPERCASE_A = 'A',
    KEY_UPPERCASE_B = 'B',
    KEY_UPPERCASE_C = 'C',
    KEY_UPPERCASE_D = 'D',
    KEY_UPPERCASE_E = 'E',
    KEY_UPPERCASE_F = 'F',
    KEY_UPPERCASE_G = 'G',
    KEY_UPPERCASE_H = 'H',
    KEY_UPPERCASE_I = 'I',
    KEY_UPPERCASE_J = 'J',
    KEY_UPPERCASE_K = 'K',
    KEY_UPPERCASE_L = 'L',
    KEY_UPPERCASE_M = 'M',
    KEY_UPPERCASE_N = 'N',
    KEY_UPPERCASE_O = 'O',
    KEY_UPPERCASE_P = 'P',
    KEY_UPPERCASE_Q = 'Q',
    KEY_UPPERCASE_R = 'R',
    KEY_UPPERCASE_S = 'S',
    KEY_UPPERCASE_T = 'T',
    KEY_UPPERCASE_U = 'U',
    KEY_UPPERCASE_V = 'V',
    KEY_UPPERCASE_W = 'W',
    KEY_UPPERCASE_X = 'X',
    KEY_UPPERCASE_Y = 'Y',
    KEY_UPPERCASE_Z = 'Z',

    KEY_BRACKET_LEFT = '[',
    KEY_BACKSLASH = '\\',
    KEY_BRACKET_RIGHT = ']',
    KEY_CARET = '^',
    KEY_UNDERSCORE = '_',

    KEY_GRAVE = '`',
    KEY_LOWERCASE_A = 'a',
    KEY_LOWERCASE_B = 'b',
    KEY_LOWERCASE_C = 'c',
    KEY_LOWERCASE_D = 'd',
    KEY_LOWERCASE_E = 'e',
    KEY_LOWERCASE_F = 'f',
    KEY_LOWERCASE_G = 'g',
    KEY_LOWERCASE_H = 'h',
    KEY_LOWERCASE_I = 'i',
    KEY_LOWERCASE_J = 'j',
    KEY_LOWERCASE_K = 'k',
    KEY_LOWERCASE_L = 'l',
    KEY_LOWERCASE_M = 'm',
    KEY_LOWERCASE_N = 'n',
    KEY_LOWERCASE_O = 'o',
    KEY_LOWERCASE_P = 'p',
    KEY_LOWERCASE_Q = 'q',
    KEY_LOWERCASE_R = 'r',
    KEY_LOWERCASE_S = 's',
    KEY_LOWERCASE_T = 't',
    KEY_LOWERCASE_U = 'u',
    KEY_LOWERCASE_V = 'v',
    KEY_LOWERCASE_W = 'w',
    KEY_LOWERCASE_X = 'x',
    KEY_LOWERCASE_Y = 'y',
    KEY_LOWERCASE_Z = 'z',
    KEY_BRACE_LEFT = '{',
    KEY_BAR = '|',
    KEY_BRACE_RIGHT = '}',
    KEY_TILDE = '~',
    KEY_DEL = 127,

    KEY_136 = 136,
    KEY_146 = 146,
    KEY_149 = 149,
    KEY_150 = 150,
    KEY_151 = 151,
    KEY_152 = 152,
    KEY_161 = 161,
    KEY_163 = 163,
    KEY_164 = 164,
    KEY_166 = 166,
    KEY_168 = 168,
    KEY_167 = 167,
    KEY_170 = 170,
    KEY_172 = 172,
    KEY_176 = 176,
    KEY_178 = 178,
    KEY_179 = 179,
    KEY_180 = 180,
    KEY_181 = 181,
    KEY_186 = 186,
    KEY_191 = 191,
    KEY_196 = 196,
    KEY_199 = 199,
    KEY_209 = 209,
    KEY_214 = 214,
    KEY_215 = 215,
    KEY_220 = 220,
    KEY_223 = 223,
    KEY_224 = 224,
    KEY_228 = 228,
    KEY_231 = 231,
    KEY_232 = 232,
    KEY_233 = 233,
    KEY_241 = 241,
    KEY_246 = 246,
    KEY_247 = 247,
    KEY_249 = 249,
    KEY_252 = 252,

    KEY_ALT_Q = 272,
    KEY_ALT_W = 273,
    KEY_ALT_E = 274,
    KEY_ALT_R = 275,
    KEY_ALT_T = 276,
    KEY_ALT_Y = 277,
    KEY_ALT_U = 278,
    KEY_ALT_I = 279,
    KEY_ALT_O = 280,
    KEY_ALT_P = 281,
    KEY_ALT_A = 286,
    KEY_ALT_S = 287,
    KEY_ALT_D = 288,
    KEY_ALT_F = 289,
    KEY_ALT_G = 290,
    KEY_ALT_H = 291,
    KEY_ALT_J = 292,
    KEY_ALT_K = 293,
    KEY_ALT_L = 294,
    KEY_ALT_Z = 300,
    KEY_ALT_X = 301,
    KEY_ALT_C = 302,
    KEY_ALT_V = 303,
    KEY_ALT_B = 304,
    KEY_ALT_N = 305,
    KEY_ALT_M = 306,

    KEY_CTRL_Q = 17,
    KEY_CTRL_W = 23,
    KEY_CTRL_E = 5,
    KEY_CTRL_R = 18,
    KEY_CTRL_T = 20,
    KEY_CTRL_Y = 25,
    KEY_CTRL_U = 21,
    KEY_CTRL_I = 9,
    KEY_CTRL_O = 15,
    KEY_CTRL_P = 16,
    KEY_CTRL_A = 1,
    KEY_CTRL_S = 19,
    KEY_CTRL_D = 4,
    KEY_CTRL_F = 6,
    KEY_CTRL_G = 7,
    KEY_CTRL_H = 8,
    KEY_CTRL_J = 10,
    KEY_CTRL_K = 11,
    KEY_CTRL_L = 12,
    KEY_CTRL_Z = 26,
    KEY_CTRL_X = 24,
    KEY_CTRL_C = 3,
    KEY_CTRL_V = 22,
    KEY_CTRL_B = 2,
    KEY_CTRL_N = 14,
    KEY_CTRL_M = 13,

    KEY_F1 = 315,
    KEY_F2 = 316,
    KEY_F3 = 317,
    KEY_F4 = 318,
    KEY_F5 = 319,
    KEY_F6 = 320,
    KEY_F7 = 321,
    KEY_F8 = 322,
    KEY_F9 = 323,
    KEY_F10 = 324,
    KEY_F11 = 389,
    KEY_F12 = 390,

    KEY_SHIFT_F1 = 340,
    KEY_SHIFT_F2 = 341,
    KEY_SHIFT_F3 = 342,
    KEY_SHIFT_F4 = 343,
    KEY_SHIFT_F5 = 344,
    KEY_SHIFT_F6 = 345,
    KEY_SHIFT_F7 = 346,
    KEY_SHIFT_F8 = 347,
    KEY_SHIFT_F9 = 348,
    KEY_SHIFT_F10 = 349,
    KEY_SHIFT_F11 = 391,
    KEY_SHIFT_F12 = 392,

    KEY_CTRL_F1 = 350,
    KEY_CTRL_F2 = 351,
    KEY_CTRL_F3 = 352,
    KEY_CTRL_F4 = 353,
    KEY_CTRL_F5 = 354,
    KEY_CTRL_F6 = 355,
    KEY_CTRL_F7 = 356,
    KEY_CTRL_F8 = 357,
    KEY_CTRL_F9 = 358,
    KEY_CTRL_F10 = 359,
    KEY_CTRL_F11 = 393,
    KEY_CTRL_F12 = 394,

    KEY_ALT_F1 = 360,
    KEY_ALT_F2 = 361,
    KEY_ALT_F3 = 362,
    KEY_ALT_F4 = 363,
    KEY_ALT_F5 = 364,
    KEY_ALT_F6 = 365,
    KEY_ALT_F7 = 366,
    KEY_ALT_F8 = 367,
    KEY_ALT_F9 = 368,
    KEY_ALT_F10 = 369,
    KEY_ALT_F11 = 395,
    KEY_ALT_F12 = 396,

    KEY_HOME = 327,
    KEY_CTRL_HOME = 375,
    KEY_ALT_HOME = 407,

    KEY_PAGE_UP = 329,
    KEY_CTRL_PAGE_UP = 388,
    KEY_ALT_PAGE_UP = 409,

    KEY_INSERT = 338,
    KEY_CTRL_INSERT = 402,
    KEY_ALT_INSERT = 418,

    KEY_DELETE = 339,
    KEY_CTRL_DELETE = 403,
    KEY_ALT_DELETE = 419,

    KEY_END = 335,
    KEY_CTRL_END = 373,
    KEY_ALT_END = 415,

    KEY_PAGE_DOWN = 337,
    KEY_ALT_PAGE_DOWN = 417,
    KEY_CTRL_PAGE_DOWN = 374,

    KEY_ARROW_UP = 328,
    KEY_CTRL_ARROW_UP = 397,
    KEY_ALT_ARROW_UP = 408,

    KEY_ARROW_DOWN = 336,
    KEY_CTRL_ARROW_DOWN = 401,
    KEY_ALT_ARROW_DOWN = 416,

    KEY_ARROW_LEFT = 331,
    KEY_CTRL_ARROW_LEFT = 371,
    KEY_ALT_ARROW_LEFT = 411,

    KEY_ARROW_RIGHT = 333,
    KEY_CTRL_ARROW_RIGHT = 372,
    KEY_ALT_ARROW_RIGHT = 413,

    KEY_CTRL_BACKSLASH = 192,

    KEY_NUMBERPAD_5 = 332,
    KEY_CTRL_NUMBERPAD_5 = 399,
    KEY_ALT_NUMBERPAD_5 = 9999,

    KEY_FIRST_INPUT_CHARACTER = KEY_SPACE,
    KEY_LAST_INPUT_CHARACTER = KEY_LOWERCASE_Z,
} Key;

extern unsigned char keys[SDL_NUM_SCANCODES];
extern int kb_layout;
extern unsigned char keynumpress;

int GNW_kb_set();
void GNW_kb_restore();
void kb_wait();
void kb_clear();
int kb_getch();
void kb_disable();
void kb_enable();
bool kb_is_disabled();
void kb_disable_numpad();
void kb_enable_numpad();
bool kb_numpad_is_disabled();
void kb_disable_numlock();
void kb_enable_numlock();
bool kb_numlock_is_disabled();
void kb_set_layout(int layout);
int kb_get_layout();
int kb_ascii_to_scan(int ascii);
unsigned int kb_elapsed_time();
void kb_reset_elapsed_time();
void kb_simulate_key(KeyboardData* data);

} // namespace fallout

#endif /* FALLOUT_PLIB_GNW_KB_H_ */
