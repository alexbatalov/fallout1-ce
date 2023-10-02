#include "game/game.h"

#include <stdio.h>
#include <string.h>

#include "game/actions.h"
#include "game/anim.h"
#include "game/automap.h"
#include "game/bmpdlog.h"
#include "game/combat.h"
#include "game/combatai.h"
#include "game/critter.h"
#include "game/cycle.h"
#include "game/display.h"
#include "game/editor.h"
#include "game/endgame.h"
#include "game/fontmgr.h"
#include "game/gconfig.h"
#include "game/gdialog.h"
#include "game/gmemory.h"
#include "game/gmouse.h"
#include "game/gmovie.h"
#include "game/gsound.h"
#include "game/intface.h"
#include "game/inventry.h"
#include "game/item.h"
#include "game/loadsave.h"
#include "game/map.h"
#include "game/moviefx.h"
#include "game/object.h"
#include "game/options.h"
#include "game/palette.h"
#include "game/party.h"
#include "game/perk.h"
#include "game/pipboy.h"
#include "game/proto.h"
#include "game/queue.h"
#include "game/roll.h"
#include "game/scripts.h"
#include "game/select.h"
#include "game/skill.h"
#include "game/skilldex.h"
#include "game/stat.h"
#include "game/tile.h"
#include "game/trait.h"
#include "game/version.h"
#include "game/worldmap.h"
#include "int/movie.h"
#include "int/window.h"
#include "platform_compat.h"
#include "plib/color/color.h"
#include "plib/gnw/debug.h"
#include "plib/gnw/gnw.h"
#include "plib/gnw/grbuf.h"
#include "plib/gnw/input.h"
#include "plib/gnw/memory.h"
#include "plib/gnw/svga.h"
#include "plib/gnw/text.h"

