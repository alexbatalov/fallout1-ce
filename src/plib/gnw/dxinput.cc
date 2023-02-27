#include "plib/gnw/dxinput.h"

namespace fallout {

enum InputType {
    INPUT_TYPE_MOUSE,
    INPUT_TYPE_TOUCH,
} InputType;

static bool dxinput_mouse_init();
static void dxinput_mouse_exit();
static bool dxinput_keyboard_init();
static void dxinput_keyboard_exit();

static int gLastInputType = INPUT_TYPE_MOUSE;

static int gTouchMouseLastX = 0;
static int gTouchMouseLastY = 0;
static int gTouchMouseDeltaX = 0;
static int gTouchMouseDeltaY = 0;

static int gTouchFingers = 0;
static unsigned int gTouchGestureLastTouchDownTimestamp = 0;
static unsigned int gTouchGestureLastTouchUpTimestamp = 0;
static int gTouchGestureTaps = 0;
static bool gTouchGestureHandled = false;

static int gMouseWheelDeltaX = 0;
static int gMouseWheelDeltaY = 0;

extern int screenGetWidth();
extern int screenGetHeight();

// 0x4E0400
bool dxinput_init()
{
    if (SDL_InitSubSystem(SDL_INIT_EVENTS) != 0) {
        return false;
    }

    if (!dxinput_mouse_init()) {
        goto err;
    }

    if (!dxinput_keyboard_init()) {
        goto err;
    }

    return true;

err:

    dxinput_mouse_exit();

    return false;
}

// 0x4E0478
void dxinput_exit()
{
    SDL_QuitSubSystem(SDL_INIT_EVENTS);
}

// 0x4E04E8
bool dxinput_acquire_mouse()
{
    return true;
}

// 0x4E0514
bool dxinput_unacquire_mouse()
{
    return true;
}

// 0x4E053C
bool dxinput_get_mouse_state(MouseData* mouseState)
{
    // CE: This function is sometimes called outside loops calling `get_input`
    // and subsequently `GNW95_process_message`, so mouse events might not be
    // handled by SDL yet.
    //
    // TODO: Move mouse events processing into `GNW95_process_message` and
    // update mouse position manually.
    SDL_PumpEvents();

    if (gLastInputType == INPUT_TYPE_TOUCH) {
        mouseState->x = gTouchMouseDeltaX;
        mouseState->y = gTouchMouseDeltaY;
        mouseState->buttons[0] = 0;
        mouseState->buttons[1] = 0;
        mouseState->wheelX = 0;
        mouseState->wheelY = 0;
        gTouchMouseDeltaX = 0;
        gTouchMouseDeltaY = 0;

        if (gTouchFingers == 0) {
            if (SDL_GetTicks() - gTouchGestureLastTouchUpTimestamp > 150) {
                if (!gTouchGestureHandled) {
                    if (gTouchGestureTaps == 2) {
                        mouseState->buttons[0] = 1;
                        gTouchGestureHandled = true;
                    } else if (gTouchGestureTaps == 3) {
                        mouseState->buttons[1] = 1;
                        gTouchGestureHandled = true;
                    }
                }
            }
        } else if (gTouchFingers == 1) {
            if (SDL_GetTicks() - gTouchGestureLastTouchDownTimestamp > 150) {
                if (gTouchGestureTaps == 1) {
                    mouseState->buttons[0] = 1;
                    gTouchGestureHandled = true;
                } else if (gTouchGestureTaps == 2) {
                    mouseState->buttons[1] = 1;
                    gTouchGestureHandled = true;
                }
            }
        }
    } else {
        Uint32 buttons = SDL_GetRelativeMouseState(&(mouseState->x), &(mouseState->y));
        mouseState->buttons[0] = (buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
        mouseState->buttons[1] = (buttons & SDL_BUTTON(SDL_BUTTON_RIGHT)) != 0;
        mouseState->wheelX = gMouseWheelDeltaX;
        mouseState->wheelY = gMouseWheelDeltaY;

        gMouseWheelDeltaX = 0;
        gMouseWheelDeltaY = 0;
    }

    return true;
}

// 0x4E05A8
bool dxinput_acquire_keyboard()
{
    return true;
}

// 0x4E05D4
bool dxinput_unacquire_keyboard()
{
    return true;
}

// 0x4E05FC
bool dxinput_flush_keyboard_buffer()
{
    SDL_FlushEvents(SDL_KEYDOWN, SDL_TEXTINPUT);
    return true;
}

// 0x4E0650
bool dxinput_read_keyboard_buffer(KeyboardData* keyboardData)
{
    return true;
}

// 0x4E070C
bool dxinput_mouse_init()
{
    return SDL_SetRelativeMouseMode(SDL_TRUE) == 0;
}

// 0x4E078C
void dxinput_mouse_exit()
{
}

// 0x4E07B8
bool dxinput_keyboard_init()
{
    return true;
}

// 0x4E0874
void dxinput_keyboard_exit()
{
}

void handleMouseEvent(SDL_Event* event)
{
    // Mouse movement and buttons are accumulated in SDL itself and will be
    // processed later in `mouseDeviceGetData` via `SDL_GetRelativeMouseState`.

    if (event->type == SDL_MOUSEWHEEL) {
        gMouseWheelDeltaX += event->wheel.x;
        gMouseWheelDeltaY += event->wheel.y;
    }

    if (gLastInputType != INPUT_TYPE_MOUSE) {
        // Reset touch data.
        gTouchMouseLastX = 0;
        gTouchMouseLastY = 0;
        gTouchMouseDeltaX = 0;
        gTouchMouseDeltaY = 0;

        gTouchFingers = 0;
        gTouchGestureLastTouchDownTimestamp = 0;
        gTouchGestureLastTouchUpTimestamp = 0;
        gTouchGestureTaps = 0;
        gTouchGestureHandled = false;

        gLastInputType = INPUT_TYPE_MOUSE;
    }
}

void handleTouchEvent(SDL_Event* event)
{
    int windowWidth = screenGetWidth();
    int windowHeight = screenGetHeight();

    if (event->tfinger.type == SDL_FINGERDOWN) {
        gTouchFingers++;

        gTouchMouseLastX = (int)(event->tfinger.x * windowWidth);
        gTouchMouseLastY = (int)(event->tfinger.y * windowHeight);
        gTouchMouseDeltaX = 0;
        gTouchMouseDeltaY = 0;

        if (event->tfinger.timestamp - gTouchGestureLastTouchDownTimestamp > 250) {
            gTouchGestureTaps = 0;
            gTouchGestureHandled = false;
        }

        gTouchGestureLastTouchDownTimestamp = event->tfinger.timestamp;
    } else if (event->tfinger.type == SDL_FINGERMOTION) {
        int prevX = gTouchMouseLastX;
        int prevY = gTouchMouseLastY;
        gTouchMouseLastX = (int)(event->tfinger.x * windowWidth);
        gTouchMouseLastY = (int)(event->tfinger.y * windowHeight);
        gTouchMouseDeltaX += gTouchMouseLastX - prevX;
        gTouchMouseDeltaY += gTouchMouseLastY - prevY;
    } else if (event->tfinger.type == SDL_FINGERUP) {
        gTouchFingers--;

        int prevX = gTouchMouseLastX;
        int prevY = gTouchMouseLastY;
        gTouchMouseLastX = (int)(event->tfinger.x * windowWidth);
        gTouchMouseLastY = (int)(event->tfinger.y * windowHeight);
        gTouchMouseDeltaX += gTouchMouseLastX - prevX;
        gTouchMouseDeltaY += gTouchMouseLastY - prevY;

        gTouchGestureTaps++;
        gTouchGestureLastTouchUpTimestamp = event->tfinger.timestamp;
    }

    if (gLastInputType != INPUT_TYPE_TOUCH) {
        // Reset mouse data.
        SDL_GetRelativeMouseState(NULL, NULL);

        gLastInputType = INPUT_TYPE_TOUCH;
    }
}

} // namespace fallout
