#include "game/options.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>

#include "game/art.h"
#include "game/combat.h"
#include "game/combatai.h"
#include "game/cycle.h"
#include "game/game.h"
#include "game/gconfig.h"
#include "game/gmouse.h"
#include "game/graphlib.h"
#include "game/gsound.h"
#include "game/loadsave.h"
#include "game/message.h"
#include "game/scripts.h"
#include "game/textobj.h"
#include "game/tile.h"
#include "game/worldmap.h"
#include "platform_compat.h"
#include "plib/color/color.h"
#include "plib/gnw/button.h"
#include "plib/gnw/debug.h"
#include "plib/gnw/gnw.h"
#include "plib/gnw/grbuf.h"
#include "plib/gnw/input.h"
#include "plib/gnw/memory.h"
#include "plib/gnw/svga.h"
#include "plib/gnw/text.h"

namespace fallout {

#define PREFERENCES_WINDOW_WIDTH 640
#define PREFERENCES_WINDOW_HEIGHT 480

#define OPTIONS_WINDOW_BUTTONS_COUNT 10
#define PRIMARY_OPTION_VALUE_COUNT 4
#define SECONDARY_OPTION_VALUE_COUNT 2

#define GAMMA_MIN 1.0
#define GAMMA_MAX 1.17999267578125
#define GAMMA_STEP 0.01124954223632812

typedef enum Preference {
    PREF_GAME_DIFFICULTY,
    PREF_COMBAT_DIFFICULTY,
    PREF_VIOLENCE_LEVEL,
    PREF_TARGET_HIGHLIGHT,
    PREF_RUNNING_BURNING_GUY,
    PREF_COMBAT_MESSAGES,
    PREF_COMBAT_TAUNTS,
    PREF_LANGUAGE_FILTER,
    PREF_RUNNING,
    PREF_SUBTITLES,
    PREF_ITEM_HIGHLIGHT,
    PREF_COMBAT_SPEED,
    PREF_TEXT_BASE_DELAY,
    PREF_MASTER_VOLUME,
    PREF_MUSIC_VOLUME,
    PREF_SFX_VOLUME,
    PREF_SPEECH_VOLUME,
    PREF_BRIGHTNESS,
    PREF_MOUSE_SENSITIVIY,
    PREF_COUNT,
    FIRST_PRIMARY_PREF = PREF_GAME_DIFFICULTY,
    LAST_PRIMARY_PREF = PREF_RUNNING_BURNING_GUY,
    PRIMARY_PREF_COUNT = LAST_PRIMARY_PREF - FIRST_PRIMARY_PREF + 1,
    FIRST_SECONDARY_PREF = PREF_COMBAT_MESSAGES,
    LAST_SECONDARY_PREF = PREF_ITEM_HIGHLIGHT,
    SECONDARY_PREF_COUNT = LAST_SECONDARY_PREF - FIRST_SECONDARY_PREF + 1,
    FIRST_RANGE_PREF = PREF_COMBAT_SPEED,
    LAST_RANGE_PREF = PREF_MOUSE_SENSITIVIY,
    RANGE_PREF_COUNT = LAST_RANGE_PREF - FIRST_RANGE_PREF + 1,
} Preference;

typedef enum PauseWindowFrm {
    PAUSE_WINDOW_FRM_BACKGROUND,
    PAUSE_WINDOW_FRM_DONE_BOX,
    PAUSE_WINDOW_FRM_LITTLE_RED_BUTTON_UP,
    PAUSE_WINDOW_FRM_LITTLE_RED_BUTTON_DOWN,
    PAUSE_WINDOW_FRM_COUNT,
} PauseWindowFrm;

typedef enum OptionsWindowFrm {
    OPTIONS_WINDOW_FRM_BACKGROUND,
    OPTIONS_WINDOW_FRM_BUTTON_ON,
    OPTIONS_WINDOW_FRM_BUTTON_OFF,
    OPTIONS_WINDOW_FRM_COUNT,
} OptionsWindowFrm;

typedef enum PreferencesWindowFrm {
    PREFERENCES_WINDOW_FRM_BACKGROUND,
    // Knob (for range preferences)
    PREFERENCES_WINDOW_FRM_KNOB_OFF,
    // 4-way switch (for primary preferences)
    PREFERENCES_WINDOW_FRM_PRIMARY_SWITCH,
    // 2-way switch (for secondary preferences)
    PREFERENCES_WINDOW_FRM_SECONDARY_SWITCH,
    PREFERENCES_WINDOW_FRM_CHECKBOX_ON,
    PREFERENCES_WINDOW_FRM_CHECKBOX_OFF,
    PREFERENCES_WINDOW_FRM_6,
    // Active knob (for range preferences)
    PREFERENCES_WINDOW_FRM_KNOB_ON,
    PREFERENCES_WINDOW_FRM_LITTLE_RED_BUTTON_UP,
    PREFERENCES_WINDOW_FRM_LITTLE_RED_BUTTON_DOWN,
    PREFERENCES_WINDOW_FRM_COUNT,
} PreferencesWindowFrm;

typedef struct PreferenceDescription {
    // The number of options.
    short valuesCount;

    // Direction of rotation:
    // 0 - clockwise (incrementing value),
    // 1 - counter-clockwise (decrementing value)
    short direction;
    short knobX;
    short knobY;
    // Min x coordinate of the preference control bounding box.
    short minX;
    // Max x coordinate of the preference control bounding box.
    short maxX;
    short labelIds[PRIMARY_OPTION_VALUE_COUNT];
    int btn;
    char name[32];
    double minValue;
    double maxValue;
    int* valuePtr;
} PreferenceDescription;

static int OptnStart();
static int OptnEnd();
static void ShadeScreen(bool a1);
static int do_prefscreen();
static int PrefStart();
static void DoThing(int eventCode);
static void UpdateThing(int index);
static int PrefEnd();
static void SetSystemPrefs();
static int SavePrefs(bool save);
static void SetDefaults(bool a1);
static void SaveSettings();
static void RestoreSettings();
static void JustUpdate();

// 0x4812B0
static const short row1Ytab[PRIMARY_PREF_COUNT] = {
    48,
    125,
    203,
    286,
    363,
};

// 0x4812BA
static const short row2Ytab[SECONDARY_PREF_COUNT] = {
    49,
    116,
    181,
    247,
    313,
    380,
};

// 0x4812C6
static const short row3Ytab[RANGE_PREF_COUNT] = {
    19,
    94,
    165,
    216,
    268,
    319,
    369,
    420,
};

// x offsets for primary preferences from the knob position
// 0x4812D6
static const short bglbx[PRIMARY_OPTION_VALUE_COUNT] = {
    2,
    25,
    46,
    46,
};

// y offsets for primary preference option values from the knob position
// 0x4812DE
static const short bglby[PRIMARY_OPTION_VALUE_COUNT] = {
    10,
    -4,
    10,
    31,
};

// x offsets for secondary prefrence option values from the knob position
// 0x48FC06
static const short smlbx[SECONDARY_OPTION_VALUE_COUNT] = {
    4,
    21,
};

// 0x505D30
static int opgrphs[OPTIONS_WINDOW_FRM_COUNT] = {
    220, // opbase.frm - character editor
    222, // opbtnon.frm - character editor
    221, // opbtnoff.frm - character editor
};

// 0x505D3C
static int prfgrphs[PREFERENCES_WINDOW_FRM_COUNT] = {
    240, // prefscrn.frm - options screen
    241, // prfsldof.frm - options screen
    242, // prfbknbs.frm - options screen
    243, // prflknbs.frm - options screen
    244, // prfxin.frm - options screen
    245, // prfxout.frm - options screen
    246, // prefcvr.frm - options screen
    247, // prfsldon.frm - options screen
    8, // lilredup.frm - little red button up
    9, // lilreddn.frm - little red button down
};

// 0x661D30
static Size ginfo[OPTIONS_WINDOW_FRM_COUNT];

// 0x661D48
static MessageList optn_msgfl;

// 0x661D50
static Size ginfo2[PREFERENCES_WINDOW_FRM_COUNT];

// 0x661DA0
static unsigned char* prfbmp[PREFERENCES_WINDOW_FRM_COUNT];

// 0x661DC8
static unsigned char* opbtns[OPTIONS_WINDOW_BUTTONS_COUNT];

// 0x661DF0
static CacheEntry* grphkey2[PREFERENCES_WINDOW_FRM_COUNT];

// 0x661E18
static double text_delay_back;

// 0x661E20
static double gamma_value_back;

// 0x661E28
static double mouse_sens_back;

// 0x661E30
static double gamma_value;

// 0x661E38
static double text_delay;

// 0x661E40
static double mouse_sens;

// 0x661E48
static unsigned char* winbuf;

// 0x661E4C
static unsigned char* prefbuf;

// 0x661E50
static MessageListItem optnmesg;

// 0x661E5C
static CacheEntry* grphkey[OPTIONS_WINDOW_FRM_COUNT];

// 0x661E68
static bool mouse_3d_was_on;

// 0x661E6C
static int optnwin;

// 0x661E70
static int prfwin;

// 0x661E74
static unsigned char* opbmp[OPTIONS_WINDOW_FRM_COUNT];

// This array stores backup for only integer-representable prefs. Because
// `player_speedup` is not a part of prefs enum, it's value is stored in
// `PREF_TEXT_BASE_DELAY`.
//
// 0x661E80
static int settings_backup[PREF_COUNT];

// 0x661ECC
static int fontsave;

// 0x661ED0
static int plyrspdbid;

// 0x661ED4
static bool changed;

// 0x661ED8
static int sndfx_volume;

// 0x661EDC
static int subtitles;

// 0x661EE0
static int language_filter;

// 0x661EE4
static int speech_volume;

// 0x661EE8
static int master_volume;

// 0x661EEC
static int player_speedup;

// 0x661EF0
static int combat_taunts;

// 0x661EF4
static int combat_messages;

// 0x661EF8
static int target_highlight;

// 0x661EFC
static int music_volume;

// 0x661F00
static bool bk_enable;

// 0x661F04
static int prf_running;

// 0x661F08
static int combat_speed;

// 0x661F0C
static int item_highlight;

// 0x661F10
static int running_burning_guy;

// 0x661F14
static int combat_difficulty;

// 0x661F18
static int violence_level;

// 0x661F1C
static int game_difficulty;

// 0x505D68
static PreferenceDescription btndat[PREF_COUNT] = {
    { 3, 0, 76, 71, 0, 0, { 203, 204, 205, 0 }, 0, GAME_CONFIG_GAME_DIFFICULTY_KEY, 0, 0, &game_difficulty },
    { 3, 0, 76, 149, 0, 0, { 206, 204, 208, 0 }, 0, GAME_CONFIG_COMBAT_DIFFICULTY_KEY, 0, 0, &combat_difficulty },
    { 4, 0, 76, 226, 0, 0, { 214, 215, 204, 216 }, 0, GAME_CONFIG_VIOLENCE_LEVEL_KEY, 0, 0, &violence_level },
    { 3, 0, 76, 309, 0, 0, { 202, 201, 213, 0 }, 0, GAME_CONFIG_TARGET_HIGHLIGHT_KEY, 0, 0, &target_highlight },
    { 2, 0, 76, 387, 0, 0, { 202, 201, 0, 0 }, 0, GAME_CONFIG_RUNNING_BURNING_GUY_KEY, 0, 0, &running_burning_guy },
    { 2, 0, 299, 74, 0, 0, { 211, 212, 0, 0 }, 0, GAME_CONFIG_COMBAT_MESSAGES_KEY, 0, 0, &combat_messages },
    { 2, 0, 299, 141, 0, 0, { 202, 201, 0, 0 }, 0, GAME_CONFIG_COMBAT_TAUNTS_KEY, 0, 0, &combat_taunts },
    { 2, 0, 299, 207, 0, 0, { 202, 201, 0, 0 }, 0, GAME_CONFIG_LANGUAGE_FILTER_KEY, 0, 0, &language_filter },
    { 2, 0, 299, 271, 0, 0, { 209, 219, 0, 0 }, 0, GAME_CONFIG_RUNNING_KEY, 0, 0, &prf_running },
    { 2, 0, 299, 338, 0, 0, { 202, 201, 0, 0 }, 0, GAME_CONFIG_SUBTITLES_KEY, 0, 0, &subtitles },
    { 2, 0, 299, 404, 0, 0, { 202, 201, 0, 0 }, 0, GAME_CONFIG_ITEM_HIGHLIGHT_KEY, 0, 0, &item_highlight },
    { 2, 0, 374, 50, 0, 0, { 207, 210, 0, 0 }, 0, GAME_CONFIG_COMBAT_SPEED_KEY, 0.0, 50.0, &combat_speed },
    { 3, 0, 374, 125, 0, 0, { 217, 209, 218, 0 }, 0, GAME_CONFIG_TEXT_BASE_DELAY_KEY, 1.0, 6.0, NULL },
    { 4, 0, 374, 196, 0, 0, { 202, 221, 209, 222 }, 0, GAME_CONFIG_MASTER_VOLUME_KEY, 0, 32767.0, &master_volume },
    { 4, 0, 374, 247, 0, 0, { 202, 221, 209, 222 }, 0, GAME_CONFIG_MUSIC_VOLUME_KEY, 0, 32767.0, &music_volume },
    { 4, 0, 374, 298, 0, 0, { 202, 221, 209, 222 }, 0, GAME_CONFIG_SNDFX_VOLUME_KEY, 0, 32767.0, &sndfx_volume },
    { 4, 0, 374, 349, 0, 0, { 202, 221, 209, 222 }, 0, GAME_CONFIG_SPEECH_VOLUME_KEY, 0, 32767.0, &speech_volume },
    { 2, 0, 374, 400, 0, 0, { 207, 223, 0, 0 }, 0, GAME_CONFIG_BRIGHTNESS_KEY, 1.0, 1.17999267578125, NULL },
    { 2, 0, 374, 451, 0, 0, { 207, 218, 0, 0 }, 0, GAME_CONFIG_MOUSE_SENSITIVITY_KEY, 1.0, 2.5, NULL },
};

// 0x481328
int do_options()
{
    if (OptnStart() == -1) {
        debug_printf("\nOPTION MENU: Error loading option dialog data!\n");
        return -1;
    }

    int rc = -1;
    while (rc == -1) {
        sharedFpsLimiter.mark();

        int keyCode = get_input();
        bool showPreferences = false;

        if (keyCode == KEY_ESCAPE || keyCode == 504 || game_user_wants_to_quit != 0) {
            rc = 0;
        } else {
            switch (keyCode) {
            case KEY_RETURN:
            case KEY_UPPERCASE_O:
            case KEY_LOWERCASE_O:
            case KEY_UPPERCASE_D:
            case KEY_LOWERCASE_D:
                gsound_play_sfx_file("ib1p1xx1");
                rc = 0;
                break;
            case KEY_UPPERCASE_S:
            case KEY_LOWERCASE_S:
            case 500:
                if (SaveGame(LOAD_SAVE_MODE_NORMAL) == 1) {
                    rc = 1;
                }
                break;
            case KEY_UPPERCASE_L:
            case KEY_LOWERCASE_L:
            case 501:
                if (LoadGame(LOAD_SAVE_MODE_NORMAL) == 1) {
                    rc = 1;
                }
                break;
            case KEY_UPPERCASE_P:
            case KEY_LOWERCASE_P:
                gsound_play_sfx_file("ib1p1xx1");
                // FALLTHROUGH
            case 502:
                // PREFERENCES
                showPreferences = true;
                break;
            case KEY_PLUS:
            case KEY_EQUAL:
                IncGamma();
                break;
            case KEY_UNDERSCORE:
            case KEY_MINUS:
                DecGamma();
                break;
            }
        }

        if (showPreferences) {
            do_prefscreen();
        } else {
            switch (keyCode) {
            case KEY_F12:
                dump_screen();
                break;
            case KEY_UPPERCASE_E:
            case KEY_LOWERCASE_E:
            case KEY_CTRL_Q:
            case KEY_CTRL_X:
            case KEY_F10:
            case 503:
                game_quit_with_confirm();
                break;
            }
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    OptnEnd();

    return rc;
}

// 0x4814D8
static int OptnStart()
{
    fontsave = text_curr();

    if (!message_init(&optn_msgfl)) {
        return -1;
    }

    char path[COMPAT_MAX_PATH];
    snprintf(path, sizeof(path), "%s%s", msg_path, "options.msg");
    if (!message_load(&optn_msgfl, path)) {
        return -1;
    }

    for (int index = 0; index < OPTIONS_WINDOW_FRM_COUNT; index++) {
        int fid = art_id(OBJ_TYPE_INTERFACE, opgrphs[index], 0, 0, 0);
        opbmp[index] = art_lock(fid, &(grphkey[index]), &(ginfo[index].width), &(ginfo[index].height));

        if (opbmp[index] == NULL) {
            while (--index >= 0) {
                art_ptr_unlock(grphkey[index]);
            }

            message_exit(&optn_msgfl);

            return -1;
        }
    }

    int cycle = 0;
    for (int index = 0; index < OPTIONS_WINDOW_BUTTONS_COUNT; index++) {
        opbtns[index] = (unsigned char*)mem_malloc(ginfo[OPTIONS_WINDOW_FRM_BUTTON_ON].width * ginfo[OPTIONS_WINDOW_FRM_BUTTON_ON].height + 1024);
        if (opbtns[index] == NULL) {
            while (--index >= 0) {
                mem_free(opbtns[index]);
            }

            for (int index = 0; index < OPTIONS_WINDOW_FRM_COUNT; index++) {
                art_ptr_unlock(grphkey[index]);
            }

            message_exit(&optn_msgfl);

            return -1;
        }

        cycle = cycle ^ 1;

        memcpy(opbtns[index], opbmp[cycle + 1], ginfo[OPTIONS_WINDOW_FRM_BUTTON_ON].width * ginfo[OPTIONS_WINDOW_FRM_BUTTON_ON].height);
    }

    int optionsWindowX = (screenGetWidth() - ginfo[OPTIONS_WINDOW_FRM_BACKGROUND].width) / 2;
    int optionsWindowY = (screenGetHeight() - ginfo[OPTIONS_WINDOW_FRM_BACKGROUND].height) / 2 - 60;
    optnwin = win_add(optionsWindowX,
        optionsWindowY,
        ginfo[0].width,
        ginfo[0].height,
        256,
        WINDOW_MODAL | WINDOW_DONT_MOVE_TOP);

    if (optnwin == -1) {
        for (int index = 0; index < OPTIONS_WINDOW_BUTTONS_COUNT; index++) {
            mem_free(opbtns[index]);
        }

        for (int index = 0; index < OPTIONS_WINDOW_FRM_COUNT; index++) {
            art_ptr_unlock(grphkey[index]);
        }

        message_exit(&optn_msgfl);

        return -1;
    }

    bk_enable = map_disable_bk_processes();

    mouse_3d_was_on = gmouse_3d_is_on();
    if (mouse_3d_was_on) {
        gmouse_3d_off();
    }

    gmouse_set_cursor(MOUSE_CURSOR_ARROW);

    winbuf = win_get_buf(optnwin);
    memcpy(winbuf, opbmp[OPTIONS_WINDOW_FRM_BACKGROUND], ginfo[OPTIONS_WINDOW_FRM_BACKGROUND].width * ginfo[OPTIONS_WINDOW_FRM_BACKGROUND].height);

    text_font(103);

    int textY = (ginfo[OPTIONS_WINDOW_FRM_BUTTON_ON].height - text_height()) / 2 + 1;
    int buttonY = 17;

    for (int index = 0; index < OPTIONS_WINDOW_BUTTONS_COUNT; index += 2) {
        char text[128];

        const char* msg = getmsg(&optn_msgfl, &optnmesg, index / 2);
        strcpy(text, msg);

        int textX = (ginfo[OPTIONS_WINDOW_FRM_BUTTON_ON].width - text_width(text)) / 2;
        if (textX < 0) {
            textX = 0;
        }

        text_to_buf(opbtns[index] + ginfo[OPTIONS_WINDOW_FRM_BUTTON_ON].width * textY + textX, text, ginfo[OPTIONS_WINDOW_FRM_BUTTON_ON].width, ginfo[OPTIONS_WINDOW_FRM_BUTTON_ON].width, colorTable[18979]);
        text_to_buf(opbtns[index + 1] + ginfo[OPTIONS_WINDOW_FRM_BUTTON_ON].width * textY + textX, text, ginfo[OPTIONS_WINDOW_FRM_BUTTON_ON].width, ginfo[OPTIONS_WINDOW_FRM_BUTTON_ON].width, colorTable[14723]);

        int btn = win_register_button(optnwin, 13, buttonY, ginfo[OPTIONS_WINDOW_FRM_BUTTON_ON].width, ginfo[OPTIONS_WINDOW_FRM_BUTTON_ON].height, -1, -1, -1, index / 2 + 500, opbtns[index], opbtns[index + 1], NULL, 32);
        if (btn != -1) {
            win_register_button_sound_func(btn, gsound_lrg_butt_press, gsound_lrg_butt_release);
        }

        buttonY += ginfo[OPTIONS_WINDOW_FRM_BUTTON_ON].height + 3;
    }

    text_font(101);

    win_draw(optnwin);

    return 0;
}

// 0x481908
static int OptnEnd()
{
    win_delete(optnwin);
    text_font(fontsave);
    message_exit(&optn_msgfl);

    for (int index = 0; index < OPTIONS_WINDOW_BUTTONS_COUNT; index++) {
        mem_free(opbtns[index]);
    }

    for (int index = 0; index < OPTIONS_WINDOW_FRM_COUNT; index++) {
        art_ptr_unlock(grphkey[index]);
    }

    if (mouse_3d_was_on) {
        gmouse_3d_on();
    }

    if (bk_enable) {
        map_enable_bk_processes();
    }

    return 0;
}

// 0x481974
int PauseWindow(bool is_world_map)
{
    // 0x4812EC
    static const int graphicIds[PAUSE_WINDOW_FRM_COUNT] = {
        208, // charwin.frm - character editor
        209, // donebox.frm - character editor
        8, // lilredup.frm - little red button up
        9, // lilreddn.frm - little red button down
    };

    unsigned char* frmData[PAUSE_WINDOW_FRM_COUNT];
    CacheEntry* frmHandles[PAUSE_WINDOW_FRM_COUNT];
    Size frmSizes[PAUSE_WINDOW_FRM_COUNT];

    bool gameMouseWasVisible;
    if (!is_world_map) {
        bk_enable = map_disable_bk_processes();
        cycle_disable();

        gameMouseWasVisible = gmouse_3d_is_on();
        if (gameMouseWasVisible) {
            gmouse_3d_off();
        }
    }

    gmouse_set_cursor(MOUSE_CURSOR_ARROW);
    ShadeScreen(is_world_map);

    for (int index = 0; index < PAUSE_WINDOW_FRM_COUNT; index++) {
        int fid = art_id(OBJ_TYPE_INTERFACE, graphicIds[index], 0, 0, 0);
        frmData[index] = art_lock(fid, &(frmHandles[index]), &(frmSizes[index].width), &(frmSizes[index].height));
        if (frmData[index] == NULL) {
            while (--index >= 0) {
                art_ptr_unlock(frmHandles[index]);
            }

            debug_printf("\n** Error loading pause window graphics! **\n");
            return -1;
        }
    }

    if (!message_init(&optn_msgfl)) {
        // FIXME: Leaking graphics.
        return -1;
    }

    char path[COMPAT_MAX_PATH];
    snprintf(path, sizeof(path), "%s%s", msg_path, "options.msg");
    if (!message_load(&optn_msgfl, path)) {
        // FIXME: Leaking graphics.
        return -1;
    }

    int pauseWindowX = (screenGetWidth() - frmSizes[PAUSE_WINDOW_FRM_BACKGROUND].width) / 2;
    int pauseWindowY = (screenGetHeight() - frmSizes[PAUSE_WINDOW_FRM_BACKGROUND].height) / 2;

    if (is_world_map) {
        pauseWindowX -= 65;
        pauseWindowY -= 24;
    } else {
        pauseWindowY -= 54;
    }

    int window = win_add(pauseWindowX,
        pauseWindowY,
        frmSizes[PAUSE_WINDOW_FRM_BACKGROUND].width,
        frmSizes[PAUSE_WINDOW_FRM_BACKGROUND].height,
        256,
        WINDOW_MODAL | WINDOW_DONT_MOVE_TOP);
    if (window == -1) {
        for (int index = 0; index < PAUSE_WINDOW_FRM_COUNT; index++) {
            art_ptr_unlock(frmHandles[index]);
        }

        message_exit(&optn_msgfl);

        debug_printf("\n** Error opening pause window! **\n");
        return -1;
    }

    unsigned char* windowBuffer = win_get_buf(window);
    memcpy(windowBuffer,
        frmData[PAUSE_WINDOW_FRM_BACKGROUND],
        frmSizes[PAUSE_WINDOW_FRM_BACKGROUND].width * frmSizes[PAUSE_WINDOW_FRM_BACKGROUND].height);

    trans_buf_to_buf(frmData[PAUSE_WINDOW_FRM_DONE_BOX],
        frmSizes[PAUSE_WINDOW_FRM_DONE_BOX].width,
        frmSizes[PAUSE_WINDOW_FRM_DONE_BOX].height,
        frmSizes[PAUSE_WINDOW_FRM_DONE_BOX].width,
        windowBuffer + frmSizes[PAUSE_WINDOW_FRM_BACKGROUND].width * 42 + 13,
        frmSizes[PAUSE_WINDOW_FRM_BACKGROUND].width);

    fontsave = text_curr();
    text_font(103);

    char* messageItemText;

    messageItemText = getmsg(&optn_msgfl, &optnmesg, 300);
    text_to_buf(windowBuffer + frmSizes[PAUSE_WINDOW_FRM_BACKGROUND].width * 45 + 52,
        messageItemText,
        frmSizes[PAUSE_WINDOW_FRM_BACKGROUND].width,
        frmSizes[PAUSE_WINDOW_FRM_BACKGROUND].width,
        colorTable[18979]);

    text_font(104);

    messageItemText = getmsg(&optn_msgfl, &optnmesg, 301);
    strcpy(path, messageItemText);

    int length = text_width(path);
    text_to_buf(windowBuffer + frmSizes[PAUSE_WINDOW_FRM_BACKGROUND].width * 10 + 2 + (frmSizes[PAUSE_WINDOW_FRM_BACKGROUND].width - length) / 2,
        path,
        frmSizes[PAUSE_WINDOW_FRM_BACKGROUND].width,
        frmSizes[PAUSE_WINDOW_FRM_BACKGROUND].width,
        colorTable[18979]);

    int doneBtn = win_register_button(window,
        26,
        46,
        frmSizes[PAUSE_WINDOW_FRM_LITTLE_RED_BUTTON_UP].width,
        frmSizes[PAUSE_WINDOW_FRM_LITTLE_RED_BUTTON_UP].height,
        -1,
        -1,
        -1,
        504,
        frmData[PAUSE_WINDOW_FRM_LITTLE_RED_BUTTON_UP],
        frmData[PAUSE_WINDOW_FRM_LITTLE_RED_BUTTON_DOWN],
        NULL,
        BUTTON_FLAG_TRANSPARENT);
    if (doneBtn != -1) {
        win_register_button_sound_func(doneBtn, gsound_red_butt_press, gsound_red_butt_release);
    }

    win_draw(window);

    bool done = false;
    while (!done) {
        sharedFpsLimiter.mark();

        int keyCode = get_input();
        switch (keyCode) {
        case KEY_PLUS:
        case KEY_EQUAL:
            IncGamma();
            break;
        case KEY_MINUS:
        case KEY_UNDERSCORE:
            DecGamma();
            break;
        default:
            if (keyCode != -1 && keyCode != -2) {
                done = true;
            }

            if (game_user_wants_to_quit != 0) {
                done = true;
            }
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    if (!is_world_map) {
        tile_refresh_display();
    }

    win_delete(window);

    for (int index = 0; index < PAUSE_WINDOW_FRM_COUNT; index++) {
        art_ptr_unlock(frmHandles[index]);
    }

    message_exit(&optn_msgfl);

    if (!is_world_map) {
        if (gameMouseWasVisible) {
            gmouse_3d_on();
        }

        if (bk_enable) {
            map_enable_bk_processes();
        }

        cycle_enable();

        gmouse_set_cursor(MOUSE_CURSOR_ARROW);
    }

    text_font(fontsave);

    return 0;
}

// 0x481E0C
static void ShadeScreen(bool is_world_map)
{
    if (is_world_map) {
        mouse_hide();
        grey_buf(win_get_buf(world_win) + 640 * 21 + 22, 450, 442, 640);
        win_draw(world_win);
    } else {
        mouse_hide();
        tile_refresh_display();

        int windowWidth = 640;
        int windowHeight = win_height(display_win);
        unsigned char* windowBuffer = win_get_buf(display_win);
        grey_buf(windowBuffer, windowWidth, windowHeight, windowWidth);

        win_draw(display_win);
    }

    mouse_show();
}

// 0x481E84
static int do_prefscreen()
{
    if (PrefStart() == -1) {
        debug_printf("\nPREFERENCE MENU: Error loading preference dialog data!\n");
        return -1;
    }

    int rc = -1;
    while (rc == -1) {
        sharedFpsLimiter.mark();

        int eventCode = get_input();

        switch (eventCode) {
        case KEY_RETURN:
        case KEY_UPPERCASE_P:
        case KEY_LOWERCASE_P:
            gsound_play_sfx_file("ib1p1xx1");
            // FALLTHROUGH
        case 504:
            rc = 1;
            break;
        case KEY_CTRL_Q:
        case KEY_CTRL_X:
        case KEY_F10:
            game_quit_with_confirm();
            break;
        case KEY_EQUAL:
        case KEY_PLUS:
            IncGamma();
            break;
        case KEY_MINUS:
        case KEY_UNDERSCORE:
            DecGamma();
            break;
        case KEY_F12:
            dump_screen();
            break;
        case 527:
            SetDefaults(true);
            break;
        default:
            if (eventCode == KEY_ESCAPE || eventCode == 528 || game_user_wants_to_quit != 0) {
                RestoreSettings();
                rc = 0;
            } else if (eventCode >= 505 && eventCode <= 524) {
                DoThing(eventCode);
            }
            break;
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    PrefEnd();

    return rc;
}

// 0x481F8C
static int PrefStart()
{
    int i;
    int fid;
    char* messageItemText;
    int x;
    int y;
    int width;
    int height;
    int messageItemId;
    int btn;
    int button_count;

    SaveSettings();

    for (i = 0; i < PREFERENCES_WINDOW_FRM_COUNT; i++) {
        fid = art_id(OBJ_TYPE_INTERFACE, prfgrphs[i], 0, 0, 0);
        prfbmp[i] = art_lock(fid, &(grphkey2[i]), &(ginfo2[i].width), &(ginfo2[i].height));
        if (prfbmp[i] == NULL) {
            while (--i >= 0) {
                art_ptr_unlock(grphkey2[i - 1]);
            }
            return -1;
        }
    }

    changed = false;

    int preferencesWindowX = (screenGetWidth() - PREFERENCES_WINDOW_WIDTH) / 2;
    int preferencesWindowY = (screenGetHeight() - PREFERENCES_WINDOW_HEIGHT) / 2;
    prfwin = win_add(preferencesWindowX,
        preferencesWindowY,
        PREFERENCES_WINDOW_WIDTH,
        PREFERENCES_WINDOW_HEIGHT,
        256,
        WINDOW_MODAL | WINDOW_DONT_MOVE_TOP);
    if (prfwin == -1) {
        for (i = 0; i < PREFERENCES_WINDOW_FRM_COUNT; i++) {
            art_ptr_unlock(grphkey2[i]);
        }
        return -1;
    }

    prefbuf = win_get_buf(prfwin);
    memcpy(prefbuf,
        prfbmp[PREFERENCES_WINDOW_FRM_BACKGROUND],
        ginfo2[PREFERENCES_WINDOW_FRM_BACKGROUND].width * ginfo2[PREFERENCES_WINDOW_FRM_BACKGROUND].height);

    text_font(104);

    messageItemText = getmsg(&optn_msgfl, &optnmesg, 100);
    text_to_buf(prefbuf + PREFERENCES_WINDOW_WIDTH * 10 + 74, messageItemText, PREFERENCES_WINDOW_WIDTH, PREFERENCES_WINDOW_WIDTH, colorTable[18979]);

    text_font(103);

    if (game_global_vars[GVAR_RUNNING_BURNING_GUY]) {
        button_count = 5;
    } else {
        buf_to_buf(prfbmp[PREFERENCES_WINDOW_FRM_6],
            ginfo2[PREFERENCES_WINDOW_FRM_6].width,
            ginfo2[PREFERENCES_WINDOW_FRM_6].height,
            ginfo2[PREFERENCES_WINDOW_FRM_6].width,
            prefbuf + PREFERENCES_WINDOW_WIDTH * 356 + 0,
            PREFERENCES_WINDOW_WIDTH);
        button_count = 4;
    }

    messageItemId = 101;
    for (i = 0; i < button_count; i++) {
        messageItemText = getmsg(&optn_msgfl, &optnmesg, messageItemId++);
        x = 99 - text_width(messageItemText) / 2;
        text_to_buf(prefbuf + PREFERENCES_WINDOW_WIDTH * row1Ytab[i] + x, messageItemText, PREFERENCES_WINDOW_WIDTH, PREFERENCES_WINDOW_WIDTH, colorTable[18979]);
    }

    if (!game_global_vars[GVAR_RUNNING_BURNING_GUY]) {
        messageItemId++;
    }

    for (i = 0; i < SECONDARY_PREF_COUNT; i++) {
        messageItemText = getmsg(&optn_msgfl, &optnmesg, messageItemId++);
        text_to_buf(prefbuf + PREFERENCES_WINDOW_WIDTH * row2Ytab[i] + 206, messageItemText, PREFERENCES_WINDOW_WIDTH, PREFERENCES_WINDOW_WIDTH, colorTable[18979]);
    }

    for (i = 0; i < RANGE_PREF_COUNT; i++) {
        messageItemText = getmsg(&optn_msgfl, &optnmesg, messageItemId++);
        text_to_buf(prefbuf + PREFERENCES_WINDOW_WIDTH * row3Ytab[i] + 384, messageItemText, PREFERENCES_WINDOW_WIDTH, PREFERENCES_WINDOW_WIDTH, colorTable[18979]);
    }

    // DEFAULT
    messageItemText = getmsg(&optn_msgfl, &optnmesg, 120);
    text_to_buf(prefbuf + PREFERENCES_WINDOW_WIDTH * 449 + 43, messageItemText, PREFERENCES_WINDOW_WIDTH, PREFERENCES_WINDOW_WIDTH, colorTable[18979]);

    // DONE
    messageItemText = getmsg(&optn_msgfl, &optnmesg, 4);
    text_to_buf(prefbuf + PREFERENCES_WINDOW_WIDTH * 449 + 169, messageItemText, PREFERENCES_WINDOW_WIDTH, PREFERENCES_WINDOW_WIDTH, colorTable[18979]);

    // CANCEL
    messageItemText = getmsg(&optn_msgfl, &optnmesg, 121);
    text_to_buf(prefbuf + PREFERENCES_WINDOW_WIDTH * 449 + 283, messageItemText, PREFERENCES_WINDOW_WIDTH, PREFERENCES_WINDOW_WIDTH, colorTable[18979]);

    // Affect player speed
    text_font(101);
    messageItemText = getmsg(&optn_msgfl, &optnmesg, 122);
    text_to_buf(prefbuf + PREFERENCES_WINDOW_WIDTH * 72 + 405, messageItemText, PREFERENCES_WINDOW_WIDTH, PREFERENCES_WINDOW_WIDTH, colorTable[18979]);

    for (i = 0; i < PREF_COUNT; i++) {
        UpdateThing(i);
    }

    for (i = 0; i < PREF_COUNT; i++) {
        int mouseEnterEventCode;
        int mouseExitEventCode;
        int mouseDownEventCode;
        int mouseUpEventCode;

        if (i >= FIRST_RANGE_PREF) {
            x = 384;
            y = btndat[i].knobY - 12;
            width = 240;
            height = 23;
            mouseEnterEventCode = 526;
            mouseExitEventCode = 526;
            mouseDownEventCode = 505 + i;
            mouseUpEventCode = 526;

        } else if (i >= FIRST_SECONDARY_PREF) {
            x = btndat[i].minX;
            y = btndat[i].knobY - 5;
            width = btndat[i].maxX - x;
            height = 28;
            mouseEnterEventCode = -1;
            mouseExitEventCode = -1;
            mouseDownEventCode = -1;
            mouseUpEventCode = 505 + i;
        } else {
            x = btndat[i].minX;
            y = btndat[i].knobY - 4;
            width = btndat[i].maxX - x;
            height = 48;
            mouseEnterEventCode = -1;
            mouseExitEventCode = -1;
            mouseDownEventCode = -1;
            mouseUpEventCode = 505 + i;
        }

        btndat[i].btn = win_register_button(prfwin, x, y, width, height, mouseEnterEventCode, mouseExitEventCode, mouseDownEventCode, mouseUpEventCode, NULL, NULL, NULL, 32);
    }

    plyrspdbid = win_register_button(prfwin,
        383,
        68,
        ginfo2[PREFERENCES_WINDOW_FRM_CHECKBOX_OFF].width,
        ginfo2[PREFERENCES_WINDOW_FRM_CHECKBOX_ON].height,
        -1,
        -1,
        524,
        524,
        prfbmp[PREFERENCES_WINDOW_FRM_CHECKBOX_OFF],
        prfbmp[PREFERENCES_WINDOW_FRM_CHECKBOX_ON],
        NULL,
        BUTTON_FLAG_TRANSPARENT | BUTTON_FLAG_0x01 | BUTTON_FLAG_0x02);
    if (plyrspdbid != -1) {
        win_set_button_rest_state(plyrspdbid, player_speedup, 0);
    }

    win_register_button_sound_func(plyrspdbid, gsound_med_butt_press, gsound_med_butt_press);

    // DEFAULT
    btn = win_register_button(prfwin,
        23,
        450,
        ginfo2[PREFERENCES_WINDOW_FRM_LITTLE_RED_BUTTON_UP].width,
        ginfo2[PREFERENCES_WINDOW_FRM_LITTLE_RED_BUTTON_DOWN].height,
        -1,
        -1,
        -1,
        527,
        prfbmp[PREFERENCES_WINDOW_FRM_LITTLE_RED_BUTTON_UP],
        prfbmp[PREFERENCES_WINDOW_FRM_LITTLE_RED_BUTTON_DOWN],
        NULL,
        BUTTON_FLAG_TRANSPARENT);
    if (btn != -1) {
        win_register_button_sound_func(btn, gsound_red_butt_press, gsound_red_butt_release);
    }

    // DONE
    btn = win_register_button(prfwin,
        148,
        450,
        ginfo2[PREFERENCES_WINDOW_FRM_LITTLE_RED_BUTTON_UP].width,
        ginfo2[PREFERENCES_WINDOW_FRM_LITTLE_RED_BUTTON_DOWN].height,
        -1,
        -1,
        -1,
        504,
        prfbmp[PREFERENCES_WINDOW_FRM_LITTLE_RED_BUTTON_UP],
        prfbmp[PREFERENCES_WINDOW_FRM_LITTLE_RED_BUTTON_DOWN],
        NULL,
        BUTTON_FLAG_TRANSPARENT);
    if (btn != -1) {
        win_register_button_sound_func(btn, gsound_red_butt_press, gsound_red_butt_release);
    }

    // CANCEL
    btn = win_register_button(prfwin,
        263,
        450,
        ginfo2[PREFERENCES_WINDOW_FRM_LITTLE_RED_BUTTON_UP].width,
        ginfo2[PREFERENCES_WINDOW_FRM_LITTLE_RED_BUTTON_DOWN].height,
        -1,
        -1,
        -1,
        528,
        prfbmp[PREFERENCES_WINDOW_FRM_LITTLE_RED_BUTTON_UP],
        prfbmp[PREFERENCES_WINDOW_FRM_LITTLE_RED_BUTTON_DOWN],
        NULL,
        BUTTON_FLAG_TRANSPARENT);
    if (btn != -1) {
        win_register_button_sound_func(btn, gsound_red_butt_press, gsound_red_butt_release);
    }

    text_font(101);

    win_draw(prfwin);

    return 0;
}

// 0x4825DC
static void DoThing(int eventCode)
{
    int x;
    int y;
    mouseGetPositionInWindow(prfwin, &x, &y);

    // This preference index also contains out-of-bounds value 19,
    // which is the only preference expressed as checkbox.
    int preferenceIndex = eventCode - 505;

    if (preferenceIndex >= FIRST_PRIMARY_PREF && preferenceIndex <= LAST_PRIMARY_PREF) {
        PreferenceDescription* meta = &(btndat[preferenceIndex]);
        int* valuePtr = meta->valuePtr;
        int value = *valuePtr;
        bool valueChanged = false;

        int v1 = meta->knobX + 23;
        int v2 = meta->knobY + 21;

        if (sqrt(pow((double)x - (double)v1, 2) + pow((double)y - (double)v2, 2)) > 16.0) {
            if (y > meta->knobY) {
                int v14 = meta->knobY + bglby[0];
                if (y >= v14 && y <= v14 + text_height()) {
                    if (x >= meta->minX && x <= meta->knobX) {
                        *valuePtr = 0;
                        meta->direction = 0;
                        valueChanged = true;
                    } else {
                        if (meta->valuesCount >= 3 && x >= meta->knobX + bglbx[2] && x <= meta->maxX) {
                            *valuePtr = 2;
                            meta->direction = 0;
                            valueChanged = true;
                        }
                    }
                }
            } else {
                if (x >= meta->knobX + 9 && x <= meta->knobX + 37) {
                    *valuePtr = 1;
                    if (value != 0) {
                        meta->direction = 1;
                    } else {
                        meta->direction = 0;
                    }
                    valueChanged = true;
                }
            }

            if (meta->valuesCount == 4) {
                int v19 = meta->knobY + bglby[3];
                if (y >= v19 && y <= v19 + 2 * text_height() && x >= meta->knobX + bglbx[3] && x <= meta->maxX) {
                    *valuePtr = 3;
                    meta->direction = 1;
                    valueChanged = true;
                }
            }
        } else {
            if (meta->direction != 0) {
                if (value == 0) {
                    meta->direction = 0;
                }
            } else {
                if (value == meta->valuesCount - 1) {
                    meta->direction = 1;
                }
            }

            if (meta->direction != 0) {
                *valuePtr = value - 1;
            } else {
                *valuePtr = value + 1;
            }

            valueChanged = true;
        }

        if (valueChanged) {
            gsound_play_sfx_file("ib3p1xx1");
            block_for_tocks(70);
            gsound_play_sfx_file("ib3lu1x1");
            UpdateThing(preferenceIndex);
            win_draw(prfwin);
            changed = true;
            return;
        }
    } else if (preferenceIndex >= FIRST_SECONDARY_PREF && preferenceIndex <= LAST_SECONDARY_PREF) {
        PreferenceDescription* meta = &(btndat[preferenceIndex]);
        int* valuePtr = meta->valuePtr;
        int value = *valuePtr;
        bool valueChanged = false;

        int v1 = meta->knobX + 11;
        int v2 = meta->knobY + 12;

        if (sqrt(pow((double)x - (double)v1, 2) + pow((double)y - (double)v2, 2)) > 10.0) {
            int v23 = meta->knobY - 5;
            if (y >= v23 && y <= v23 + text_height() + 2) {
                if (x >= meta->minX && x <= meta->knobX) {
                    *valuePtr = preferenceIndex == PREF_COMBAT_MESSAGES ? 1 : 0;
                    valueChanged = true;
                } else if (x >= meta->knobX + 22.0 && x <= meta->maxX) {
                    *valuePtr = preferenceIndex == PREF_COMBAT_MESSAGES ? 0 : 1;
                    valueChanged = true;
                }
            }
        } else {
            *valuePtr ^= 1;
            valueChanged = true;
        }

        if (valueChanged) {
            gsound_play_sfx_file("ib2p1xx1");
            block_for_tocks(70);
            gsound_play_sfx_file("ib2lu1x1");
            UpdateThing(preferenceIndex);
            win_draw(prfwin);
            changed = true;
            return;
        }
    } else if (preferenceIndex >= FIRST_RANGE_PREF && preferenceIndex <= LAST_RANGE_PREF) {
        PreferenceDescription* meta = &(btndat[preferenceIndex]);
        int* valuePtr = meta->valuePtr;

        gsound_play_sfx_file("ib1p1xx1");

        double value;
        switch (preferenceIndex) {
        case PREF_TEXT_BASE_DELAY:
            value = 6.0 - text_delay + 1.0;
            break;
        case PREF_BRIGHTNESS:
            value = gamma_value;
            break;
        case PREF_MOUSE_SENSITIVIY:
            value = mouse_sens;
            break;
        default:
            value = *valuePtr;
            break;
        }

        int knobX = (int)(219.0 / (meta->maxValue - meta->minValue));
        int v31 = (int)((value - meta->minValue) * (219.0 / (meta->maxValue - meta->minValue)) + 384.0);
        buf_to_buf(prfbmp[PREFERENCES_WINDOW_FRM_BACKGROUND] + PREFERENCES_WINDOW_WIDTH * meta->knobY + 384, 240, 12, PREFERENCES_WINDOW_WIDTH, prefbuf + PREFERENCES_WINDOW_WIDTH * meta->knobY + 384, PREFERENCES_WINDOW_WIDTH);
        trans_buf_to_buf(prfbmp[PREFERENCES_WINDOW_FRM_KNOB_ON], 21, 12, 21, prefbuf + PREFERENCES_WINDOW_WIDTH * meta->knobY + v31, PREFERENCES_WINDOW_WIDTH);

        win_draw(prfwin);

        int sfxVolumeExample = 0;
        int speechVolumeExample = 0;
        while (true) {
            sharedFpsLimiter.mark();

            get_input();

            int tick = get_time();

            mouseGetPositionInWindow(prfwin, &x, &y);

            if (mouse_get_buttons() & 0x10) {
                gsound_play_sfx_file("ib1lu1x1");
                UpdateThing(preferenceIndex);
                win_draw(prfwin);
                renderPresent();
                changed = true;
                return;
            }

            if (v31 + 14 > x) {
                if (v31 + 6 > x) {
                    v31 = x - 6;
                    if (v31 < 384) {
                        v31 = 384;
                    }
                }
            } else {
                v31 = x - 6;
                if (v31 > 603) {
                    v31 = 603;
                }
            }

            double newValue = ((double)v31 - 384.0) / (219.0 / (meta->maxValue - meta->minValue)) + meta->minValue;

            int v52 = 0;

            switch (preferenceIndex) {
            case PREF_COMBAT_SPEED:
                *meta->valuePtr = (int)newValue;
                break;
            case PREF_TEXT_BASE_DELAY:
                text_delay = 6.0 - newValue + 1.0;
                break;
            case PREF_MASTER_VOLUME:
                *meta->valuePtr = (int)newValue;
                gsound_set_master_volume(master_volume);
                v52 = 1;
                break;
            case PREF_MUSIC_VOLUME:
                *meta->valuePtr = (int)newValue;
                gsound_background_volume_set(music_volume);
                v52 = 1;
                break;
            case PREF_SFX_VOLUME:
                *meta->valuePtr = (int)newValue;
                gsound_set_sfx_volume(sndfx_volume);
                v52 = 1;
                if (sfxVolumeExample == 0) {
                    gsound_play_sfx_file("butin1");
                    sfxVolumeExample = 7;
                } else {
                    sfxVolumeExample--;
                }
                break;
            case PREF_SPEECH_VOLUME:
                *meta->valuePtr = (int)newValue;
                gsound_speech_volume_set(speech_volume);
                v52 = 1;
                if (speechVolumeExample == 0) {
                    gsound_speech_play("narrator\\options", 12, 13, 15);
                    speechVolumeExample = 40;
                } else {
                    speechVolumeExample--;
                }
                break;
            case PREF_BRIGHTNESS:
                gamma_value = newValue;
                colorGamma(newValue);
                break;
            case PREF_MOUSE_SENSITIVIY:
                mouse_sens = newValue;
                break;
            }

            if (v52) {
                int off = PREFERENCES_WINDOW_WIDTH * (meta->knobY - 12) + 384;
                buf_to_buf(prfbmp[PREFERENCES_WINDOW_FRM_BACKGROUND] + off, 240, 24, PREFERENCES_WINDOW_WIDTH, prefbuf + off, PREFERENCES_WINDOW_WIDTH);

                for (int optionIndex = 0; optionIndex < meta->valuesCount; optionIndex++) {
                    const char* str = getmsg(&optn_msgfl, &optnmesg, meta->labelIds[optionIndex]);

                    int x;
                    switch (optionIndex) {
                    case 0:
                        // 0x4926AA
                        x = 384;
                        // TODO: Incomplete.
                        break;
                    case 1:
                        // 0x4926F3
                        switch (meta->valuesCount) {
                        case 2:
                            x = 624 - text_width(str);
                            break;
                        case 3:
                            // This code path does not use floating-point arithmetic
                            x = 504 - text_width(str) / 2 - 2;
                            break;
                        case 4:
                            // Uses floating-point arithmetic
                            x = 444 + text_width(str) / 2 - 8;
                            break;
                        }
                        break;
                    case 2:
                        // 0x492766
                        switch (meta->valuesCount) {
                        case 3:
                            x = 624 - text_width(str);
                            break;
                        case 4:
                            // Uses floating-point arithmetic
                            x = 564 - text_width(str) - 4;
                            break;
                        }
                        break;
                    case 3:
                        // 0x49279E
                        x = 624 - text_width(str);
                        break;
                    }
                    text_to_buf(prefbuf + PREFERENCES_WINDOW_WIDTH * (meta->knobY - 12) + x, str, PREFERENCES_WINDOW_WIDTH, PREFERENCES_WINDOW_WIDTH, colorTable[18979]);
                }
            } else {
                int off = PREFERENCES_WINDOW_WIDTH * meta->knobY + 384;
                buf_to_buf(prfbmp[PREFERENCES_WINDOW_FRM_BACKGROUND] + off, 240, 12, PREFERENCES_WINDOW_WIDTH, prefbuf + off, PREFERENCES_WINDOW_WIDTH);
            }

            trans_buf_to_buf(prfbmp[PREFERENCES_WINDOW_FRM_KNOB_ON], 21, 12, 21, prefbuf + PREFERENCES_WINDOW_WIDTH * meta->knobY + v31, PREFERENCES_WINDOW_WIDTH);
            win_draw(prfwin);

            while (elapsed_time(tick) < 35) {
            }

            renderPresent();
            sharedFpsLimiter.throttle();
        }
    } else if (preferenceIndex == 19) {
        player_speedup ^= 1;
    }

    changed = true;
}

// 0x483170
static void UpdateThing(int index)
{
    text_font(101);

    PreferenceDescription* meta = &(btndat[index]);

    if (index >= FIRST_PRIMARY_PREF && index <= LAST_PRIMARY_PREF) {
        // 0x4812FC
        static const int offsets[PRIMARY_PREF_COUNT] = {
            66, // game difficulty
            143, // combat difficulty
            222, // violence level
            304, // target highlight
            382, // combat looks
        };

        int primaryOptionIndex = index - FIRST_PRIMARY_PREF;

        if (primaryOptionIndex == PREF_RUNNING_BURNING_GUY && !game_global_vars[GVAR_RUNNING_BURNING_GUY]) {
            return;
        }

        buf_to_buf(prfbmp[PREFERENCES_WINDOW_FRM_BACKGROUND] + 640 * offsets[primaryOptionIndex] + 23, 160, 54, 640, prefbuf + 640 * offsets[primaryOptionIndex] + 23, 640);

        for (int valueIndex = 0; valueIndex < meta->valuesCount; valueIndex++) {
            const char* text = getmsg(&optn_msgfl, &optnmesg, meta->labelIds[valueIndex]);

            char copy[100]; // TODO: Size is probably wrong.
            strcpy(copy, text);

            int x = meta->knobX + bglbx[valueIndex];
            int len = text_width(copy);
            switch (valueIndex) {
            case 0:
                x -= text_width(copy);
                meta->minX = x;
                break;
            case 1:
                x -= len / 2;
                meta->maxX = x + len;
                break;
            case 2:
            case 3:
                meta->maxX = x + len;
                break;
            }

            char* p = copy;
            while (*p != '\0' && *p != ' ') {
                p++;
            }

            int y = meta->knobY + bglby[valueIndex];
            const char* s;
            if (*p != '\0') {
                *p = '\0';
                text_to_buf(prefbuf + 640 * y + x, copy, 640, 640, colorTable[18979]);
                s = p + 1;
                y += text_height();
            } else {
                s = copy;
            }

            text_to_buf(prefbuf + 640 * y + x, s, 640, 640, colorTable[18979]);
        }

        int value = *(meta->valuePtr);
        trans_buf_to_buf(prfbmp[PREFERENCES_WINDOW_FRM_PRIMARY_SWITCH] + (46 * 47) * value, 46, 47, 46, prefbuf + 640 * meta->knobY + meta->knobX, 640);
    } else if (index >= FIRST_SECONDARY_PREF && index <= LAST_SECONDARY_PREF) {
        // 0x481310
        static const int offsets[SECONDARY_PREF_COUNT] = {
            66, // combat messages
            133, // combat taunts
            200, // language filter
            264, // running
            331, // subtitles
            397, // item highlight
        };

        int secondaryOptionIndex = index - FIRST_SECONDARY_PREF;

        buf_to_buf(prfbmp[PREFERENCES_WINDOW_FRM_BACKGROUND] + 640 * offsets[secondaryOptionIndex] + 251, 113, 34, 640, prefbuf + 640 * offsets[secondaryOptionIndex] + 251, 640);

        // Secondary options are booleans, so it's index is also it's value.
        for (int value = 0; value < 2; value++) {
            const char* text = getmsg(&optn_msgfl, &optnmesg, meta->labelIds[value]);

            int x;
            if (value) {
                x = meta->knobX + smlbx[value];
                meta->maxX = x + text_width(text);
            } else {
                x = meta->knobX + smlbx[value] - text_width(text);
                meta->minX = x;
            }
            text_to_buf(prefbuf + 640 * (meta->knobY - 5) + x, text, 640, 640, colorTable[18979]);
        }

        int value = *(meta->valuePtr);
        if (index == PREF_COMBAT_MESSAGES) {
            value ^= 1;
        }
        trans_buf_to_buf(prfbmp[PREFERENCES_WINDOW_FRM_SECONDARY_SWITCH] + (22 * 25) * value, 22, 25, 22, prefbuf + 640 * meta->knobY + meta->knobX, 640);
    } else if (index >= FIRST_RANGE_PREF && index <= LAST_RANGE_PREF) {
        buf_to_buf(prfbmp[PREFERENCES_WINDOW_FRM_BACKGROUND] + 640 * (meta->knobY - 12) + 384, 240, 24, 640, prefbuf + 640 * (meta->knobY - 12) + 384, 640);
        switch (index) {
        case PREF_COMBAT_SPEED:
            if (1) {
                double value = *meta->valuePtr;
                value = std::clamp(value, 0.0, 50.0);

                int x = (int)((value - meta->minValue) * 219.0 / (meta->maxValue - meta->minValue) + 384.0);
                trans_buf_to_buf(prfbmp[PREFERENCES_WINDOW_FRM_KNOB_OFF], 21, 12, 21, prefbuf + 640 * meta->knobY + x, 640);
            }
            break;
        case PREF_TEXT_BASE_DELAY:
            if (1) {
                text_delay = std::clamp(text_delay, 1.0, 6.0);

                int x = (int)((6.0 - text_delay) * 43.8 + 384.0);
                trans_buf_to_buf(prfbmp[PREFERENCES_WINDOW_FRM_KNOB_OFF], 21, 12, 21, prefbuf + 640 * meta->knobY + x, 640);

                double value = (text_delay - 1.0) * 0.2 * 2.0;
                value = std::clamp(value, 0.0, 2.0);

                text_object_set_base_delay(text_delay);
                text_object_set_line_delay(value);
            }
            break;
        case PREF_MASTER_VOLUME:
        case PREF_MUSIC_VOLUME:
        case PREF_SFX_VOLUME:
        case PREF_SPEECH_VOLUME:
            if (1) {
                double value = *meta->valuePtr;
                value = std::clamp(value, meta->minValue, meta->maxValue);

                int x = (int)((value - meta->minValue) * 219.0 / (meta->maxValue - meta->minValue) + 384.0);
                trans_buf_to_buf(prfbmp[PREFERENCES_WINDOW_FRM_KNOB_OFF], 21, 12, 21, prefbuf + 640 * meta->knobY + x, 640);

                switch (index) {
                case PREF_MASTER_VOLUME:
                    gsound_set_master_volume(master_volume);
                    break;
                case PREF_MUSIC_VOLUME:
                    gsound_background_volume_set(music_volume);
                    break;
                case PREF_SFX_VOLUME:
                    gsound_set_sfx_volume(sndfx_volume);
                    break;
                case PREF_SPEECH_VOLUME:
                    gsound_speech_volume_set(speech_volume);
                    break;
                }
            }
            break;
        case PREF_BRIGHTNESS:
            if (1) {
                gamma_value = std::clamp(gamma_value, 1.0, 1.17999267578125);

                int x = (int)((gamma_value - meta->minValue) * (219.0 / (meta->maxValue - meta->minValue)) + 384.0);
                trans_buf_to_buf(prfbmp[PREFERENCES_WINDOW_FRM_KNOB_OFF], 21, 12, 21, prefbuf + 640 * meta->knobY + x, 640);

                colorGamma(gamma_value);
            }
            break;
        case PREF_MOUSE_SENSITIVIY:
            if (1) {
                mouse_sens = std::clamp(mouse_sens, 1.0, 2.5);

                int x = (int)((mouse_sens - meta->minValue) * (219.0 / (meta->maxValue - meta->minValue)) + 384.0);
                trans_buf_to_buf(prfbmp[PREFERENCES_WINDOW_FRM_KNOB_OFF], 21, 12, 21, prefbuf + 640 * meta->knobY + x, 640);

                mouse_set_sensitivity(mouse_sens);
            }
            break;
        }

        for (int optionIndex = 0; optionIndex < meta->valuesCount; optionIndex++) {
            const char* str = getmsg(&optn_msgfl, &optnmesg, meta->labelIds[optionIndex]);

            int x;
            switch (optionIndex) {
            case 0:
                // 0x4926AA
                x = 384;
                // TODO: Incomplete.
                break;
            case 1:
                // 0x4926F3
                switch (meta->valuesCount) {
                case 2:
                    x = 624 - text_width(str);
                    break;
                case 3:
                    // This code path does not use floating-point arithmetic
                    x = 504 - text_width(str) / 2 - 2;
                    break;
                case 4:
                    // Uses floating-point arithmetic
                    x = 444 + text_width(str) / 2 - 8;
                    break;
                }
                break;
            case 2:
                // 0x492766
                switch (meta->valuesCount) {
                case 3:
                    x = 624 - text_width(str);
                    break;
                case 4:
                    // Uses floating-point arithmetic
                    x = 564 - text_width(str) - 4;
                    break;
                }
                break;
            case 3:
                // 0x49279E
                x = 624 - text_width(str);
                break;
            }
            text_to_buf(prefbuf + 640 * (meta->knobY - 12) + x, str, 640, 640, colorTable[18979]);
        }
    } else {
        // return false;
    }

    // TODO: Incomplete.

    // return true;
}

// 0x483F28
static int PrefEnd()
{
    if (changed) {
        SavePrefs(1);
        JustUpdate();
        combat_highlight_change();
    }

    win_delete(prfwin);

    for (int index = 0; index < PREFERENCES_WINDOW_FRM_COUNT; index++) {
        art_ptr_unlock(grphkey2[index]);
    }

    return 0;
}

// 0x483F70
int init_options_menu()
{
    for (int index = 0; index < 11; index++) {
        btndat[index].direction = 0;
    }

    SetSystemPrefs();

    InitGreyTable(0, 255);

    return 0;
}

// 0x483F9C
void IncGamma()
{
    gamma_value = GAMMA_MIN;
    config_get_double(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_BRIGHTNESS_KEY, &gamma_value);

    if (gamma_value < GAMMA_MAX) {
        gamma_value += GAMMA_STEP;

        if (gamma_value >= GAMMA_MIN) {
            if (gamma_value > GAMMA_MAX) {
                gamma_value = GAMMA_MAX;
            }
        } else {
            gamma_value = GAMMA_MIN;
        }

        colorGamma(gamma_value);

        config_set_double(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_BRIGHTNESS_KEY, gamma_value);

        gconfig_save();
    }
}

// 0x48407C
void DecGamma()
{
    gamma_value = GAMMA_MIN;
    config_get_double(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_BRIGHTNESS_KEY, &gamma_value);

    if (gamma_value > GAMMA_MIN) {
        gamma_value -= GAMMA_STEP;

        if (gamma_value >= GAMMA_MIN) {
            if (gamma_value > GAMMA_MAX) {
                gamma_value = GAMMA_MAX;
            }
        } else {
            gamma_value = GAMMA_MIN;
        }

        colorGamma(gamma_value);

        config_set_double(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_BRIGHTNESS_KEY, gamma_value);

        gconfig_save();
    }
}

// 0x484158
static void SetSystemPrefs()
{
    SetDefaults(false);

    config_get_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_GAME_DIFFICULTY_KEY, &game_difficulty);
    config_get_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_COMBAT_DIFFICULTY_KEY, &combat_difficulty);
    config_get_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_VIOLENCE_LEVEL_KEY, &violence_level);
    config_get_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_TARGET_HIGHLIGHT_KEY, &target_highlight);
    config_get_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_COMBAT_MESSAGES_KEY, &combat_messages);
    config_get_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_RUNNING_BURNING_GUY_KEY, &running_burning_guy);
    config_get_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_COMBAT_TAUNTS_KEY, &combat_taunts);
    config_get_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_LANGUAGE_FILTER_KEY, &language_filter);
    config_get_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_RUNNING_KEY, &prf_running);
    config_get_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_SUBTITLES_KEY, &subtitles);
    config_get_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_ITEM_HIGHLIGHT_KEY, &item_highlight);
    config_get_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_COMBAT_SPEED_KEY, &combat_speed);
    config_get_double(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_TEXT_BASE_DELAY_KEY, &text_delay);
    config_get_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_PLAYER_SPEEDUP_KEY, &player_speedup);
    config_get_value(&game_config, GAME_CONFIG_SOUND_KEY, GAME_CONFIG_MASTER_VOLUME_KEY, &master_volume);
    config_get_value(&game_config, GAME_CONFIG_SOUND_KEY, GAME_CONFIG_MUSIC_VOLUME_KEY, &music_volume);
    config_get_value(&game_config, GAME_CONFIG_SOUND_KEY, GAME_CONFIG_SNDFX_VOLUME_KEY, &sndfx_volume);
    config_get_value(&game_config, GAME_CONFIG_SOUND_KEY, GAME_CONFIG_SPEECH_VOLUME_KEY, &speech_volume);
    config_get_double(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_BRIGHTNESS_KEY, &gamma_value);
    config_get_double(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_MOUSE_SENSITIVITY_KEY, &mouse_sens);

    JustUpdate();
}

