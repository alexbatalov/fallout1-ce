#ifndef FALLOUT_PLIB_GNW_TOUCH_H
#define FALLOUT_PLIB_GNW_TOUCH_H

#include <SDL.h>

namespace fallout {

enum GestureType {
    kUnrecognized,
    kTap,
    kLongPress,
    kPan,
};

enum GestureState {
    kPossible,
    kBegan,
    kChanged,
    kEnded,
};

struct Gesture {
    GestureType type;
    GestureState state;
    int numberOfTouches;
    int x;
    int y;
};

void touch_handle_start(SDL_TouchFingerEvent* event);
void touch_handle_move(SDL_TouchFingerEvent* event);
void touch_handle_end(SDL_TouchFingerEvent* event);
void touch_process_gesture();
bool touch_get_gesture(Gesture* gesture);

} // namespace fallout

#endif /* FALLOUT_PLIB_GNW_TOUCH_H */
