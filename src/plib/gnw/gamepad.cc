#include <algorithm>
#include <cmath>

#include <SDL.h>

#include "plib/gnw/debug.h"
#include "plib/gnw/gamepad.hpp"
#include "plib/gnw/mouse.h"
#include "plib/gnw/svga.h"

namespace fallout {

namespace {

    // [-32767.0..+32767.0] -> [-1.0..1.0]
    void ScaleJoystickAxes(float* x, float* y, float deadzone)
    {
        if (deadzone == 0) {
            return;
        }
        if (deadzone >= 1.0) {
            *x = 0;
            *y = 0;
            return;
        }

        const float maximum = 32767.0;
        float analogX = *x;
        float analogY = *y;
        float deadZone = deadzone * maximum;

        float magnitude = std::sqrt(analogX * analogX + analogY * analogY);
        if (magnitude >= deadZone) {
            float scalingFactor = 1.F / magnitude * (magnitude - deadZone) / (maximum - deadZone);
            analogX = (analogX * scalingFactor);
            analogY = (analogY * scalingFactor);

            float clampingFactor = 1.F;
            float absAnalogX = std::fabs(analogX);
            float absAnalogY = std::fabs(analogY);
            if (absAnalogX > 1.0 || absAnalogY > 1.0) {
                if (absAnalogX > absAnalogY) {
                    clampingFactor = 1.F / absAnalogX;
                } else {
                    clampingFactor = 1.F / absAnalogY;
                }
            }
            *x = (clampingFactor * analogX);
            *y = (clampingFactor * analogY);
        } else {
            *x = 0;
            *y = 0;
        }
    }

    float leftStickXUnscaled, leftStickYUnscaled, rightStickXUnscaled, rightStickYUnscaled;

    void ScaleLeftJoystick()
    {
        const float leftDeadzone = 0.07F;
        leftStickX = leftStickXUnscaled;
        leftStickY = leftStickYUnscaled;
        ScaleJoystickAxes(&leftStickX, &leftStickY, leftDeadzone);
    }

    void ScaleRightJoystick()
    {
        const float rightDeadzone = 0.07F;
        rightStickX = rightStickXUnscaled;
        rightStickY = rightStickYUnscaled;
        ScaleJoystickAxes(&rightStickX, &rightStickY, rightDeadzone);
    }

    struct RightStickAccumulator {

        RightStickAccumulator()
        {
            lastTc = SDL_GetTicks();
            hiresDX = 0;
            hiresDY = 0;
        }

        void Pool(int* x, int* y, int slowdown)
        {
            const Uint32 tc = SDL_GetTicks();
            const int dtc = tc - lastTc;
            hiresDX += rightStickX * dtc;
            hiresDY += rightStickY * dtc;
            const int dx = static_cast<int>(hiresDX / slowdown);
            const int dy = static_cast<int>(hiresDY / slowdown);
            *x += dx;
            *y -= dy;
            lastTc = tc;
            // keep track of remainder for sub-pixel motion
            hiresDX -= dx * slowdown;
            hiresDY -= dy * slowdown;
        }

        void Clear()
        {
            lastTc = SDL_GetTicks();
        }

        uint32_t lastTc;
        float hiresDX;
        float hiresDY;
    };

} // namespace

float leftStickX, leftStickY, rightStickX, rightStickY;

void HandleControllerAxisMotion(const SDL_Event& event)
{
    switch (event.caxis.axis) {
    case SDL_CONTROLLER_AXIS_LEFTX:
        leftStickXUnscaled = static_cast<float>(event.caxis.value);
        ScaleLeftJoystick();
        break;
    case SDL_CONTROLLER_AXIS_LEFTY:
        leftStickYUnscaled = static_cast<float>(-event.caxis.value);
        ScaleLeftJoystick();
        break;
    case SDL_CONTROLLER_AXIS_RIGHTX:
        rightStickXUnscaled = static_cast<float>(event.caxis.value);
        ScaleRightJoystick();
        break;
    case SDL_CONTROLLER_AXIS_RIGHTY:
        rightStickYUnscaled = static_cast<float>(-event.caxis.value);
        ScaleRightJoystick();
        break;
    }
}

void ProcessLeftStick()
{
}

void ProcessRightStick()
{
    static RightStickAccumulator acc;
    // deadzone is handled in ScaleJoystickAxes() already
    if (rightStickX == 0 && rightStickY == 0) {
        acc.Clear();
        return;
    }

    int x, y;
    SDL_GetRelativeMouseState(&x, &y);
    int newX = x;
    int newY = y;
    acc.Pool(&newX, &newY, 2);

    // clipping to viewport is handled in mouse_simulate_input
    if (newX != x || newY != y) {
        mouse_simulate_input(newX - x, newY - y, 0);
    }
}

void HandleJoystickDeviceAdded(const SDL_Event& event)
{
    const int32_t deviceIndex = event.jdevice.which;
    if (SDL_NumJoysticks() <= deviceIndex)
        return;
    debug_printf("Adding joystick %d: %s\n", deviceIndex,
        SDL_JoystickNameForIndex(deviceIndex));
    SDL_Joystick* const joystick = SDL_JoystickOpen(deviceIndex);
    if (joystick == nullptr) {
        debug_printf("%s", SDL_GetError());
        SDL_ClearError();
        return;
    }
    SDL_JoystickGUID guid = SDL_JoystickGetGUID(joystick);
    char guidBuf[33];
    SDL_JoystickGetGUIDString(guid, guidBuf, sizeof(guidBuf));
    debug_printf("Added joystick %d {%s}\n", deviceIndex, guidBuf);
}

void HandleJoystickDeviceRemoved(const SDL_Event& event)
{
    const int32_t deviceIndex = event.jdevice.which;
    debug_printf("Removed joystick %d\n", deviceIndex);
}

void HandleControllerDeviceAdded(const SDL_Event& event)
{
    const int32_t joystickIndex = event.cdevice.which;
    debug_printf("Opening game controller for joystick %d\n", joystickIndex);
    SDL_GameController* const controller = SDL_GameControllerOpen(joystickIndex);
    if (controller == nullptr) {
        debug_printf("%s", SDL_GetError());
        SDL_ClearError();
        return;
    }
    SDL_Joystick* const sdlJoystick = SDL_GameControllerGetJoystick(controller);
    const SDL_JoystickGUID guid = SDL_JoystickGetGUID(sdlJoystick);
    char* mapping = SDL_GameControllerMappingForGUID(guid);
    if (mapping != nullptr) {
        debug_printf("Opened game controller with mapping:\n%s\n", mapping);
        SDL_free(mapping);
    }
}

void HandleControllerDeviceRemoved(const SDL_Event& event)
{
    const int32_t joystickIndex = event.cdevice.which;
    debug_printf("Removed game controller for joystick %d\n", joystickIndex);
}

} // namespace fallout