// 0x484360
static int SavePrefs(bool save)
{
    config_set_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_GAME_DIFFICULTY_KEY, game_difficulty);
    config_set_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_COMBAT_DIFFICULTY_KEY, combat_difficulty);
    config_set_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_VIOLENCE_LEVEL_KEY, violence_level);
    config_set_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_TARGET_HIGHLIGHT_KEY, target_highlight);
    config_set_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_COMBAT_MESSAGES_KEY, combat_messages);
    config_set_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_COMBAT_LOOKS_KEY, running_burning_guy);
    config_set_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_COMBAT_TAUNTS_KEY, combat_taunts);
    config_set_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_LANGUAGE_FILTER_KEY, language_filter);
    config_set_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_RUNNING_KEY, prf_running);
    config_set_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_SUBTITLES_KEY, subtitles);
    config_set_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_ITEM_HIGHLIGHT_KEY, item_highlight);
    config_set_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_COMBAT_SPEED_KEY, combat_speed);
    config_set_double(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_TEXT_BASE_DELAY_KEY, text_delay);

    double textLineDelay = (text_delay - 1.0) / 5.0 * 2.0;
    if (textLineDelay >= 0.0) {
        if (textLineDelay > 2.0) {
            textLineDelay = 2.0;
        }

        config_set_double(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_TEXT_LINE_DELAY_KEY, textLineDelay);
    } else {
        config_set_double(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_TEXT_LINE_DELAY_KEY, 0.0);
    }

    config_set_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_PLAYER_SPEEDUP_KEY, player_speedup);
    config_set_value(&game_config, GAME_CONFIG_SOUND_KEY, GAME_CONFIG_MASTER_VOLUME_KEY, master_volume);
    config_set_value(&game_config, GAME_CONFIG_SOUND_KEY, GAME_CONFIG_MUSIC_VOLUME_KEY, music_volume);
    config_set_value(&game_config, GAME_CONFIG_SOUND_KEY, GAME_CONFIG_SNDFX_VOLUME_KEY, sndfx_volume);
    config_set_value(&game_config, GAME_CONFIG_SOUND_KEY, GAME_CONFIG_SPEECH_VOLUME_KEY, speech_volume);

    config_set_double(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_BRIGHTNESS_KEY, gamma_value);
    config_set_double(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_MOUSE_SENSITIVITY_KEY, mouse_sens);

    if (save) {
        gconfig_save();
    }

    return 0;
}

