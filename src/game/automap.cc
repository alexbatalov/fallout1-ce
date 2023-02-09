#include "game/automap.h"

#include <stdio.h>
#include <string.h>

#include <algorithm>

#include "game/bmpdlog.h"
#include "game/config.h"
#include "game/game.h"
#include "game/gconfig.h"
#include "game/gmouse.h"
#include "game/graphlib.h"
#include "game/gsound.h"
#include "game/item.h"
#include "game/map.h"
#include "game/object.h"
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

#define AUTOMAP_OFFSET_COUNT (AUTOMAP_MAP_COUNT * ELEVATION_COUNT)

#define AUTOMAP_WINDOW_WIDTH 519
#define AUTOMAP_WINDOW_HEIGHT 480

#define AUTOMAP_PIPBOY_VIEW_X 238
#define AUTOMAP_PIPBOY_VIEW_Y 105

// View options for rendering automap for map window. These are stored in
// [autoflags] and is saved in save game file.
typedef enum AutomapFlags {
    // NOTE: This is a special flag to denote the map is activated in the game (as
    // opposed to the mapper). It's always on. Turning it off produces nice color
    // coded map with all objects and their types visible, however there is no way
    // you can do it within the game UI.
    AUTOMAP_IN_GAME = 0x01,

    // High details is on.
    AUTOMAP_WTH_HIGH_DETAILS = 0x02,

    // Scanner is active.
    AUTOMAP_WITH_SCANNER = 0x04,
} AutomapFlags;

typedef enum AutomapFrm {
    AUTOMAP_FRM_BACKGROUND,
    AUTOMAP_FRM_BUTTON_UP,
    AUTOMAP_FRM_BUTTON_DOWN,
    AUTOMAP_FRM_SWITCH_UP,
    AUTOMAP_FRM_SWITCH_DOWN,
    AUTOMAP_FRM_COUNT,
} AutomapFrm;

static void draw_top_down_map(int window, int elevation, unsigned char* backgroundData, int flags);
static int WriteAM_Entry(DB_FILE* stream);
static int AM_ReadEntry(int map, int elevation);
static int WriteAM_Header(DB_FILE* stream);
static int AM_ReadMainHeader(DB_FILE* stream);
static void decode_map_data(int elevation);
static int am_pip_init();
static int copy_file_data(DB_FILE* stream1, DB_FILE* stream2, int length);

// 0x41A420
static const int defam[AUTOMAP_MAP_COUNT][ELEVATION_COUNT] = {
    /*  DESERT1 */ { -1, -1, -1 },
    /*  DESERT2 */ { -1, -1, -1 },
    /*  DESERT3 */ { -1, -1, -1 },
    /*  HALLDED */ { 0, 0, 0 },
    /*    HOTEL */ { 0, 0, 0 },
    /*  WATRSHD */ { 0, 0, 0 },
    /*  VAULT13 */ { 0, 0, 0 },
    /* VAULTENT */ { 0, 0, 0 },
    /* VAULTBUR */ { 0, 0, 0 },
    /* VAULTNEC */ { 0, 0, 0 },
    /*  JUNKENT */ { 0, 0, 0 },
    /* JUNKCSNO */ { 0, 0, 0 },
    /* JUNKKILL */ { 0, 0, 0 },
    /* BROHDENT */ { 0, 0, 0 },
    /*  BROHD12 */ { 0, 0, 0 },
    /*  BROHD34 */ { 0, 0, 0 },
    /*    CAVES */ { 0, 0, 0 },
    /* CHILDRN1 */ { 0, 0, 0 },
    /* CHILDRN2 */ { 0, 0, 0 },
    /*    CITY1 */ { -1, -1, -1 },
    /*   COAST1 */ { -1, -1, -1 },
    /*   COAST2 */ { -1, -1, -1 },
    /* COLATRUK */ { -1, -1, -1 },
    /*  FSAUSER */ { -1, -1, -1 },
    /*  RAIDERS */ { 0, 0, 0 },
    /*   SHADYE */ { 0, 0, 0 },
    /*   SHADYW */ { 0, 0, 0 },
    /*  GLOWENT */ { 0, 0, 0 },
    /* LAADYTUM */ { 0, 0, 0 },
    /* LAFOLLWR */ { 0, 0, 0 },
    /*    MBENT */ { 0, 0, 0 },
    /* MBSTRG12 */ { 0, 0, 0 },
    /* MBVATS12 */ { 0, 0, 0 },
    /* MSTRLR12 */ { 0, 0, 0 },
    /* MSTRLR34 */ { 0, 0, 0 },
    /*   V13ENT */ { 0, 0, 0 },
    /*   HUBENT */ { 0, 0, 0 },
    /* DETHCLAW */ { 0, 0, 0 },
    /* HUBDWNTN */ { 0, 0, 0 },
    /* HUBHEIGT */ { 0, 0, 0 },
    /* HUBOLDTN */ { 0, 0, 0 },
    /* HUBWATER */ { 0, 0, 0 },
    /*    GLOW1 */ { 0, 0, 0 },
    /*    GLOW2 */ { 0, 0, 0 },
    /* LABLADES */ { 0, 0, 0 },
    /* LARIPPER */ { 0, 0, 0 },
    /* LAGUNRUN */ { 0, 0, 0 },
    /* CHILDEAD */ { 0, 0, 0 },
    /*   MBDEAD */ { 0, 0, 0 },
    /*  MOUNTN1 */ { -1, -1, -1 },
    /*  MOUNTN2 */ { -1, -1, -1 },
    /*     FOOT */ { -1, -1, -1 },
    /*   TARDIS */ { -1, -1, -1 },
    /*  TALKCOW */ { -1, -1, -1 },
    /*  USEDCAR */ { -1, -1, -1 },
    /*  BRODEAD */ { 0, 0, 0 },
    /* DESCRVN1 */ { -1, -1, -1 },
    /* DESCRVN2 */ { -1, -1, -1 },
    /* MNTCRVN1 */ { -1, -1, -1 },
    /* MNTCRVN2 */ { -1, -1, -1 },
    /*   VIPERS */ { -1, -1, -1 },
    /* DESCRVN3 */ { -1, -1, -1 },
    /* MNTCRVN3 */ { -1, -1, -1 },
    /* DESCRVN4 */ { -1, -1, -1 },
    /* MNTCRVN4 */ { -1, -1, -1 },
    /*  HUBMIS1 */ { -1, -1, -1 },
};

