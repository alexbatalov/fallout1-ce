#include "game/select.h"

#include <stdio.h>
#include <string.h>

#include "game/art.h"
#include "game/critter.h"
#include "game/editor.h"
#include "game/game.h"
#include "game/gsound.h"
#include "game/message.h"
#include "game/object.h"
#include "game/options.h"
#include "game/palette.h"
#include "game/proto.h"
#include "game/skill.h"
#include "game/stat.h"
#include "game/trait.h"
#include "platform_compat.h"
#include "plib/color/color.h"
#include "plib/db/db.h"
#include "plib/gnw/button.h"
#include "plib/gnw/debug.h"
#include "plib/gnw/gnw.h"
#include "plib/gnw/grbuf.h"
#include "plib/gnw/input.h"
#include "plib/gnw/memory.h"
#include "plib/gnw/svga.h"
#include "plib/gnw/text.h"

namespace fallout {

#define CS_WINDOW_WIDTH 640
#define CS_WINDOW_HEIGHT 480

#define CS_WINDOW_BACKGROUND_X 40
#define CS_WINDOW_BACKGROUND_Y 30
#define CS_WINDOW_BACKGROUND_WIDTH 560
#define CS_WINDOW_BACKGROUND_HEIGHT 300

#define CS_WINDOW_PREVIOUS_BUTTON_X 292
#define CS_WINDOW_PREVIOUS_BUTTON_Y 320

#define CS_WINDOW_NEXT_BUTTON_X 318
#define CS_WINDOW_NEXT_BUTTON_Y 320

#define CS_WINDOW_TAKE_BUTTON_X 81
#define CS_WINDOW_TAKE_BUTTON_Y 323

#define CS_WINDOW_MODIFY_BUTTON_X 435
#define CS_WINDOW_MODIFY_BUTTON_Y 320

#define CS_WINDOW_CREATE_BUTTON_X 80
#define CS_WINDOW_CREATE_BUTTON_Y 425

#define CS_WINDOW_BACK_BUTTON_X 461
#define CS_WINDOW_BACK_BUTTON_Y 425

#define CS_WINDOW_NAME_MID_X 318
#define CS_WINDOW_PRIMARY_STAT_MID_X 348
#define CS_WINDOW_SECONDARY_STAT_MID_X 365
#define CS_WINDOW_BIO_X 420

typedef enum PremadeCharacter {
    PREMADE_CHARACTER_NARG,
    PREMADE_CHARACTER_CHITSA,
    PREMADE_CHARACTER_MINGUN,
    PREMADE_CHARACTER_COUNT,
} PremadeCharacter;

typedef struct PremadeCharacterDescription {
    char fileName[20];
    int face;
    char vid[20];
} PremadeCharacterDescription;

static void select_exit();
static bool select_update_display();
static bool select_display_portrait();
static bool select_display_stats();
static bool select_display_bio();
static bool select_fatal_error(bool rc);

// 0x50797C
int select_window_id = -1;

// 0x507980
static unsigned char* select_window_buffer = NULL;

// 0x507984
static unsigned char* monitor = NULL;

// 0x507988
static Rect monitor_rect = { 40, 30, 599, 329 };

// 0x507998
static int previous_button = -1;

// 0x50799C
static CacheEntry* previous_button_up_key = NULL;

// 0x5079A0
static CacheEntry* previous_button_down_key = NULL;

// 0x5079A4
static int next_button = -1;

// 0x5079A8
static CacheEntry* next_button_up_key = NULL;

// 0x5079AC
static CacheEntry* next_button_down_key = NULL;

// 0x5079B0
static int take_button = -1;

// 0x5079B4
static CacheEntry* take_button_up_key = NULL;

// 0x5079B8
static CacheEntry* take_button_down_key = NULL;

// 0x5079BC
static int modify_button = -1;

// 0x5079C0
static CacheEntry* modify_button_up_key = NULL;

// 0x5079C4
static CacheEntry* modify_button_down_key = NULL;

// 0x5079C8
static int create_button = -1;

// 0x5079CC
static CacheEntry* create_button_up_key = NULL;

// 0x5079D0
static CacheEntry* create_button_down_key = NULL;

// 0x5079D4
static int back_button = -1;

// 0x5079D8
static CacheEntry* back_button_up_key = NULL;

// 0x5079DC
static CacheEntry* back_button_down_key = NULL;

// 0x5079E0
static int premade_index = PREMADE_CHARACTER_NARG;

// 0x5079E4
static PremadeCharacterDescription premade_characters[PREMADE_CHARACTER_COUNT] = {
    { "premade\\combat", 201, "VID 208-197-88-125" },
    { "premade\\stealth", 202, "VID 208-206-49-229" },
    { "premade\\diplomat", 203, "VID 208-206-49-227" },
};

// 0x507A68
static int premade_total = PREMADE_CHARACTER_COUNT;

// 0x664FCC
static unsigned char* take_button_up;

// 0x664FD4
static unsigned char* modify_button_down;

// 0x664FBC
static unsigned char* back_button_up;

// 0x664FE0
static unsigned char* create_button_up;

// 0x664FDC
static unsigned char* modify_button_up;

// 0x664FC0
static unsigned char* back_button_down;

// 0x664FE0
static unsigned char* create_button_down;

// 0x664FE4
static unsigned char* take_button_down;

// 0x664FC4
static unsigned char* next_button_down;

// 0x664FC8
static unsigned char* next_button_up;

// 0x664FD0
static unsigned char* previous_button_up;

// 0x664FE8
static unsigned char* previous_button_down;

// 0x495260
int select_character()
{
    if (!select_init()) {
        return 0;
    }

    bool cursorWasHidden = mouse_hidden();
    if (cursorWasHidden) {
        mouse_show();
    }

    loadColorTable("color.pal");
    palette_fade_to(cmap);

    int rc = 0;
    bool done = false;
    while (!done) {
        sharedFpsLimiter.mark();

        if (game_user_wants_to_quit != 0) {
            break;
        }

        int keyCode = get_input();

        switch (keyCode) {
        case KEY_MINUS:
        case KEY_UNDERSCORE:
            DecGamma();
            break;
        case KEY_EQUAL:
        case KEY_PLUS:
            IncGamma();
            break;
        case KEY_UPPERCASE_B:
        case KEY_LOWERCASE_B:
        case KEY_ESCAPE:
            rc = 3;
            done = true;
            break;
        case KEY_UPPERCASE_C:
        case KEY_LOWERCASE_C:
            ResetPlayer();
            if (editor_design(1) == 0) {
                rc = 2;
                done = true;
            } else {
                select_update_display();
            }

            break;
        case KEY_UPPERCASE_M:
        case KEY_LOWERCASE_M:
            if (!editor_design(1)) {
                rc = 2;
                done = true;
            } else {
                select_update_display();
            }

            break;
        case KEY_UPPERCASE_T:
        case KEY_LOWERCASE_T:
            rc = 2;
            done = true;

            break;
        case KEY_F10:
            game_quit_with_confirm();
            break;
        case KEY_ARROW_LEFT:
            gsound_play_sfx_file("ib2p1xx1");
            // FALLTHROUGH
        case 500:
            premade_index -= 1;
            if (premade_index < 0) {
                premade_index = premade_total - 1;
            }

            select_update_display();
            break;
        case KEY_ARROW_RIGHT:
            gsound_play_sfx_file("ib2p1xx1");
            // FALLTHROUGH
        case 501:
            premade_index += 1;
            if (premade_index >= premade_total) {
                premade_index = 0;
            }

            select_update_display();
            break;
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    palette_fade_to(black_palette);
    select_exit();

    if (cursorWasHidden) {
        mouse_hide();
    }

    return rc;
}

// 0x4954F8
bool select_init()
{
    int backgroundFid;
    unsigned char* backgroundFrmData;

    if (select_window_id != -1) {
        return false;
    }

    int characterSelectorWindowX = (screenGetWidth() - CS_WINDOW_WIDTH) / 2;
    int characterSelectorWindowY = (screenGetHeight() - CS_WINDOW_HEIGHT) / 2;
    select_window_id = win_add(characterSelectorWindowX, characterSelectorWindowY, CS_WINDOW_WIDTH, CS_WINDOW_HEIGHT, colorTable[0], 0);
    if (select_window_id == -1) {
        return select_fatal_error(false);
    }

    select_window_buffer = win_get_buf(select_window_id);
    if (select_window_buffer == NULL) {
        return select_fatal_error(false);
    }

    CacheEntry* backgroundFrmHandle;
    backgroundFid = art_id(OBJ_TYPE_INTERFACE, 174, 0, 0, 0);
    backgroundFrmData = art_ptr_lock_data(backgroundFid, 0, 0, &backgroundFrmHandle);
    if (backgroundFrmData == NULL) {
        return select_fatal_error(false);
    }

    buf_to_buf(backgroundFrmData,
        CS_WINDOW_WIDTH,
        CS_WINDOW_HEIGHT,
        CS_WINDOW_WIDTH,
        select_window_buffer,
        CS_WINDOW_WIDTH);

    monitor = (unsigned char*)mem_malloc(CS_WINDOW_BACKGROUND_WIDTH * CS_WINDOW_BACKGROUND_HEIGHT);
    if (monitor == NULL)
        return select_fatal_error(false);

    buf_to_buf(backgroundFrmData + CS_WINDOW_WIDTH * CS_WINDOW_BACKGROUND_Y + CS_WINDOW_BACKGROUND_X,
        CS_WINDOW_BACKGROUND_WIDTH,
        CS_WINDOW_BACKGROUND_HEIGHT,
        CS_WINDOW_WIDTH,
        monitor,
        CS_WINDOW_BACKGROUND_WIDTH);

    art_ptr_unlock(backgroundFrmHandle);

    int fid;

    // Setup "Previous" button.
    fid = art_id(OBJ_TYPE_INTERFACE, 122, 0, 0, 0);
    previous_button_up = art_ptr_lock_data(fid, 0, 0, &previous_button_up_key);
    if (previous_button_up == NULL) {
        return select_fatal_error(false);
    }

    fid = art_id(OBJ_TYPE_INTERFACE, 123, 0, 0, 0);
    previous_button_down = art_ptr_lock_data(fid, 0, 0, &previous_button_down_key);
    if (previous_button_down == NULL) {
        return select_fatal_error(false);
    }

    previous_button = win_register_button(select_window_id,
        CS_WINDOW_PREVIOUS_BUTTON_X,
        CS_WINDOW_PREVIOUS_BUTTON_Y,
        20,
        18,
        -1,
        -1,
        -1,
        500,
        previous_button_up,
        previous_button_down,
        NULL,
        0);
    if (previous_button == -1) {
        return select_fatal_error(false);
    }

    win_register_button_sound_func(previous_button, gsound_med_butt_press, gsound_med_butt_release);

    // Setup "Next" button.
    fid = art_id(OBJ_TYPE_INTERFACE, 124, 0, 0, 0);
    next_button_up = art_ptr_lock_data(fid, 0, 0, &next_button_up_key);
    if (next_button_up == NULL) {
        return select_fatal_error(false);
    }

    fid = art_id(OBJ_TYPE_INTERFACE, 125, 0, 0, 0);
    next_button_down = art_ptr_lock_data(fid, 0, 0, &next_button_down_key);
    if (next_button_down == NULL) {
        return select_fatal_error(false);
    }

    next_button = win_register_button(select_window_id,
        CS_WINDOW_NEXT_BUTTON_X,
        CS_WINDOW_NEXT_BUTTON_Y,
        20,
        18,
        -1,
        -1,
        -1,
        501,
        next_button_up,
        next_button_down,
        NULL,
        0);
    if (next_button == -1) {
        return select_fatal_error(false);
    }

    win_register_button_sound_func(next_button, gsound_med_butt_press, gsound_med_butt_release);

    // Setup "Take" button.
    fid = art_id(OBJ_TYPE_INTERFACE, 8, 0, 0, 0);
    take_button_up = art_ptr_lock_data(fid, 0, 0, &take_button_up_key);
    if (take_button_up == NULL) {
        return select_fatal_error(false);
    }

    fid = art_id(OBJ_TYPE_INTERFACE, 9, 0, 0, 0);
    take_button_down = art_ptr_lock_data(fid, 0, 0, &take_button_down_key);
    if (take_button_down == NULL) {
        return select_fatal_error(false);
    }

    take_button = win_register_button(select_window_id,
        CS_WINDOW_TAKE_BUTTON_X,
        CS_WINDOW_TAKE_BUTTON_Y,
        15,
        16,
        -1,
        -1,
        -1,
        KEY_LOWERCASE_T,
        take_button_up,
        take_button_down,
        NULL,
        BUTTON_FLAG_TRANSPARENT);
    if (take_button == -1) {
        return select_fatal_error(false);
    }

    win_register_button_sound_func(take_button, gsound_red_butt_press, gsound_red_butt_release);

    // Setup "Modify" button.
    fid = art_id(OBJ_TYPE_INTERFACE, 8, 0, 0, 0);
    modify_button_up = art_ptr_lock_data(fid, 0, 0, &modify_button_up_key);
    if (modify_button_up == NULL)
        return select_fatal_error(false);

    fid = art_id(OBJ_TYPE_INTERFACE, 9, 0, 0, 0);
    modify_button_down = art_ptr_lock_data(fid, 0, 0, &modify_button_down_key);
    if (modify_button_down == NULL) {
        return select_fatal_error(false);
    }

    modify_button = win_register_button(select_window_id,
        CS_WINDOW_MODIFY_BUTTON_X,
        CS_WINDOW_MODIFY_BUTTON_Y,
        15,
        16,
        -1,
        -1,
        -1,
        KEY_LOWERCASE_M,
        modify_button_up,
        modify_button_down,
        NULL,
        BUTTON_FLAG_TRANSPARENT);
    if (modify_button == -1) {
        return select_fatal_error(false);
    }

    win_register_button_sound_func(modify_button, gsound_red_butt_press, gsound_red_butt_release);

    // Setup "Create" button.
    fid = art_id(OBJ_TYPE_INTERFACE, 8, 0, 0, 0);
    create_button_up = art_ptr_lock_data(fid, 0, 0, &create_button_up_key);
    if (create_button_up == NULL) {
        return select_fatal_error(false);
    }

    fid = art_id(OBJ_TYPE_INTERFACE, 9, 0, 0, 0);
    create_button_down = art_ptr_lock_data(fid, 0, 0, &create_button_down_key);
    if (create_button_down == NULL) {
        return select_fatal_error(false);
    }

    create_button = win_register_button(select_window_id,
        CS_WINDOW_CREATE_BUTTON_X,
        CS_WINDOW_CREATE_BUTTON_Y,
        15,
        16,
        -1,
        -1,
        -1,
        KEY_LOWERCASE_C,
        create_button_up,
        create_button_down,
        NULL,
        BUTTON_FLAG_TRANSPARENT);
    if (create_button == -1) {
        return select_fatal_error(false);
    }

    win_register_button_sound_func(create_button, gsound_red_butt_press, gsound_red_butt_release);

    // Setup "Back" button.
    fid = art_id(OBJ_TYPE_INTERFACE, 8, 0, 0, 0);
    back_button_up = art_ptr_lock_data(fid, 0, 0, &back_button_up_key);
    if (back_button_up == NULL) {
        return select_fatal_error(false);
    }

    fid = art_id(OBJ_TYPE_INTERFACE, 9, 0, 0, 0);
    back_button_down = art_ptr_lock_data(fid, 0, 0, &back_button_down_key);
    if (back_button_down == NULL) {
        return select_fatal_error(false);
    }

    back_button = win_register_button(select_window_id,
        CS_WINDOW_BACK_BUTTON_X,
        CS_WINDOW_BACK_BUTTON_Y,
        15,
        16,
        -1,
        -1,
        -1,
        KEY_ESCAPE,
        back_button_up,
        back_button_down,
        NULL,
        BUTTON_FLAG_TRANSPARENT);
    if (back_button == -1) {
        return select_fatal_error(false);
    }

    win_register_button_sound_func(back_button, gsound_red_butt_press, gsound_red_butt_release);

    premade_index = PREMADE_CHARACTER_NARG;

    win_draw(select_window_id);

    if (!select_update_display()) {
        return select_fatal_error(false);
    }

    return true;
}

// 0x495B64
static void select_exit()
{
    if (select_window_id == -1) {
        return;
    }

    if (previous_button != -1) {
        win_delete_button(previous_button);
        previous_button = -1;
    }

    if (previous_button_down != NULL) {
        art_ptr_unlock(previous_button_down_key);
        previous_button_down_key = NULL;
        previous_button_down = NULL;
    }

    if (previous_button_up != NULL) {
        art_ptr_unlock(previous_button_up_key);
        previous_button_up_key = NULL;
        previous_button_up = NULL;
    }

    if (next_button != -1) {
        win_delete_button(next_button);
        next_button = -1;
    }

    if (next_button_down != NULL) {
        art_ptr_unlock(next_button_down_key);
        next_button_down_key = NULL;
        next_button_down = NULL;
    }

    if (next_button_up != NULL) {
        art_ptr_unlock(next_button_up_key);
        next_button_up_key = NULL;
        next_button_up = NULL;
    }

    if (take_button != -1) {
        win_delete_button(take_button);
        take_button = -1;
    }

    if (take_button_down != NULL) {
        art_ptr_unlock(take_button_down_key);
        take_button_down_key = NULL;
        take_button_down = NULL;
    }

    if (take_button_up != NULL) {
        art_ptr_unlock(take_button_up_key);
        take_button_up_key = NULL;
        take_button_up = NULL;
    }

    if (modify_button != -1) {
        win_delete_button(modify_button);
        modify_button = -1;
    }

    if (modify_button_down != NULL) {
        art_ptr_unlock(modify_button_down_key);
        modify_button_down_key = NULL;
        modify_button_down = NULL;
    }

    if (modify_button_up != NULL) {
        art_ptr_unlock(modify_button_up_key);
        modify_button_up_key = NULL;
        modify_button_up = NULL;
    }

    if (create_button != -1) {
        win_delete_button(create_button);
        create_button = -1;
    }

    if (create_button_down != NULL) {
        art_ptr_unlock(create_button_down_key);
        create_button_down_key = NULL;
        create_button_down = NULL;
    }

    if (create_button_up != NULL) {
        art_ptr_unlock(create_button_up_key);
        create_button_up_key = NULL;
        create_button_up = NULL;
    }

    if (back_button != -1) {
        win_delete_button(back_button);
        back_button = -1;
    }

    if (back_button_down != NULL) {
        art_ptr_unlock(back_button_down_key);
        back_button_down_key = NULL;
        back_button_down = NULL;
    }

    if (back_button_up != NULL) {
        art_ptr_unlock(back_button_up_key);
        back_button_up_key = NULL;
        back_button_up = NULL;
    }

    if (monitor != NULL) {
        mem_free(monitor);
        monitor = NULL;
    }

    win_delete(select_window_id);
    select_window_id = -1;
}

// 0x495DE8
static bool select_update_display()
{
    char path[COMPAT_MAX_PATH];
    snprintf(path, sizeof(path), "%s.gcd", premade_characters[premade_index].fileName);
    if (proto_dude_init(path) == -1) {
        debug_printf("\n ** Error in dude init! **\n");
        return false;
    }

    buf_to_buf(monitor,
        CS_WINDOW_BACKGROUND_WIDTH,
        CS_WINDOW_BACKGROUND_HEIGHT,
        CS_WINDOW_BACKGROUND_WIDTH,
        select_window_buffer + CS_WINDOW_WIDTH * CS_WINDOW_BACKGROUND_Y + CS_WINDOW_BACKGROUND_X,
        CS_WINDOW_WIDTH);

    bool success = false;
    if (select_display_portrait()) {
        if (select_display_stats()) {
            success = select_display_bio();
        }
    }

    win_draw_rect(select_window_id, &monitor_rect);

    return success;
}

// 0x495E9C
static bool select_display_portrait()
{
    int old_font;
    int id_width;
    bool success = false;

    CacheEntry* faceFrmHandle;
    int faceFid = art_id(OBJ_TYPE_INTERFACE, premade_characters[premade_index].face, 0, 0, 0);
    Art* frm = art_ptr_lock(faceFid, &faceFrmHandle);
    if (frm != NULL) {
        unsigned char* data = art_frame_data(frm, 0, 0);
        if (data != NULL) {
            int width = art_frame_width(frm, 0, 0);
            int height = art_frame_length(frm, 0, 0);
            int y;

            for (y = 1; y < height; y += 2) {
                memset(data + y * width, 0, width);
            }

            trans_buf_to_buf(data,
                width,
                height,
                width,
                select_window_buffer + CS_WINDOW_WIDTH * (240 - height) + 150 - (width / 2),
                CS_WINDOW_WIDTH);

            old_font = text_curr();
            text_font(101);

            id_width = text_width(premade_characters[premade_index].vid);
            text_to_buf(select_window_buffer + CS_WINDOW_WIDTH * 252 + 150 - id_width / 2,
                premade_characters[premade_index].vid,
                id_width,
                CS_WINDOW_WIDTH,
                colorTable[992]);

            text_font(old_font);

            success = true;
        }
        art_ptr_unlock(faceFrmHandle);
    }

    return success;
}

// 0x49602C
static bool select_display_stats()
{
    char* str;
    char text[260];
    int length;
    int value;
    MessageListItem messageListItem;

    int oldFont = text_curr();
    text_font(101);

    text_char_width(0x20);

    int vh = text_height();
    int y = 40;

    // NAME
    str = object_name(obj_dude);
    strcpy(text, str);

    length = text_width(text);
    text_to_buf(select_window_buffer + CS_WINDOW_WIDTH * y + CS_WINDOW_NAME_MID_X - (length / 2), text, 160, CS_WINDOW_WIDTH, colorTable[992]);

    // STRENGTH
    y += vh + vh + vh;

    value = stat_level(obj_dude, STAT_STRENGTH);
    str = stat_name(STAT_STRENGTH);

    snprintf(text, sizeof(text), "%s %02d", str, value);

    length = text_width(text);
    text_to_buf(select_window_buffer + CS_WINDOW_WIDTH * y + CS_WINDOW_PRIMARY_STAT_MID_X - length, text, length, CS_WINDOW_WIDTH, colorTable[992]);

    str = stat_level_description(value);
    snprintf(text, sizeof(text), "  %s", str);

    length = text_width(text);
    text_to_buf(select_window_buffer + CS_WINDOW_WIDTH * y + CS_WINDOW_PRIMARY_STAT_MID_X, text, length, CS_WINDOW_WIDTH, colorTable[992]);

    // PERCEPTION
    y += vh;

    value = stat_level(obj_dude, STAT_PERCEPTION);
    str = stat_name(STAT_PERCEPTION);

    snprintf(text, sizeof(text), "%s %02d", str, value);

    length = text_width(text);
    text_to_buf(select_window_buffer + CS_WINDOW_WIDTH * y + CS_WINDOW_PRIMARY_STAT_MID_X - length, text, length, CS_WINDOW_WIDTH, colorTable[992]);

    str = stat_level_description(value);
    snprintf(text, sizeof(text), "  %s", str);

    length = text_width(text);
    text_to_buf(select_window_buffer + CS_WINDOW_WIDTH * y + CS_WINDOW_PRIMARY_STAT_MID_X, text, length, CS_WINDOW_WIDTH, colorTable[992]);

    // ENDURANCE
    y += vh;

    value = stat_level(obj_dude, STAT_ENDURANCE);
    str = stat_name(STAT_ENDURANCE);

    snprintf(text, sizeof(text), "%s %02d", str, value);

    length = text_width(text);
    text_to_buf(select_window_buffer + CS_WINDOW_WIDTH * y + CS_WINDOW_PRIMARY_STAT_MID_X - length, text, length, CS_WINDOW_WIDTH, colorTable[992]);

    str = stat_level_description(value);
    snprintf(text, sizeof(text), "  %s", str);

    length = text_width(text);
    text_to_buf(select_window_buffer + CS_WINDOW_WIDTH * y + CS_WINDOW_PRIMARY_STAT_MID_X, text, length, CS_WINDOW_WIDTH, colorTable[992]);

    // CHARISMA
    y += vh;

    value = stat_level(obj_dude, STAT_CHARISMA);
    str = stat_name(STAT_CHARISMA);

    snprintf(text, sizeof(text), "%s %02d", str, value);

    length = text_width(text);
    text_to_buf(select_window_buffer + CS_WINDOW_WIDTH * y + CS_WINDOW_PRIMARY_STAT_MID_X - length, text, length, CS_WINDOW_WIDTH, colorTable[992]);

    str = stat_level_description(value);
    snprintf(text, sizeof(text), "  %s", str);

    length = text_width(text);
    text_to_buf(select_window_buffer + CS_WINDOW_WIDTH * y + CS_WINDOW_PRIMARY_STAT_MID_X, text, length, CS_WINDOW_WIDTH, colorTable[992]);

    // INTELLIGENCE
    y += vh;

    value = stat_level(obj_dude, STAT_INTELLIGENCE);
    str = stat_name(STAT_INTELLIGENCE);

    snprintf(text, sizeof(text), "%s %02d", str, value);

    length = text_width(text);
    text_to_buf(select_window_buffer + CS_WINDOW_WIDTH * y + CS_WINDOW_PRIMARY_STAT_MID_X - length, text, length, CS_WINDOW_WIDTH, colorTable[992]);

    str = stat_level_description(value);
    snprintf(text, sizeof(text), "  %s", str);

    length = text_width(text);
    text_to_buf(select_window_buffer + CS_WINDOW_WIDTH * y + CS_WINDOW_PRIMARY_STAT_MID_X, text, length, CS_WINDOW_WIDTH, colorTable[992]);

    // AGILITY
    y += vh;

    value = stat_level(obj_dude, STAT_AGILITY);
    str = stat_name(STAT_AGILITY);

    snprintf(text, sizeof(text), "%s %02d", str, value);

    length = text_width(text);
    text_to_buf(select_window_buffer + CS_WINDOW_WIDTH * y + CS_WINDOW_PRIMARY_STAT_MID_X - length, text, length, CS_WINDOW_WIDTH, colorTable[992]);

    str = stat_level_description(value);
    snprintf(text, sizeof(text), "  %s", str);

    length = text_width(text);
    text_to_buf(select_window_buffer + CS_WINDOW_WIDTH * y + CS_WINDOW_PRIMARY_STAT_MID_X, text, length, CS_WINDOW_WIDTH, colorTable[992]);

    // LUCK
    y += vh;

    value = stat_level(obj_dude, STAT_LUCK);
    str = stat_name(STAT_LUCK);

    snprintf(text, sizeof(text), "%s %02d", str, value);

    length = text_width(text);
    text_to_buf(select_window_buffer + CS_WINDOW_WIDTH * y + CS_WINDOW_PRIMARY_STAT_MID_X - length, text, length, CS_WINDOW_WIDTH, colorTable[992]);

    str = stat_level_description(value);
    snprintf(text, sizeof(text), "  %s", str);

    length = text_width(text);
    text_to_buf(select_window_buffer + CS_WINDOW_WIDTH * y + CS_WINDOW_PRIMARY_STAT_MID_X, text, length, CS_WINDOW_WIDTH, colorTable[992]);

    y += vh; // blank line

    // HIT POINTS
    y += vh;

    messageListItem.num = 16;
    text[0] = '\0';
    if (message_search(&misc_message_file, &messageListItem)) {
        strcpy(text, messageListItem.text);
    }

    length = text_width(text);
    text_to_buf(select_window_buffer + CS_WINDOW_WIDTH * y + CS_WINDOW_SECONDARY_STAT_MID_X - length, text, length, CS_WINDOW_WIDTH, colorTable[992]);

    value = stat_level(obj_dude, STAT_MAXIMUM_HIT_POINTS);
    snprintf(text, sizeof(text), " %d/%d", critter_get_hits(obj_dude), value);

    length = text_width(text);
    text_to_buf(select_window_buffer + CS_WINDOW_WIDTH * y + CS_WINDOW_SECONDARY_STAT_MID_X, text, length, CS_WINDOW_WIDTH, colorTable[992]);

    // ARMOR CLASS
    y += vh;

    str = stat_name(STAT_ARMOR_CLASS);
    strcpy(text, str);

    length = text_width(text);
    text_to_buf(select_window_buffer + CS_WINDOW_WIDTH * y + CS_WINDOW_SECONDARY_STAT_MID_X - length, text, length, CS_WINDOW_WIDTH, colorTable[992]);

    value = stat_level(obj_dude, STAT_ARMOR_CLASS);
    snprintf(text, sizeof(text), " %d", value);

    length = text_width(text);
    text_to_buf(select_window_buffer + CS_WINDOW_WIDTH * y + CS_WINDOW_SECONDARY_STAT_MID_X, text, length, CS_WINDOW_WIDTH, colorTable[992]);

    // ACTION POINTS
    y += vh;

    messageListItem.num = 15;
    text[0] = '\0';
    if (message_search(&misc_message_file, &messageListItem)) {
        strcpy(text, messageListItem.text);
    }

    length = text_width(text);
    text_to_buf(select_window_buffer + CS_WINDOW_WIDTH * y + CS_WINDOW_SECONDARY_STAT_MID_X - length, text, length, CS_WINDOW_WIDTH, colorTable[992]);

    value = stat_level(obj_dude, STAT_MAXIMUM_ACTION_POINTS);
    snprintf(text, sizeof(text), " %d", value);

    length = text_width(text);
    text_to_buf(select_window_buffer + CS_WINDOW_WIDTH * y + CS_WINDOW_SECONDARY_STAT_MID_X, text, length, CS_WINDOW_WIDTH, colorTable[992]);

    // MELEE DAMAGE
    y += vh;

    str = stat_name(STAT_MELEE_DAMAGE);
    strcpy(text, str);

    length = text_width(text);
    text_to_buf(select_window_buffer + CS_WINDOW_WIDTH * y + CS_WINDOW_SECONDARY_STAT_MID_X - length, text, length, CS_WINDOW_WIDTH, colorTable[992]);

    value = stat_level(obj_dude, STAT_MELEE_DAMAGE);
    snprintf(text, sizeof(text), " %d", value);

    length = text_width(text);
    text_to_buf(select_window_buffer + CS_WINDOW_WIDTH * y + CS_WINDOW_SECONDARY_STAT_MID_X, text, length, CS_WINDOW_WIDTH, colorTable[992]);

    y += vh; // blank line

    // SKILLS
    int skills[DEFAULT_TAGGED_SKILLS];
    skill_get_tags(skills, DEFAULT_TAGGED_SKILLS);

    for (int index = 0; index < DEFAULT_TAGGED_SKILLS; index++) {
        y += vh;

        str = skill_name(skills[index]);
        strcpy(text, str);

        length = text_width(text);
        text_to_buf(select_window_buffer + CS_WINDOW_WIDTH * y + CS_WINDOW_SECONDARY_STAT_MID_X - length, text, length, CS_WINDOW_WIDTH, colorTable[992]);

        value = skill_level(obj_dude, skills[index]);
        snprintf(text, sizeof(text), " %d%%", value);

        length = text_width(text);
        text_to_buf(select_window_buffer + CS_WINDOW_WIDTH * y + CS_WINDOW_SECONDARY_STAT_MID_X, text, length, CS_WINDOW_WIDTH, colorTable[992]);
    }

    // TRAITS
    int traits[PC_TRAIT_MAX];
    trait_get(&(traits[0]), &(traits[1]));

    for (int index = 0; index < PC_TRAIT_MAX; index++) {
        y += vh;

        str = trait_name(traits[index]);
        strcpy(text, str);

        length = text_width(text);
        text_to_buf(select_window_buffer + CS_WINDOW_WIDTH * y + CS_WINDOW_SECONDARY_STAT_MID_X - length, text, length, CS_WINDOW_WIDTH, colorTable[992]);
    }

    text_font(oldFont);

    return true;
}

// 0x496C60
static bool select_display_bio()
{
    int oldFont = text_curr();
    text_font(101);

    char path[COMPAT_MAX_PATH];
    snprintf(path, sizeof(path), "%s.bio", premade_characters[premade_index].fileName);

    DB_FILE* stream = db_fopen(path, "rt");
    if (stream != NULL) {
        int y = 40;
        int lineHeight = text_height();

        char string[256];
        while (db_fgets(string, 256, stream) && y < 260) {
            text_to_buf(select_window_buffer + CS_WINDOW_WIDTH * y + CS_WINDOW_BIO_X,
                string,
                CS_WINDOW_WIDTH - CS_WINDOW_BIO_X,
                CS_WINDOW_WIDTH,
                colorTable[992]);
            y += lineHeight;
        }

        db_fclose(stream);
    }

    text_font(oldFont);

    return true;
}

// 0x496D4C
static bool select_fatal_error(bool rc)
{
    select_exit();
    return rc;
}

} // namespace fallout