// 0x48460C
static void SetDefaults(bool a1)
{
    combat_difficulty = COMBAT_DIFFICULTY_NORMAL;
    violence_level = VIOLENCE_LEVEL_MAXIMUM_BLOOD;
    target_highlight = TARGET_HIGHLIGHT_TARGETING_ONLY;
    combat_messages = 1;
    running_burning_guy = 1;
    combat_taunts = 1;
    prf_running = 0;
    subtitles = 0;
    item_highlight = 1;
    combat_speed = 0;
    player_speedup = 0;
    text_delay = 3.5;
    gamma_value = 1.0;
    mouse_sens = 1.0;
    game_difficulty = 1;
    language_filter = 0;
    master_volume = 22281;
    music_volume = 22281;
    sndfx_volume = 22281;
    speech_volume = 22281;

    if (a1) {
        for (int index = 0; index < PREF_COUNT; index++) {
            UpdateThing(index);
        }
        win_set_button_rest_state(plyrspdbid, player_speedup, 0);
        win_draw(prfwin);
        changed = true;
    }
}

// 0x484704
static void SaveSettings()
{
    settings_backup[PREF_GAME_DIFFICULTY] = game_difficulty;
    settings_backup[PREF_COMBAT_DIFFICULTY] = combat_difficulty;
    settings_backup[PREF_VIOLENCE_LEVEL] = violence_level;
    settings_backup[PREF_TARGET_HIGHLIGHT] = target_highlight;
    settings_backup[PREF_RUNNING_BURNING_GUY] = running_burning_guy;
    settings_backup[PREF_COMBAT_MESSAGES] = combat_messages;
    settings_backup[PREF_COMBAT_TAUNTS] = combat_taunts;
    settings_backup[PREF_LANGUAGE_FILTER] = language_filter;
    settings_backup[PREF_RUNNING] = prf_running;
    settings_backup[PREF_SUBTITLES] = subtitles;
    settings_backup[PREF_ITEM_HIGHLIGHT] = item_highlight;
    settings_backup[PREF_COMBAT_SPEED] = combat_speed;
    settings_backup[PREF_TEXT_BASE_DELAY] = player_speedup;
    settings_backup[PREF_MASTER_VOLUME] = master_volume;
    text_delay_back = text_delay;
    settings_backup[PREF_MUSIC_VOLUME] = music_volume;
    gamma_value_back = gamma_value;
    settings_backup[PREF_SFX_VOLUME] = sndfx_volume;
    mouse_sens_back = mouse_sens;
    settings_backup[PREF_SPEECH_VOLUME] = speech_volume;
}

