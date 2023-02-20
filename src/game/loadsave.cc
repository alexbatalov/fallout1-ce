#include "game/loadsave.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <algorithm>

#include "game/automap.h"
#include "game/bmpdlog.h"
#include "game/combat.h"
#include "game/combatai.h"
#include "game/critter.h"
#include "game/cycle.h"
#include "game/display.h"
#include "game/editor.h"
#include "game/game.h"
#include "game/gconfig.h"
#include "game/gmouse.h"
#include "game/gmovie.h"
#include "game/gsound.h"
#include "game/intface.h"
#include "game/item.h"
#include "game/map.h"
#include "game/object.h"
#include "game/options.h"
#include "game/perk.h"
#include "game/pipboy.h"
#include "game/proto.h"
#include "game/queue.h"
#include "game/roll.h"
#include "game/scripts.h"
#include "game/skill.h"
#include "game/stat.h"
#include "game/tile.h"
#include "game/trait.h"
#include "game/version.h"
#include "game/wordwrap.h"
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

#define LOAD_SAVE_SIGNATURE "FALLOUT SAVE FILE"
#define LOAD_SAVE_DESCRIPTION_LENGTH 30
#define LOAD_SAVE_HANDLER_COUNT 27

#define LSGAME_MSG_NAME "LSGAME.MSG"

#define LS_WINDOW_WIDTH 640
#define LS_WINDOW_HEIGHT 480

#define LS_PREVIEW_WIDTH 224
#define LS_PREVIEW_HEIGHT 133
#define LS_PREVIEW_SIZE ((LS_PREVIEW_WIDTH) * (LS_PREVIEW_HEIGHT))

#define LS_COMMENT_WINDOW_X 169
#define LS_COMMENT_WINDOW_Y 116

typedef int LoadGameHandler(DB_FILE* stream);
typedef int SaveGameHandler(DB_FILE* stream);

typedef enum LoadSaveWindowType {
    LOAD_SAVE_WINDOW_TYPE_SAVE_GAME,
    LOAD_SAVE_WINDOW_TYPE_PICK_QUICK_SAVE_SLOT,
    LOAD_SAVE_WINDOW_TYPE_LOAD_GAME,
    LOAD_SAVE_WINDOW_TYPE_LOAD_GAME_FROM_MAIN_MENU,
    LOAD_SAVE_WINDOW_TYPE_PICK_QUICK_LOAD_SLOT,
} LoadSaveWindowType;

typedef enum LoadSaveSlotState {
    SLOT_STATE_EMPTY,
    SLOT_STATE_OCCUPIED,
    SLOT_STATE_ERROR,
    SLOT_STATE_UNSUPPORTED_VERSION,
} LoadSaveSlotState;

typedef enum LoadSaveScrollDirection {
    LOAD_SAVE_SCROLL_DIRECTION_NONE,
    LOAD_SAVE_SCROLL_DIRECTION_UP,
    LOAD_SAVE_SCROLL_DIRECTION_DOWN,
} LoadSaveScrollDirection;

typedef struct LoadSaveSlotData {
    char signature[24];
    short versionMinor;
    short versionMajor;
    // TODO: The type is probably char, but it's read with the same function as
    // reading unsigned chars, which in turn probably result of collapsing
    // reading functions.
    unsigned char versionRelease;
    char characterName[32];
    char description[LOAD_SAVE_DESCRIPTION_LENGTH];
    short fileMonth;
    short fileDay;
    short fileYear;
    int fileTime;
    short gameMonth;
    short gameDay;
    short gameYear;
    int gameTime;
    short elevation;
    short map;
    char fileName[16];
} LoadSaveSlotData;

typedef enum LoadSaveFrm {
    LOAD_SAVE_FRM_BACKGROUND,
    LOAD_SAVE_FRM_BOX,
    LOAD_SAVE_FRM_PREVIEW_COVER,
    LOAD_SAVE_FRM_RED_BUTTON_PRESSED,
    LOAD_SAVE_FRM_RED_BUTTON_NORMAL,
    LOAD_SAVE_FRM_ARROW_DOWN_NORMAL,
    LOAD_SAVE_FRM_ARROW_DOWN_PRESSED,
    LOAD_SAVE_FRM_ARROW_UP_NORMAL,
    LOAD_SAVE_FRM_ARROW_UP_PRESSED,
    LOAD_SAVE_FRM_COUNT,
} LoadSaveFrm;

static int QuickSnapShot();
static int LSGameStart(int windowType);
static int LSGameEnd(int windowType);
static int SaveSlot();
static int LoadSlot(int slot);
static void GetTimeDate(short* day, short* month, short* year, int* hour);
static int SaveHeader(int slot);
static int LoadHeader(int slot);
static int GetSlotList();
static void ShowSlotList(int a1);
static void DrawInfoBox(int a1);
static int LoadTumbSlot(int a1);
static int GetComment(int a1);
static int get_input_str2(int win, int doneKeyCode, int cancelKeyCode, char* description, int maxLength, int x, int y, int textColor, int backgroundColor, int flags);
static int DummyFunc(DB_FILE* stream);
static int PrepLoad(DB_FILE* stream);
static int EndLoad(DB_FILE* stream);
static int GameMap2Slot(DB_FILE* stream);
static int SlotMap2Game(DB_FILE* stream);
static int mygets(char* dest, DB_FILE* stream);
static int copy_file(const char* a1, const char* a2);
static int SaveBackup();
static int RestoreSave();
static int LoadObjDudeCid(DB_FILE* stream);
static int SaveObjDudeCid(DB_FILE* stream);
static int EraseSave();

// 0x46D930
static const int lsgrphs[LOAD_SAVE_FRM_COUNT] = {
    237, // lsgame.frm - load/save game
    238, // lsgbox.frm - load/save game
    239, // lscover.frm - load/save game
    9, // lilreddn.frm - little red button down
    8, // lilredup.frm - little red button up
    181, // dnarwoff.frm - character editor
    182, // dnarwon.frm - character editor
    199, // uparwoff.frm - character editor
    200, // uparwon.frm - character editor
};

// 0x50595C
static int slot_cursor = 0;

// 0x505960
static bool quick_done = false;

// 0x505964
static bool bk_enable = false;

// 0x505968
static int map_backup_count = -1;

// 0x50596C
static int automap_db_flag = 0;

// 0x505970
static char* patches = NULL;

// 0x505974
static char emgpath[] = "\\FALLOUT\\CD\\DATA\\SAVEGAME";

// 0x505990
static SaveGameHandler* master_save_list[LOAD_SAVE_HANDLER_COUNT] = {
    DummyFunc,
    SaveObjDudeCid,
    scr_game_save,
    GameMap2Slot,
    scr_game_save,
    obj_save_dude,
    critter_save,
    critter_kill_count_save,
    skill_save,
    roll_save,
    perk_save,
    combat_save,
    combat_ai_save,
    stat_save,
    item_save,
    queue_save,
    trait_save,
    automap_save,
    save_options,
    editor_save,
    save_world_map,
    save_pipboy,
    gmovie_save,
    skill_use_slot_save,
    partyMemberSave,
    intface_save,
    DummyFunc,
};

// 0x5059FC
static LoadGameHandler* master_load_list[LOAD_SAVE_HANDLER_COUNT] = {
    PrepLoad,
    LoadObjDudeCid,
    scr_game_load,
    SlotMap2Game,
    scr_game_load2,
    obj_load_dude,
    critter_load,
    critter_kill_count_load,
    skill_load,
    roll_load,
    perk_load,
    combat_load,
    combat_ai_load,
    stat_load,
    item_load,
    queue_load,
    trait_load,
    automap_load,
    load_options,
    editor_load,
    load_world_map,
    load_pipboy,
    gmovie_load,
    skill_use_slot_load,
    partyMemberLoad,
    intface_load,
    EndLoad,
};

// 0x505A68
static int loadingGame = 0;

// 0x612260
static Size ginfo[LOAD_SAVE_FRM_COUNT];

// lsgame.msg
//
// 0x6122A8
static MessageList lsgame_msgfl;

// 0x6122B0
static LoadSaveSlotData LSData[10];

// 0x612800
static int LSstatus[10];

// 0x612828
static unsigned char* thumbnail_image[2];

// 0x612D44
static MessageListItem lsgmesg;

// 0x612D54
static int dbleclkcntr;

// 0x612D58
static int lsgwin;

// 0x612D5C
static unsigned char* lsbmp[LOAD_SAVE_FRM_COUNT];

// 0x612D80
static unsigned char* snapshot;

// 0x612830
static char str2[COMPAT_MAX_PATH];

// 0x612934
static char str0[COMPAT_MAX_PATH];

// 0x612A38
static char str1[COMPAT_MAX_PATH];

// 0x612B3C
static char str[COMPAT_MAX_PATH];

// 0x612DB0
static unsigned char* lsgbuf;

// 0x612C40
static char gmpath[COMPAT_MAX_PATH];

// 0x612D50
static DB_FILE* flptr;

// 0x612D84
static int ls_error_code;

// 0x612D88
static int fontsave;

// 0x612D8C
static CacheEntry* grphkey[LOAD_SAVE_FRM_COUNT];

// 0x46D954
void InitLoadSave()
{
    quick_done = false;
    slot_cursor = 0;

    if (!config_get_string(&game_config, GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_MASTER_PATCHES_KEY, &patches)) {
        debug_printf("\nLOADSAVE: Error reading patches config variable! Using default.\n");
        patches = emgpath;
    }

    MapDirErase("MAPS\\", "SAV");
}

// 0x46D9B0
void ResetLoadSave()
{
    MapDirErase("MAPS\\", "SAV");
}