namespace fallout {

#define HELP_SCREEN_WIDTH 640
#define HELP_SCREEN_HEIGHT 480

#define SPLASH_WIDTH 640
#define SPLASH_HEIGHT 480
#define SPLASH_COUNT 10

static int game_screendump(int width, int height, unsigned char* buffer, unsigned char* palette);
static void game_unload_info();
static void game_help();
static int game_init_databases();
static void game_splash_screen();

// TODO: Remove.
// 0x4F190C
char version_build_time[] = VERSION_BUILD_TIME;

// 0x504F90
static FontMgr alias_mgr = {
    100,
    110,
    FMtext_font,
    FMtext_to_buf,
    FMtext_height,
    FMtext_width,
    FMtext_char_width,
    FMtext_mono_width,
    FMtext_spacing,
    FMtext_size,
    FMtext_max,
};

// 0x504FBC
static bool game_ui_disabled = false;

// 0x504FC0
static int game_state_cur = GAME_STATE_0;

// 0x504FC4
static bool game_in_mapper = false;

// 0x504FC8
int* game_global_vars = NULL;

// 0x504FCC
int num_game_global_vars = 0;

// 0x504FD0
const char* msg_path = "game\\";

// 0x504FD4
int game_user_wants_to_quit = 0;

// misc.msg
//
// 0x58CC10
MessageList misc_message_file;

// 0x58CC18
DB_DATABASE* master_db_handle;

// 0x58CC1C
DB_DATABASE* critter_db_handle;

// 0x43B080
int game_init(const char* windowTitle, bool isMapper, int font, int flags, int argc, char** argv)
{
    char path[COMPAT_MAX_PATH];

    if (gmemory_init() == -1) {
        return -1;
    }

    gconfig_init(isMapper, argc, argv);

    game_in_mapper = isMapper;

    if (game_init_databases() == -1) {
        gconfig_exit(false);
        return -1;
    }

    win_set_minimized_title(windowTitle);

    VideoOptions video_options;
    video_options.width = 640;
    video_options.height = 480;
    video_options.fullscreen = true;
    video_options.scale = 1;

    Config resolutionConfig;
    if (config_init(&resolutionConfig)) {
        if (config_load(&resolutionConfig, "f1_res.ini", false)) {
            int screenWidth;
            if (config_get_value(&resolutionConfig, "MAIN", "SCR_WIDTH", &screenWidth)) {
                video_options.width = std::max(screenWidth, 640);
            }

            int screenHeight;
            if (config_get_value(&resolutionConfig, "MAIN", "SCR_HEIGHT", &screenHeight)) {
                video_options.height = std::max(screenHeight, 480);
            }

            bool windowed;
            if (configGetBool(&resolutionConfig, "MAIN", "WINDOWED", &windowed)) {
                video_options.fullscreen = !windowed;
            }

            int scaleValue;
            if (config_get_value(&resolutionConfig, "MAIN", "SCALE_2X", &scaleValue)) {
                video_options.scale = scaleValue + 1;
                video_options.width /= video_options.scale;
                video_options.height /= video_options.scale;
            }
        }
        config_exit(&resolutionConfig);
    }

    initWindow(&video_options, flags);
    palette_init();

    if (!game_in_mapper) {
        game_splash_screen();
    }

    FMInit();
    text_add_manager(&alias_mgr);
    text_font(font);

    register_screendump(KEY_F12, game_screendump);
    register_pause(-1, NULL);

    tile_disable_refresh();

    // FIXME: Meaningless, `patches` path used by this function call is not
    // properly initialized yet.
    KillOldMaps();

    roll_init();
    init_message();
    skill_init();
    stat_init();
    perk_init();
    trait_init();
    item_init();
    queue_init();
    critter_init();
    combat_ai_init();
    inven_reset_dude();

    if (gsound_init() != 0) {
        debug_printf("Sound initialization failed.\n");
    }

    debug_printf(">gsound_init\t");

    initMovie();
    debug_printf(">initMovie\t\t");

    if (gmovie_init() != 0) {
        debug_printf("Failed on gmovie_init\n");
        return -1;
    }

    debug_printf(">gmovie_init\t");

    if (moviefx_init() != 0) {
        debug_printf("Failed on moviefx_init\n");
        return -1;
    }

    debug_printf(">moviefx_init\t");

    if (iso_init() != 0) {
        debug_printf("Failed on iso_init\n");
        return -1;
    }

    debug_printf(">iso_init\t");

    if (gmouse_init() != 0) {
        debug_printf("Failed on gmouse_init\n");
        return -1;
    }

    debug_printf(">gmouse_init\t");

    if (proto_init() != 0) {
        debug_printf("Failed on proto_init\n");
        return -1;
    }

    debug_printf(">proto_init\t");

    anim_init();
    debug_printf(">anim_init\t");

    if (scr_init() != 0) {
        debug_printf("Failed on scr_init\n");
        return -1;
    }

    debug_printf(">scr_init\t");

    if (game_load_info() != 0) {
        debug_printf("Failed on game_load_info\n");
        return -1;
    }

    debug_printf(">game_load_info\t");

    if (scr_game_init() != 0) {
        debug_printf("Failed on scr_game_init\n");
        return -1;
    }

    debug_printf(">scr_game_init\t");

    if (init_world_map() != 0) {
        debug_printf("Failed on init_world_map\n");
        return -1;
    }

    debug_printf(">init_world_map\t");

    CharEditInit();
    debug_printf(">CharEditInit\t");

    pip_init();
    debug_printf(">pip_init\t\t");

    InitLoadSave();
    debug_printf(">InitLoadSave\t");

    if (gdialog_init() != 0) {
        debug_printf("Failed on gdialog_init\n");
        return -1;
    }

    debug_printf(">gdialog_init\t");

    if (combat_init() != 0) {
        debug_printf("Failed on combat_init\n");
        return -1;
    }

    debug_printf(">combat_init\t");

    if (automap_init() != 0) {
        debug_printf("Failed on automap_init\n");
        return -1;
    }

    debug_printf(">automap_init\t");

    if (!message_init(&misc_message_file)) {
        debug_printf("Failed on message_init\n");
        return -1;
    }

    debug_printf(">message_init\t");

    snprintf(path, sizeof(path), "%s%s", msg_path, "misc.msg");

    if (!message_load(&misc_message_file, path)) {
        debug_printf("Failed on message_load\n");
        return -1;
    }

    debug_printf(">message_load\t");

    if (scr_disable() != 0) {
        debug_printf("Failed on scr_disable\n");
        return -1;
    }

    debug_printf(">scr_disable\t");

    if (init_options_menu() != 0) {
        debug_printf("Failed on init_options_menu\n");
        return -1;
    }

    debug_printf(">init_options_menu\n");

    return 0;
}

// 0x43B5A8
void game_reset()
{
    tile_disable_refresh();
    palette_reset();
    roll_reset();
    skill_reset();
    stat_reset();
    perk_reset();
    trait_reset();
    item_reset();
    queue_reset();
    anim_reset();
    KillOldMaps();
    critter_reset();
    combat_ai_reset();
    inven_reset_dude();
    gsound_reset();
    movieStop();
    moviefx_reset();
    gmovie_reset();
    iso_reset();
    gmouse_reset();
    proto_reset();
    scr_reset();
    game_load_info();
    scr_game_reset();
    init_world_map();
    CharEditInit();
    pip_init();
    ResetLoadSave();
    gdialog_reset();
    combat_reset();
    game_user_wants_to_quit = 0;
    automap_reset();
    init_options_menu();
}

// 0x43B654
void game_exit()
{
    tile_disable_refresh();
    message_exit(&misc_message_file);
    combat_exit();
    gdialog_exit();
    scr_game_exit();

    // NOTE: Uninline.
    game_unload_info();

    scr_exit();
    anim_exit();
    proto_exit();
    gmouse_exit();
    iso_exit();
    moviefx_exit();
    gmovie_exit();
    movieClose();
    gsound_exit();
    combat_ai_exit();
    critter_exit();
    item_exit();
    queue_exit();
    perk_exit();
    stat_exit();
    skill_exit();
    trait_exit();
    roll_exit();
    exit_message();
    automap_exit();
    palette_exit();
    FMExit();
    windowClose();
    db_exit();
    gconfig_exit(true);
}

// 0x43B748
int game_handle_input(int eventCode, bool isInCombatMode)
{
    // NOTE: Uninline.
    if (game_state() == GAME_STATE_5) {
        dialogue_system_enter();
    }

    if (eventCode == -1) {
        if ((mouse_get_buttons() & MOUSE_EVENT_WHEEL) != 0) {
            int wheelX;
            int wheelY;
            mouseGetWheel(&wheelX, &wheelY);

            int dx = 0;
            if (wheelX > 0) {
                dx = 1;
            } else if (wheelX < 0) {
                dx = -1;
            }

            int dy = 0;
            if (wheelY > 0) {
                dy = -1;
            } else if (wheelY < 0) {
                dy = 1;
            }

            map_scroll(dx, dy);
        }
        return 0;
    }

    if (eventCode == -2) {
        int mouseState = mouse_get_buttons();
        int mouseX;
        int mouseY;
        mouse_get_position(&mouseX, &mouseY);

        if ((mouseState & MOUSE_EVENT_LEFT_BUTTON_DOWN) != 0) {
            if ((mouseState & MOUSE_EVENT_LEFT_BUTTON_REPEAT) == 0) {
                if (mouseX == scr_size.ulx || mouseX == scr_size.lrx
                    || mouseY == scr_size.uly || mouseY == scr_size.lry) {
                    gmouse_clicked_on_edge = true;
                } else {
                    gmouse_clicked_on_edge = false;
                }
            }
        } else {
            if ((mouseState & MOUSE_EVENT_LEFT_BUTTON_UP) != 0) {
                gmouse_clicked_on_edge = false;
            }
        }

        gmouse_handle_event(mouseX, mouseY, mouseState);
        return 0;
    }

    if (gmouse_is_scrolling()) {
        return 0;
    }

    switch (eventCode) {
    case -20:
        if (intface_is_enabled()) {
            intface_use_item();
        }
        break;
    case -2:
        if (1) {
            int mouseEvent = mouse_get_buttons();
            int mouseX;
            int mouseY;
            mouse_get_position(&mouseX, &mouseY);

            if ((mouseEvent & MOUSE_EVENT_LEFT_BUTTON_DOWN) != 0) {
                if ((mouseEvent & MOUSE_EVENT_LEFT_BUTTON_REPEAT) == 0) {
                    if (mouseX == scr_size.ulx || mouseX == scr_size.lrx
                        || mouseY == scr_size.uly || mouseY == scr_size.lry) {
                        gmouse_clicked_on_edge = true;
                    } else {
                        gmouse_clicked_on_edge = false;
                    }
                }
            } else {
                if ((mouseEvent & MOUSE_EVENT_LEFT_BUTTON_UP) != 0) {
                    gmouse_clicked_on_edge = false;
                }
            }

            gmouse_handle_event(mouseX, mouseY, mouseEvent);
        }
        break;
    case KEY_CTRL_Q:
    case KEY_CTRL_X:
    case KEY_F10:
        gsound_play_sfx_file("ib1p1xx1");
        game_quit_with_confirm();
        break;
    case KEY_TAB:
        if (intface_is_enabled()
            && keys[SDL_SCANCODE_LALT] == 0
            && keys[SDL_SCANCODE_RALT] == 0) {
            gsound_play_sfx_file("ib1p1xx1");
            automap(true, false);
        }
        break;
    case KEY_CTRL_P:
        gsound_play_sfx_file("ib1p1xx1");
        PauseWindow(false);
        break;
    case KEY_UPPERCASE_A:
    case KEY_LOWERCASE_A:
        if (intface_is_enabled()) {
            if (!isInCombatMode) {
                combat(NULL);
            }
        }
        break;
    case KEY_UPPERCASE_N:
    case KEY_LOWERCASE_N:
        if (intface_is_enabled()) {
            gsound_play_sfx_file("ib1p1xx1");
            intface_toggle_item_state();
        }
        break;
    case KEY_UPPERCASE_M:
    case KEY_LOWERCASE_M:
        gmouse_3d_toggle_mode();
        break;
    case KEY_UPPERCASE_B:
    case KEY_LOWERCASE_B:
        // change active hand
        if (intface_is_enabled()) {
            gsound_play_sfx_file("ib1p1xx1");
            intface_toggle_items(true);
        }
        break;
    case KEY_UPPERCASE_C:
    case KEY_LOWERCASE_C:
        if (intface_is_enabled()) {
            gsound_play_sfx_file("ib1p1xx1");
            bool isoWasEnabled = map_disable_bk_processes();
            editor_design(false);
            if (isoWasEnabled) {
                map_enable_bk_processes();
            }
        }
        break;
    case KEY_UPPERCASE_I:
    case KEY_LOWERCASE_I:
        // open inventory
        if (intface_is_enabled()) {
            gsound_play_sfx_file("ib1p1xx1");
            handle_inventory();
        }
        break;
    case KEY_ESCAPE:
    case KEY_UPPERCASE_O:
    case KEY_LOWERCASE_O:
        // options
        if (intface_is_enabled()) {
            gsound_play_sfx_file("ib1p1xx1");
            do_options();
        }
        break;
    case KEY_UPPERCASE_P:
    case KEY_LOWERCASE_P:
        // pipboy
        if (intface_is_enabled()) {
            if (isInCombatMode) {
                gsound_play_sfx_file("iisxxxx1");

                // Pipboy not available in combat!
                MessageListItem messageListItem;
                char title[128];
                strcpy(title, getmsg(&misc_message_file, &messageListItem, 7));
                dialog_out(title, NULL, 0, 192, 116, colorTable[32328], NULL, colorTable[32328], 0);
            } else {
                gsound_play_sfx_file("ib1p1xx1");
                pipboy(false);
            }
        }
        break;
    case KEY_UPPERCASE_S:
    case KEY_LOWERCASE_S:
        // skilldex
        if (intface_is_enabled()) {
            gsound_play_sfx_file("ib1p1xx1");

            int mode = -1;

            // NOTE: There is an `inc` for this value to build jump table which
            // is not needed.
            int rc = skilldex_select();

            // Remap Skilldex result code to action.
            switch (rc) {
            case SKILLDEX_RC_ERROR:
                debug_printf("\n ** Error calling skilldex_select()! ** \n");
                break;
            case SKILLDEX_RC_SNEAK:
                action_skill_use(SKILL_SNEAK);
                break;
            case SKILLDEX_RC_LOCKPICK:
                mode = GAME_MOUSE_MODE_USE_LOCKPICK;
                break;
            case SKILLDEX_RC_STEAL:
                mode = GAME_MOUSE_MODE_USE_STEAL;
                break;
            case SKILLDEX_RC_TRAPS:
                mode = GAME_MOUSE_MODE_USE_TRAPS;
                break;
            case SKILLDEX_RC_FIRST_AID:
                mode = GAME_MOUSE_MODE_USE_FIRST_AID;
                break;
            case SKILLDEX_RC_DOCTOR:
                mode = GAME_MOUSE_MODE_USE_DOCTOR;
                break;
            case SKILLDEX_RC_SCIENCE:
                mode = GAME_MOUSE_MODE_USE_SCIENCE;
                break;
            case SKILLDEX_RC_REPAIR:
                mode = GAME_MOUSE_MODE_USE_REPAIR;
                break;
            default:
                break;
            }

            if (mode != -1) {
                gmouse_set_cursor(MOUSE_CURSOR_USE_CROSSHAIR);
                gmouse_3d_set_mode(mode);
            }
        }
        break;
    case KEY_UPPERCASE_Z:
    case KEY_LOWERCASE_Z:
        if (intface_is_enabled()) {
            if (isInCombatMode) {
                gsound_play_sfx_file("iisxxxx1");

                // Pipboy not available in combat!
                MessageListItem messageListItem;
                char title[128];
                strcpy(title, getmsg(&misc_message_file, &messageListItem, 7));
                dialog_out(title, NULL, 0, 192, 116, colorTable[32328], NULL, colorTable[32328], 0);
            } else {
                gsound_play_sfx_file("ib1p1xx1");
                pipboy(true);
            }
        }
        break;
    case KEY_HOME:
        if (obj_dude->elevation != map_elevation) {
            map_set_elevation(obj_dude->elevation);
        }

        if (game_in_mapper) {
            tile_set_center(obj_dude->tile, TILE_SET_CENTER_REFRESH_WINDOW);
        } else {
            tile_scroll_to(obj_dude->tile, 2);
        }

        break;
    case KEY_1:
    case KEY_EXCLAMATION:
        if (intface_is_enabled()) {
            gsound_play_sfx_file("ib1p1xx1");
            gmouse_set_cursor(MOUSE_CURSOR_USE_CROSSHAIR);
            action_skill_use(SKILL_SNEAK);
        }
        break;
    case KEY_2:
    case KEY_AT:
        if (intface_is_enabled()) {
            gsound_play_sfx_file("ib1p1xx1");
            gmouse_set_cursor(MOUSE_CURSOR_USE_CROSSHAIR);
            gmouse_3d_set_mode(GAME_MOUSE_MODE_USE_LOCKPICK);
        }
        break;
    case KEY_3:
    case KEY_NUMBER_SIGN:
        if (intface_is_enabled()) {
            gsound_play_sfx_file("ib1p1xx1");
            gmouse_set_cursor(MOUSE_CURSOR_USE_CROSSHAIR);
            gmouse_3d_set_mode(GAME_MOUSE_MODE_USE_STEAL);
        }
        break;
    case KEY_4:
    case KEY_DOLLAR:
        if (intface_is_enabled()) {
            gsound_play_sfx_file("ib1p1xx1");
            gmouse_set_cursor(MOUSE_CURSOR_USE_CROSSHAIR);
            gmouse_3d_set_mode(GAME_MOUSE_MODE_USE_TRAPS);
        }
        break;
    case KEY_5:
    case KEY_PERCENT:
        if (intface_is_enabled()) {
            gsound_play_sfx_file("ib1p1xx1");
            gmouse_set_cursor(MOUSE_CURSOR_USE_CROSSHAIR);
            gmouse_3d_set_mode(GAME_MOUSE_MODE_USE_FIRST_AID);
        }
        break;
    case KEY_6:
    case KEY_CARET:
        if (intface_is_enabled()) {
            gsound_play_sfx_file("ib1p1xx1");
            gmouse_set_cursor(MOUSE_CURSOR_USE_CROSSHAIR);
            gmouse_3d_set_mode(GAME_MOUSE_MODE_USE_DOCTOR);
        }
        break;
    case KEY_7:
    case KEY_AMPERSAND:
        if (intface_is_enabled()) {
            gsound_play_sfx_file("ib1p1xx1");
            gmouse_set_cursor(MOUSE_CURSOR_USE_CROSSHAIR);
            gmouse_3d_set_mode(GAME_MOUSE_MODE_USE_SCIENCE);
        }
        break;
    case KEY_8:
    case KEY_ASTERISK:
        if (intface_is_enabled()) {
            gsound_play_sfx_file("ib1p1xx1");
            gmouse_set_cursor(MOUSE_CURSOR_USE_CROSSHAIR);
            gmouse_3d_set_mode(GAME_MOUSE_MODE_USE_REPAIR);
        }
        break;
    case KEY_MINUS:
    case KEY_UNDERSCORE:
        DecGamma();
        break;
    case KEY_EQUAL:
    case KEY_PLUS:
        IncGamma();
        break;
    case KEY_COMMA:
    case KEY_LESS:
        if (register_begin(ANIMATION_REQUEST_RESERVED) == 0) {
            register_object_dec_rotation(obj_dude);
            register_end();
        }
        break;
    case KEY_DOT:
    case KEY_GREATER:
        if (register_begin(ANIMATION_REQUEST_RESERVED) == 0) {
            register_object_inc_rotation(obj_dude);
            register_end();
        }
        break;
    case KEY_SLASH:
    case KEY_QUESTION:
        if (1) {
            gsound_play_sfx_file("ib1p1xx1");

            int month;
            int day;
            int year;
            game_time_date(&month, &day, &year);

            MessageList messageList;
            if (message_init(&messageList)) {
                char path[COMPAT_MAX_PATH];
                snprintf(path, sizeof(path), "%s%s", msg_path, "editor.msg");

                if (message_load(&messageList, path)) {
                    MessageListItem messageListItem;
                    messageListItem.num = 500 + month - 1;
                    if (message_search(&messageList, &messageListItem)) {
                        char* time = game_time_hour_str();

                        char date[128];
                        snprintf(date, sizeof(date), "%s %d, %d %s", messageListItem.text, day, year, time);

                        display_print(date);
                    }
                }

                message_exit(&messageList);
            }
        }
        break;
    case KEY_F1:
        gsound_play_sfx_file("ib1p1xx1");
        game_help();
        break;
    case KEY_F2:
        gsound_set_master_volume(gsound_get_master_volume() - 2047);
        break;
    case KEY_F3:
        gsound_set_master_volume(gsound_get_master_volume() + 2047);
        break;
    case KEY_CTRL_S:
    case KEY_F4:
        gsound_play_sfx_file("ib1p1xx1");
        if (SaveGame(1) == -1) {
            debug_printf("\n ** Error calling SaveGame()! **\n");
        }
        break;
    case KEY_CTRL_L:
    case KEY_F5:
        gsound_play_sfx_file("ib1p1xx1");
        if (LoadGame(LOAD_SAVE_MODE_NORMAL) == -1) {
            debug_printf("\n ** Error calling LoadGame()! **\n");
        }
        break;
    case KEY_F6:
        if (1) {
            gsound_play_sfx_file("ib1p1xx1");

            int rc = SaveGame(LOAD_SAVE_MODE_QUICK);
            if (rc == -1) {
                debug_printf("\n ** Error calling SaveGame()! **\n");
            } else if (rc == 1) {
                MessageListItem messageListItem;
                // Quick save game successfully saved.
                char* msg = getmsg(&misc_message_file, &messageListItem, 5);
                display_print(msg);
            }
        }
        break;
    case KEY_F7:
        if (1) {
            gsound_play_sfx_file("ib1p1xx1");

            int rc = LoadGame(LOAD_SAVE_MODE_QUICK);
            if (rc == -1) {
                debug_printf("\n ** Error calling LoadGame()! **\n");
            } else if (rc == 1) {
                MessageListItem messageListItem;
                // Quick load game successfully loaded.
                char* msg = getmsg(&misc_message_file, &messageListItem, 4);
                display_print(msg);
            }
        }
        break;
    case KEY_CTRL_V:
        if (1) {
            gsound_play_sfx_file("ib1p1xx1");

            char version[VERSION_MAX];
            getverstr(version, sizeof(version));
            display_print(version);
            display_print(version_build_time);
        }
        break;
    case KEY_ARROW_LEFT:
        map_scroll(-1, 0);
        break;
    case KEY_ARROW_RIGHT:
        map_scroll(1, 0);
        break;
    case KEY_ARROW_UP:
        map_scroll(0, -1);
        break;
    case KEY_ARROW_DOWN:
        map_scroll(0, 1);
        break;
    }

    return 0;
}

// 0x43C584
void game_ui_disable(int a1)
{
    if (!game_ui_disabled) {
        gmouse_3d_off();
        gmouse_disable(a1);
        kb_disable();
        intface_disable();
        game_ui_disabled = true;
    }
}

// 0x43C5B8
void game_ui_enable()
{
    if (game_ui_disabled) {
        intface_enable();
        kb_enable();
        kb_clear();
        gmouse_enable();
        gmouse_3d_on();
        game_ui_disabled = false;
    }
}

// 0x43C5E0
bool game_ui_is_disabled()
{
    return game_ui_disabled;
}

// 0x43C5E8
int game_get_global_var(int var)
{
    if (var < 0 || var >= num_game_global_vars) {
        debug_printf("ERROR: attempt to reference global var out of range: %d", var);
        return 0;
    }

    return game_global_vars[var];
}

// 0x43C618
int game_set_global_var(int var, int value)
{
    if (var < 0 || var >= num_game_global_vars) {
        debug_printf("ERROR: attempt to reference global var out of range: %d", var);
        return -1;
    }

    game_global_vars[var] = value;

    return 0;
}

// 0x43C648
int game_load_info()
{
    return game_load_info_vars("data\\vault13.gam", "GAME_GLOBAL_VARS:", &num_game_global_vars, &game_global_vars);
}

// 0x43C668
int game_load_info_vars(const char* path, const char* section, int* variablesListLengthPtr, int** variablesListPtr)
{
    inven_reset_dude();

    DB_FILE* stream = db_fopen(path, "rt");
    if (stream == NULL) {
        return -1;
    }

    if (*variablesListLengthPtr != 0) {
        mem_free(*variablesListPtr);
        *variablesListPtr = NULL;
        *variablesListLengthPtr = 0;
    }

    char string[260];
    if (section != NULL) {
        while (db_fgets(string, 258, stream)) {
            if (strncmp(string, section, 16) == 0) {
                break;
            }
        }
    }

    while (db_fgets(string, 258, stream)) {
        if (string[0] == '\n') {
            continue;
        }

        if (string[0] == '/' && string[1] == '/') {
            continue;
        }

        char* semicolon = strchr(string, ';');
        if (semicolon != NULL) {
            *semicolon = '\0';
        }

        *variablesListLengthPtr = *variablesListLengthPtr + 1;
        *variablesListPtr = (int*)mem_realloc(*variablesListPtr, sizeof(int) * *variablesListLengthPtr);

        if (*variablesListPtr == NULL) {
            exit(1);
        }

        char* equals = strchr(string, '=');
        if (equals != NULL) {
            sscanf(equals + 1, "%d", *variablesListPtr + *variablesListLengthPtr - 1);
        } else {
            (*variablesListPtr)[*variablesListLengthPtr - 1] = 0;
        }
    }

    db_fclose(stream);

    return 0;
}

// 0x43C7AC
int game_state()
{
    return game_state_cur;
}

// 0x43C7B4
int game_state_request(int a1)
{
    if (a1 == GAME_STATE_0) {
        a1 = GAME_STATE_1;
    } else if (a1 == GAME_STATE_2) {
        a1 = GAME_STATE_3;
    } else if (a1 == GAME_STATE_4) {
        a1 = GAME_STATE_5;
    }

    if (game_state_cur != GAME_STATE_4 || a1 != GAME_STATE_5) {
        game_state_cur = a1;
        return 0;
    }

    return -1;
}

// 0x43C810
void game_state_update()
{
    int v0;

    v0 = game_state_cur;
    switch (game_state_cur) {
    case GAME_STATE_1:
        v0 = GAME_STATE_0;
        break;
    case GAME_STATE_3:
        v0 = GAME_STATE_2;
        break;
    case GAME_STATE_5:
        v0 = GAME_STATE_4;
    }

    game_state_cur = v0;
}

// 0x43D0AC
static int game_screendump(int width, int height, unsigned char* buffer, unsigned char* palette)
{
    MessageListItem messageListItem;

    if (default_screendump(width, height, buffer, palette) != 0) {
        // Error saving screenshot.
        messageListItem.num = 8;
        if (message_search(&misc_message_file, &messageListItem)) {
            display_print(messageListItem.text);
        }

        return -1;
    }

    // Saved screenshot.
    messageListItem.num = 3;
    if (message_search(&misc_message_file, &messageListItem)) {
        display_print(messageListItem.text);
    }

    return 0;
}

// 0x43D10C
static void game_unload_info()
{
    num_game_global_vars = 0;
    if (game_global_vars != NULL) {
        mem_free(game_global_vars);
        game_global_vars = NULL;
    }
}

// 0x43D130
static void game_help()
{
    bool isoWasEnabled = map_disable_bk_processes();
    gmouse_3d_off();

    gmouse_set_cursor(MOUSE_CURSOR_NONE);

    bool colorCycleWasEnabled = cycle_is_enabled();
    cycle_disable();

    // CE: Help screen uses separate color palette which is incompatible with
    // colors in other windows. Setup overlay to hide everything.
    int overlay = win_add(0, 0, screenGetWidth(), screenGetHeight(), 0, WINDOW_HIDDEN | WINDOW_MOVE_ON_TOP);

    int helpWindowX = (screenGetWidth() - HELP_SCREEN_WIDTH) / 2;
    int helpWindowY = (screenGetHeight() - HELP_SCREEN_HEIGHT) / 2;
    int win = win_add(helpWindowX, helpWindowY, HELP_SCREEN_WIDTH, HELP_SCREEN_HEIGHT, 0, WINDOW_HIDDEN | WINDOW_MOVE_ON_TOP);
    if (win != -1) {
        unsigned char* windowBuffer = win_get_buf(win);
        if (windowBuffer != NULL) {
            int backgroundFid = art_id(OBJ_TYPE_INTERFACE, 297, 0, 0, 0);
            CacheEntry* backgroundHandle;
            unsigned char* backgroundData = art_ptr_lock_data(backgroundFid, 0, 0, &backgroundHandle);
            if (backgroundData != NULL) {
                palette_set_to(black_palette);
                buf_to_buf(backgroundData, HELP_SCREEN_WIDTH, HELP_SCREEN_HEIGHT, HELP_SCREEN_WIDTH, windowBuffer, HELP_SCREEN_WIDTH);
                art_ptr_unlock(backgroundHandle);
                loadColorTable("art\\intrface\\helpscrn.pal");
                palette_set_to(cmap);

                // CE: Fill overlay with darkest color in the palette. It might
                // not be completely black, but at least it's uniform.
                buf_fill(win_get_buf(overlay),
                    screenGetWidth(),
                    screenGetHeight(),
                    screenGetWidth(),
                    intensityColorTable[colorTable[0]][0]);

                win_show(overlay);
                win_show(win);

                while (get_input() == -1 && game_user_wants_to_quit == 0) {
                    sharedFpsLimiter.mark();
                    renderPresent();
                    sharedFpsLimiter.throttle();
                }

                while (mouse_get_buttons() != 0) {
                    sharedFpsLimiter.mark();

                    get_input();

                    renderPresent();
                    sharedFpsLimiter.throttle();
                }

                palette_set_to(black_palette);
            }
        }

        win_delete(overlay);
        win_delete(win);
        loadColorTable("color.pal");
        palette_set_to(cmap);
    }

    if (colorCycleWasEnabled) {
        cycle_enable();
    }

    gmouse_3d_on();

    if (isoWasEnabled) {
        map_enable_bk_processes();
    }
}

// 0x43D274
int game_quit_with_confirm()
{
    bool isoWasEnabled = map_disable_bk_processes();

    bool gameMouseWasVisible;
    if (isoWasEnabled) {
        gameMouseWasVisible = gmouse_3d_is_on();
    } else {
        gameMouseWasVisible = false;
    }

    if (gameMouseWasVisible) {
        gmouse_3d_off();
    }

    bool cursorWasHidden = mouse_hidden();
    if (cursorWasHidden) {
        mouse_show();
    }

    int oldCursor = gmouse_get_cursor();
    gmouse_set_cursor(MOUSE_CURSOR_ARROW);

    int rc;

    // Are you sure you want to quit?
    MessageListItem messageListItem;
    messageListItem.num = 0;
    if (message_search(&misc_message_file, &messageListItem)) {
        rc = dialog_out(messageListItem.text, 0, 0, 169, 117, colorTable[32328], NULL, colorTable[32328], DIALOG_BOX_YES_NO);
        if (rc != 0) {
            game_user_wants_to_quit = 2;
        }
    } else {
        rc = -1;
    }

    gmouse_set_cursor(oldCursor);

    if (cursorWasHidden) {
        mouse_hide();
    }

    if (gameMouseWasVisible) {
        gmouse_3d_on();
    }

    if (isoWasEnabled) {
        map_enable_bk_processes();
    }

    return rc;
}

// 0x43D348
static int game_init_databases()
{
    int hashing;
    char* main_file_name;
    char* patch_file_name;

    hashing = 0;
    main_file_name = NULL;
    patch_file_name = NULL;

    if (config_get_value(&game_config, GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_HASHING_KEY, &hashing)) {
        db_enable_hash_table();
    }

    config_get_string(&game_config, GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_MASTER_DAT_KEY, &main_file_name);
    if (*main_file_name == '\0') {
        main_file_name = NULL;
    }

    config_get_string(&game_config, GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_MASTER_PATCHES_KEY, &patch_file_name);
    if (*patch_file_name == '\0') {
        patch_file_name = NULL;
    }

    master_db_handle = db_init(main_file_name, NULL, patch_file_name, 1);
    if (master_db_handle == INVALID_DATABASE_HANDLE) {
        GNWSystemError("Could not find the master datafile. Please make sure the FALLOUT CD is in the drive and that you are running FALLOUT from the directory you installed it to.");
        return -1;
    }

    config_get_string(&game_config, GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_CRITTER_DAT_KEY, &main_file_name);
    if (*main_file_name == '\0') {
        main_file_name = NULL;
    }

    config_get_string(&game_config, GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_CRITTER_PATCHES_KEY, &patch_file_name);
    if (*patch_file_name == '\0') {
        patch_file_name = NULL;
    }

    critter_db_handle = db_init(main_file_name, NULL, patch_file_name, 1);
    if (critter_db_handle == INVALID_DATABASE_HANDLE) {
        db_select(master_db_handle);
        GNWSystemError("Could not find the critter datafile. Please make sure the FALLOUT CD is in the drive and that you are running FALLOUT from the directory you installed it to.");
        return -1;
    }

    db_select(master_db_handle);

    return 0;
}

// 0x43D53C
static void game_splash_screen()
{
    int splash;
    config_get_value(&game_config, GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_SPLASH_KEY, &splash);

    DB_FILE* stream;
    for (int index = 0; index < SPLASH_COUNT; index++) {
        char filePath[64];
        snprintf(filePath, sizeof(filePath), "art\\splash\\splash%d.rix", splash);
        stream = db_fopen(filePath, "rb");
        if (stream != NULL) {
            break;
        }

        splash++;

        if (splash >= SPLASH_COUNT) {
            splash = 0;
        }
    }

    if (stream == NULL) {
        return;
    }

    unsigned char* palette = (unsigned char*)mem_malloc(768);
    if (palette == NULL) {
        db_fclose(stream);
        return;
    }

    unsigned char* data = (unsigned char*)mem_malloc(SPLASH_WIDTH * SPLASH_HEIGHT);
    if (data == NULL) {
        mem_free(palette);
        db_fclose(stream);
        return;
    }

    palette_set_to(black_palette);
    db_fseek(stream, 10, SEEK_SET);
    db_fread(palette, 1, 768, stream);
    db_fread(data, 1, SPLASH_WIDTH * SPLASH_HEIGHT, stream);
    db_fclose(stream);

    int splashWindowX = (screenGetWidth() - SPLASH_WIDTH) / 2;
    int splashWindowY = (screenGetHeight() - SPLASH_HEIGHT) / 2;
    scr_blit(data, SPLASH_WIDTH, SPLASH_HEIGHT, 0, 0, SPLASH_WIDTH, SPLASH_HEIGHT, splashWindowX, splashWindowY);
    palette_fade_to(palette);

    mem_free(data);
    mem_free(palette);

    config_set_value(&game_config, GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_SPLASH_KEY, splash + 1);
}

} // namespace fallout