// 0x4847D4
static void RestoreSettings()
{
    game_difficulty = settings_backup[PREF_GAME_DIFFICULTY];
    combat_difficulty = settings_backup[PREF_COMBAT_DIFFICULTY];
    violence_level = settings_backup[PREF_VIOLENCE_LEVEL];
    target_highlight = settings_backup[PREF_TARGET_HIGHLIGHT];
    running_burning_guy = settings_backup[PREF_RUNNING_BURNING_GUY];
    combat_messages = settings_backup[PREF_COMBAT_MESSAGES];
    combat_taunts = settings_backup[PREF_COMBAT_TAUNTS];
    language_filter = settings_backup[PREF_LANGUAGE_FILTER];
    prf_running = settings_backup[PREF_RUNNING];
    subtitles = settings_backup[PREF_SUBTITLES];
    item_highlight = settings_backup[PREF_ITEM_HIGHLIGHT];
    combat_speed = settings_backup[PREF_COMBAT_SPEED];
    player_speedup = settings_backup[PREF_TEXT_BASE_DELAY];
    master_volume = settings_backup[PREF_MASTER_VOLUME];
    text_delay = text_delay_back;
    music_volume = settings_backup[PREF_MUSIC_VOLUME];
    gamma_value = gamma_value_back;
    sndfx_volume = settings_backup[PREF_SFX_VOLUME];
    mouse_sens = mouse_sens_back;
    speech_volume = settings_backup[PREF_SPEECH_VOLUME];

    JustUpdate();
}