// 0x46D9C4
int SaveGame(int mode)
{
    MessageListItem messageListItem;

    ls_error_code = 0;

    if (!config_get_string(&game_config, GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_MASTER_PATCHES_KEY, &patches)) {
        debug_printf("\nLOADSAVE: Error reading patches config variable! Using default.\n");
        patches = emgpath;
    }

    if (mode == LOAD_SAVE_MODE_QUICK && quick_done) {
        snprintf(gmpath, sizeof(gmpath), "%s\\%s%.2d\\", "SAVEGAME", "SLOT", slot_cursor + 1);
        strcat(gmpath, "SAVE.DAT");

        flptr = db_fopen(gmpath, "rb");
        if (flptr != NULL) {
            LoadHeader(slot_cursor);
            db_fclose(flptr);
        }

        thumbnail_image[1] = NULL;
        int v6 = QuickSnapShot();
        if (v6 == 1) {
            int v7 = SaveSlot();
            if (v7 != -1) {
                v6 = v7;
            }
        }

        if (thumbnail_image[1] != NULL) {
            mem_free(snapshot);
        }

        gmouse_set_cursor(MOUSE_CURSOR_ARROW);

        if (v6 != -1) {
            return 1;
        }

        if (!message_init(&lsgame_msgfl)) {
            return -1;
        }

        char path[COMPAT_MAX_PATH];
        snprintf(path, sizeof(path), "%s%s", msg_path, "LSGAME.MSG");
        if (!message_load(&lsgame_msgfl, path)) {
            return -1;
        }

        gsound_play_sfx_file("iisxxxx1");

        // Error saving game!
        strcpy(str0, getmsg(&lsgame_msgfl, &messageListItem, 132));
        // Unable to save game.
        strcpy(str1, getmsg(&lsgame_msgfl, &messageListItem, 133));

        const char* body[] = {
            str1,
        };
        dialog_out(str0, body, 1, 169, 116, colorTable[32328], NULL, colorTable[32328], DIALOG_BOX_LARGE);

        message_exit(&lsgame_msgfl);

        return -1;
    }

    quick_done = false;

    int windowType = mode == LOAD_SAVE_MODE_QUICK
        ? LOAD_SAVE_WINDOW_TYPE_PICK_QUICK_SAVE_SLOT
        : LOAD_SAVE_WINDOW_TYPE_SAVE_GAME;
    if (LSGameStart(windowType) == -1) {
        debug_printf("\nLOADSAVE: ** Error loading save game screen data! **\n");
        return -1;
    }

    if (GetSlotList() == -1) {
        win_draw(lsgwin);

        gsound_play_sfx_file("iisxxxx1");

        // Error loading save game list!
        strcpy(str0, getmsg(&lsgame_msgfl, &messageListItem, 106));
        // Save game directory:
        strcpy(str1, getmsg(&lsgame_msgfl, &messageListItem, 107));

        snprintf(str2, sizeof(str2), "\"%s\\\"", "SAVEGAME");

        // TODO: Check.
        strcpy(str2, getmsg(&lsgame_msgfl, &messageListItem, 108));

        const char* body[] = {
            str1,
            str2,
        };
        dialog_out(str0, body, 2, 169, 116, colorTable[32328], NULL, colorTable[32328], DIALOG_BOX_LARGE);

        LSGameEnd(0);

        return -1;
    }

    switch (LSstatus[slot_cursor]) {
    case SLOT_STATE_EMPTY:
    case SLOT_STATE_ERROR:
    case SLOT_STATE_UNSUPPORTED_VERSION:
        buf_to_buf(thumbnail_image[1],
            LS_PREVIEW_WIDTH,
            LS_PREVIEW_HEIGHT,
            LS_PREVIEW_WIDTH,
            lsgbuf + LS_WINDOW_WIDTH * 58 + 366,
            LS_WINDOW_WIDTH);
        break;
    default:
        LoadTumbSlot(slot_cursor);
        buf_to_buf(thumbnail_image[0],
            LS_PREVIEW_WIDTH,
            LS_PREVIEW_HEIGHT,
            LS_PREVIEW_WIDTH,
            lsgbuf + LS_WINDOW_WIDTH * 58 + 366,
            LS_WINDOW_WIDTH);
        break;
    }

    ShowSlotList(0);
    DrawInfoBox(slot_cursor);
    win_draw(lsgwin);
    renderPresent();

    dbleclkcntr = 24;

    int rc = -1;
    int doubleClickSlot = -1;
    while (rc == -1) {
        sharedFpsLimiter.mark();

        unsigned int tick = get_time();
        int keyCode = get_input();
        bool selectionChanged = false;
        int scrollDirection = LOAD_SAVE_SCROLL_DIRECTION_NONE;

        convertMouseWheelToArrowKey(&keyCode);

        if (keyCode == KEY_ESCAPE || keyCode == 501 || game_user_wants_to_quit != 0) {
            rc = 0;
        } else {
            switch (keyCode) {
            case KEY_ARROW_UP:
                slot_cursor -= 1;
                if (slot_cursor < 0) {
                    slot_cursor = 0;
                }
                selectionChanged = true;
                doubleClickSlot = -1;
                break;
            case KEY_ARROW_DOWN:
                slot_cursor += 1;
                if (slot_cursor > 9) {
                    slot_cursor = 9;
                }
                selectionChanged = true;
                doubleClickSlot = -1;
                break;
            case KEY_HOME:
                slot_cursor = 0;
                selectionChanged = true;
                doubleClickSlot = -1;
                break;
            case KEY_END:
                slot_cursor = 9;
                selectionChanged = true;
                doubleClickSlot = -1;
                break;
            case 506:
                scrollDirection = LOAD_SAVE_SCROLL_DIRECTION_UP;
                break;
            case 504:
                scrollDirection = LOAD_SAVE_SCROLL_DIRECTION_DOWN;
                break;
            case 502:
                if (1) {
                    int mouseX;
                    int mouseY;
                    mouseGetPositionInWindow(lsgwin, &mouseX, &mouseY);

                    slot_cursor = (mouseY - 79) / (3 * text_height() + 4);
                    if (slot_cursor < 0) {
                        slot_cursor = 0;
                    }
                    if (slot_cursor > 9) {
                        slot_cursor = 9;
                    }

                    selectionChanged = true;

                    if (slot_cursor == doubleClickSlot) {
                        keyCode = 500;
                        gsound_play_sfx_file("ib1p1xx1");
                    }

                    doubleClickSlot = slot_cursor;
                    scrollDirection = LOAD_SAVE_SCROLL_DIRECTION_NONE;
                }
                break;
            case KEY_CTRL_Q:
            case KEY_CTRL_X:
            case KEY_F10:
                game_quit_with_confirm();

                if (game_user_wants_to_quit != 0) {
                    rc = 0;
                }
                break;
            case KEY_PLUS:
            case KEY_EQUAL:
                IncGamma();
                break;
            case KEY_MINUS:
            case KEY_UNDERSCORE:
                DecGamma();
                break;
            case KEY_RETURN:
                keyCode = 500;
                break;
            }
        }

        if (keyCode == 500) {
            if (LSstatus[slot_cursor] == SLOT_STATE_OCCUPIED) {
                rc = 1;
                // Save game already exists, overwrite?
                const char* title = getmsg(&lsgame_msgfl, &lsgmesg, 131);
                if (dialog_out(title, NULL, 0, 169, 131, colorTable[32328], NULL, colorTable[32328], DIALOG_BOX_YES_NO) == 0) {
                    rc = -1;
                }
            } else {
                rc = 1;
            }

            selectionChanged = true;
            scrollDirection = LOAD_SAVE_SCROLL_DIRECTION_NONE;
        }

        if (scrollDirection) {
            unsigned int scrollVelocity = 4;
            bool isScrolling = false;
            int scrollCounter = 0;
            do {
                sharedFpsLimiter.mark();

                unsigned int start = get_time();
                scrollCounter += 1;

                if ((!isScrolling && scrollCounter == 1) || (isScrolling && scrollCounter > 14.4)) {
                    isScrolling = true;

                    if (scrollCounter > 14.4) {
                        scrollVelocity += 1;
                        if (scrollVelocity > 24) {
                            scrollVelocity = 24;
                        }
                    }

                    if (scrollDirection == LOAD_SAVE_SCROLL_DIRECTION_UP) {
                        slot_cursor -= 1;
                        if (slot_cursor < 0) {
                            slot_cursor = 0;
                        }
                    } else {
                        slot_cursor += 1;
                        if (slot_cursor > 9) {
                            slot_cursor = 9;
                        }
                    }

                    // TODO: Does not check for unsupported version error like
                    // other switches do.
                    switch (LSstatus[slot_cursor]) {
                    case SLOT_STATE_EMPTY:
                    case SLOT_STATE_ERROR:
                        buf_to_buf(thumbnail_image[1],
                            LS_PREVIEW_WIDTH,
                            LS_PREVIEW_HEIGHT,
                            LS_PREVIEW_WIDTH,
                            lsgbuf + LS_WINDOW_WIDTH * 58 + 366,
                            LS_WINDOW_WIDTH);
                        break;
                    default:
                        LoadTumbSlot(slot_cursor);
                        buf_to_buf(thumbnail_image[0],
                            LS_PREVIEW_WIDTH,
                            LS_PREVIEW_HEIGHT,
                            LS_PREVIEW_WIDTH,
                            lsgbuf + LS_WINDOW_WIDTH * 58 + 366,
                            LS_WINDOW_WIDTH);
                        break;
                    }

                    ShowSlotList(LOAD_SAVE_WINDOW_TYPE_SAVE_GAME);
                    DrawInfoBox(slot_cursor);
                    win_draw(lsgwin);
                }

                if (scrollCounter > 14.4) {
                    while (elapsed_time(start) < 1000 / scrollVelocity) { }
                } else {
                    while (elapsed_time(start) < 1000 / 24) { }
                }

                keyCode = get_input();

                renderPresent();
                sharedFpsLimiter.throttle();
            } while (keyCode != 505 && keyCode != 503);
        } else {
            if (selectionChanged) {
                switch (LSstatus[slot_cursor]) {
                case SLOT_STATE_EMPTY:
                case SLOT_STATE_ERROR:
                case SLOT_STATE_UNSUPPORTED_VERSION:
                    buf_to_buf(thumbnail_image[1],
                        LS_PREVIEW_WIDTH,
                        LS_PREVIEW_HEIGHT,
                        LS_PREVIEW_WIDTH,
                        lsgbuf + LS_WINDOW_WIDTH * 58 + 366,
                        LS_WINDOW_WIDTH);
                    break;
                default:
                    LoadTumbSlot(slot_cursor);
                    buf_to_buf(thumbnail_image[0],
                        LS_PREVIEW_WIDTH,
                        LS_PREVIEW_HEIGHT,
                        LS_PREVIEW_WIDTH,
                        lsgbuf + LS_WINDOW_WIDTH * 58 + 366,
                        LS_WINDOW_WIDTH);
                    break;
                }

                DrawInfoBox(slot_cursor);
                ShowSlotList(LOAD_SAVE_WINDOW_TYPE_SAVE_GAME);
            }

            win_draw(lsgwin);

            dbleclkcntr -= 1;
            if (dbleclkcntr == 0) {
                dbleclkcntr = 24;
                doubleClickSlot = -1;
            }

            while (elapsed_time(tick) < 1000 / 24) {
            }
        }

        if (rc == 1) {
            int v50 = GetComment(slot_cursor);
            if (v50 == -1) {
                gmouse_set_cursor(MOUSE_CURSOR_ARROW);
                gsound_play_sfx_file("iisxxxx1");
                debug_printf("\nLOADSAVE: ** Error getting save file comment **\n");

                // Error saving game!
                strcpy(str0, getmsg(&lsgame_msgfl, &lsgmesg, 132));
                // Unable to save game.
                strcpy(str1, getmsg(&lsgame_msgfl, &lsgmesg, 133));

                const char* body[1] = {
                    str1,
                };
                dialog_out(str0, body, 1, 169, 116, colorTable[32328], NULL, colorTable[32328], DIALOG_BOX_LARGE);
                rc = -1;
            } else if (v50 == 0) {
                gmouse_set_cursor(MOUSE_CURSOR_ARROW);
                rc = -1;
            } else if (v50 == 1) {
                if (SaveSlot() == -1) {
                    gmouse_set_cursor(MOUSE_CURSOR_ARROW);
                    gsound_play_sfx_file("iisxxxx1");

                    // Error saving game!
                    strcpy(str0, getmsg(&lsgame_msgfl, &lsgmesg, 132));
                    // Unable to save game.
                    strcpy(str1, getmsg(&lsgame_msgfl, &lsgmesg, 133));

                    rc = -1;

                    const char* body[1] = {
                        str1,
                    };
                    dialog_out(str0, body, 1, 169, 116, colorTable[32328], NULL, colorTable[32328], DIALOG_BOX_LARGE);

                    if (GetSlotList() == -1) {
                        win_draw(lsgwin);
                        gsound_play_sfx_file("iisxxxx1");

                        // Error loading save agme list!
                        strcpy(str0, getmsg(&lsgame_msgfl, &lsgmesg, 106));
                        // Save game directory:
                        strcpy(str1, getmsg(&lsgame_msgfl, &lsgmesg, 107));

                        snprintf(str2, sizeof(str2), "\"%s\\\"", "SAVEGAME");

                        char text[260];
                        // Doesn't exist or is corrupted.
                        strcpy(text, getmsg(&lsgame_msgfl, &lsgmesg, 107));

                        const char* body[2] = {
                            str1,
                            str2,
                        };
                        dialog_out(str0, body, 2, 169, 116, colorTable[32328], NULL, colorTable[32328], DIALOG_BOX_LARGE);

                        LSGameEnd(0);

                        return -1;
                    }

                    switch (LSstatus[slot_cursor]) {
                    case SLOT_STATE_EMPTY:
                    case SLOT_STATE_ERROR:
                    case SLOT_STATE_UNSUPPORTED_VERSION:
                        buf_to_buf(thumbnail_image[1],
                            LS_PREVIEW_WIDTH,
                            LS_PREVIEW_HEIGHT,
                            LS_PREVIEW_WIDTH,
                            lsgbuf + LS_WINDOW_WIDTH * 58 + 366,
                            LS_WINDOW_WIDTH);
                        break;
                    default:
                        LoadTumbSlot(slot_cursor);
                        buf_to_buf(thumbnail_image[0],
                            LS_PREVIEW_WIDTH,
                            LS_PREVIEW_HEIGHT,
                            LS_PREVIEW_WIDTH,
                            lsgbuf + LS_WINDOW_WIDTH * 58 + 366,
                            LS_WINDOW_WIDTH);
                        break;
                    }

                    ShowSlotList(LOAD_SAVE_WINDOW_TYPE_SAVE_GAME);
                    DrawInfoBox(slot_cursor);
                    win_draw(lsgwin);
                    dbleclkcntr = 24;
                }
            }
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    gmouse_set_cursor(MOUSE_CURSOR_ARROW);

    LSGameEnd(LOAD_SAVE_WINDOW_TYPE_SAVE_GAME);

    tile_refresh_display();

    if (mode == LOAD_SAVE_MODE_QUICK) {
        if (rc == 1) {
            quick_done = true;
        }
    }

    return rc;
}

// 0x46E6C8
static int QuickSnapShot()
{
    snapshot = (unsigned char*)mem_malloc(LS_PREVIEW_SIZE);
    if (snapshot == NULL) {
        return -1;
    }

    bool gameMouseWasVisible = gmouse_3d_is_on();
    if (gameMouseWasVisible) {
        gmouse_3d_off();
    }

    mouse_hide();
    tile_refresh_display();
    mouse_show();

    if (gameMouseWasVisible) {
        gmouse_3d_on();
    }

    int isoWindowWidth = win_width(display_win);
    int isoWindowHeight = win_height(display_win);
    unsigned char* windowBuf = win_get_buf(display_win)
        + isoWindowWidth * (isoWindowHeight - ORIGINAL_ISO_WINDOW_HEIGHT) / 2
        + (isoWindowWidth - ORIGINAL_ISO_WINDOW_WIDTH) / 2;
    cscale(windowBuf,
        ORIGINAL_ISO_WINDOW_WIDTH,
        ORIGINAL_ISO_WINDOW_HEIGHT,
        isoWindowWidth,
        snapshot,
        LS_PREVIEW_WIDTH,
        LS_PREVIEW_HEIGHT,
        LS_PREVIEW_WIDTH);

    thumbnail_image[1] = snapshot;

    return 1;
}

// 0x46E754
int LoadGame(int mode)
{
    MessageListItem messageListItem;

    const char* body[] = {
        str1,
        str2,
    };

    ls_error_code = 0;

    if (!config_get_string(&game_config, GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_MASTER_PATCHES_KEY, &patches)) {
        debug_printf("\nLOADSAVE: Error reading patches config variable! Using default.\n");
        patches = emgpath;
    }

    if (mode == LOAD_SAVE_MODE_QUICK && quick_done) {
        int quickSaveWindowX = (screenGetWidth() - LS_WINDOW_WIDTH) / 2;
        int quickSaveWindowY = (screenGetHeight() - LS_WINDOW_HEIGHT) / 2;
        int window = win_add(quickSaveWindowX,
            quickSaveWindowY,
            LS_WINDOW_WIDTH,
            LS_WINDOW_HEIGHT,
            256,
            WINDOW_MODAL | WINDOW_DONT_MOVE_TOP);
        if (window != -1) {
            unsigned char* windowBuffer = win_get_buf(window);
            buf_fill(windowBuffer, LS_WINDOW_WIDTH, LS_WINDOW_HEIGHT, LS_WINDOW_WIDTH, colorTable[0]);
            win_draw(window);
            renderPresent();
        }

        if (LoadSlot(slot_cursor) != -1) {
            if (window != -1) {
                win_delete(window);
            }
            gmouse_set_cursor(MOUSE_CURSOR_ARROW);
            return 1;
        }

        if (!message_init(&lsgame_msgfl)) {
            return -1;
        }

        char path[COMPAT_MAX_PATH];
        snprintf(path, sizeof(path), "%s\\%s", msg_path, "LSGAME.MSG");
        if (!message_load(&lsgame_msgfl, path)) {
            return -1;
        }

        if (window != -1) {
            win_delete(window);
        }

        gmouse_set_cursor(MOUSE_CURSOR_ARROW);
        gsound_play_sfx_file("iisxxxx1");
        strcpy(str0, getmsg(&lsgame_msgfl, &messageListItem, 134));
        strcpy(str1, getmsg(&lsgame_msgfl, &messageListItem, 135));
        dialog_out(str0, body, 1, 169, 116, colorTable[32328], 0, colorTable[32328], DIALOG_BOX_LARGE);

        message_exit(&lsgame_msgfl);
        map_new_map();
        game_user_wants_to_quit = 2;

        return -1;
    }

    quick_done = false;

    int windowType;
    switch (mode) {
    case LOAD_SAVE_MODE_FROM_MAIN_MENU:
        windowType = LOAD_SAVE_WINDOW_TYPE_LOAD_GAME_FROM_MAIN_MENU;
        break;
    case LOAD_SAVE_MODE_NORMAL:
        windowType = LOAD_SAVE_WINDOW_TYPE_LOAD_GAME;
        break;
    case LOAD_SAVE_MODE_QUICK:
        windowType = LOAD_SAVE_WINDOW_TYPE_PICK_QUICK_LOAD_SLOT;
        break;
    default:
        assert(false && "Should be unreachable");
    }

    if (LSGameStart(windowType) == -1) {
        debug_printf("\nLOADSAVE: ** Error loading save game screen data! **\n");
        return -1;
    }

    if (GetSlotList() == -1) {
        gmouse_set_cursor(MOUSE_CURSOR_ARROW);
        win_draw(lsgwin);
        renderPresent();
        gsound_play_sfx_file("iisxxxx1");
        strcpy(str0, getmsg(&lsgame_msgfl, &lsgmesg, 106));
        strcpy(str1, getmsg(&lsgame_msgfl, &lsgmesg, 107));
        snprintf(str2, sizeof(str2), "\"%s\\\"", "SAVEGAME");
        dialog_out(str0, body, 2, 169, 116, colorTable[32328], 0, colorTable[32328], DIALOG_BOX_LARGE);
        LSGameEnd(windowType);
        return -1;
    }

    switch (LSstatus[slot_cursor]) {
    case SLOT_STATE_EMPTY:
    case SLOT_STATE_ERROR:
    case SLOT_STATE_UNSUPPORTED_VERSION:
        buf_to_buf(lsbmp[LOAD_SAVE_FRM_PREVIEW_COVER],
            ginfo[LOAD_SAVE_FRM_PREVIEW_COVER].width,
            ginfo[LOAD_SAVE_FRM_PREVIEW_COVER].height,
            ginfo[LOAD_SAVE_FRM_PREVIEW_COVER].width,
            lsgbuf + LS_WINDOW_WIDTH * 39 + 340,
            LS_WINDOW_WIDTH);
        break;
    default:
        LoadTumbSlot(slot_cursor);
        buf_to_buf(thumbnail_image[0],
            LS_PREVIEW_WIDTH,
            LS_PREVIEW_HEIGHT,
            LS_PREVIEW_WIDTH,
            lsgbuf + LS_WINDOW_WIDTH * 58 + 366,
            LS_WINDOW_WIDTH);
        break;
    }

    ShowSlotList(2);
    DrawInfoBox(slot_cursor);
    win_draw(lsgwin);
    dbleclkcntr = 24;

    int rc = -1;
    int doubleClickSlot = -1;
    while (rc == -1) {
        sharedFpsLimiter.mark();

        unsigned int time = get_time();
        int keyCode = get_input();
        bool selectionChanged = false;
        int scrollDirection = 0;

        convertMouseWheelToArrowKey(&keyCode);

        if (keyCode == KEY_ESCAPE || keyCode == 501 || game_user_wants_to_quit != 0) {
            rc = 0;
        } else {
            switch (keyCode) {
            case KEY_ARROW_UP:
                if (--slot_cursor < 0) {
                    slot_cursor = 0;
                }
                selectionChanged = true;
                doubleClickSlot = -1;
                break;
            case KEY_ARROW_DOWN:
                if (++slot_cursor > 9) {
                    slot_cursor = 9;
                }
                selectionChanged = true;
                doubleClickSlot = -1;
                break;
            case KEY_HOME:
                slot_cursor = 0;
                selectionChanged = true;
                doubleClickSlot = -1;
                break;
            case KEY_END:
                slot_cursor = 9;
                selectionChanged = true;
                doubleClickSlot = -1;
                break;
            case 506:
                scrollDirection = LOAD_SAVE_SCROLL_DIRECTION_UP;
                break;
            case 504:
                scrollDirection = LOAD_SAVE_SCROLL_DIRECTION_DOWN;
                break;
            case 502:
                if (1) {
                    int mouseX;
                    int mouseY;
                    mouseGetPositionInWindow(lsgwin, &mouseX, &mouseY);

                    int clickedSlot = (mouseY - 79) / (3 * text_height() + 4);
                    if (clickedSlot < 0) {
                        clickedSlot = 0;
                    } else if (clickedSlot > 9) {
                        clickedSlot = 9;
                    }

                    slot_cursor = clickedSlot;
                    if (clickedSlot == doubleClickSlot) {
                        keyCode = 500;
                        gsound_play_sfx_file("ib1p1xx1");
                    }

                    selectionChanged = true;
                    scrollDirection = LOAD_SAVE_SCROLL_DIRECTION_NONE;
                    doubleClickSlot = slot_cursor;
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
            case KEY_RETURN:
                keyCode = 500;
                break;
            case KEY_CTRL_Q:
            case KEY_CTRL_X:
            case KEY_F10:
                game_quit_with_confirm();
                if (game_user_wants_to_quit != 0) {
                    rc = 0;
                }
                break;
            }
        }

        if (keyCode == 500) {
            if (LSstatus[slot_cursor] != SLOT_STATE_EMPTY) {
                rc = 1;
            } else {
                rc = -1;
            }

            selectionChanged = true;
            scrollDirection = LOAD_SAVE_SCROLL_DIRECTION_NONE;
        }

        if (scrollDirection != LOAD_SAVE_SCROLL_DIRECTION_NONE) {
            unsigned int scrollVelocity = 4;
            bool isScrolling = false;
            int scrollCounter = 0;
            do {
                sharedFpsLimiter.mark();

                unsigned int start = get_time();
                scrollCounter += 1;

                if ((!isScrolling && scrollCounter == 1) || (isScrolling && scrollCounter > 14.4)) {
                    isScrolling = true;

                    if (scrollCounter > 14.4) {
                        scrollVelocity += 1;
                        if (scrollVelocity > 24) {
                            scrollVelocity = 24;
                        }
                    }

                    if (scrollDirection == LOAD_SAVE_SCROLL_DIRECTION_UP) {
                        slot_cursor -= 1;
                        if (slot_cursor < 0) {
                            slot_cursor = 0;
                        }
                    } else {
                        slot_cursor += 1;
                        if (slot_cursor > 9) {
                            slot_cursor = 9;
                        }
                    }

                    switch (LSstatus[slot_cursor]) {
                    case SLOT_STATE_EMPTY:
                    case SLOT_STATE_ERROR:
                    case SLOT_STATE_UNSUPPORTED_VERSION:
                        buf_to_buf(lsbmp[LOAD_SAVE_FRM_PREVIEW_COVER],
                            ginfo[LOAD_SAVE_FRM_PREVIEW_COVER].width,
                            ginfo[LOAD_SAVE_FRM_PREVIEW_COVER].height,
                            ginfo[LOAD_SAVE_FRM_PREVIEW_COVER].width,
                            lsgbuf + LS_WINDOW_WIDTH * 39 + 340,
                            LS_WINDOW_WIDTH);
                        break;
                    default:
                        LoadTumbSlot(slot_cursor);
                        buf_to_buf(lsbmp[LOAD_SAVE_FRM_BACKGROUND] + LS_WINDOW_WIDTH * 39 + 340,
                            ginfo[LOAD_SAVE_FRM_PREVIEW_COVER].width,
                            ginfo[LOAD_SAVE_FRM_PREVIEW_COVER].height,
                            LS_WINDOW_WIDTH,
                            lsgbuf + LS_WINDOW_WIDTH * 39 + 340,
                            LS_WINDOW_WIDTH);
                        buf_to_buf(thumbnail_image[0],
                            LS_PREVIEW_WIDTH,
                            LS_PREVIEW_HEIGHT,
                            LS_PREVIEW_WIDTH,
                            lsgbuf + LS_WINDOW_WIDTH * 58 + 366,
                            LS_WINDOW_WIDTH);
                        break;
                    }

                    ShowSlotList(2);
                    DrawInfoBox(slot_cursor);
                    win_draw(lsgwin);
                }

                if (scrollCounter > 14.4) {
                    while (elapsed_time(start) < 1000 / scrollVelocity) { }
                } else {
                    while (elapsed_time(start) < 1000 / 24) { }
                }

                keyCode = get_input();

                renderPresent();
                sharedFpsLimiter.throttle();
            } while (keyCode != 505 && keyCode != 503);
        } else {
            if (selectionChanged) {
                switch (LSstatus[slot_cursor]) {
                case SLOT_STATE_EMPTY:
                case SLOT_STATE_ERROR:
                case SLOT_STATE_UNSUPPORTED_VERSION:
                    buf_to_buf(lsbmp[LOAD_SAVE_FRM_PREVIEW_COVER],
                        ginfo[LOAD_SAVE_FRM_PREVIEW_COVER].width,
                        ginfo[LOAD_SAVE_FRM_PREVIEW_COVER].height,
                        ginfo[LOAD_SAVE_FRM_PREVIEW_COVER].width,
                        lsgbuf + LS_WINDOW_WIDTH * 39 + 340,
                        LS_WINDOW_WIDTH);
                    break;
                default:
                    LoadTumbSlot(slot_cursor);
                    buf_to_buf(lsbmp[LOAD_SAVE_FRM_BACKGROUND] + LS_WINDOW_WIDTH * 39 + 340,
                        ginfo[LOAD_SAVE_FRM_PREVIEW_COVER].width,
                        ginfo[LOAD_SAVE_FRM_PREVIEW_COVER].height,
                        LS_WINDOW_WIDTH,
                        lsgbuf + LS_WINDOW_WIDTH * 39 + 340,
                        LS_WINDOW_WIDTH);
                    buf_to_buf(thumbnail_image[0],
                        LS_PREVIEW_WIDTH,
                        LS_PREVIEW_HEIGHT,
                        LS_PREVIEW_WIDTH,
                        lsgbuf + LS_WINDOW_WIDTH * 58 + 366,
                        LS_WINDOW_WIDTH);
                    break;
                }

                DrawInfoBox(slot_cursor);
                ShowSlotList(2);
            }

            win_draw(lsgwin);

            dbleclkcntr -= 1;
            if (dbleclkcntr == 0) {
                dbleclkcntr = 24;
                doubleClickSlot = -1;
            }

            while (elapsed_time(time) < 1000 / 24) { }
        }

        if (rc == 1) {
            switch (LSstatus[slot_cursor]) {
            case SLOT_STATE_UNSUPPORTED_VERSION:
                gsound_play_sfx_file("iisxxxx1");
                strcpy(str0, getmsg(&lsgame_msgfl, &lsgmesg, 134));
                strcpy(str1, getmsg(&lsgame_msgfl, &lsgmesg, 136));
                strcpy(str2, getmsg(&lsgame_msgfl, &lsgmesg, 135));
                dialog_out(str0, body, 2, 169, 116, colorTable[32328], 0, colorTable[32328], DIALOG_BOX_LARGE);
                rc = -1;
                break;
            case SLOT_STATE_ERROR:
                gsound_play_sfx_file("iisxxxx1");
                strcpy(str0, getmsg(&lsgame_msgfl, &lsgmesg, 134));
                strcpy(str1, getmsg(&lsgame_msgfl, &lsgmesg, 136));
                dialog_out(str0, body, 1, 169, 116, colorTable[32328], 0, colorTable[32328], DIALOG_BOX_LARGE);
                rc = -1;
                break;
            default:
                if (LoadSlot(slot_cursor) == -1) {
                    gmouse_set_cursor(MOUSE_CURSOR_ARROW);
                    gsound_play_sfx_file("iisxxxx1");
                    strcpy(str0, getmsg(&lsgame_msgfl, &lsgmesg, 134));
                    strcpy(str1, getmsg(&lsgame_msgfl, &lsgmesg, 135));
                    dialog_out(str0, body, 1, 169, 116, colorTable[32328], 0, colorTable[32328], DIALOG_BOX_LARGE);
                    map_new_map();
                    game_user_wants_to_quit = 2;
                    rc = -1;
                }
                break;
            }
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    LSGameEnd(mode == LOAD_SAVE_MODE_FROM_MAIN_MENU
            ? LOAD_SAVE_WINDOW_TYPE_LOAD_GAME_FROM_MAIN_MENU
            : LOAD_SAVE_WINDOW_TYPE_LOAD_GAME);

    if (mode == LOAD_SAVE_MODE_QUICK) {
        if (rc == 1) {
            quick_done = true;
        }
    }

    return rc;
}

// 0x46F3D0
static int LSGameStart(int windowType)
{
    fontsave = text_curr();
    text_font(103);

    bk_enable = false;
    if (!message_init(&lsgame_msgfl)) {
        return -1;
    }

    snprintf(str, sizeof(str), "%s%s", msg_path, LSGAME_MSG_NAME);
    if (!message_load(&lsgame_msgfl, str)) {
        return -1;
    }

    snapshot = (unsigned char*)mem_malloc(61632);
    if (snapshot == NULL) {
        message_exit(&lsgame_msgfl);
        text_font(fontsave);
        return -1;
    }

    thumbnail_image[0] = snapshot;
    thumbnail_image[1] = snapshot + LS_PREVIEW_SIZE;

    if (windowType != LOAD_SAVE_WINDOW_TYPE_LOAD_GAME_FROM_MAIN_MENU) {
        bk_enable = map_disable_bk_processes();
    }

    cycle_disable();

    gmouse_set_cursor(MOUSE_CURSOR_ARROW);

    if (windowType == LOAD_SAVE_WINDOW_TYPE_SAVE_GAME || windowType == LOAD_SAVE_WINDOW_TYPE_PICK_QUICK_SAVE_SLOT) {
        bool gameMouseWasVisible = gmouse_3d_is_on();
        if (gameMouseWasVisible) {
            gmouse_3d_off();
        }

        mouse_hide();
        tile_refresh_display();
        mouse_show();

        if (gameMouseWasVisible) {
            gmouse_3d_on();
        }

        int isoWindowWidth = win_width(display_win);
        int isoWindowHeight = win_height(display_win);
        unsigned char* windowBuf = win_get_buf(display_win)
            + isoWindowWidth * (isoWindowHeight - ORIGINAL_ISO_WINDOW_HEIGHT) / 2
            + (isoWindowWidth - ORIGINAL_ISO_WINDOW_WIDTH) / 2;
        cscale(windowBuf,
            ORIGINAL_ISO_WINDOW_WIDTH,
            ORIGINAL_ISO_WINDOW_HEIGHT,
            isoWindowWidth,
            thumbnail_image[1],
            LS_PREVIEW_WIDTH,
            LS_PREVIEW_HEIGHT,
            LS_PREVIEW_WIDTH);
    }

    for (int index = 0; index < LOAD_SAVE_FRM_COUNT; index++) {
        int fid = art_id(OBJ_TYPE_INTERFACE, lsgrphs[index], 0, 0, 0);
        lsbmp[index] = art_lock(fid,
            &(grphkey[index]),
            &(ginfo[index].width),
            &(ginfo[index].height));

        if (lsbmp[index] == NULL) {
            while (--index >= 0) {
                art_ptr_unlock(grphkey[index]);
            }
            mem_free(snapshot);
            message_exit(&lsgame_msgfl);
            text_font(fontsave);

            if (windowType != LOAD_SAVE_WINDOW_TYPE_LOAD_GAME_FROM_MAIN_MENU) {
                if (bk_enable) {
                    map_enable_bk_processes();
                }
            }

            cycle_enable();
            gmouse_set_cursor(MOUSE_CURSOR_ARROW);
            return -1;
        }
    }

    int lsWindowX = (screenGetWidth() - LS_WINDOW_WIDTH) / 2;
    int lsWindowY = (screenGetHeight() - LS_WINDOW_HEIGHT) / 2;
    lsgwin = win_add(lsWindowX,
        lsWindowY,
        LS_WINDOW_WIDTH,
        LS_WINDOW_HEIGHT,
        256,
        WINDOW_MODAL | WINDOW_MOVE_ON_TOP);
    if (lsgwin == -1) {
        // FIXME: Leaking frms.
        mem_free(snapshot);
        message_exit(&lsgame_msgfl);
        text_font(fontsave);

        if (windowType != LOAD_SAVE_WINDOW_TYPE_LOAD_GAME_FROM_MAIN_MENU) {
            if (bk_enable) {
                map_enable_bk_processes();
            }
        }

        cycle_enable();
        gmouse_set_cursor(MOUSE_CURSOR_ARROW);
        return -1;
    }

    lsgbuf = win_get_buf(lsgwin);
    memcpy(lsgbuf, lsbmp[LOAD_SAVE_FRM_BACKGROUND], LS_WINDOW_WIDTH * LS_WINDOW_HEIGHT);

    int messageId;
    switch (windowType) {
    case LOAD_SAVE_WINDOW_TYPE_SAVE_GAME:
        // SAVE GAME
        messageId = 102;
        break;
    case LOAD_SAVE_WINDOW_TYPE_PICK_QUICK_SAVE_SLOT:
        // PICK A QUICK SAVE SLOT
        messageId = 103;
        break;
    case LOAD_SAVE_WINDOW_TYPE_LOAD_GAME:
    case LOAD_SAVE_WINDOW_TYPE_LOAD_GAME_FROM_MAIN_MENU:
        // LOAD GAME
        messageId = 100;
        break;
    case LOAD_SAVE_WINDOW_TYPE_PICK_QUICK_LOAD_SLOT:
        // PICK A QUICK LOAD SLOT
        messageId = 101;
        break;
    default:
        assert(false && "Should be unreachable");
    }

    char* msg;

    msg = getmsg(&lsgame_msgfl, &lsgmesg, messageId);
    text_to_buf(lsgbuf + LS_WINDOW_WIDTH * 27 + 48, msg, LS_WINDOW_WIDTH, LS_WINDOW_WIDTH, colorTable[18979]);

    // DONE
    msg = getmsg(&lsgame_msgfl, &lsgmesg, 104);
    text_to_buf(lsgbuf + LS_WINDOW_WIDTH * 348 + 410, msg, LS_WINDOW_WIDTH, LS_WINDOW_WIDTH, colorTable[18979]);

    // CANCEL
    msg = getmsg(&lsgame_msgfl, &lsgmesg, 105);
    text_to_buf(lsgbuf + LS_WINDOW_WIDTH * 348 + 515, msg, LS_WINDOW_WIDTH, LS_WINDOW_WIDTH, colorTable[18979]);

    int btn;

    btn = win_register_button(lsgwin,
        391,
        349,
        ginfo[LOAD_SAVE_FRM_RED_BUTTON_PRESSED].width,
        ginfo[LOAD_SAVE_FRM_RED_BUTTON_PRESSED].height,
        -1,
        -1,
        -1,
        500,
        lsbmp[LOAD_SAVE_FRM_RED_BUTTON_NORMAL],
        lsbmp[LOAD_SAVE_FRM_RED_BUTTON_PRESSED],
        NULL,
        BUTTON_FLAG_TRANSPARENT);
    if (btn != -1) {
        win_register_button_sound_func(btn, gsound_red_butt_press, gsound_red_butt_release);
    }

    btn = win_register_button(lsgwin,
        495,
        349,
        ginfo[LOAD_SAVE_FRM_RED_BUTTON_PRESSED].width,
        ginfo[LOAD_SAVE_FRM_RED_BUTTON_PRESSED].height,
        -1,
        -1,
        -1,
        501,
        lsbmp[LOAD_SAVE_FRM_RED_BUTTON_NORMAL],
        lsbmp[LOAD_SAVE_FRM_RED_BUTTON_PRESSED],
        NULL,
        BUTTON_FLAG_TRANSPARENT);
    if (btn != -1) {
        win_register_button_sound_func(btn, gsound_red_butt_press, gsound_red_butt_release);
    }

    btn = win_register_button(lsgwin,
        35,
        58,
        ginfo[LOAD_SAVE_FRM_ARROW_UP_PRESSED].width,
        ginfo[LOAD_SAVE_FRM_ARROW_UP_PRESSED].height,
        -1,
        505,
        506,
        505,
        lsbmp[LOAD_SAVE_FRM_ARROW_UP_NORMAL],
        lsbmp[LOAD_SAVE_FRM_ARROW_UP_PRESSED],
        NULL,
        BUTTON_FLAG_TRANSPARENT);
    if (btn != -1) {
        win_register_button_sound_func(btn, gsound_red_butt_press, gsound_red_butt_release);
    }

    btn = win_register_button(lsgwin,
        35,
        ginfo[LOAD_SAVE_FRM_ARROW_UP_PRESSED].height + 58,
        ginfo[LOAD_SAVE_FRM_ARROW_DOWN_PRESSED].width,
        ginfo[LOAD_SAVE_FRM_ARROW_DOWN_PRESSED].height,
        -1,
        503,
        504,
        503,
        lsbmp[LOAD_SAVE_FRM_ARROW_DOWN_NORMAL],
        lsbmp[LOAD_SAVE_FRM_ARROW_DOWN_PRESSED],
        NULL,
        BUTTON_FLAG_TRANSPARENT);
    if (btn != -1) {
        win_register_button_sound_func(btn, gsound_red_butt_press, gsound_red_butt_release);
    }

    win_register_button(lsgwin, 55, 87, 230, 353, -1, -1, -1, 502, NULL, NULL, NULL, BUTTON_FLAG_TRANSPARENT);
    text_font(101);

    return 0;
}

// 0x46F910
static int LSGameEnd(int windowType)
{
    win_delete(lsgwin);
    text_font(fontsave);
    message_exit(&lsgame_msgfl);

    for (int index = 0; index < LOAD_SAVE_FRM_COUNT; index++) {
        art_ptr_unlock(grphkey[index]);
    }

    mem_free(snapshot);

    if (windowType != LOAD_SAVE_WINDOW_TYPE_LOAD_GAME_FROM_MAIN_MENU) {
        if (bk_enable) {
            map_enable_bk_processes();
        }
    }

    cycle_enable();
    gmouse_set_cursor(MOUSE_CURSOR_ARROW);

    return 0;
}

// 0x46F978
static int SaveSlot()
{
    ls_error_code = 0;
    map_backup_count = -1;
    gmouse_set_cursor(MOUSE_CURSOR_WAIT_PLANET);

    gsound_background_pause();

    snprintf(gmpath, sizeof(gmpath), "%s\\%s", patches, "SAVEGAME");
    compat_mkdir(gmpath);

    snprintf(gmpath, sizeof(gmpath), "%s\\%s\\%s%.2d", patches, "SAVEGAME", "SLOT", slot_cursor + 1);
    compat_mkdir(gmpath);

    if (SaveBackup() == -1) {
        debug_printf("\nLOADSAVE: Warning, can't backup save file!\n");
    }

    snprintf(gmpath, sizeof(gmpath), "%s\\%s%.2d\\", "SAVEGAME", "SLOT", slot_cursor + 1);
    strcat(gmpath, "SAVE.DAT");

    debug_printf("\nLOADSAVE: Save name: %s\n", gmpath);

    flptr = db_fopen(gmpath, "wb");
    if (flptr == NULL) {
        debug_printf("\nLOADSAVE: ** Error opening save game for writing! **\n");
        RestoreSave();
        snprintf(gmpath, sizeof(gmpath), "%s\\%s%.2d\\", "SAVEGAME", "SLOT", slot_cursor + 1);
        MapDirErase(gmpath, "BAK");
        partyMemberUnPrepSave();
        gsound_background_unpause();
        return -1;
    }

    long pos = db_ftell(flptr);
    if (SaveHeader(slot_cursor) == -1) {
        debug_printf("\nLOADSAVE: ** Error writing save game header! **\n");
        debug_printf("LOADSAVE: Save file header size written: %d bytes.\n", db_ftell(flptr) - pos);
        db_fclose(flptr);
        RestoreSave();
        snprintf(gmpath, sizeof(gmpath), "%s\\%s%.2d\\", "SAVEGAME", "SLOT", slot_cursor + 1);
        MapDirErase(gmpath, "BAK");
        partyMemberUnPrepSave();
        gsound_background_unpause();
        return -1;
    }

    for (int index = 0; index < LOAD_SAVE_HANDLER_COUNT; index++) {
        long pos = db_ftell(flptr);
        SaveGameHandler* handler = master_save_list[index];
        if (handler(flptr) == -1) {
            debug_printf("\nLOADSAVE: ** Error writing save function #%d data! **\n", index);
            db_fclose(flptr);
            RestoreSave();
            snprintf(gmpath, sizeof(gmpath), "%s\\%s%.2d\\", "SAVEGAME", "SLOT", slot_cursor + 1);
            MapDirErase(gmpath, "BAK");
            partyMemberUnPrepSave();
            gsound_background_unpause();
            return -1;
        }

        debug_printf("LOADSAVE: Save function #%d data size written: %d bytes.\n", index, db_ftell(flptr) - pos);
    }

    debug_printf("LOADSAVE: Total save data written: %ld bytes.\n", db_ftell(flptr));

    db_fclose(flptr);

    snprintf(gmpath, sizeof(gmpath), "%s\\%s%.2d\\", "SAVEGAME", "SLOT", slot_cursor + 1);
    MapDirErase(gmpath, "BAK");

    lsgmesg.num = 140;
    if (message_search(&lsgame_msgfl, &lsgmesg)) {
        display_print(lsgmesg.text);
    } else {
        debug_printf("\nError: Couldn't find LoadSave Message!");
    }

    gsound_background_unpause();

    return 0;
}

// 0x46FCC4
int isLoadingGame()
{
    return loadingGame;
}

// 0x46FCCC
static int LoadSlot(int slot)
{
    gmouse_set_cursor(MOUSE_CURSOR_WAIT_PLANET);

    if (isInCombat()) {
        intface_end_window_close(false);
        combat_over_from_load();
        gmouse_set_cursor(MOUSE_CURSOR_WAIT_PLANET);
    }

    loadingGame = 1;

    snprintf(gmpath, sizeof(gmpath), "%s\\%s%.2d\\", "SAVEGAME", "SLOT", slot_cursor + 1);
    strcat(gmpath, "SAVE.DAT");

    LoadSaveSlotData* ptr = &(LSData[slot]);
    debug_printf("\nLOADSAVE: Load name: %s\n", ptr->description);

    flptr = db_fopen(gmpath, "rb");
    if (flptr == NULL) {
        debug_printf("\nLOADSAVE: ** Error opening load game file for reading! **\n");
        loadingGame = 0;
        return -1;
    }

    long pos = db_ftell(flptr);
    if (LoadHeader(slot) == -1) {
        debug_printf("\nLOADSAVE: ** Error reading save  game header! **\n");
        db_fclose(flptr);
        game_reset();
        loadingGame = 0;
        return -1;
    }

    debug_printf("LOADSAVE: Load file header size read: %d bytes.\n", db_ftell(flptr) - pos);

    for (int index = 0; index < LOAD_SAVE_HANDLER_COUNT; index += 1) {
        long pos = db_ftell(flptr);
        LoadGameHandler* handler = master_load_list[index];
        if (handler(flptr) == -1) {
            debug_printf("\nLOADSAVE: ** Error reading load function #%d data! **\n", index);
            int v12 = db_ftell(flptr);
            debug_printf("LOADSAVE: Load function #%d data size read: %d bytes.\n", index, db_ftell(flptr) - pos);
            db_fclose(flptr);
            game_reset();
            loadingGame = 0;
            return -1;
        }

        debug_printf("LOADSAVE: Load function #%d data size read: %d bytes.\n", index, db_ftell(flptr) - pos);
    }

    debug_printf("LOADSAVE: Total load data read: %ld bytes.\n", db_ftell(flptr));
    db_fclose(flptr);

    snprintf(str, sizeof(str), "%s\\", "MAPS");
    MapDirErase(str, "BAK");
    proto_dude_update_gender();

    // Game Loaded.
    lsgmesg.num = 141;
    if (message_search(&lsgame_msgfl, &lsgmesg) == 1) {
        display_print(lsgmesg.text);
    } else {
        debug_printf("\nError: Couldn't find LoadSave Message!");
    }

    loadingGame = 0;

    return 0;
}

// 0x46FF38
static void GetTimeDate(short* day, short* month, short* year, int* hour)
{
    time_t now;
    struct tm* local;

    now = time(NULL);
    local = localtime(&now);

    *day = local->tm_mday;
    *month = local->tm_mon + 1;
    *year = local->tm_year + 1900;
    *hour = local->tm_hour + local->tm_min;
}

// 0x46FF80
static int SaveHeader(int slot)
{
    ls_error_code = 4;

    LoadSaveSlotData* ptr = &(LSData[slot]);
    strncpy(ptr->signature, LOAD_SAVE_SIGNATURE, 24);

    if (db_fwrite(ptr->signature, 1, 24, flptr) == -1) {
        return -1;
    }

    short temp[3];
    temp[0] = VERSION_MAJOR;
    temp[1] = VERSION_MINOR;

    ptr->versionMinor = temp[0];
    ptr->versionMajor = temp[1];

    if (db_fwriteInt16List(flptr, temp, 2) == -1) {
        return -1;
    }

    ptr->versionRelease = VERSION_RELEASE;
    if (db_fwriteByte(flptr, VERSION_RELEASE) == -1) {
        return -1;
    }

    char* characterName = critter_name(obj_dude);
    strncpy(ptr->characterName, characterName, 32);

    if (db_fwrite(ptr->characterName, 32, 1, flptr) != 1) {
        return -1;
    }

    if (db_fwrite(ptr->description, 30, 1, flptr) != 1) {
        return -1;
    }

    // NOTE: Uninline.
    int file_time;
    GetTimeDate(&(temp[0]), &(temp[1]), &(temp[2]), &file_time);

    ptr->fileDay = temp[0];
    ptr->fileMonth = temp[1];
    ptr->fileYear = temp[2];
    ptr->fileTime = file_time;

    if (db_fwriteInt16List(flptr, temp, 3) == -1) {
        return -1;
    }

    if (db_fwriteInt32(flptr, ptr->fileTime) == -1) {
        return -1;
    }

    int month;
    int day;
    int year;
    game_time_date(&month, &day, &year);

    temp[0] = month;
    temp[1] = day;
    temp[2] = year;
    ptr->gameTime = game_time();

    if (db_fwriteInt16List(flptr, temp, 3) == -1) {
        return -1;
    }

    if (db_fwriteInt32(flptr, ptr->gameTime) == -1) {
        return -1;
    }

    ptr->elevation = map_elevation;
    if (db_fwriteShort(flptr, ptr->elevation) == -1) {
        return -1;
    }

    ptr->map = map_get_index_number();
    if (db_fwriteShort(flptr, ptr->map) == -1) {
        return -1;
    }

    char mapName[128];
    strcpy(mapName, map_data.name);

    char* v1 = strmfe(str, mapName, "sav");
    strncpy(ptr->fileName, v1, 16);
    if (db_fwrite(ptr->fileName, 16, 1, flptr) != 1) {
        return -1;
    }

    if (db_fwrite(thumbnail_image[1], LS_PREVIEW_SIZE, 1, flptr) != 1) {
        return -1;
    }

    memset(mapName, 0, 128);
    if (db_fwrite(mapName, 1, 128, flptr) != 128) {
        return -1;
    }

    ls_error_code = 0;

    return 0;
}

// 0x470350
static int LoadHeader(int slot)
{
    ls_error_code = 3;

    LoadSaveSlotData* ptr = &(LSData[slot]);

    if (db_fread(ptr->signature, 1, 24, flptr) != 24) {
        return -1;
    }

    if (strncmp(ptr->signature, LOAD_SAVE_SIGNATURE, 18) != 0) {
        debug_printf("\nLOADSAVE: ** Invalid save file on load! **\n");
        ls_error_code = 2;
        return -1;
    }

    short v8[3];
    if (db_freadInt16List(flptr, v8, 2) == -1) {
        return -1;
    }

    ptr->versionMinor = v8[0];
    ptr->versionMajor = v8[1];

    if (db_freadByte(flptr, &(ptr->versionRelease)) == -1) {
        return -1;
    }

    if (db_fread(ptr->characterName, 32, 1, flptr) != 1) {
        return -1;
    }

    if (db_fread(ptr->description, 30, 1, flptr) != 1) {
        return -1;
    }

    if (db_freadInt16List(flptr, v8, 3) == -1) {
        return -1;
    }

    ptr->fileMonth = v8[0];
    ptr->fileDay = v8[1];
    ptr->fileYear = v8[2];

    if (db_freadInt32(flptr, &(ptr->fileTime)) == -1) {
        return -1;
    }

    if (db_freadInt16List(flptr, v8, 3) == -1) {
        return -1;
    }

    ptr->gameMonth = v8[0];
    ptr->gameDay = v8[1];
    ptr->gameYear = v8[2];

    if (db_freadInt32(flptr, &(ptr->gameTime)) == -1) {
        return -1;
    }

    if (db_freadInt16(flptr, &(ptr->elevation)) == -1) {
        return -1;
    }

    if (db_freadInt16(flptr, &(ptr->map)) == -1) {
        return -1;
    }

    if (db_fread(ptr->fileName, 1, 16, flptr) != 16) {
        return -1;
    }

    if (db_fseek(flptr, LS_PREVIEW_SIZE, SEEK_CUR) != 0) {
        return -1;
    }

    if (db_fseek(flptr, 128, 1) != 0) {
        return -1;
    }

    ls_error_code = 0;

    return 0;
}

// 0x4705C4
static int GetSlotList()
{
    dir_entry de;
    int index = 0;
    for (; index < 10; index += 1) {
        snprintf(str, sizeof(str), "%s\\%s%.2d\\%s", "SAVEGAME", "SLOT", index + 1, "SAVE.DAT");

        if (db_dir_entry(str, &de) != 0) {
            LSstatus[index] = SLOT_STATE_EMPTY;
        } else {
            flptr = db_fopen(str, "rb");

            if (flptr == NULL) {
                debug_printf("\nLOADSAVE: ** Error opening save  game for reading! **\n");
                return -1;
            }

            if (LoadHeader(index) == -1) {
                if (ls_error_code == 1) {
                    debug_printf("LOADSAVE: ** save file #%d is an older version! **\n", slot_cursor);
                    LSstatus[index] = SLOT_STATE_UNSUPPORTED_VERSION;
                } else {
                    debug_printf("LOADSAVE: ** Save file #%d corrupt! **", index);
                    LSstatus[index] = SLOT_STATE_ERROR;
                }
            } else {
                LSstatus[index] = SLOT_STATE_OCCUPIED;
            }

            db_fclose(flptr);
        }
    }
    return index;
}

// 0x4706CC
static void ShowSlotList(int a1)
{
    buf_fill(lsgbuf + LS_WINDOW_WIDTH * 87 + 55, 230, 353, LS_WINDOW_WIDTH, lsgbuf[LS_WINDOW_WIDTH * 86 + 55] & 0xFF);

    int y = 87;
    for (int index = 0; index < 10; index += 1) {

        int color = index == slot_cursor ? colorTable[32747] : colorTable[992];
        const char* text = getmsg(&lsgame_msgfl, &lsgmesg, a1 != 0 ? 110 : 109);
        snprintf(str, sizeof(str), "[   %s %.2d:   ]", text, index + 1);
        text_to_buf(lsgbuf + LS_WINDOW_WIDTH * y + 55, str, LS_WINDOW_WIDTH, LS_WINDOW_WIDTH, color);

        y += text_height();
        switch (LSstatus[index]) {
        case SLOT_STATE_OCCUPIED:
            strcpy(str, LSData[index].description);
            break;
        case SLOT_STATE_EMPTY:
            // - EMPTY -
            text = getmsg(&lsgame_msgfl, &lsgmesg, 111);
            snprintf(str, sizeof(str), "       %s", text);
            break;
        case SLOT_STATE_ERROR:
            // - CORRUPT SAVE FILE -
            text = getmsg(&lsgame_msgfl, &lsgmesg, 112);
            snprintf(str, sizeof(str), "%s", text);
            color = colorTable[32328];
            break;
        case SLOT_STATE_UNSUPPORTED_VERSION:
            // - OLD VERSION -
            text = getmsg(&lsgame_msgfl, &lsgmesg, 113);
            snprintf(str, sizeof(str), " %s", text);
            color = colorTable[32328];
            break;
        }

        text_to_buf(lsgbuf + LS_WINDOW_WIDTH * y + 55, str, LS_WINDOW_WIDTH, LS_WINDOW_WIDTH, color);
        y += 2 * text_height() + 4;
    }
}

// 0x4708D4
static void DrawInfoBox(int a1)
{
    buf_to_buf(lsbmp[LOAD_SAVE_FRM_BACKGROUND] + LS_WINDOW_WIDTH * 254 + 396, 164, 60, LS_WINDOW_WIDTH, lsgbuf + LS_WINDOW_WIDTH * 254 + 396, 640);

    unsigned char* dest;
    const char* text;
    int color = colorTable[992];

    switch (LSstatus[a1]) {
    case SLOT_STATE_OCCUPIED:
        do {
            LoadSaveSlotData* ptr = &(LSData[a1]);
            text_to_buf(lsgbuf + LS_WINDOW_WIDTH * 254 + 396, ptr->characterName, LS_WINDOW_WIDTH, LS_WINDOW_WIDTH, color);

            int v4 = ptr->gameTime / 600;
            int minutes = v4 % 60;
            int v6 = 25 * (v4 / 60 % 24);
            int time = 4 * v6 + minutes;

            text = getmsg(&lsgame_msgfl, &lsgmesg, 116 + ptr->gameMonth);
            snprintf(str, sizeof(str), "%.2d %s %.4d   %.4d", ptr->gameDay, text, ptr->gameYear, time);

            int v2 = text_height();
            text_to_buf(lsgbuf + LS_WINDOW_WIDTH * (256 + v2) + 397, str, LS_WINDOW_WIDTH, LS_WINDOW_WIDTH, color);

            const char* v22 = map_get_elev_idx(ptr->map, ptr->elevation);
            const char* v9 = map_get_short_name(ptr->map);
            snprintf(str, sizeof(str), "%s %s", v9, v22);

            int y = v2 + 3 + v2 + 256;
            short beginnings[WORD_WRAP_MAX_COUNT];
            short count;
            if (word_wrap(str, 164, beginnings, &count) == 0) {
                for (int index = 0; index < count - 1; index += 1) {
                    char* beginning = str + beginnings[index];
                    char* ending = str + beginnings[index + 1];
                    char c = *ending;
                    *ending = '\0';
                    text_to_buf(lsgbuf + LS_WINDOW_WIDTH * y + 399, beginning, 164, LS_WINDOW_WIDTH, color);
                    y += v2 + 2;
                }
            }
        } while (0);
        return;
    case SLOT_STATE_EMPTY:
        // Empty.
        text = getmsg(&lsgame_msgfl, &lsgmesg, 114);
        dest = lsgbuf + LS_WINDOW_WIDTH * 262 + 404;
        break;
    case SLOT_STATE_ERROR:
        // Error!
        text = getmsg(&lsgame_msgfl, &lsgmesg, 115);
        dest = lsgbuf + LS_WINDOW_WIDTH * 262 + 404;
        color = colorTable[32328];
        break;
    case SLOT_STATE_UNSUPPORTED_VERSION:
        // Old version.
        text = getmsg(&lsgame_msgfl, &lsgmesg, 116);
        dest = lsgbuf + LS_WINDOW_WIDTH * 262 + 400;
        color = colorTable[32328];
        break;
    default:
        assert(false && "Should be unreachable");
    }

    text_to_buf(dest, text, LS_WINDOW_WIDTH, LS_WINDOW_WIDTH, color);
}

// 0x470C3C
static int LoadTumbSlot(int a1)
{
    DB_FILE* stream;
    int v2;

    v2 = LSstatus[slot_cursor];
    if (v2 != 0 && v2 != 2 && v2 != 3) {
        snprintf(str, sizeof(str), "%s\\%s%.2d\\%s", "SAVEGAME", "SLOT", slot_cursor + 1, "SAVE.DAT");
        debug_printf(" Filename %s\n", str);

        stream = db_fopen(str, "rb");
        if (stream == NULL) {
            debug_printf("\nLOADSAVE: ** (A) Error reading thumbnail #%d! **\n", a1);
            return -1;
        }

        if (db_fseek(stream, 131, SEEK_SET) != 0) {
            debug_printf("\nLOADSAVE: ** (B) Error reading thumbnail #%d! **\n", a1);
            db_fclose(stream);
            return -1;
        }

        if (db_fread(thumbnail_image[0], LS_PREVIEW_SIZE, 1, stream) != 1) {
            debug_printf("\nLOADSAVE: ** (C) Error reading thumbnail #%d! **\n", a1);
            db_fclose(stream);
            return -1;
        }

        db_fclose(stream);
    }

    return 0;
}

// 0x470D50
static int GetComment(int a1)
{
    // Maintain original position in original resolution, otherwise center it.
    int commentWindowX = screenGetWidth() != 640
        ? (screenGetWidth() - ginfo[LOAD_SAVE_FRM_BOX].width) / 2
        : LS_COMMENT_WINDOW_X;
    int commentWindowY = screenGetHeight() != 480
        ? (screenGetHeight() - ginfo[LOAD_SAVE_FRM_BOX].height) / 2
        : LS_COMMENT_WINDOW_Y;
    int window = win_add(commentWindowX,
        commentWindowY,
        ginfo[LOAD_SAVE_FRM_BOX].width,
        ginfo[LOAD_SAVE_FRM_BOX].height,
        256,
        WINDOW_MODAL | WINDOW_MOVE_ON_TOP);
    if (window == -1) {
        return -1;
    }

    unsigned char* windowBuffer = win_get_buf(window);
    memcpy(windowBuffer,
        lsbmp[LOAD_SAVE_FRM_BOX],
        ginfo[LOAD_SAVE_FRM_BOX].height * ginfo[LOAD_SAVE_FRM_BOX].width);

    text_font(103);

    const char* msg;

    // DONE
    msg = getmsg(&lsgame_msgfl, &lsgmesg, 104);
    text_to_buf(windowBuffer + ginfo[LOAD_SAVE_FRM_BOX].width * 57 + 56,
        msg,
        ginfo[LOAD_SAVE_FRM_BOX].width,
        ginfo[LOAD_SAVE_FRM_BOX].width,
        colorTable[18979]);

    // CANCEL
    msg = getmsg(&lsgame_msgfl, &lsgmesg, 105);
    text_to_buf(windowBuffer + ginfo[LOAD_SAVE_FRM_BOX].width * 57 + 181,
        msg,
        ginfo[LOAD_SAVE_FRM_BOX].width,
        ginfo[LOAD_SAVE_FRM_BOX].width,
        colorTable[18979]);

    // DESCRIPTION
    msg = getmsg(&lsgame_msgfl, &lsgmesg, 130);

    char title[260];
    strcpy(title, msg);

    int width = text_width(title);
    text_to_buf(windowBuffer + ginfo[LOAD_SAVE_FRM_BOX].width * 7 + (ginfo[LOAD_SAVE_FRM_BOX].width - width) / 2,
        title,
        ginfo[LOAD_SAVE_FRM_BOX].width,
        ginfo[LOAD_SAVE_FRM_BOX].width,
        colorTable[18979]);

    text_font(101);

    int btn;

    // DONE
    btn = win_register_button(window,
        34,
        58,
        ginfo[LOAD_SAVE_FRM_RED_BUTTON_PRESSED].width,
        ginfo[LOAD_SAVE_FRM_RED_BUTTON_PRESSED].height,
        -1,
        -1,
        -1,
        507,
        lsbmp[LOAD_SAVE_FRM_RED_BUTTON_NORMAL],
        lsbmp[LOAD_SAVE_FRM_RED_BUTTON_PRESSED],
        NULL,
        BUTTON_FLAG_TRANSPARENT);
    if (btn == -1) {
        win_register_button_sound_func(btn, gsound_red_butt_press, gsound_red_butt_release);
    }

    // CANCEL
    btn = win_register_button(window,
        160,
        58,
        ginfo[LOAD_SAVE_FRM_RED_BUTTON_PRESSED].width,
        ginfo[LOAD_SAVE_FRM_RED_BUTTON_PRESSED].height,
        -1,
        -1,
        -1,
        508,
        lsbmp[LOAD_SAVE_FRM_RED_BUTTON_NORMAL],
        lsbmp[LOAD_SAVE_FRM_RED_BUTTON_PRESSED],
        NULL,
        BUTTON_FLAG_TRANSPARENT);
    if (btn == -1) {
        win_register_button_sound_func(btn, gsound_red_butt_press, gsound_red_butt_release);
    }

    win_draw(window);

    char description[LOAD_SAVE_DESCRIPTION_LENGTH];
    if (LSstatus[slot_cursor] == SLOT_STATE_OCCUPIED) {
        strncpy(description, LSData[a1].description, LOAD_SAVE_DESCRIPTION_LENGTH);
    } else {
        memset(description, '\0', LOAD_SAVE_DESCRIPTION_LENGTH);
    }

    int rc;

    if (get_input_str2(window, 507, 508, description, LOAD_SAVE_DESCRIPTION_LENGTH - 1, 24, 35, colorTable[992], lsbmp[LOAD_SAVE_FRM_BOX][ginfo[1].width * 35 + 24], 0) == 0) {
        strncpy(LSData[a1].description, description, LOAD_SAVE_DESCRIPTION_LENGTH);
        LSData[a1].description[LOAD_SAVE_DESCRIPTION_LENGTH - 1] = '\0';
        rc = 1;
    } else {
        rc = 0;
    }

    win_delete(window);

    return rc;
}

// 0x471070
static int get_input_str2(int win, int doneKeyCode, int cancelKeyCode, char* description, int maxLength, int x, int y, int textColor, int backgroundColor, int flags)
{
    int cursorWidth = text_width("_") - 4;
    int windowWidth = win_width(win);
    int lineHeight = text_height();
    unsigned char* windowBuffer = win_get_buf(win);
    if (maxLength > 255) {
        maxLength = 255;
    }

    char text[256];
    strcpy(text, description);

    int textLength = strlen(text);
    text[textLength] = ' ';
    text[textLength + 1] = '\0';

    int nameWidth = text_width(text);

    buf_fill(windowBuffer + windowWidth * y + x, nameWidth, lineHeight, windowWidth, backgroundColor);
    text_to_buf(windowBuffer + windowWidth * y + x, text, windowWidth, windowWidth, textColor);

    win_draw(win);
    renderPresent();

    beginTextInput();

    int blinkCounter = 3;
    bool blink = false;

    int v1 = 0;

    int rc = 1;
    while (rc == 1) {
        sharedFpsLimiter.mark();

        int tick = get_time();

        int keyCode = get_input();
        if ((keyCode & 0x80000000) == 0) {
            v1++;
        }

        if (keyCode == doneKeyCode || keyCode == KEY_RETURN) {
            rc = 0;
        } else if (keyCode == cancelKeyCode || keyCode == KEY_ESCAPE) {
            rc = -1;
        } else {
            if ((keyCode == KEY_DELETE || keyCode == KEY_BACKSPACE) && textLength > 0) {
                buf_fill(windowBuffer + windowWidth * y + x, text_width(text), lineHeight, windowWidth, backgroundColor);

                // TODO: Probably incorrect, needs testing.
                if (v1 == 1) {
                    textLength = 1;
                }

                text[textLength - 1] = ' ';
                text[textLength] = '\0';
                text_to_buf(windowBuffer + windowWidth * y + x, text, windowWidth, windowWidth, textColor);
                textLength--;
            } else if ((keyCode >= KEY_FIRST_INPUT_CHARACTER && keyCode <= KEY_LAST_INPUT_CHARACTER) && textLength < maxLength) {
                if ((flags & 0x01) != 0) {
                    if (!isdoschar(keyCode)) {
                        break;
                    }
                }

                buf_fill(windowBuffer + windowWidth * y + x, text_width(text), lineHeight, windowWidth, backgroundColor);

                text[textLength] = keyCode & 0xFF;
                text[textLength + 1] = ' ';
                text[textLength + 2] = '\0';
                text_to_buf(windowBuffer + windowWidth * y + x, text, windowWidth, windowWidth, textColor);
                textLength++;

                win_draw(win);
            }
        }

        blinkCounter -= 1;
        if (blinkCounter == 0) {
            blinkCounter = 3;
            blink = !blink;

            int color = blink ? backgroundColor : textColor;
            buf_fill(windowBuffer + windowWidth * y + x + text_width(text) - cursorWidth, cursorWidth, lineHeight - 2, windowWidth, color);
            win_draw(win);
        }

        while (elapsed_time(tick) < 1000 / 24) {
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    endTextInput();

    if (rc == 0) {
        text[textLength] = '\0';
        strcpy(description, text);
    }

    return rc;
}

// 0x471444
static int DummyFunc(DB_FILE* stream)
{
    return 0;
}

// 0x471448
static int PrepLoad(DB_FILE* stream)
{
    game_reset();
    map_data.name[0] = '\0';
    set_game_time(LSData[slot_cursor].gameTime);
    return 0;
}

// 0x471474
static int EndLoad(DB_FILE* stream)
{
    PlayCityMapMusic();
    critter_pc_set_name(LSData[slot_cursor].characterName);
    intface_redraw();
    refresh_box_bar_win();
    tile_refresh_display();
    if (isInCombat()) {
        scripts_request_combat(NULL);
    }
    return 0;
}

// 0x4714BC
static int GameMap2Slot(DB_FILE* stream)
{
    if (partyMemberPrepSave() == -1) {
        return -1;
    }

    if (map_save_in_game(false) == -1) {
        return -1;
    }

    snprintf(str0, sizeof(str0), "%s\\*.%s", "MAPS", "SAV");

    char** fileNameList;
    int fileNameListLength = db_get_file_list(str0, &fileNameList, NULL, 0);
    if (fileNameListLength == -1) {
        return -1;
    }

    if (db_fwriteInt(stream, fileNameListLength) == -1) {
        db_free_file_list(&fileNameList, NULL);
        return -1;
    }

    if (fileNameListLength == 0) {
        db_free_file_list(&fileNameList, NULL);
        return -1;
    }

    snprintf(gmpath, sizeof(gmpath), "%s\\%s%.2d\\", "SAVEGAME", "SLOT", slot_cursor + 1);

    if (MapDirErase(gmpath, "SAV") == -1) {
        db_free_file_list(&fileNameList, NULL);
        return -1;
    }

    snprintf(gmpath, sizeof(gmpath), "%s\\%s\\%s%.2d\\", patches, "SAVEGAME", "SLOT", slot_cursor + 1);
    strmfe(str0, "AUTOMAP.DB", "SAV");
    strcat(gmpath, str0);
    compat_remove(gmpath);

    for (int index = 0; index < fileNameListLength; index += 1) {
        char* string = fileNameList[index];
        if (db_fwrite(string, strlen(string) + 1, 1, stream) == -1) {
            db_free_file_list(&fileNameList, NULL);
            return -1;
        }

        snprintf(str0, sizeof(str0), "%s\\%s", "MAPS", string);
        snprintf(str1, sizeof(str1), "%s\\%s%.2d\\%s", "SAVEGAME", "SLOT", slot_cursor + 1, string);
        if (copy_file(str0, str1) == -1) {
            db_free_file_list(&fileNameList, NULL);
            return -1;
        }
    }

    db_free_file_list(&fileNameList, NULL);

    strmfe(str0, "AUTOMAP.DB", "SAV");
    snprintf(str1, sizeof(str1), "%s\\%s%.2d\\%s", "SAVEGAME", "SLOT", slot_cursor + 1, str0);
    snprintf(str0, sizeof(str0), "%s\\%s", "MAPS", "AUTOMAP.DB");

    if (copy_file(str0, str1) == -1) {
        return -1;
    }

    snprintf(str0, sizeof(str0), "%s\\%s", "MAPS", "AUTOMAP.DB");
    DB_FILE* automap_stream = db_fopen(str0, "rb");
    if (automap_stream == NULL) {
        return -1;
    }

    int automap_size = db_filelength(automap_stream);
    if (automap_size == -1) {
        db_fclose(automap_stream);
        return -1;
    }

    db_fclose(automap_stream);

    if (db_fwriteInt(stream, automap_size) == -1) {
        return -1;
    }

    if (partyMemberUnPrepSave() == -1) {
        return -1;
    }

    return 0;
}

// 0x4717E0
static int SlotMap2Game(DB_FILE* stream)
{
    int fileNameListLength;
    if (db_freadInt(stream, &fileNameListLength) == -1) {
        return -1;
    }

    if (fileNameListLength == 0) {
        return -1;
    }

    snprintf(str0, sizeof(str0), "%s\\", "MAPS");
    if (MapDirErase(str0, "SAV") == -1) {
        return -1;
    }

    snprintf(str0, sizeof(str0), "%s\\%s\\%s", patches, "MAPS", "AUTOMAP.DB");
    compat_remove(str0);

    for (int index = 0; index < fileNameListLength; index += 1) {
        char fileName[COMPAT_MAX_PATH];
        if (mygets(fileName, stream) == -1) {
            break;
        }

        snprintf(str0, sizeof(str0), "%s\\%s%.2d\\%s", "SAVEGAME", "SLOT", slot_cursor + 1, fileName);
        snprintf(str1, sizeof(str1), "%s\\%s", "MAPS", fileName);

        if (copy_file(str0, str1) == -1) {
            debug_printf("LOADSAVE: returning 7\n");
            return -1;
        }
    }

    const char* automapFileName = strmfe(str1, "AUTOMAP.DB", "SAV");
    snprintf(str0, sizeof(str0), "%s\\%s%.2d\\%s", "SAVEGAME", "SLOT", slot_cursor + 1, automapFileName);
    snprintf(str1, sizeof(str1), "%s\\%s", "MAPS", "AUTOMAP.DB");
    if (copy_file(str0, str1) == -1) {
        return -1;
    }

    int saved_automap_size;
    if (db_freadInt(stream, &saved_automap_size) == -1) {
        return -1;
    }

    DB_FILE* automap_stream = db_fopen(str1, "rb");
    if (automap_stream == NULL) {
        return -1;
    }

    int automap_size = db_filelength(automap_stream);
    if (automap_size == -1) {
        db_fclose(automap_stream);
        return -1;
    }

    db_fclose(automap_stream);
    if (saved_automap_size != automap_size) {
        return -1;
    }

    if (map_load_in_game(LSData[slot_cursor].fileName) == -1) {
        debug_printf("LOADSAVE: returning 13\n");
        return -1;
    }

    return 0;
}

// 0x471A44
static int mygets(char* dest, DB_FILE* stream)
{
    int index = 14;
    while (true) {
        int c = db_fgetc(stream);
        if (c == -1) {
            return -1;
        }

        index -= 1;

        *dest = c & 0xFF;
        dest += 1;

        if (index == -1 || c == '\0') {
            break;
        }
    }

    if (index == 0) {
        return -1;
    }

    return 0;
}

// 0x471A88
static int copy_file(const char* a1, const char* a2)
{
    DB_FILE* stream1;
    DB_FILE* stream2;
    int length;
    int chunk_length;
    void* buf;
    int result;

    stream1 = NULL;
    stream2 = NULL;
    buf = NULL;
    result = -1;

    stream1 = db_fopen(a1, "rb");
    if (stream1 == NULL) {
        goto out;
    }

    length = db_filelength(stream1);
    if (length == -1) {
        goto out;
    }

    stream2 = db_fopen(a2, "wb");
    if (stream2 == NULL) {
        goto out;
    }

    buf = mem_malloc(0xFFFF);
    if (buf == NULL) {
        goto out;
    }

    while (length != 0) {
        chunk_length = std::min(length, 0xFFFF);

        if (db_fread(buf, chunk_length, 1, stream1) != 1) {
            break;
        }

        if (db_fwrite(buf, chunk_length, 1, stream2) != 1) {
            break;
        }

        length -= chunk_length;
    }

    if (length != 0) {
        goto out;
    }

    result = 0;

out:

    if (stream1 != NULL) {
        db_fclose(stream1);
    }

    if (stream2 != NULL) {
        db_fclose(stream2);
    }

    if (buf != NULL) {
        mem_free(buf);
    }

    return result;
}

// 0x471C3C
void KillOldMaps()
{
    snprintf(str, sizeof(str), "%s\\", "MAPS");
    MapDirErase(str, "SAV");
}

// 0x471C68
int MapDirErase(const char* relativePath, const char* extension)
{
    char path[COMPAT_MAX_PATH];
    snprintf(path, sizeof(path), "%s*.%s", relativePath, extension);

    char** fileList;
    int fileListLength = db_get_file_list(path, &fileList, NULL, 0);
    if (fileListLength == -1) {
        return -1;
    }

    while (--fileListLength >= 0) {
        snprintf(path, sizeof(path), "%s\\%s%s", patches, relativePath, fileList[fileListLength]);
        if (compat_remove(path) != 0) {
            db_free_file_list(&fileList, NULL);
            return -1;
        }
    }
    db_free_file_list(&fileList, NULL);

    return 0;
}

// 0x471CFC
int MapDirEraseFile(const char* a1, const char* a2)
{
    char path[COMPAT_MAX_PATH];

    snprintf(path, sizeof(path), "%s\\%s%s", patches, a1, a2);
    if (compat_remove(path) != 0) {
        return -1;
    }

    return 0;
}

// 0x471D38
static int SaveBackup()
{
    debug_printf("\nLOADSAVE: Backing up save slot files..\n");

    snprintf(gmpath, sizeof(gmpath), "%s\\%s\\%s%.2d\\", patches, "SAVEGAME", "SLOT", slot_cursor + 1);
    strcpy(str0, gmpath);

    strcat(str0, "SAVE.DAT");

    strmfe(str1, str0, "BAK");

    DB_FILE* stream1 = db_fopen(str0, "rb");
    if (stream1 != NULL) {
        db_fclose(stream1);
        if (compat_rename(str0, str1) != 0) {
            return -1;
        }
    }

    snprintf(gmpath, sizeof(gmpath), "%s\\%s%.2d\\", "SAVEGAME", "SLOT", slot_cursor + 1);
    snprintf(str0, sizeof(str0), "%s*.%s", gmpath, "SAV");

    char** fileList;
    int fileListLength = db_get_file_list(str0, &fileList, NULL, 0);
    if (fileListLength == -1) {
        return -1;
    }

    map_backup_count = fileListLength;

    snprintf(gmpath, sizeof(gmpath), "%s\\%s\\%s%.2d\\", patches, "SAVEGAME", "SLOT", slot_cursor + 1);
    for (int index = fileListLength - 1; index >= 0; index--) {
        strcpy(str0, gmpath);
        strcat(str0, fileList[index]);

        strmfe(str1, str0, "BAK");
        if (compat_rename(str0, str1) != 0) {
            db_free_file_list(&fileList, NULL);
            return -1;
        }
    }

    db_free_file_list(&fileList, NULL);

    debug_printf("\nLOADSAVE: %d map files backed up.\n", fileListLength);

    snprintf(gmpath, sizeof(gmpath), "%s\\%s%.2d\\", "SAVEGAME", "SLOT", slot_cursor + 1);

    char* v1 = strmfe(str2, "AUTOMAP.DB", "SAV");
    snprintf(str0, sizeof(str0), "%s\\%s", gmpath, v1);

    char* v2 = strmfe(str2, "AUTOMAP.DB", "BAK");
    snprintf(str1, sizeof(str1), "%s\\%s", gmpath, v2);

    automap_db_flag = 0;

    DB_FILE* stream2 = db_fopen(str0, "rb");
    if (stream2 != NULL) {
        db_fclose(stream2);

        if (copy_file(str0, str1) == -1) {
            return -1;
        }

        automap_db_flag = 1;
    }

    return 0;
}

// 0x47200C
static int RestoreSave()
{
    debug_printf("\nLOADSAVE: Restoring save file backup...\n");

    EraseSave();

    snprintf(gmpath, sizeof(gmpath), "%s\\%s\\%s%.2d\\", patches, "SAVEGAME", "SLOT", slot_cursor + 1);
    strcpy(str0, gmpath);
    strcat(str0, "SAVE.DAT");
    strmfe(str1, str0, "BAK");
    compat_remove(str0);

    if (compat_rename(str1, str0) != 0) {
        EraseSave();
        return -1;
    }

    snprintf(gmpath, sizeof(gmpath), "%s\\%s%.2d\\", "SAVEGAME", "SLOT", slot_cursor + 1);
    snprintf(str0, sizeof(str0), "%s*.%s", gmpath, "BAK");

    char** fileList;
    int fileListLength = db_get_file_list(str0, &fileList, NULL, 0);
    if (fileListLength == -1) {
        return -1;
    }

    if (fileListLength != map_backup_count) {
        // FIXME: Probably leaks fileList.
        EraseSave();
        return -1;
    }

    snprintf(gmpath, sizeof(gmpath), "%s\\%s\\%s%.2d\\", patches, "SAVEGAME", "SLOT", slot_cursor + 1);

    for (int index = fileListLength - 1; index >= 0; index--) {
        strcpy(str0, gmpath);
        strcat(str0, fileList[index]);
        strmfe(str1, str0, "SAV");
        compat_remove(str1);
        if (compat_rename(str0, str1) != 0) {
            // FIXME: Probably leaks fileList.
            EraseSave();
            return -1;
        }
    }

    db_free_file_list(&fileList, NULL);

    if (!automap_db_flag) {
        return 0;
    }

    snprintf(gmpath, sizeof(gmpath), "%s\\%s\\%s%.2d\\", patches, "SAVEGAME", "SLOT", slot_cursor + 1);
    char* v1 = strmfe(str2, "AUTOMAP.DB", "BAK");
    strcpy(str0, gmpath);
    strcat(str0, v1);

    char* v2 = strmfe(str2, "AUTOMAP.DB", "SAV");
    strcpy(str1, gmpath);
    strcat(str1, v2);

    if (compat_rename(str0, str1) != 0) {
        EraseSave();
        return -1;
    }

    return 0;
}

// 0x472344
static int LoadObjDudeCid(DB_FILE* stream)
{
    int value;

    if (db_freadInt(stream, &value) == -1) {
        return -1;
    }

    obj_dude->cid = value;

    return 0;
}

// 0x472368
static int SaveObjDudeCid(DB_FILE* stream)
{
    return db_fwriteInt(stream, obj_dude->cid);
}

// 0x472388
static int EraseSave()
{
    debug_printf("\nLOADSAVE: Erasing save(bad) slot...\n");

    snprintf(gmpath, sizeof(gmpath), "%s\\%s\\%s%.2d\\", patches, "SAVEGAME", "SLOT", slot_cursor + 1);
    strcpy(str0, gmpath);
    strcat(str0, "SAVE.DAT");
    compat_remove(str0);

    snprintf(gmpath, sizeof(gmpath), "%s\\%s%.2d\\", "SAVEGAME", "SLOT", slot_cursor + 1);
    snprintf(str0, sizeof(str0), "%s*.%s", gmpath, "SAV");

    char** fileList;
    int fileListLength = db_get_file_list(str0, &fileList, NULL, 0);
    if (fileListLength == -1) {
        return -1;
    }

    snprintf(gmpath, sizeof(gmpath), "%s\\%s\\%s%.2d\\", patches, "SAVEGAME", "SLOT", slot_cursor + 1);
    for (int index = fileListLength - 1; index >= 0; index--) {
        strcpy(str0, gmpath);
        strcat(str0, fileList[index]);
        compat_remove(str0);
    }

    db_free_file_list(&fileList, NULL);

    snprintf(gmpath, sizeof(gmpath), "%s\\%s\\%s%.2d\\", patches, "SAVEGAME", "SLOT", slot_cursor + 1);

    char* v1 = strmfe(str1, "AUTOMAP.DB", "SAV");
    strcpy(str0, gmpath);
    strcat(str0, v1);

    compat_remove(str0);

    return 0;
}

} // namespace fallout
