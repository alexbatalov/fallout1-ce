#include "game/endgame.h"

#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "game/bmpdlog.h"
#include "game/credits.h"
#include "game/cycle.h"
#include "game/game.h"
#include "game/gconfig.h"
#include "game/gmouse.h"
#include "game/gmovie.h"
#include "game/gsound.h"
#include "game/map.h"
#include "game/object.h"
#include "game/palette.h"
#include "game/pipboy.h"
#include "game/roll.h"
#include "game/stat.h"
#include "game/wordwrap.h"
#include "game/worldmap.h"
#include "platform_compat.h"
#include "plib/color/color.h"
#include "plib/db/db.h"
#include "plib/gnw/debug.h"
#include "plib/gnw/gnw.h"
#include "plib/gnw/grbuf.h"
#include "plib/gnw/input.h"
#include "plib/gnw/memory.h"
#include "plib/gnw/svga.h"
#include "plib/gnw/text.h"

namespace fallout {

// The maximum number of subtitle lines per slide.
#define ENDGAME_ENDING_MAX_SUBTITLES 50

#define ENDGAME_ENDING_WINDOW_WIDTH 640
#define ENDGAME_ENDING_WINDOW_HEIGHT 480

typedef struct EndgameDeathEnding {
    int gvar;
    int value;
    int worldAreaKnown;
    int worldAreaNotKnown;
    int min_level;
    int percentage;
    char voiceOverBaseName[16];

    // This flag denotes that the conditions for this ending is met and it was
    // selected as a candidate for final random selection.
    bool enabled;
} EndgameDeathEnding;

typedef struct EndgameEnding {
    int gvar;
    int value;
    int art_num;
    char voiceOverBaseName[12];
    int direction;
} EndgameEnding;

static void endgame_pan_desert(int direction, const char* narratorFileName);
static void endgame_display_image(int fid, const char* narratorFileName);
static int endgame_init();
static void endgame_exit();
static void endgame_load_voiceover(const char* fname);
static void endgame_play_voiceover();
static void endgame_stop_voiceover();
static void endgame_load_palette(int type, int id);
static void endgame_voiceover_callback();
static int endgame_load_subtitles(const char* filePath);
static void endgame_show_subtitles();
static void endgame_clear_subtitles();
static void endgame_movie_callback();
static void endgame_movie_bk_process();
static int endgame_load_slide_info();
static void endgame_unload_slide_info();
static int endgameSetupInit(int* percentage);

static void endgameEndingUpdateOverlay();

// 0x504F6C
static const char* endgame_voiceover_path = "narrator\\";

// The number of lines in current subtitles file.
//
// It's used as a length for two arrays:
// - [endgame_subtitle_text]
// - [endgame_subtitle_times]
//
// This value does not exceed [ENDGAME_ENDING_SUBTITLES_CAPACITY].
//
// 0x504F78
static int endgame_subtitle_count = 0;

// The number of characters in current subtitles file.
//
// This value is used to determine
//
// 0x504F7C
static int endgame_subtitle_characters = 0;

// 0x504F80
static int endgame_current_subtitle = 0;

// 0x504F84
static int endgame_maybe_done = 0;

// This flag denotes whether speech sound was successfully loaded for
// the current slide.
//
// 0x56ED90
static bool endgame_voiceover_loaded;

// 0x56ED94
static char endgame_subtitle_path[COMPAT_MAX_PATH];

// The flag used to denote voice over speech for current slide has ended.
//
// 0x56EE98
static bool endgame_voiceover_done;

// The array of text lines in current subtitles file.
//
// The length is specified in [endgame_subtitle_count]. It's capacity
// is [ENDGAME_ENDING_SUBTITLES_CAPACITY].
//
// 0x56EE9C
static char** endgame_subtitle_text;

// 0x56EEA0
static bool endgame_do_subtitles;

// The flag used to denote voice over subtitles for current slide has ended.
//
// 0x56EEA4
static bool endgame_subtitle_done;

// 0x56EEA8
static bool endgame_map_enabled;

// 0x56EEAC
static bool endgame_mouse_state;

// This flag denotes whether subtitles was successfully loaded for
// the current slide.
//
// 0x56EEB0
static bool endgame_subtitle_loaded;

// Reference time is a timestamp when subtitle is first displayed.
//
// This value is used together with [endgame_subtitle_times] array to
// determine when next line needs to be displayed.
//
// 0x56EEB4
static unsigned int endgame_subtitle_start_time;

// 0x56EEB8
static unsigned char* endgame_window_buffer;

// The array of timings for each line in current subtitles file.
//
// The length is specified in [endgame_subtitle_count]. It's capacity
// is [ENDGAME_ENDING_SUBTITLES_CAPACITY].
//
// 0x56EEBC
static unsigned int* endgame_subtitle_times;

// 0x56EEC0
static int endgame_window;

// Font that was current before endgame slideshow window was created.
//
// 0x56EEC4
static int endgame_old_font;

static int gEndgameEndingOverlay;

// 0x438670
void endgame_slideshow()
{
    int fid;
    int v1;

    if (endgame_init() != -1) {
        if (game_get_global_var(GVAR_VATS_STATUS)) {
            endgame_pan_desert(1, "nar_11");
        } else {
            endgame_pan_desert(1, "nar_10");
        }

        if (game_get_global_var(GVAR_NECROPOLIS_INVADED)) {
            fid = art_id(OBJ_TYPE_INTERFACE, 311, 0, 0, 0);
            endgame_display_image(fid, "nar_15");
        } else if (game_get_global_var(GVAR_NECROP_WATER_CHIP_TAKEN)) {
            if (game_get_global_var(GVAR_NECROP_WATER_PUMP_FIXED) == 2) {
                fid = art_id(OBJ_TYPE_INTERFACE, 312, 0, 0, 0);
                endgame_display_image(fid, "nar_13");
            } else {
                fid = art_id(OBJ_TYPE_INTERFACE, 311, 0, 0, 0);
                endgame_display_image(fid, "nar_12");
            }
        }

        if (game_get_global_var(GVAR_FOLLOWERS_INVADED)) {
            fid = art_id(OBJ_TYPE_INTERFACE, 314, 0, 0, 0);
            endgame_display_image(fid, "nar_18");
        } else if (game_get_global_var(GVAR_TRAIN_FOLLOWERS)) {
            fid = art_id(OBJ_TYPE_INTERFACE, 313, 0, 0, 0);
            endgame_display_image(fid, "nar_16");
        }

        if (game_get_global_var(GVAR_SHADY_SANDS_INVADED)) {
            fid = art_id(OBJ_TYPE_INTERFACE, 324, 0, 0, 0);
            endgame_display_image(fid, "nar_23");
        } else {
            v1 = game_get_global_var(GVAR_TANDI_STATUS);
            if (game_get_global_var(GVAR_ARADESH_STATUS)) {
                if (v1 != 2 && v1 != 0) {
                    fid = art_id(OBJ_TYPE_INTERFACE, 324, 0, 0, 0);
                    endgame_display_image(fid, "nar_22");
                } else {
                    fid = art_id(OBJ_TYPE_INTERFACE, 323, 0, 0, 0);
                    endgame_display_image(fid, "nar_21");
                }
            } else {
                if (v1 != 2 && v1 != 0) {
                    fid = art_id(OBJ_TYPE_INTERFACE, 323, 0, 0, 0);
                    endgame_display_image(fid, "nar_20");
                } else {
                    fid = art_id(OBJ_TYPE_INTERFACE, 323, 0, 0, 0);
                    endgame_display_image(fid, "nar_19");
                }
            }
        }

        if (game_get_global_var(GVAR_JUNKTOWN_INVADED)) {
            fid = art_id(OBJ_TYPE_INTERFACE, 317, 0, 0, 0);
            endgame_display_image(fid, "nar_27");
        } else if (game_get_global_var(GVAR_CAPTURE_GIZMO) != 2 || game_get_global_var(GVAR_KILLIAN_DEAD)) {
            if (!game_get_global_var(GVAR_GIZMO_DEAD)) {
                fid = art_id(OBJ_TYPE_INTERFACE, 316, 0, 0, 0);
                endgame_display_image(fid, "nar_25");
            }
        } else {
            fid = art_id(OBJ_TYPE_INTERFACE, 315, 0, 0, 0);
            endgame_display_image(fid, "nar_24");
        }

        if (game_get_global_var(GVAR_BECOME_AN_INITIATE) == 2 && game_get_global_var(GVAR_ENEMY_BROTHERHOOD)) {
            fid = art_id(OBJ_TYPE_INTERFACE, 319, 0, 0, 0);
            endgame_display_image(fid, "nar_29");
        } else {
            fid = art_id(OBJ_TYPE_INTERFACE, 318, 0, 0, 0);
            endgame_display_image(fid, "nar_28");
        }

        if (game_get_global_var(GVAR_HUB_INVADED)) {
            fid = art_id(OBJ_TYPE_INTERFACE, 326, 0, 0, 0);
            endgame_display_image(fid, "nar_34");
        } else if (game_get_global_var(GVAR_KIND_TO_HAROLD) == 1) {
            fid = art_id(OBJ_TYPE_INTERFACE, 325, 0, 0, 0);
            endgame_display_image(fid, "nar_32");
        }

        if (game_get_global_var(GVAR_RAIDERS) < 2) {
            fid = art_id(OBJ_TYPE_INTERFACE, 320, 0, 0, 0);
            endgame_display_image(fid, "nar_37");
        } else {
            v1 = game_get_global_var(GVAR_TOTAL_RAIDERS);
            if (game_get_global_var(GVAR_GARL_DEAD) && v1 < 8 || v1 < 4) {
                fid = art_id(OBJ_TYPE_INTERFACE, 320, 0, 0, 0);
                endgame_display_image(fid, "nar_35");
            } else {
                fid = art_id(OBJ_TYPE_INTERFACE, 320, 0, 0, 0);
                endgame_display_image(fid, "nar_36");
            }
        }

        endgame_pan_desert(-1, "nar_40");

        endgame_exit();
    }

    game_set_global_var(GVAR_CALM_REBELS_2, 0);
}

// 0x438A4C
void endgame_movie()
{
    gsound_background_stop();
    map_disable_bk_processes();
    palette_fade_to(black_palette);
    endgame_maybe_done = 0;
    add_bk_process(endgame_movie_bk_process);
    gsound_background_callback_set(endgame_movie_callback);
    gsound_background_play("maybe", 12, 14, 15);
    pause_for_tocks(3000);

    // NOTE: Result is ignored. I guess there was some kind of switch for male
    // vs. female ending, but it was not implemented.
    if (stat_level(obj_dude, STAT_GENDER) == GENDER_MALE) {
        gmovie_play(MOVIE_WALKM, 0);
    } else {
        gmovie_play(MOVIE_WALKW, 0);
    }

    credits("credits.txt", -1, false);
    gsound_background_stop();
    gsound_background_callback_set(NULL);
    remove_bk_process(endgame_movie_bk_process);
    gsound_background_stop();
    game_user_wants_to_quit = 2;
}

// 0x438B04
static int endgame_init()
{
    gsound_background_stop();

    endgame_map_enabled = map_disable_bk_processes();

    cycle_disable();
    gmouse_set_cursor(MOUSE_CURSOR_NONE);

    bool oldCursorIsHidden = mouse_hidden();
    endgame_mouse_state = oldCursorIsHidden == 0;

    if (oldCursorIsHidden) {
        mouse_show();
    }

    endgame_old_font = text_curr();
    text_font(101);

    palette_fade_to(black_palette);

    // CE: Every slide has a separate color palette which is incompatible with
    // main color palette. Setup overlay to hide everything.
    gEndgameEndingOverlay = win_add(0, 0, screenGetWidth(), screenGetHeight(), colorTable[0], WINDOW_MOVE_ON_TOP);
    if (gEndgameEndingOverlay == -1) {
        return -1;
    }

    int windowEndgameEndingX = (screenGetWidth() - ENDGAME_ENDING_WINDOW_WIDTH) / 2;
    int windowEndgameEndingY = (screenGetHeight() - ENDGAME_ENDING_WINDOW_HEIGHT) / 2;
    endgame_window = win_add(windowEndgameEndingX,
        windowEndgameEndingY,
        ENDGAME_ENDING_WINDOW_WIDTH,
        ENDGAME_ENDING_WINDOW_HEIGHT,
        colorTable[0],
        WINDOW_MOVE_ON_TOP);
    if (endgame_window == -1) {
        return -1;
    }

    endgame_window_buffer = win_get_buf(endgame_window);
    if (endgame_window_buffer == NULL) {
        return -1;
    }

    cycle_disable();

    gsound_speech_callback_set(endgame_voiceover_callback);

    endgame_do_subtitles = false;
    configGetBool(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_SUBTITLES_KEY, &endgame_do_subtitles);
    if (!endgame_do_subtitles) {
        return 0;
    }

    char* language;
    if (!config_get_string(&game_config, GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_LANGUAGE_KEY, &language)) {
        endgame_do_subtitles = false;
        return 0;
    }

    snprintf(endgame_subtitle_path, sizeof(endgame_subtitle_path), "text\\%s\\cuts\\", language);

    endgame_subtitle_text = (char**)mem_malloc(sizeof(*endgame_subtitle_text) * ENDGAME_ENDING_MAX_SUBTITLES);
    if (endgame_subtitle_text == NULL) {
        endgame_do_subtitles = false;
        return 0;
    }

    for (int index = 0; index < ENDGAME_ENDING_MAX_SUBTITLES; index++) {
        endgame_subtitle_text[index] = NULL;
    }

    endgame_subtitle_times = (unsigned int*)mem_malloc(sizeof(*endgame_subtitle_times) * ENDGAME_ENDING_MAX_SUBTITLES);
    if (endgame_subtitle_times == NULL) {
        mem_free(endgame_subtitle_text);
        endgame_do_subtitles = false;
        return 0;
    }

    return 0;
}

// 0x438C84
static void endgame_exit()
{
    if (endgame_do_subtitles) {
        endgame_clear_subtitles();

        mem_free(endgame_subtitle_times);
        mem_free(endgame_subtitle_text);

        endgame_subtitle_text = NULL;
        endgame_do_subtitles = false;
    }

    text_font(endgame_old_font);

    gsound_speech_callback_set(NULL);
    win_delete(endgame_window);
    win_delete(gEndgameEndingOverlay);

    if (!endgame_mouse_state) {
        mouse_hide();
    }

    gmouse_set_cursor(MOUSE_CURSOR_ARROW);

    loadColorTable("color.pal");
    palette_fade_to(cmap);

    cycle_enable();

    if (endgame_map_enabled) {
        map_enable_bk_processes();
    }
}

// 0x438D14
static void endgame_pan_desert(int direction, const char* narratorFileName)
{
    int fid = art_id(OBJ_TYPE_INTERFACE, 327, 0, 0, 0);

    CacheEntry* backgroundHandle;
    Art* background = art_ptr_lock(fid, &backgroundHandle);
    if (background != NULL) {
        int width = art_frame_width(background, 0, 0);
        int height = art_frame_length(background, 0, 0);
        unsigned char* backgroundData = art_frame_data(background, 0, 0);
        buf_fill(endgame_window_buffer, ENDGAME_ENDING_WINDOW_WIDTH, ENDGAME_ENDING_WINDOW_HEIGHT, ENDGAME_ENDING_WINDOW_WIDTH, colorTable[0]);
        endgame_load_palette(6, 327);

        // CE: Update overlay.
        endgameEndingUpdateOverlay();

        unsigned char palette[768];
        memcpy(palette, cmap, 768);

        palette_set_to(black_palette);
        endgame_load_voiceover(narratorFileName);

        // TODO: Unclear math.
        int v8 = width - 640;
        int v32 = v8 / 4;
        unsigned int v9 = 16 * v8 / v8;
        unsigned int v9_ = 16 * v8;

        if (endgame_voiceover_loaded) {
            unsigned int v10 = 1000 * gsound_speech_length_get();
            if (v10 > v9_ / 2) {
                v9 = (v10 + v9 * (v8 / 2)) / v8;
            }
        }

        int start;
        int end;
        if (direction == -1) {
            start = width - 640;
            end = 0;
        } else {
            start = 0;
            end = width - 640;
        }

        disable_bk();

        bool subtitlesLoaded = false;

        unsigned int since = 0;
        while (start != end) {
            sharedFpsLimiter.mark();

            int v12 = 640 - v32;

            // TODO: Complex math, setup scene in debugger.
            if (elapsed_time(since) >= v9) {
                buf_to_buf(backgroundData + start, ENDGAME_ENDING_WINDOW_WIDTH, ENDGAME_ENDING_WINDOW_HEIGHT, width, endgame_window_buffer, ENDGAME_ENDING_WINDOW_WIDTH);

                if (subtitlesLoaded) {
                    endgame_show_subtitles();
                }

                win_draw(endgame_window);

                since = get_time();

                bool v14;
                double v31;
                if (start > v32) {
                    if (v12 > start) {
                        v14 = false;
                    } else {
                        int v28 = v32 - (start - v12);
                        v31 = (double)v28 / (double)v32;
                        v14 = true;
                    }
                } else {
                    v14 = true;
                    v31 = (double)start / (double)v32;
                }

                if (v14) {
                    unsigned char darkenedPalette[768];
                    for (int index = 0; index < 768; index++) {
                        darkenedPalette[index] = (unsigned char)trunc(palette[index] * v31);
                    }
                    palette_set_to(darkenedPalette);
                }

                start += direction;

                if (direction == 1 && (start == v32)) {
                    // NOTE: Uninline.
                    endgame_play_voiceover();
                    subtitlesLoaded = true;
                } else if (direction == -1 && (start == v12)) {
                    // NOTE: Uninline.
                    endgame_play_voiceover();
                    subtitlesLoaded = true;
                }
            }

            soundUpdate();

            if (get_input() != -1) {
                // NOTE: Uninline.
                endgame_stop_voiceover();
                break;
            }

            renderPresent();
            sharedFpsLimiter.throttle();
        }

        enable_bk();
        art_ptr_unlock(backgroundHandle);

        palette_fade_to(black_palette);
        buf_fill(endgame_window_buffer, ENDGAME_ENDING_WINDOW_WIDTH, ENDGAME_ENDING_WINDOW_HEIGHT, ENDGAME_ENDING_WINDOW_WIDTH, colorTable[0]);
        win_draw(endgame_window);
    }

    while (mouse_get_buttons() != 0) {
        sharedFpsLimiter.mark();

        get_input();

        renderPresent();
        sharedFpsLimiter.throttle();
    }
}

// 0x43915C
static void endgame_display_image(int fid, const char* narratorFileName)
{
    CacheEntry* backgroundHandle;
    Art* background = art_ptr_lock(fid, &backgroundHandle);
    if (background == NULL) {
        return;
    }

    unsigned char* backgroundData = art_frame_data(background, 0, 0);
    if (backgroundData != NULL) {
        buf_to_buf(backgroundData, ENDGAME_ENDING_WINDOW_WIDTH, ENDGAME_ENDING_WINDOW_HEIGHT, ENDGAME_ENDING_WINDOW_WIDTH, endgame_window_buffer, ENDGAME_ENDING_WINDOW_WIDTH);
        win_draw(endgame_window);

        endgame_load_palette(FID_TYPE(fid), fid & 0xFFF);

        // CE: Update overlay.
        endgameEndingUpdateOverlay();

        endgame_load_voiceover(narratorFileName);

        unsigned int delay;
        if (endgame_subtitle_loaded || endgame_voiceover_loaded) {
            delay = UINT_MAX;
        } else {
            delay = 3000;
        }

        palette_fade_to(cmap);

        pause_for_tocks(500);

        // NOTE: Uninline.
        endgame_play_voiceover();

        unsigned int referenceTime = get_time();
        disable_bk();

        int keyCode;
        while (true) {
            sharedFpsLimiter.mark();

            keyCode = get_input();
            if (keyCode != -1) {
                break;
            }

            if (endgame_voiceover_done) {
                break;
            }

            if (endgame_subtitle_done) {
                break;
            }

            if (elapsed_time(referenceTime) > delay) {
                break;
            }

            buf_to_buf(backgroundData, ENDGAME_ENDING_WINDOW_WIDTH, ENDGAME_ENDING_WINDOW_HEIGHT, ENDGAME_ENDING_WINDOW_WIDTH, endgame_window_buffer, ENDGAME_ENDING_WINDOW_WIDTH);
            endgame_show_subtitles();
            win_draw(endgame_window);
            soundUpdate();

            renderPresent();
            sharedFpsLimiter.throttle();
        }

        enable_bk();
        gsound_speech_stop();
        endgame_clear_subtitles();

        endgame_voiceover_loaded = false;
        endgame_subtitle_loaded = false;

        if (keyCode == -1) {
            pause_for_tocks(500);
        }

        palette_fade_to(black_palette);

        while (mouse_get_buttons() != 0) {
            sharedFpsLimiter.mark();

            get_input();

            renderPresent();
            sharedFpsLimiter.throttle();
        }
    }

    art_ptr_unlock(backgroundHandle);
}

// 0x4392F8
static void endgame_load_voiceover(const char* fileBaseName)
{
    char path[COMPAT_MAX_PATH];

    // NOTE: Uninline.
    endgame_stop_voiceover();

    endgame_voiceover_loaded = false;
    endgame_subtitle_loaded = false;

    // Build speech file path.
    snprintf(path, sizeof(path), "%s%s", "narrator\\", fileBaseName);

    if (gsound_speech_play(path, 10, 14, 15) != -1) {
        endgame_voiceover_loaded = true;
    }

    if (endgame_do_subtitles) {
        // Build subtitles file path.
        snprintf(path, sizeof(path), "%s%s.txt", endgame_subtitle_path, fileBaseName);

        if (endgame_load_subtitles(path) != 0) {
            return;
        }

        double durationPerCharacter;
        if (endgame_voiceover_loaded) {
            durationPerCharacter = (double)gsound_speech_length_get() / (double)endgame_subtitle_characters;
        } else {
            durationPerCharacter = 0.08;
        }

        unsigned int timing = 0;
        for (int index = 0; index < endgame_subtitle_count; index++) {
            double charactersCount = strlen(endgame_subtitle_text[index]);
            // NOTE: There is floating point math at 0x4402E6 used to add
            // timing.
            timing += (unsigned int)trunc(charactersCount * durationPerCharacter * 1000.0);
            endgame_subtitle_times[index] = timing;
        }

        endgame_subtitle_loaded = true;
    }
}

// 0x439478
static void endgame_play_voiceover()
{
    endgame_subtitle_done = false;
    endgame_voiceover_done = false;

    if (endgame_voiceover_loaded) {
        gsound_speech_play_preloaded();
    }

    if (endgame_subtitle_loaded) {
        endgame_subtitle_start_time = get_time();
    }
}

// 0x4394B0
static void endgame_stop_voiceover()
{
    gsound_speech_stop();
    endgame_clear_subtitles();
    endgame_voiceover_loaded = false;
    endgame_subtitle_loaded = false;
}

// 0x4394CC
static void endgame_load_palette(int type, int id)
{
    char fileName[13];
    if (art_get_base_name(type, id, fileName) != 0) {
        return;
    }

    // Remove extension from file name.
    char* pch = strrchr(fileName, '.');
    if (pch != NULL) {
        *pch = '\0';
    }

    if (strlen(fileName) <= 8) {
        char path[COMPAT_MAX_PATH];
        snprintf(path, sizeof(path), "%s\\%s.pal", "art\\intrface", fileName);
        loadColorTable(path);
    }
}

// 0x439544
static void endgame_voiceover_callback()
{
    endgame_voiceover_done = true;
}

// Loads subtitles file.
//
// 0x439550
static int endgame_load_subtitles(const char* filePath)
{
    endgame_clear_subtitles();

    DB_FILE* stream = db_fopen(filePath, "rt");
    if (stream == NULL) {
        return -1;
    }

    // FIXME: There is at least one subtitle for Arroyo ending (nar_ar1) that
    // does not fit into this buffer.
    char string[256];
    while (db_fgets(string, sizeof(string), stream)) {
        char* pch;

        // Find and clamp string at EOL.
        pch = strchr(string, '\n');
        if (pch != NULL) {
            *pch = '\0';
        }

        // Find separator. The value before separator is ignored (as opposed to
        // movie subtitles, where the value before separator is a timing).
        pch = strchr(string, ':');
        if (pch != NULL) {
            if (endgame_subtitle_count < ENDGAME_ENDING_MAX_SUBTITLES) {
                endgame_subtitle_text[endgame_subtitle_count] = mem_strdup(pch + 1);
                endgame_subtitle_count++;
                endgame_subtitle_characters += strlen(pch + 1);
            }
        }
    }

    db_fclose(stream);

    return 0;
}

// Refreshes subtitles.
//
// 0x439640
static void endgame_show_subtitles()
{
    if (endgame_subtitle_count <= endgame_current_subtitle) {
        if (endgame_subtitle_loaded) {
            endgame_subtitle_done = true;
        }
        return;
    }

    if (elapsed_time(endgame_subtitle_start_time) > endgame_subtitle_times[endgame_current_subtitle]) {
        endgame_current_subtitle++;
        return;
    }

    char* text = endgame_subtitle_text[endgame_current_subtitle];
    if (text == NULL) {
        return;
    }

    short beginnings[WORD_WRAP_MAX_COUNT];
    short count;
    if (word_wrap(text, 540, beginnings, &count) != 0) {
        return;
    }

    int height = text_height();
    int y = 480 - height * count;

    for (int index = 0; index < count - 1; index++) {
        char* beginning = text + beginnings[index];
        char* ending = text + beginnings[index + 1];

        if (ending[-1] == ' ') {
            ending--;
        }

        char c = *ending;
        *ending = '\0';

        int width = text_width(beginning);
        int x = (640 - width) / 2;
        buf_fill(endgame_window_buffer + 640 * y + x, width, height, 640, colorTable[0]);
        text_to_buf(endgame_window_buffer + 640 * y + x, beginning, width, 640, colorTable[32767]);

        *ending = c;

        y += height;
    }
}

// 0x439820
static void endgame_clear_subtitles()
{
    for (int index = 0; index < endgame_subtitle_count; index++) {
        if (endgame_subtitle_text[index] != NULL) {
            mem_free(endgame_subtitle_text[index]);
            endgame_subtitle_text[index] = NULL;
        }
    }

    endgame_current_subtitle = 0;
    endgame_subtitle_characters = 0;
    endgame_subtitle_count = 0;
}

// 0x43987C
static void endgame_movie_callback()
{
    endgame_maybe_done = 1;
}

// 0x439888
static void endgame_movie_bk_process()
{
    if (endgame_maybe_done) {
        gsound_background_play("10labone", 11, 14, 16);
        gsound_background_callback_set(NULL);
        remove_bk_process(endgame_movie_bk_process);
    }
}

void endgameEndingUpdateOverlay()
{
    buf_fill(win_get_buf(gEndgameEndingOverlay),
        win_width(gEndgameEndingOverlay),
        win_height(gEndgameEndingOverlay),
        win_width(gEndgameEndingOverlay),
        intensityColorTable[colorTable[0]][0]);
    win_draw(gEndgameEndingOverlay);
}

} // namespace fallout