// 0x4848A4
static void JustUpdate()
{
    game_difficulty = std::clamp(game_difficulty, 0, 2);
    combat_difficulty = std::clamp(combat_difficulty, 0, 2);
    violence_level = std::clamp(violence_level, 0, 3);
    target_highlight = std::clamp(target_highlight, 0, 2);
    combat_messages = std::clamp(combat_messages, 0, 1);
    running_burning_guy = std::clamp(running_burning_guy, 0, 1);
    combat_taunts = std::clamp(combat_taunts, 0, 1);
    language_filter = std::clamp(language_filter, 0, 1);
    prf_running = std::clamp(prf_running, 0, 1);
    subtitles = std::clamp(subtitles, 0, 1);
    item_highlight = std::clamp(item_highlight, 0, 1);
    combat_speed = std::clamp(combat_speed, 0, 50);
    player_speedup = std::clamp(player_speedup, 0, 1);
    text_delay = std::clamp(text_delay, 1.0, 6.0);
    master_volume = std::clamp(master_volume, 0, VOLUME_MAX);
    music_volume = std::clamp(music_volume, 0, VOLUME_MAX);
    sndfx_volume = std::clamp(sndfx_volume, 0, VOLUME_MAX);
    speech_volume = std::clamp(speech_volume, 0, VOLUME_MAX);
    gamma_value = std::clamp(gamma_value, 1.0, 1.17999267578125);
    mouse_sens = std::clamp(mouse_sens, 1.0, 2.5);

    text_object_set_base_delay(text_delay);
    gmouse_3d_synch_item_highlight();

    double textLineDelay = (text_delay + (-1.0)) * 0.2 * 2.0;
    textLineDelay = std::clamp(textLineDelay, 0.0, 2.0);

    text_object_set_line_delay(textLineDelay);
    combatai_refresh_messages();
    scr_message_free();
    gsound_set_master_volume(master_volume);
    gsound_background_volume_set(music_volume);
    gsound_set_sfx_volume(sndfx_volume);
    gsound_speech_volume_set(speech_volume);
    mouse_set_sensitivity(mouse_sens);
    colorGamma(gamma_value);
}

