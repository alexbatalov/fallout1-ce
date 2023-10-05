#include "main.h"

// NOTE: Actual file name is unknown. Functions in this module do not present
// in debug symbols from `mapper2.exe`. In OS X binary these functions appear
// very far from the ones found in `mainmenu.c`, implying they are in separate
// compilation unit. In Windows binary these functions appear between
// `loadsave.c` and `mainmenu.c`. Based on the order it's file name should be
// between these two, so `main.c` is a perfect candidate, but again, it's just a
// guess.
//
// Function names and visibility scope are from in OS X binary.

#include <limits.h>
#include <stddef.h>

#include "game/amutex.h"
#include "game/art.h"
#include "game/credits.h"
#include "game/cycle.h"
#include "game/endgame.h"
#include "game/game.h"
#include "game/gconfig.h"
#include "game/gmouse.h"
#include "game/gmovie.h"
#include "game/gsound.h"
#include "game/loadsave.h"
#include "game/mainmenu.h"
#include "game/map.h"
#include "game/object.h"
#include "game/options.h"
#include "game/palette.h"
#include "game/proto.h"
#include "game/roll.h"
#include "game/scripts.h"
#include "game/select.h"
#include "game/selfrun.h"
#include "game/wordwrap.h"
#include "game/worldmap.h"
#include "plib/color/color.h"
#include "plib/gnw/debug.h"
#include "plib/gnw/gnw.h"
#include "plib/gnw/grbuf.h"
#include "plib/gnw/input.h"
#include "plib/gnw/intrface.h"
#include "plib/gnw/svga.h"
#include "plib/gnw/text.h"

namespace fallout {

#define DEATH_WINDOW_WIDTH 640
#define DEATH_WINDOW_HEIGHT 480

static bool main_init_system(int argc, char** argv);
static int main_reset_system();
static void main_exit_system();
static int main_load_new(char* fname);
static int main_loadgame_new();
static void main_unload_new();
static void main_game_loop();
static bool main_selfrun_init();
static void main_selfrun_exit();
static void main_selfrun_record();
static void main_selfrun_play();
static void main_death_scene();
static void main_death_voiceover_callback();

// 0x4F9F70
static char mainMap[] = "V13Ent.map";

// 0x505A6C
int main_game_paused = 0;

// 0x505A70
static char** main_selfrun_list = NULL;

// 0x505A74
static int main_selfrun_count = 0;

// 0x505A78
static int main_selfrun_index = 0;

// 0x505A7C
static bool main_show_death_scene = false;

// 0x614838
static bool main_death_voiceover_done;

// 0x4725E8
int gnw_main(int argc, char** argv)
{
    if (!autorun_mutex_create()) {
        return 1;
    }

    if (!main_init_system(argc, argv)) {
        return 1;
    }

    gmovie_play(MOVIE_IPLOGO, GAME_MOVIE_FADE_IN);
    gmovie_play(MOVIE_INTRO, 0);

    if (main_menu_create() == 0) {
        int language_filter = 1;
        bool done = false;

        config_get_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_LANGUAGE_FILTER_KEY, &language_filter);

        while (!done) {
            kb_clear();
            gsound_background_play_level_music("07desert", 11);
            main_menu_show(1);

            mouse_show();
            int mainMenuRc = main_menu_loop();
            mouse_hide();

            switch (mainMenuRc) {
            case MAIN_MENU_INTRO:
                main_menu_hide(true);
                gmovie_play(MOVIE_INTRO, GAME_MOVIE_PAUSE_MUSIC);
                break;
            case MAIN_MENU_NEW_GAME:
                main_menu_hide(true);
                main_menu_destroy();
                if (select_character() == 2) {
                    gmovie_play(MOVIE_OVRINTRO, GAME_MOVIE_STOP_MUSIC);
                    roll_set_seed(-1);
                    main_load_new(mainMap);
                    main_game_loop();
                    palette_fade_to(white_palette);

                    // NOTE: Uninline.
                    main_unload_new();

                    // NOTE: Uninline.
                    main_reset_system();

                    if (main_show_death_scene != 0) {
                        main_death_scene();
                        main_show_death_scene = 0;
                    }
                }

                main_menu_create();

                break;
            case MAIN_MENU_LOAD_GAME:
                if (1) {
                    int win = win_add(0, 0, screenGetWidth(), screenGetHeight(), colorTable[0], WINDOW_MODAL | WINDOW_MOVE_ON_TOP);
                    main_menu_hide(true);
                    main_menu_destroy();
                    gsound_background_stop();

                    // NOTE: Uninline.
                    main_loadgame_new();

                    loadColorTable("color.pal");
                    palette_fade_to(cmap);
                    int loadGameRc = LoadGame(LOAD_SAVE_MODE_FROM_MAIN_MENU);
                    if (loadGameRc == -1) {
                        debug_printf("\n ** Error running LoadGame()! **\n");
                    } else if (loadGameRc != 0) {
                        win_delete(win);
                        win = -1;
                        main_game_loop();
                    }
                    palette_fade_to(white_palette);
                    if (win != -1) {
                        win_delete(win);
                    }

                    // NOTE: Uninline.
                    main_unload_new();

                    // NOTE: Uninline.
                    main_reset_system();

                    if (main_show_death_scene != 0) {
                        main_death_scene();
                        main_show_death_scene = 0;
                    }
                    main_menu_create();
                }
                break;
            case MAIN_MENU_TIMEOUT:
                debug_printf("Main menu timed-out\n");
                // FALLTHROUGH
            case MAIN_MENU_SCREENSAVER:
                main_selfrun_play();
                break;
            case MAIN_MENU_CREDITS:
                main_menu_hide(true);
                credits("credits.txt", -1, false);
                break;
            case MAIN_MENU_QUOTES:
                if (language_filter == 0) {
                    main_menu_hide(true);
                    credits("quotes.txt", -1, true);
                }
                break;
            case MAIN_MENU_EXIT:
            case -1:
                done = true;
                main_menu_hide(true);
                main_menu_destroy();
                gsound_background_stop();
                break;
            case MAIN_MENU_SELFRUN:
                main_selfrun_record();
                break;
            }
        }
    }