// 0x4FEC08
static int autoflags = 0;

// 0x56B878
static AutomapHeader amdbhead;

// 0x56BB98
static AutomapEntry amdbsubhead;

// 0x56BBA0
static unsigned char* cmpbuf;

// 0x56BBA4
static unsigned char* ambuf;

// 0x41A74C
int automap_init()
{
    autoflags = 0;
    am_pip_init();
    return 0;
}

// 0x41A760
int automap_reset()
{
    autoflags = 0;
    am_pip_init();
    return 0;
}

// 0x41A774
void automap_exit()
{
    char* masterPatchesPath;
    if (config_get_string(&game_config, GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_MASTER_PATCHES_KEY, &masterPatchesPath)) {
        char path[COMPAT_MAX_PATH];
        snprintf(path, sizeof(path), "%s\\%s\\%s", masterPatchesPath, "MAPS", AUTOMAP_DB);
        compat_remove(path);
    }
}

// 0x41A7D4
int automap_load(DB_FILE* stream)
{
    return db_freadInt(stream, &autoflags);
}

// 0x41A7F0
int automap_save(DB_FILE* stream)
{
    return db_fwriteInt(stream, autoflags);
}

// 0x41A80C
void automap(bool isInGame, bool isUsingScanner)
{
    // 0x41A738
    static const int frmIds[AUTOMAP_FRM_COUNT] = {
        171, // automap.frm - automap window
        8, // lilredup.frm - little red button up
        9, // lilreddn.frm - little red button down
        172, // autoup.frm - switch up
        173, // autodwn.frm - switch down
    };

    unsigned char* frmData[AUTOMAP_FRM_COUNT];
    CacheEntry* frmHandle[AUTOMAP_FRM_COUNT];
    for (int index = 0; index < AUTOMAP_FRM_COUNT; index++) {
        int fid = art_id(OBJ_TYPE_INTERFACE, frmIds[index], 0, 0, 0);
        frmData[index] = art_ptr_lock_data(fid, 0, 0, &(frmHandle[index]));
        if (frmData[index] == NULL) {
            while (--index >= 0) {
                art_ptr_unlock(frmHandle[index]);
            }
            return;
        }
    }

    int color;
    if (isInGame) {
        color = colorTable[8456];
        obj_process_seen();
    } else {
        color = colorTable[22025];
    }

    int oldFont = text_curr();
    text_font(101);

    int automapWindowX = (screenGetWidth() - AUTOMAP_WINDOW_WIDTH) / 2;
    int automapWindowY = (screenGetHeight() - AUTOMAP_WINDOW_HEIGHT) / 2;
    int window = win_add(automapWindowX, automapWindowY, AUTOMAP_WINDOW_WIDTH, AUTOMAP_WINDOW_HEIGHT, color, WINDOW_MODAL | WINDOW_MOVE_ON_TOP);

    int scannerBtn = win_register_button(window, 111, 454, 15, 16, -1, -1, -1, KEY_LOWERCASE_S, frmData[AUTOMAP_FRM_BUTTON_UP], frmData[AUTOMAP_FRM_BUTTON_DOWN], NULL, BUTTON_FLAG_TRANSPARENT);
    if (scannerBtn != -1) {
        win_register_button_sound_func(scannerBtn, gsound_red_butt_press, gsound_red_butt_release);
    }

    int cancelBtn = win_register_button(window, 277, 454, 15, 16, -1, -1, -1, KEY_ESCAPE, frmData[AUTOMAP_FRM_BUTTON_UP], frmData[AUTOMAP_FRM_BUTTON_DOWN], NULL, BUTTON_FLAG_TRANSPARENT);
    if (cancelBtn != -1) {
        win_register_button_sound_func(cancelBtn, gsound_red_butt_press, gsound_red_butt_release);
    }

    int switchBtn = win_register_button(window, 457, 340, 42, 74, -1, -1, KEY_LOWERCASE_L, KEY_LOWERCASE_H, frmData[AUTOMAP_FRM_SWITCH_UP], frmData[AUTOMAP_FRM_SWITCH_DOWN], NULL, BUTTON_FLAG_TRANSPARENT | BUTTON_FLAG_0x01);
    if (switchBtn != -1) {
        win_register_button_sound_func(switchBtn, gsound_toggle_butt_press, gsound_toggle_butt_release);
    }

    if ((autoflags & AUTOMAP_WTH_HIGH_DETAILS) == 0) {
        win_set_button_rest_state(switchBtn, 1, 0);
    }

    int elevation = map_elevation;

    autoflags &= AUTOMAP_WTH_HIGH_DETAILS;

    if (isInGame) {
        autoflags |= AUTOMAP_IN_GAME;
    }

    if (isUsingScanner) {
        autoflags |= AUTOMAP_WITH_SCANNER;
    }

    draw_top_down_map(window, elevation, frmData[AUTOMAP_FRM_BACKGROUND], autoflags);

    bool isoWasEnabled = map_disable_bk_processes();
    gmouse_set_cursor(MOUSE_CURSOR_ARROW);

    bool done = false;
    while (!done) {
        sharedFpsLimiter.mark();

        bool needsRefresh = false;

        // FIXME: There is minor bug in the interface - pressing H/L to toggle
        // high/low details does not update switch state.
        int keyCode = get_input();
        switch (keyCode) {
        case KEY_TAB:
        case KEY_ESCAPE:
        case KEY_UPPERCASE_A:
        case KEY_LOWERCASE_A:
            done = true;
            break;
        case KEY_UPPERCASE_H:
        case KEY_LOWERCASE_H:
            if ((autoflags & AUTOMAP_WTH_HIGH_DETAILS) == 0) {
                autoflags |= AUTOMAP_WTH_HIGH_DETAILS;
                needsRefresh = true;
            }
            break;
        case KEY_UPPERCASE_L:
        case KEY_LOWERCASE_L:
            if ((autoflags & AUTOMAP_WTH_HIGH_DETAILS) != 0) {
                autoflags &= ~AUTOMAP_WTH_HIGH_DETAILS;
                needsRefresh = true;
            }
            break;
        case KEY_UPPERCASE_S:
        case KEY_LOWERCASE_S:
            if (elevation != map_elevation) {
                elevation = map_elevation;
                needsRefresh = true;
            }

            if ((autoflags & AUTOMAP_WITH_SCANNER) == 0) {
                Object* scanner = NULL;

                Object* item1 = inven_left_hand(obj_dude);
                if (item1 != NULL && item1->pid == PROTO_ID_MOTION_SENSOR) {
                    scanner = item1;
                } else {
                    Object* item2 = inven_right_hand(obj_dude);
                    if (item2 != NULL && item2->pid == PROTO_ID_MOTION_SENSOR) {
                        scanner = item2;
                    }
                }

                if (scanner != NULL && item_m_curr_charges(scanner) > 0) {
                    needsRefresh = true;
                    autoflags |= AUTOMAP_WITH_SCANNER;
                    item_m_dec_charges(scanner);
                } else {
                    gsound_play_sfx_file("iisxxxx1");

                    MessageListItem messageListItem;
                    // 17 - The motion sensor is not installed.
                    // 18 - The motion sensor has no charges remaining.
                    const char* title = getmsg(&misc_message_file, &messageListItem, scanner != NULL ? 18 : 17);
                    dialog_out(title, NULL, 0, 165, 140, colorTable[32328], NULL, colorTable[32328], 0);
                }
            }

            break;
        case KEY_CTRL_Q:
        case KEY_ALT_X:
        case KEY_F10:
            game_quit_with_confirm();
            break;
        case KEY_F12:
            dump_screen();
            break;
        }

        if (game_user_wants_to_quit != 0) {
            break;
        }

        if (needsRefresh) {
            draw_top_down_map(window, elevation, frmData[AUTOMAP_FRM_BACKGROUND], autoflags);
            needsRefresh = false;
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    if (isoWasEnabled) {
        map_enable_bk_processes();
    }

    win_delete(window);
    text_font(oldFont);

    for (int index = 0; index < AUTOMAP_FRM_COUNT; index++) {
        art_ptr_unlock(frmHandle[index]);
    }
}

// Renders automap in Map window.
//
// 0x41AC74
static void draw_top_down_map(int window, int elevation, unsigned char* backgroundData, int flags)
{
    int color;
    if ((flags & AUTOMAP_IN_GAME) != 0) {
        color = colorTable[8456];
    } else {
        color = colorTable[22025];
    }

    win_fill(window, 0, 0, AUTOMAP_WINDOW_WIDTH, AUTOMAP_WINDOW_HEIGHT, color);
    win_border(window);

    unsigned char* windowBuffer = win_get_buf(window);
    buf_to_buf(backgroundData, AUTOMAP_WINDOW_WIDTH, AUTOMAP_WINDOW_HEIGHT, AUTOMAP_WINDOW_WIDTH, windowBuffer, AUTOMAP_WINDOW_WIDTH);

    for (Object* object = obj_find_first_at(elevation); object != NULL; object = obj_find_next_at()) {
        if (object->tile == -1) {
            continue;
        }

        int objectType = FID_TYPE(object->fid);
        unsigned char objectColor;

        if ((flags & AUTOMAP_IN_GAME) != 0) {
            if (objectType == OBJ_TYPE_CRITTER
                && (object->flags & OBJECT_HIDDEN) == 0
                && (flags & AUTOMAP_WITH_SCANNER) != 0
                && (object->data.critter.combat.results & DAM_DEAD) == 0) {
                objectColor = colorTable[31744];
            } else {
                if ((object->flags & OBJECT_SEEN) == 0) {
                    continue;
                }

                if (object->pid == PROTO_ID_0x2000031) {
                    objectColor = colorTable[32328];
                } else if (objectType == OBJ_TYPE_WALL) {
                    objectColor = colorTable[992];
                } else if (objectType == OBJ_TYPE_SCENERY
                    && (flags & AUTOMAP_WTH_HIGH_DETAILS) != 0
                    && object->pid != PROTO_ID_0x2000158) {
                    objectColor = colorTable[480];
                } else if (object == obj_dude) {
                    objectColor = colorTable[31744];
                } else {
                    objectColor = colorTable[0];
                }
            }
        }

        int v10 = -2 * (object->tile % 200) - 10 + AUTOMAP_WINDOW_WIDTH * (2 * (object->tile / 200) + 9) - 60;
        if ((flags & AUTOMAP_IN_GAME) == 0) {
            switch (objectType) {
            case OBJ_TYPE_ITEM:
                objectColor = colorTable[6513];
                break;
            case OBJ_TYPE_CRITTER:
                objectColor = colorTable[28672];
                break;
            case OBJ_TYPE_SCENERY:
                objectColor = colorTable[448];
                break;
            case OBJ_TYPE_WALL:
                objectColor = colorTable[12546];
                break;
            case OBJ_TYPE_MISC:
                objectColor = colorTable[31650];
                break;
            default:
                objectColor = colorTable[0];
            }
        }

        if (objectColor != colorTable[0]) {
            unsigned char* v12 = windowBuffer + v10;
            if ((flags & AUTOMAP_IN_GAME) != 0) {
                if (*v12 != colorTable[992] || objectColor != colorTable[480]) {
                    v12[0] = objectColor;
                    v12[1] = objectColor;
                }

                if (object == obj_dude) {
                    v12[-1] = objectColor;
                    v12[-AUTOMAP_WINDOW_WIDTH] = objectColor;
                    v12[AUTOMAP_WINDOW_WIDTH] = objectColor;
                }
            } else {
                v12[0] = objectColor;
                v12[1] = objectColor;
                v12[AUTOMAP_WINDOW_WIDTH] = objectColor;
                v12[AUTOMAP_WINDOW_WIDTH + 1] = objectColor;

                v12[AUTOMAP_WINDOW_WIDTH - 1] = objectColor;
                v12[AUTOMAP_WINDOW_WIDTH + 2] = objectColor;
                v12[AUTOMAP_WINDOW_WIDTH * 2] = objectColor;
                v12[AUTOMAP_WINDOW_WIDTH * 2 + 1] = objectColor;
            }
        }
    }

    int textColor;
    if ((flags & AUTOMAP_IN_GAME) != 0) {
        textColor = colorTable[992];
    } else {
        textColor = colorTable[12546];
    }

    if (map_get_index_number() != -1) {
        char* areaName = map_get_short_name(map_get_index_number());
        win_print(window, areaName, 240, 150, 380, textColor | 0x2000000);

        char* mapName = map_get_elev_idx(map_get_index_number(), elevation);
        win_print(window, mapName, 240, 150, 396, textColor | 0x2000000);
    }

    win_draw(window);
}

// Renders automap in Pipboy window.
//
// 0x41AF5C
int draw_top_down_map_pipboy(int window, int map, int elevation)
{
    unsigned char* windowBuffer = win_get_buf(window) + 640 * AUTOMAP_PIPBOY_VIEW_Y + AUTOMAP_PIPBOY_VIEW_X;

    unsigned char wallColor = colorTable[992];
    unsigned char sceneryColor = colorTable[480];

    ambuf = (unsigned char*)mem_malloc(11024);
    if (ambuf == NULL) {
        debug_printf("\nAUTOMAP: Error allocating data buffer!\n");
        return -1;
    }

    if (AM_ReadEntry(map, elevation) == -1) {
        mem_free(ambuf);
        return -1;
    }

    int v1 = 0;
    unsigned char v2 = 0;
    unsigned char* ptr = ambuf;

    // FIXME: This loop is implemented incorrectly. Automap requires 400x400 px,
    // but it's top offset is 105, which gives max y 505. It only works because
    // lower portions of automap data contains zeroes. If it doesn't this loop
    // will try to set pixels outside of window buffer, which usually leads to
    // crash.
    for (int y = 0; y < HEX_GRID_HEIGHT; y++) {
        for (int x = 0; x < HEX_GRID_WIDTH; x++) {
            v1 -= 1;
            if (v1 <= 0) {
                v1 = 4;
                v2 = *ptr++;
            }

            switch ((v2 & 0xC0) >> 6) {
            case 1:
                *windowBuffer++ = wallColor;
                *windowBuffer++ = wallColor;
                break;
            case 2:
                *windowBuffer++ = sceneryColor;
                *windowBuffer++ = sceneryColor;
                break;
            default:
                windowBuffer += 2;
                break;
            }

            v2 <<= 2;
        }

        windowBuffer += 640 + 240;
    }

    mem_free(ambuf);

    return 0;
}

// 0x41B048
int automap_pip_save()
{
    int map = map_get_index_number();
    int elevation = map_elevation;

    int entryOffset = amdbhead.offsets[map][elevation];
    if (entryOffset < 0) {
        return 0;
    }

    debug_printf("\nAUTOMAP: Saving AutoMap DB index %d, level %d\n", map, elevation);

    bool dataBuffersAllocated = false;
    ambuf = (unsigned char*)mem_malloc(11024);
    if (ambuf != NULL) {
        cmpbuf = (unsigned char*)mem_malloc(11024);
        if (cmpbuf != NULL) {
            dataBuffersAllocated = true;
        }
    }

    if (!dataBuffersAllocated) {
        // FIXME: Leaking ambuf.
        debug_printf("\nAUTOMAP: Error allocating data buffers!\n");
        return -1;
    }

    // NOTE: Not sure about the size.
    char path[256];
    snprintf(path, sizeof(path), "%s\\%s", "MAPS", AUTOMAP_DB);

    DB_FILE* stream1 = db_fopen(path, "r+b");
    if (stream1 == NULL) {
        debug_printf("\nAUTOMAP: Error opening automap database file!\n");
        debug_printf("Error continued: automap_pip_save: path: %s", path);
        mem_free(ambuf);
        mem_free(cmpbuf);
        return -1;
    }

    if (AM_ReadMainHeader(stream1) == -1) {
        debug_printf("\nAUTOMAP: Error reading automap database file header!\n");
        mem_free(ambuf);
        mem_free(cmpbuf);
        db_fclose(stream1);
        return -1;
    }

    decode_map_data(elevation);

    int compressedDataSize = CompLZS(ambuf, cmpbuf, 10000);
    if (compressedDataSize == -1) {
        amdbsubhead.dataSize = 10000;
        amdbsubhead.isCompressed = 0;
    } else {
        amdbsubhead.dataSize = compressedDataSize;
        amdbsubhead.isCompressed = 1;
    }

    if (entryOffset != 0) {
        snprintf(path, sizeof(path), "%s\\%s", "MAPS", AUTOMAP_TMP);

        DB_FILE* stream2 = db_fopen(path, "wb");
        if (stream2 == NULL) {
            debug_printf("\nAUTOMAP: Error creating temp file!\n");
            mem_free(ambuf);
            mem_free(cmpbuf);
            db_fclose(stream1);
            return -1;
        }

        db_rewind(stream1);

        if (copy_file_data(stream1, stream2, entryOffset) == -1) {
            debug_printf("\nAUTOMAP: Error copying file data!\n");
            db_fclose(stream1);
            db_fclose(stream2);
            mem_free(ambuf);
            mem_free(cmpbuf);
            return -1;
        }

        if (WriteAM_Entry(stream2) == -1) {
            db_fclose(stream1);
            mem_free(ambuf);
            mem_free(cmpbuf);
            return -1;
        }

        int nextEntryDataSize;
        if (db_freadInt32(stream1, &nextEntryDataSize) == -1) {
            debug_printf("\nAUTOMAP: Error reading database #1!\n");
            db_fclose(stream1);
            db_fclose(stream2);
            mem_free(ambuf);
            mem_free(cmpbuf);
            return -1;
        }

        int automapDataSize = db_filelength(stream1);
        if (automapDataSize == -1) {
            debug_printf("\nAUTOMAP: Error reading database #2!\n");
            db_fclose(stream1);
            db_fclose(stream2);
            mem_free(ambuf);
            mem_free(cmpbuf);
            return -1;
        }

        int nextEntryOffset = entryOffset + nextEntryDataSize + 5;
        if (automapDataSize != nextEntryOffset) {
            if (db_fseek(stream1, nextEntryOffset, SEEK_SET) == -1) {
                debug_printf("\nAUTOMAP: Error writing temp data!\n");
                db_fclose(stream1);
                db_fclose(stream2);
                mem_free(ambuf);
                mem_free(cmpbuf);
                return -1;
            }

            if (copy_file_data(stream1, stream2, automapDataSize - nextEntryOffset) == -1) {
                debug_printf("\nAUTOMAP: Error copying file data!\n");
                db_fclose(stream1);
                db_fclose(stream2);
                mem_free(ambuf);
                mem_free(cmpbuf);
                return -1;
            }
        }

        int diff = amdbsubhead.dataSize - nextEntryDataSize;
        for (int map = 0; map < AUTOMAP_MAP_COUNT; map++) {
            for (int elevation = 0; elevation < ELEVATION_COUNT; elevation++) {
                if (amdbhead.offsets[map][elevation] > entryOffset) {
                    amdbhead.offsets[map][elevation] += diff;
                }
            }
        }

        amdbhead.dataSize += diff;

        if (WriteAM_Header(stream2) == -1) {
            db_fclose(stream1);
            mem_free(ambuf);
            mem_free(cmpbuf);
            return -1;
        }

        db_fseek(stream2, 0, SEEK_END);
        db_fclose(stream2);
        db_fclose(stream1);
        mem_free(ambuf);
        mem_free(cmpbuf);

        char* masterPatchesPath;
        if (!config_get_string(&game_config, GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_MASTER_PATCHES_KEY, &masterPatchesPath)) {
            debug_printf("\nAUTOMAP: Error reading config info!\n");
            return -1;
        }

        // NOTE: Not sure about the size.
        char automapDbPath[512];
        snprintf(automapDbPath, sizeof(automapDbPath), "%s\\%s\\%s", masterPatchesPath, "MAPS", AUTOMAP_DB);
        if (compat_remove(automapDbPath) != 0) {
            debug_printf("\nAUTOMAP: Error removing database!\n");
            return -1;
        }

        // NOTE: Not sure about the size.
        char automapTmpPath[512];
        snprintf(automapTmpPath, sizeof(automapTmpPath), "%s\\%s\\%s", masterPatchesPath, "MAPS", AUTOMAP_TMP);
        if (compat_rename(automapTmpPath, automapDbPath) != 0) {
            debug_printf("\nAUTOMAP: Error renaming database!\n");
            return -1;
        }
    } else {
        bool proceed = true;
        if (db_fseek(stream1, 0, SEEK_END) != -1) {
            if (db_ftell(stream1) != amdbhead.dataSize) {
                proceed = false;
            }
        } else {
            proceed = false;
        }

        if (!proceed) {
            debug_printf("\nAUTOMAP: Error reading automap database file header!\n");
            mem_free(ambuf);
            mem_free(cmpbuf);
            db_fclose(stream1);
            return -1;
        }

        if (WriteAM_Entry(stream1) == -1) {
            mem_free(ambuf);
            mem_free(cmpbuf);
            return -1;
        }

        amdbhead.offsets[map][elevation] = amdbhead.dataSize;
        amdbhead.dataSize += amdbsubhead.dataSize + 5;

        if (WriteAM_Header(stream1) == -1) {
            mem_free(ambuf);
            mem_free(cmpbuf);
            return -1;
        }

        db_fseek(stream1, 0, SEEK_END);
        db_fclose(stream1);
        mem_free(ambuf);
        mem_free(cmpbuf);
    }

    return 1;
}

// Saves automap entry into stream.
//
// 0x41B798
static int WriteAM_Entry(DB_FILE* stream)
{
    unsigned char* buffer;
    if (amdbsubhead.isCompressed == 1) {
        buffer = cmpbuf;
    } else {
        buffer = ambuf;
    }

    if (db_fwriteLong(stream, amdbsubhead.dataSize) == -1) {
        goto err;
    }

    if (db_fwriteByte(stream, amdbsubhead.isCompressed) == -1) {
        goto err;
    }

    if (db_fwriteByteCount(stream, buffer, amdbsubhead.dataSize) == -1) {
        goto err;
    }

    return 0;

err:

    debug_printf("\nAUTOMAP: Error writing automap database entry data!\n");
    db_fclose(stream);

    return -1;
}

// 0x41B820
static int AM_ReadEntry(int map, int elevation)
{
    cmpbuf = NULL;

    char path[COMPAT_MAX_PATH];
    snprintf(path, sizeof(path), "%s\\%s", "MAPS", AUTOMAP_DB);

    bool success = true;

    DB_FILE* stream = db_fopen(path, "r+b");
    if (stream == NULL) {
        debug_printf("\nAUTOMAP: Error opening automap database file!\n");
        debug_printf("Error continued: AM_ReadEntry: path: %s", path);
        return -1;
    }

    if (AM_ReadMainHeader(stream) == -1) {
        debug_printf("\nAUTOMAP: Error reading automap database header!\n");
        db_fclose(stream);
        return -1;
    }

    if (amdbhead.offsets[map][elevation] <= 0) {
        success = false;
        goto out;
    }

    if (db_fseek(stream, amdbhead.offsets[map][elevation], SEEK_SET) == -1) {
        success = false;
        goto out;
    }

    if (db_freadInt32(stream, &(amdbsubhead.dataSize)) == -1) {
        success = false;
        goto out;
    }

    if (db_freadByte(stream, &(amdbsubhead.isCompressed)) == -1) {
        success = false;
        goto out;
    }

    if (amdbsubhead.isCompressed == 1) {
        cmpbuf = (unsigned char*)mem_malloc(11024);
        if (cmpbuf == NULL) {
            debug_printf("\nAUTOMAP: Error allocating decompression buffer!\n");
            db_fclose(stream);
            return -1;
        }

        if (db_freadByteCount(stream, cmpbuf, amdbsubhead.dataSize) == -1) {
            success = 0;
            goto out;
        }

        if (DecodeLZS(cmpbuf, ambuf, 10000) == -1) {
            debug_printf("\nAUTOMAP: Error decompressing DB entry!\n");
            db_fclose(stream);
            return -1;
        }
    } else {
        if (db_freadByteCount(stream, ambuf, amdbsubhead.dataSize) == -1) {
            success = false;
            goto out;
        }
    }

out:

    db_fclose(stream);

    if (!success) {
        debug_printf("\nAUTOMAP: Error reading automap database entry data!\n");

        return -1;
    }

    if (cmpbuf != NULL) {
        mem_free(cmpbuf);
    }

    return 0;
}

// Saves automap.db header.
//
// 0x41BA2C
static int WriteAM_Header(DB_FILE* stream)
{
    db_rewind(stream);

    if (db_fwriteByte(stream, amdbhead.version) == -1) {
        goto err;
    }

    if (db_fwriteInt32(stream, amdbhead.dataSize) == -1) {
        goto err;
    }

    if (db_fwriteInt32List(stream, (int*)amdbhead.offsets, AUTOMAP_OFFSET_COUNT) == -1) {
        goto err;
    }

    return 0;

err:

    debug_printf("\nAUTOMAP: Error writing automap database header!\n");

    db_fclose(stream);

    return -1;
}

// Loads automap.db header.
//
// 0x41BAA4
static int AM_ReadMainHeader(DB_FILE* stream)
{

    if (db_freadByte(stream, &(amdbhead.version)) == -1) {
        return -1;
    }

    if (db_freadInt32(stream, &(amdbhead.dataSize)) == -1) {
        return -1;
    }

    if (db_freadInt32List(stream, (int*)amdbhead.offsets, AUTOMAP_OFFSET_COUNT) == -1) {
        return -1;
    }

    if (amdbhead.version != 1) {
        return -1;
    }

    return 0;
}

// 0x41BAF8
static void decode_map_data(int elevation)
{
    memset(ambuf, 0, SQUARE_GRID_SIZE);

    obj_process_seen();

    Object* object = obj_find_first_at(elevation);
    while (object != NULL) {
        if (object->tile != -1 && (object->flags & OBJECT_SEEN) != 0) {
            int contentType;

            int objectType = FID_TYPE(object->fid);
            if (objectType == OBJ_TYPE_SCENERY && object->pid != PROTO_ID_0x2000158) {
                contentType = 2;
            } else if (objectType == OBJ_TYPE_WALL) {
                contentType = 1;
            } else {
                contentType = 0;
            }

            if (contentType != 0) {
                int v1 = 200 - object->tile % 200;
                int v2 = v1 / 4 + 50 * (object->tile / 200);
                int v3 = 2 * (3 - v1 % 4);
                ambuf[v2] &= ~(0x03 << v3);
                ambuf[v2] |= (contentType << v3);
            }
        }
        object = obj_find_next_at();
    }
}

// 0x41BBEC
static int am_pip_init()
{
    amdbhead.version = 1;
    amdbhead.dataSize = 797;
    memcpy(amdbhead.offsets, defam, sizeof(defam));

    char path[COMPAT_MAX_PATH];
    snprintf(path, sizeof(path), "%s\\%s", "MAPS", AUTOMAP_DB);

    DB_FILE* stream = db_fopen(path, "wb");
    if (stream == NULL) {
        debug_printf("\nAUTOMAP: Error creating automap database file!\n");
        return -1;
    }

    if (WriteAM_Header(stream) == -1) {
        return -1;
    }

    db_fclose(stream);

    return 0;
}

// 0x41BC88
int YesWriteIndex(int mapIndex, int elevation)
{
    if (mapIndex < AUTOMAP_MAP_COUNT && elevation < ELEVATION_COUNT && mapIndex >= 0 && elevation >= 0) {
        return defam[mapIndex][elevation] >= 0;
    }

    return 0;
}

// Copy data from stream1 to stream2.
//
// 0x41BCC0
static int copy_file_data(DB_FILE* stream1, DB_FILE* stream2, int length)
{
    void* buffer = mem_malloc(0xFFFF);
    if (buffer == NULL) {
        return -1;
    }

    // NOTE: Original code is slightly different, but does the same thing.
    while (length != 0) {
        int chunkLength = std::min(length, 0xFFFF);

        if (db_fread(buffer, chunkLength, 1, stream1) != 1) {
            break;
        }

        if (db_fwrite(buffer, chunkLength, 1, stream2) != 1) {
            break;
        }

        length -= chunkLength;
    }

    mem_free(buffer);

    if (length != 0) {
        return -1;
    }

    return 0;
}

// 0x41BDC8
int ReadAMList(AutomapHeader** automapHeaderPtr)
{
    char path[COMPAT_MAX_PATH];
    snprintf(path, sizeof(path), "%s\\%s", "MAPS", AUTOMAP_DB);

    DB_FILE* stream = db_fopen(path, "rb");
    if (stream == NULL) {
        debug_printf("\nAUTOMAP: Error opening database file for reading!\n");
        debug_printf("Error continued: ReadAMList: path: %s", path);
        return -1;
    }

    if (AM_ReadMainHeader(stream) == -1) {
        debug_printf("\nAUTOMAP: Error reading automap database header pt2!\n");
        db_fclose(stream);
        return -1;
    }

    db_fclose(stream);

    *automapHeaderPtr = &amdbhead;

    return 0;
}

} // namespace fallout