// 0x4848C8
int save_options(DB_FILE* stream)
{
    float textBaseDelay = (float)text_delay;
    float brightness = (float)gamma_value;
    float mouseSensitivity = (float)mouse_sens;

    if (db_fwriteInt(stream, game_difficulty) == -1) goto err;
    if (db_fwriteInt(stream, combat_difficulty) == -1) goto err;
    if (db_fwriteInt(stream, violence_level) == -1) goto err;
    if (db_fwriteInt(stream, target_highlight) == -1) goto err;
    if (db_fwriteInt(stream, running_burning_guy) == -1) goto err;
    if (db_fwriteInt(stream, combat_messages) == -1) goto err;
    if (db_fwriteInt(stream, combat_taunts) == -1) goto err;
    if (db_fwriteInt(stream, language_filter) == -1) goto err;
    if (db_fwriteInt(stream, prf_running) == -1) goto err;
    if (db_fwriteInt(stream, subtitles) == -1) goto err;
    if (db_fwriteInt(stream, item_highlight) == -1) goto err;
    if (db_fwriteInt(stream, combat_speed) == -1) goto err;
    if (db_fwriteInt(stream, player_speedup) == -1) goto err;
    if (db_fwriteFloat(stream, textBaseDelay) == -1) goto err;
    if (db_fwriteInt(stream, master_volume) == -1) goto err;
    if (db_fwriteInt(stream, music_volume) == -1) goto err;
    if (db_fwriteInt(stream, sndfx_volume) == -1) goto err;
    if (db_fwriteInt(stream, speech_volume) == -1) goto err;
    if (db_fwriteFloat(stream, brightness) == -1) goto err;
    if (db_fwriteFloat(stream, mouseSensitivity) == -1) goto err;

    return 0;

err:

    debug_printf("\nOPTION MENU: Error save option data!\n");

    return -1;
}