    // NOTE: Uninline.
    main_exit_system();

    autorun_mutex_destroy();

    return 0;
}

// 0x4728CC
static bool main_init_system(int argc, char** argv)
{
    if (game_init("FALLOUT", false, 0, 0, argc, argv) == -1) {
        return false;
    }

    // NOTE: Uninline.
    main_selfrun_init();

    return true;
}

// 0x472918
static int main_reset_system()
{
    game_reset();

    return 1;
}

// 0x472924
static void main_exit_system()
{
    gsound_background_stop();

    // NOTE: Uninline.
    main_selfrun_exit();

    game_exit();

    // TODO: Find a better place for this call.
    SDL_Quit();
}

// 0x472958
static int main_load_new(char* mapFileName)
{
    game_user_wants_to_quit = 0;
    main_show_death_scene = 0;
    obj_dude->flags &= ~OBJECT_FLAT;
    obj_turn_on(obj_dude, NULL);
    mouse_hide();

    int win = win_add(0, 0, screenGetWidth(), screenGetHeight(), colorTable[0], WINDOW_MODAL | WINDOW_MOVE_ON_TOP);
    win_draw(win);

    loadColorTable("color.pal");
    palette_fade_to(cmap);
    map_init();
    gmouse_set_cursor(MOUSE_CURSOR_NONE);
    mouse_show();
    map_load(mapFileName);
    PlayCityMapMusic();
    palette_fade_to(white_palette);
    win_delete(win);
    loadColorTable("color.pal");
    palette_fade_to(cmap);
    return 0;
}

// 0x472A04
static int main_loadgame_new()
{
    game_user_wants_to_quit = 0;
    main_show_death_scene = 0;

    obj_dude->flags &= ~OBJECT_FLAT;

    obj_turn_on(obj_dude, NULL);
    mouse_hide();

    map_init();

    gmouse_set_cursor(MOUSE_CURSOR_NONE);
    mouse_show();

    return 0;
}

// 0x472A40
static void main_unload_new()
{
    obj_turn_off(obj_dude, NULL);
    map_exit();
}

