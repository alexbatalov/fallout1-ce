#include "game/cycle.h"

#include "game/gconfig.h"
#include "game/palette.h"
#include "plib/color/color.h"
#include "plib/gnw/input.h"

namespace fallout {

#define COLOR_CYCLE_PERIOD_SLOW 200U
#define COLOR_CYCLE_PERIOD_MEDIUM 142U
#define COLOR_CYCLE_PERIOD_FAST 100U
#define COLOR_CYCLE_PERIOD_VERY_FAST 33U

static void cycle_colors();

// 0x504E3C
static int cycle_speed_factor = 1;

// 0x504E40
unsigned char slime[12] = {
    // clang-format off
    0, 108, 0,
    11, 115, 7,
    27, 123, 15,
    43, 131, 27,
    // clang-format on
};

// 0x504E4C
unsigned char shoreline[18] = {
    // clang-format off
    83, 63, 43,
    75, 59, 43,
    67, 55, 39,
    63, 51, 39,
    55, 47, 35,
    51, 43, 35,
    // clang-format on
};

// 0x504E5E
unsigned char fire_slow[15] = {
    // clang-format off
    255, 0, 0,
    215, 0, 0,
    147, 43, 11,
    255, 119, 0,
    255, 59, 0,
    // clang-format on
};

// 0x504E6D
unsigned char fire_fast[15] = {
    // clang-format off
    71, 0, 0,
    123, 0, 0,
    179, 0, 0,
    123, 0, 0,
    71, 0, 0,
    // clang-format on
};

// 0x504E7C
unsigned char monitors[15] = {
    // clang-format off
    107, 107, 111,
    99, 103, 127,
    87, 107, 143,
    0, 147, 163,
    107, 187, 255,
    // clang-format on
};

// 0x504E8C
static bool cycle_initialized = false;

// 0x504E90
static bool cycle_enabled = false;

// 0x56BF60
static unsigned int last_cycle_fast;

// 0x56BF64
static unsigned int last_cycle_slow;

// 0x56BF68
static unsigned int last_cycle_medium;

// 0x56BF6C
static unsigned int last_cycle_very_fast;

// 0x428D60
void cycle_init()
{
    bool colorCycling;
    int index;
    int cycleSpeedFactor;

    if (cycle_initialized) {
        return;
    }

    if (!configGetBool(&game_config, GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_COLOR_CYCLING_KEY, &colorCycling)) {
        colorCycling = true;
    }

    if (!colorCycling) {
        return;
    }

    for (index = 0; index < 12; index++) {
        slime[index] >>= 2;
    }

    for (index = 0; index < 18; index++) {
        shoreline[index] >>= 2;
    }

    for (index = 0; index < 15; index++) {
        fire_slow[index] >>= 2;
    }

    for (index = 0; index < 15; index++) {
        fire_fast[index] >>= 2;
    }

    for (index = 0; index < 15; index++) {
        monitors[index] >>= 2;
    }

    add_bk_process(cycle_colors);

    cycle_initialized = true;
    cycle_enabled = true;

    if (!config_get_value(&game_config, GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_CYCLE_SPEED_FACTOR_KEY, &cycleSpeedFactor)) {
        cycleSpeedFactor = 1;
    }

    change_cycle_speed(cycleSpeedFactor);
}

// 0x428EAC
void cycle_reset()
{
    if (cycle_initialized) {
        last_cycle_slow = 0;
        last_cycle_medium = 0;
        last_cycle_fast = 0;
        last_cycle_very_fast = 0;
        add_bk_process(cycle_colors);
        cycle_enabled = true;
    }
}

// 0x428EEC
void cycle_exit()
{
    if (cycle_initialized) {
        remove_bk_process(cycle_colors);
        cycle_initialized = false;
        cycle_enabled = false;
    }
}

// 0x428F10
void cycle_disable()
{
    cycle_enabled = false;
}

// 0x428F1C
void cycle_enable()
{
    cycle_enabled = true;
}

// 0x428F28
bool cycle_is_enabled()
{
    return cycle_enabled;
}

// 0x428F5C
static void cycle_colors()
{
    // 0x504E94
    static int slime_start = 0;

    // 0x504E98
    static int shoreline_start = 0;

    // 0x504E9C
    static int fire_slow_start = 0;

    // 0x504EA0
    static int fire_fast_start = 0;

    // 0x504EA4
    static int monitors_start = 0;

    // 0x504EA8
    static unsigned char bobber_red = 0;

    // 0x504EA9
    static signed char bobber_diff = -4;

    if (!cycle_enabled) {
        return;
    }

    bool changed = false;

    unsigned char* palette = getSystemPalette();
    unsigned int time = get_time();

    if (elapsed_tocks(time, last_cycle_slow) >= COLOR_CYCLE_PERIOD_SLOW * cycle_speed_factor) {
        changed = true;
        last_cycle_slow = time;

        int paletteIndex = 229 * 3;

        for (int index = slime_start; index < 12; index++) {
            palette[paletteIndex++] = slime[index];
        }

        for (int index = 0; index < slime_start; index++) {
            palette[paletteIndex++] = slime[index];
        }

        slime_start -= 3;
        if (slime_start < 0) {
            slime_start = 9;
        }

        paletteIndex = 248 * 3;

        for (int index = shoreline_start; index < 18; index++) {
            palette[paletteIndex++] = shoreline[index];
        }

        for (int index = 0; index < shoreline_start; index++) {
            palette[paletteIndex++] = shoreline[index];
        }

        shoreline_start -= 3;
        if (shoreline_start < 0) {
            shoreline_start = 15;
        }

        paletteIndex = 238 * 3;

        for (int index = fire_slow_start; index < 15; index++) {
            palette[paletteIndex++] = fire_slow[index];
        }

        for (int index = 0; index < fire_slow_start; index++) {
            palette[paletteIndex++] = fire_slow[index];
        }

        fire_slow_start -= 3;
        if (fire_slow_start < 0) {
            fire_slow_start = 12;
        }
    }

    if (elapsed_tocks(time, last_cycle_medium) >= COLOR_CYCLE_PERIOD_MEDIUM * cycle_speed_factor) {
        changed = true;
        last_cycle_medium = time;

        int paletteIndex = 243 * 3;

        for (int index = fire_fast_start; index < 15; index++) {
            palette[paletteIndex++] = fire_fast[index];
        }

        for (int index = 0; index < fire_fast_start; index++) {
            palette[paletteIndex++] = fire_fast[index];
        }

        fire_fast_start -= 3;
        if (fire_fast_start < 0) {
            fire_fast_start = 12;
        }
    }

    if (elapsed_tocks(time, last_cycle_fast) >= COLOR_CYCLE_PERIOD_FAST * cycle_speed_factor) {
        changed = true;
        last_cycle_fast = time;

        int paletteIndex = 233 * 3;

        for (int index = monitors_start; index < 15; index++) {
            palette[paletteIndex++] = monitors[index];
        }

        for (int index = 0; index < monitors_start; index++) {
            palette[paletteIndex++] = monitors[index];
        }

        monitors_start -= 3;

        if (monitors_start < 0) {
            monitors_start = 12;
        }
    }

    if (elapsed_tocks(time, last_cycle_very_fast) >= COLOR_CYCLE_PERIOD_VERY_FAST * cycle_speed_factor) {
        changed = true;
        last_cycle_very_fast = time;

        if (bobber_red == 0 || bobber_red == 60) {
            bobber_diff = -bobber_diff;
        }

        bobber_red += bobber_diff;

        int paletteIndex = 254 * 3;
        palette[paletteIndex++] = bobber_red;
        palette[paletteIndex++] = 0;
        palette[paletteIndex++] = 0;
    }

    if (changed) {
        palette_set_entries(palette + 229 * 3, 229, 255);
    }
}

// 0x428F30
void change_cycle_speed(int value)
{
    cycle_speed_factor = value;
    config_set_value(&game_config, GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_CYCLE_SPEED_FACTOR_KEY, value);
}

// 0x428F54
int get_cycle_speed()
{
    return cycle_speed_factor;
}

} // namespace fallout