// 0x484AAC
int load_options(DB_FILE* stream)
{
    float textBaseDelay;
    float brightness;
    float mouseSensitivity;

    SetDefaults(false);

    if (db_freadInt(stream, &game_difficulty) == -1) goto err;
    if (db_freadInt(stream, &combat_difficulty) == -1) goto err;
    if (db_freadInt(stream, &violence_level) == -1) goto err;
    if (db_freadInt(stream, &target_highlight) == -1) goto err;
    if (db_freadInt(stream, &running_burning_guy) == -1) goto err;
    if (db_freadInt(stream, &combat_messages) == -1) goto err;
    if (db_freadInt(stream, &combat_taunts) == -1) goto err;
    if (db_freadInt(stream, &language_filter) == -1) goto err;
    if (db_freadInt(stream, &prf_running) == -1) goto err;
    if (db_freadInt(stream, &subtitles) == -1) goto err;
    if (db_freadInt(stream, &item_highlight) == -1) goto err;
    if (db_freadInt(stream, &combat_speed) == -1) goto err;
    if (db_freadInt(stream, &player_speedup) == -1) goto err;
    if (db_freadFloat(stream, &textBaseDelay) == -1) goto err;
    if (db_freadInt(stream, &master_volume) == -1) goto err;
    if (db_freadInt(stream, &music_volume) == -1) goto err;
    if (db_freadInt(stream, &sndfx_volume) == -1) goto err;
    if (db_freadInt(stream, &speech_volume) == -1) goto err;
    if (db_freadFloat(stream, &brightness) == -1) goto err;
    if (db_freadFloat(stream, &mouseSensitivity) == -1) goto err;

    gamma_value = brightness;
    mouse_sens = mouseSensitivity;
    text_delay = textBaseDelay;

    JustUpdate();
    SavePrefs(0);

    return 0;

err:

    debug_printf("\nOPTION MENU: Error loading option data!, using defaults.\n");

    SetDefaults(false);
    JustUpdate();
    SavePrefs(0);

    return -1;
}

} // namespace fallout