// 0x472A54
static void main_game_loop()
{
    bool cursorWasHidden = mouse_hidden();
    if (cursorWasHidden) {
        mouse_show();
    }

    main_game_paused = 0;

    scr_enable();

    while (game_user_wants_to_quit == 0) {
        sharedFpsLimiter.mark();

        int keyCode = get_input();
        game_handle_input(keyCode, false);

        scripts_check_state();

        map_check_state();

        if (main_game_paused != 0) {
            main_game_paused = 0;
        }

        if ((obj_dude->data.critter.combat.results & (DAM_DEAD | DAM_KNOCKED_OUT)) != 0) {
            main_show_death_scene = 1;
            game_user_wants_to_quit = 2;
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    scr_disable();

    if (cursorWasHidden) {
        mouse_hide();
    }
}

// 0x472AE8
static bool main_selfrun_init()
{
    if (main_selfrun_list != NULL) {
        // NOTE: Uninline.
        main_selfrun_exit();
    }

    if (selfrun_get_list(&main_selfrun_list, &main_selfrun_count) != 0) {
        return false;
    }

    main_selfrun_index = 0;

    return true;
}

// 0x472B3C
static void main_selfrun_exit()
{
    if (main_selfrun_list != NULL) {
        selfrun_free_list(&main_selfrun_list);
    }

    main_selfrun_count = 0;
    main_selfrun_index = 0;
    main_selfrun_list = NULL;
}

// 0x472B68
static void main_selfrun_record()
{
    SelfrunData selfrunData;
    bool ready = false;

    char** fileList;
    int fileListLength = db_get_file_list("maps\\*.map", &fileList, NULL, 0);
    if (fileListLength != 0) {
        int selectedFileIndex = win_list_select("Select Map", fileList, fileListLength, 0, 80, 80, 0x10000 | 0x100 | 4);
        if (selectedFileIndex != -1) {
            // NOTE: It's size is likely 13 chars (on par with SelfrunData
            // fields), but due to the padding it takes 16 chars on stack.
            char recordingName[SELFRUN_RECORDING_FILE_NAME_LENGTH];
            recordingName[0] = '\0';
            if (win_get_str(recordingName, sizeof(recordingName) - 2, "Enter name for recording (8 characters max, no extension):", 100, 100) == 0) {
                memset(&selfrunData, 0, sizeof(selfrunData));
                if (selfrun_prep_recording(recordingName, fileList[selectedFileIndex], &selfrunData) == 0) {
                    ready = true;
                }
            }
        }
        db_free_file_list(&fileList, NULL);
    }

    if (ready) {
        main_menu_hide(true);
        main_menu_destroy();
        gsound_background_stop();
        roll_set_seed(0xBEEFFEED);

        // NOTE: Uninline.
        main_reset_system();

        proto_dude_init("premade\\combat.gcd");
        main_load_new(selfrunData.mapFileName);
        selfrun_recording_loop(&selfrunData);
        palette_fade_to(white_palette);

        // NOTE: Uninline.
        main_unload_new();

        // NOTE: Uninline.
        main_reset_system();

        main_menu_create();

        // NOTE: Uninline.
        main_selfrun_init();
    }
}

// 0x472CA0
static void main_selfrun_play()
{
    // A switch to pick selfrun vs. intro video for screensaver:
    // - `false` - will play next selfrun recording
    // - `true` - will play intro video
    //
    // This value will alternate on every attempt, even if there are no selfrun
    // recordings.
    //
    // 0x505A80
    static bool toggle = false;

    if (!toggle && main_selfrun_count > 0) {
        SelfrunData selfrunData;
        if (selfrun_prep_playback(main_selfrun_list[main_selfrun_index], &selfrunData) == 0) {
            main_menu_hide(true);
            main_menu_destroy();
            gsound_background_stop();
            roll_set_seed(0xBEEFFEED);

            // NOTE: Uninline.
            main_reset_system();

            proto_dude_init("premade\\combat.gcd");
            main_load_new(selfrunData.mapFileName);
            selfrun_playback_loop(&selfrunData);
            palette_fade_to(white_palette);

            // NOTE: Uninline.
            main_unload_new();

            // NOTE: Uninline.
            main_reset_system();

            main_menu_create();
        }

        main_selfrun_index++;
        if (main_selfrun_index >= main_selfrun_count) {
            main_selfrun_index = 0;
        }
    } else {
        main_menu_hide(true);
        gmovie_play(MOVIE_INTRO, GAME_MOVIE_PAUSE_MUSIC);
    }

    toggle = 1 - toggle;
}

// 0x472D90
static void main_death_scene()
{
    // 0x4725B0
    static const char* deathFileNameList[] = {
        "narrator\\nar_3",
        "narrator\\nar_4",
        "narrator\\nar_5",
        "narrator\\nar_6",
    };

    art_flush();
    cycle_disable();
    gmouse_set_cursor(MOUSE_CURSOR_NONE);

    bool oldCursorIsHidden = mouse_hidden();
    if (oldCursorIsHidden) {
        mouse_show();
    }

    int deathWindowX = (screenGetWidth() - DEATH_WINDOW_WIDTH) / 2;
    int deathWindowY = (screenGetHeight() - DEATH_WINDOW_HEIGHT) / 2;
    int win = win_add(deathWindowX,
        deathWindowY,
        DEATH_WINDOW_WIDTH,
        DEATH_WINDOW_HEIGHT,
        0,
        WINDOW_MOVE_ON_TOP);
    if (win != -1) {
        unsigned char* windowBuffer = win_get_buf(win);
        if (windowBuffer != NULL) {
            // DEATH.FRM
            CacheEntry* backgroundHandle;
            int fid = art_id(OBJ_TYPE_INTERFACE, 309, 0, 0, 0);
            unsigned char* background = art_ptr_lock_data(fid, 0, 0, &backgroundHandle);
            if (background != NULL) {
                while (mouse_get_buttons() != 0) {
                    sharedFpsLimiter.mark();

                    get_input();

                    renderPresent();
                    sharedFpsLimiter.throttle();
                }

                kb_clear();
                flush_input_buffer();

                buf_to_buf(background,
                    DEATH_WINDOW_WIDTH,
                    DEATH_WINDOW_HEIGHT,
                    DEATH_WINDOW_WIDTH,
                    windowBuffer,
                    DEATH_WINDOW_WIDTH);
                art_ptr_unlock(backgroundHandle);

                win_draw(win);

                loadColorTable("art\\intrface\\death.pal");
                palette_fade_to(cmap);

                int deathFileNameIndex = roll_random(1, sizeof(deathFileNameList) / sizeof(*deathFileNameList)) - 1;

                main_death_voiceover_done = false;
                gsound_speech_callback_set(main_death_voiceover_callback);

                unsigned int delay;
                if (gsound_speech_play(deathFileNameList[deathFileNameIndex], 10, 14, 15) == -1) {
                    delay = 3000;
                } else {
                    delay = UINT_MAX;
                }

                gsound_speech_play_preloaded();

                unsigned int time = get_time();
                int keyCode;
                do {
                    sharedFpsLimiter.mark();

                    keyCode = get_input();
                    if (keyCode != -1) {
                        break;
                    }

                    if (main_death_voiceover_done) {
                        break;
                    }

                    renderPresent();
                    sharedFpsLimiter.throttle();
                } while (elapsed_time(time) < delay);

                gsound_speech_callback_set(NULL);

                gsound_speech_stop();

                while (mouse_get_buttons() != 0) {
                    sharedFpsLimiter.mark();

                    get_input();

                    renderPresent();
                    sharedFpsLimiter.throttle();
                }

                if (keyCode == -1) {
                    pause_for_tocks(500);
                }

                palette_fade_to(black_palette);
                loadColorTable("color.pal");
            }
        }

        win_delete(win);
    }

    if (oldCursorIsHidden) {
        mouse_hide();
    }

    gmouse_set_cursor(MOUSE_CURSOR_ARROW);

    cycle_enable();
}

// 0x472F68
static void main_death_voiceover_callback()
{
    main_death_voiceover_done = true;
}

} // namespace fallout
