#include "game/elevator.h"

#include <ctype.h>
#include <string.h>

#include "game/art.h"
#include "game/cycle.h"
#include "game/gmouse.h"
#include "game/gsound.h"
#include "game/intface.h"
#include "game/map.h"
#include "game/pipboy.h"
#include "game/scripts.h"
#include "plib/gnw/button.h"
#include "plib/gnw/debug.h"
#include "plib/gnw/gnw.h"
#include "plib/gnw/grbuf.h"
#include "plib/gnw/input.h"
#include "plib/gnw/rect.h"
#include "plib/gnw/svga.h"

namespace fallout {

// The maximum number of elevator levels.
#define ELEVATOR_LEVEL_MAX 4

// NOTE: There are two variables which hold background data used in the
// elevator window - [grphbmp[ELEVATOR_FRM_BACKGROUND]] and [grphbmp[ELEVATOR_FRM_PANEL]].
// For unknown reason they are using -1 to denote that they are not set
// (instead of using NULL).
#define ELEVATOR_BACKGROUND_NULL ((unsigned char*)(-1))

// NOTE: This enum is a little bit unsual. It contains two types of members:
// static and dynamic. Static part of enum is always accessed with array
// operations as commonly seen in other UI setup/teardown loops. Background and
// panel are always accessed separately without using loops, but they are a part
// of globals holding state (buffers, cache keys, dimensions).
typedef enum ElevatorFrm {
    ELEVATOR_FRM_BUTTON_DOWN,
    ELEVATOR_FRM_BUTTON_UP,
    ELEVATOR_FRM_GAUGE,
    ELEVATOR_FRM_BACKGROUND,
    ELEVATOR_FRM_PANEL,
    ELEVATOR_FRM_COUNT,
    ELEVATOR_FRM_STATIC_COUNT = 3,
} ElevatorFrm;

typedef struct ElevatorBackground {
    int backgroundFrmId;
    int panelFrmId;
} ElevatorBackground;

typedef struct ElevatorDescription {
    int map;
    int elevation;
    int tile;
} ElevatorDescription;

static int elevator_start(int elevator);
static void elevator_end();
static int Check4Keys(int elevator, int keyCode);

// 0x437B80
static const int grph_id[ELEVATOR_FRM_STATIC_COUNT] = {
    141, // ebut_in.frm - map elevator screen
    142, // ebut_out.frm - map elevator screen
    149, // gaj000.frm - map elevator screen
};

// 0x437B8C
static const ElevatorBackground intotal[ELEVATOR_COUNT] = {
    /*    BROTHERHOOD_OF_STEEL_MAIN */ { 143, -1 },
    /* BROTHERHOOD_OF_STEEL_SURFACE */ { 143, 150 },
    /*                 MASTER_UPPER */ { 144, -1 },
    /*                 MASTER_LOWER */ { 144, 145 },
    /*          MILITARY_BASE_UPPER */ { 146, -1 },
    /*          MILITARY_BASE_LOWER */ { 146, 147 },
    /*                   GLOW_UPPER */ { 146, -1 },
    /*                   GLOW_LOWER */ { 146, 151 },
    /*                     VAULT_13 */ { 148, -1 },
    /*                   NECROPOLIS */ { 148, -1 },
    /*                     SIERRA_1 */ { 148, -1 },
    /*                     SIERRA_2 */ { 146, 152 },
};

// Number of levels for eleveators.
//
// 0x437BEC
static const int btncnt[ELEVATOR_COUNT] = {
    /*    BROTHERHOOD_OF_STEEL_MAIN */ 4,
    /* BROTHERHOOD_OF_STEEL_SURFACE */ 2,
    /*                 MASTER_UPPER */ 3,
    /*                 MASTER_LOWER */ 2,
    /*          MILITARY_BASE_UPPER */ 3,
    /*          MILITARY_BASE_LOWER */ 2,
    /*                   GLOW_UPPER */ 3,
    /*                   GLOW_LOWER */ 3,
    /*                     VAULT_13 */ 3,
    /*                   NECROPOLIS */ 3,
    /*                     SIERRA_1 */ 3,
    /*                     SIERRA_2 */ 3,
};

// 0x437C1C
static const ElevatorDescription retvals[ELEVATOR_COUNT][ELEVATOR_LEVEL_MAX] = {
    {
        { 14, 0, 18940 },
        { 14, 1, 18936 },
        { 15, 0, 21340 },
        { 15, 1, 21340 },
    },
    {
        { 13, 0, 20502 },
        { 14, 0, 14912 },
        { 0, 0, -1 },
        { 0, 0, -1 },
    },
    {
        { 33, 0, 12498 },
        { 33, 1, 20094 },
        { 34, 0, 17312 },
        { 0, 0, -1 },
    },
    {
        { 34, 0, 16140 },
        { 34, 1, 16140 },
        { 0, 0, -1 },
        { 0, 0, -1 },
    },
    {
        { 31, 0, 14920 },
        { 31, 1, 14920 },
        { 32, 0, 12944 },
        { 0, 0, -1 },
    },
    {
        { 32, 0, 24520 },
        { 32, 1, 24520 },
        { 0, 0, -1 },
        { 0, 0, -1 },
    },
    {
        { 42, 0, 22526 },
        { 42, 1, 22526 },
        { 42, 2, 22526 },
        { 0, 0, -1 },
    },
    {
        { 42, 2, 14086 },
        { 43, 0, 14086 },
        { 43, 2, 14086 },
        { 0, 0, -1 },
    },
    {
        { 6, 0, 14104 },
        { 6, 1, 22504 },
        { 6, 2, 17312 },
        { 0, 0, -1 },
    },
    {
        { 9, 0, 13704 },
        { 9, 1, 23302 },
        { 9, 2, 17308 },
        { 0, 0, -1 },
    },
    {
        { 9, 0, 13704 },
        { 9, 1, 23302 },
        { 9, 2, 17308 },
        { 0, 0, -1 },
    },
    {
        { 43, 0, 14130 },
        { 43, 1, 14130 },
        { 43, 2, 14130 },
        { 0, 0, -1 },
    },
};

// NOTE: These values are also used as key bindings.
//
// 0x437E5C
static const char keytable[ELEVATOR_COUNT][ELEVATOR_LEVEL_MAX] = {
    { '1', '2', '3', '4' },
    { 'G', '1', '\0', '\0' },
    { '1', '2', '3', '\0' },
    { '3', '4', '\0', '\0' },
    { '1', '2', '3', '\0' },
    { '3', '4', '\0', '\0' },
    { '1', '2', '3', '\0' },
    { '3', '4', '6', '\0' },
    { '1', '2', '3', '\0' },
    { '1', '2', '3', '\0' },
    { '1', '2', '3', '\0' },
    { '4', '5', '6', '\0' },
};

// 0x504F3C
static const char* sfxtable[ELEVATOR_LEVEL_MAX - 1][ELEVATOR_LEVEL_MAX] = {
    {
        "ELV1_1",
        "ELV1_1",
        "ERROR",
        "ERROR",
    },
    {
        "ELV1_2",
        "ELV1_2",
        "ELV1_1",
        "ERROR",
    },
    {
        "ELV1_3",
        "ELV1_3",
        "ELV2_3",
        "ELV1_1",
    },
};

// 0x56ED30
static Size GInfo[ELEVATOR_FRM_COUNT];

// 0x56ED58
static int elev_win;

// 0x56ED5C
static unsigned char* win_buf;

// 0x56ED60
static bool bk_enable;

// 0x56ED64
static CacheEntry* grph_key[ELEVATOR_FRM_COUNT];

// 0x56ED78
static unsigned char* grphbmp[ELEVATOR_FRM_COUNT];

// Presents elevator dialog for player to pick a desired level.
//
// 0x437E8C
int elevator_select(int elevator, int* mapPtr, int* elevationPtr, int* tilePtr)
{
    if (elevator < 0 || elevator >= ELEVATOR_COUNT) {
        return -1;
    }

    if (elevator_start(elevator) == -1) {
        return -1;
    }

    const ElevatorDescription* elevatorDescription = retvals[elevator];

    int index;
    for (index = 0; index < ELEVATOR_LEVEL_MAX; index++) {
        if (elevatorDescription[index].map == *mapPtr) {
            break;
        }
    }

    if (index < ELEVATOR_LEVEL_MAX) {
        if (elevatorDescription[*elevationPtr + index].tile != -1) {
            *elevationPtr += index;
        }
    }

    if (elevator == ELEVATOR_GLOW_LOWER && *mapPtr == 42) {
        *elevationPtr -= 2;
    }

    debug_printf("\n the start elev level %d\n", *elevationPtr);

    int v18 = (GInfo[ELEVATOR_FRM_GAUGE].width * GInfo[ELEVATOR_FRM_GAUGE].height) / 13;
    float v42 = 12.0f / (float)(btncnt[elevator] - 1);
    buf_to_buf(
        grphbmp[ELEVATOR_FRM_GAUGE] + v18 * (int)((float)(*elevationPtr) * v42),
        GInfo[ELEVATOR_FRM_GAUGE].width,
        GInfo[ELEVATOR_FRM_GAUGE].height / 13,
        GInfo[ELEVATOR_FRM_GAUGE].width,
        win_buf + GInfo[ELEVATOR_FRM_BACKGROUND].width * 41 + 121,
        GInfo[ELEVATOR_FRM_BACKGROUND].width);
    win_draw(elev_win);

    bool done = false;
    int keyCode;
    while (!done) {
        sharedFpsLimiter.mark();

        keyCode = get_input();
        if (keyCode == KEY_ESCAPE) {
            done = true;
        }

        if (keyCode >= 500 && keyCode < 504) {
            done = true;
        }

        if (keyCode > 0 && keyCode < 500) {
            int level = Check4Keys(elevator, keyCode);
            if (level != 0) {
                keyCode = 500 + level - 1;
                done = true;
            }
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    if (keyCode != KEY_ESCAPE) {
        keyCode -= 500;

        if (*elevationPtr != keyCode) {
            float v43 = (float)(btncnt[elevator] - 1) / 12.0f;

            unsigned int delay = (unsigned int)(v43 * 276.92307);

            if (keyCode < *elevationPtr) {
                v43 = -v43;
            }

            int numberOfLevelsTravelled = keyCode - *elevationPtr;
            if (numberOfLevelsTravelled < 0) {
                numberOfLevelsTravelled = -numberOfLevelsTravelled;
            }

            gsound_play_sfx_file(sfxtable[btncnt[elevator] - 2][numberOfLevelsTravelled]);

            float v41 = (float)keyCode * v42;
            float v44 = (float)(*elevationPtr) * v42;
            do {
                sharedFpsLimiter.mark();

                unsigned int tick = get_time();
                v44 += v43;
                buf_to_buf(
                    grphbmp[ELEVATOR_FRM_GAUGE] + v18 * (int)v44,
                    GInfo[ELEVATOR_FRM_GAUGE].width,
                    GInfo[ELEVATOR_FRM_GAUGE].height / 13,
                    GInfo[ELEVATOR_FRM_GAUGE].width,
                    win_buf + GInfo[ELEVATOR_FRM_BACKGROUND].width * 41 + 121,
                    GInfo[ELEVATOR_FRM_BACKGROUND].width);

                win_draw(elev_win);

                while (elapsed_time(tick) < delay) {
                }

                renderPresent();
                sharedFpsLimiter.throttle();
            } while ((v43 <= 0.0 || v44 < v41) && (v43 > 0.0 || v44 > v41));

            pause_for_tocks(200);
        }
    }

    elevator_end();

    if (keyCode != KEY_ESCAPE) {
        const ElevatorDescription* description = &(elevatorDescription[keyCode]);
        *mapPtr = description->map;
        *elevationPtr = description->elevation;
        *tilePtr = description->tile;
    } else {
        if (elevator == ELEVATOR_GLOW_LOWER) {
            *elevationPtr += 2;
        }
    }

    return 0;
}

// 0x43821C
static int elevator_start(int elevator)
{
    bk_enable = map_disable_bk_processes();
    cycle_disable();

    gmouse_set_cursor(MOUSE_CURSOR_ARROW);
    gmouse_3d_off();

    gmouse_set_cursor(MOUSE_CURSOR_ARROW);

    int index;
    for (index = 0; index < ELEVATOR_FRM_STATIC_COUNT; index++) {
        int fid = art_id(OBJ_TYPE_INTERFACE, grph_id[index], 0, 0, 0);
        grphbmp[index] = art_lock(fid, &(grph_key[index]), &(GInfo[index].width), &(GInfo[index].height));
        if (grphbmp[index] == NULL) {
            break;
        }
    }

    if (index != ELEVATOR_FRM_STATIC_COUNT) {
        for (int reversedIndex = index - 1; reversedIndex >= 0; reversedIndex--) {
            art_ptr_unlock(grph_key[reversedIndex]);
        }

        if (bk_enable) {
            map_enable_bk_processes();
        }

        cycle_enable();
        gmouse_set_cursor(MOUSE_CURSOR_ARROW);
        return -1;
    }

    grphbmp[ELEVATOR_FRM_PANEL] = ELEVATOR_BACKGROUND_NULL;
    grphbmp[ELEVATOR_FRM_BACKGROUND] = ELEVATOR_BACKGROUND_NULL;

    const ElevatorBackground* elevatorBackground = &(intotal[elevator]);
    bool backgroundsLoaded = true;

    int backgroundFid = art_id(OBJ_TYPE_INTERFACE, elevatorBackground->backgroundFrmId, 0, 0, 0);
    grphbmp[ELEVATOR_FRM_BACKGROUND] = art_lock(backgroundFid, &grph_key[ELEVATOR_FRM_BACKGROUND], &(GInfo[ELEVATOR_FRM_BACKGROUND].width), &(GInfo[ELEVATOR_FRM_BACKGROUND].height));
    if (grphbmp[ELEVATOR_FRM_BACKGROUND] != NULL) {
        if (elevatorBackground->panelFrmId != -1) {
            int panelFid = art_id(OBJ_TYPE_INTERFACE, elevatorBackground->panelFrmId, 0, 0, 0);
            grphbmp[ELEVATOR_FRM_PANEL] = art_lock(panelFid, &grph_key[ELEVATOR_FRM_PANEL], &(GInfo[ELEVATOR_FRM_PANEL].width), &(GInfo[ELEVATOR_FRM_PANEL].height));
            if (grphbmp[ELEVATOR_FRM_PANEL] == NULL) {
                grphbmp[ELEVATOR_FRM_PANEL] = ELEVATOR_BACKGROUND_NULL;
                backgroundsLoaded = false;
            }
        }
    } else {
        grphbmp[ELEVATOR_FRM_BACKGROUND] = ELEVATOR_BACKGROUND_NULL;
        backgroundsLoaded = false;
    }

    if (!backgroundsLoaded) {
        if (grphbmp[ELEVATOR_FRM_BACKGROUND] != ELEVATOR_BACKGROUND_NULL) {
            art_ptr_unlock(grph_key[ELEVATOR_FRM_BACKGROUND]);
        }

        if (grphbmp[ELEVATOR_FRM_PANEL] != ELEVATOR_BACKGROUND_NULL) {
            art_ptr_unlock(grph_key[ELEVATOR_FRM_PANEL]);
        }

        for (int index = 0; index < ELEVATOR_FRM_STATIC_COUNT; index++) {
            art_ptr_unlock(grph_key[index]);
        }

        if (bk_enable) {
            map_enable_bk_processes();
        }

        cycle_enable();
        gmouse_set_cursor(MOUSE_CURSOR_ARROW);
        return -1;
    }

    int elevatorWindowX = (screenGetWidth() - GInfo[ELEVATOR_FRM_BACKGROUND].width) / 2;
    int elevatorWindowY = (screenGetHeight() - INTERFACE_BAR_HEIGHT - 1 - GInfo[ELEVATOR_FRM_BACKGROUND].height) / 2;
    elev_win = win_add(
        elevatorWindowX,
        elevatorWindowY,
        GInfo[ELEVATOR_FRM_BACKGROUND].width,
        GInfo[ELEVATOR_FRM_BACKGROUND].height,
        256,
        WINDOW_MODAL | WINDOW_DONT_MOVE_TOP);
    if (elev_win == -1) {
        if (grphbmp[ELEVATOR_FRM_BACKGROUND] != ELEVATOR_BACKGROUND_NULL) {
            art_ptr_unlock(grph_key[ELEVATOR_FRM_BACKGROUND]);
        }

        if (grphbmp[ELEVATOR_FRM_PANEL] != ELEVATOR_BACKGROUND_NULL) {
            art_ptr_unlock(grph_key[ELEVATOR_FRM_PANEL]);
        }

        for (int index = 0; index < ELEVATOR_FRM_STATIC_COUNT; index++) {
            art_ptr_unlock(grph_key[index]);
        }

        if (bk_enable) {
            map_enable_bk_processes();
        }

        cycle_enable();
        gmouse_set_cursor(MOUSE_CURSOR_ARROW);
        return -1;
    }

    win_buf = win_get_buf(elev_win);
    memcpy(win_buf, (unsigned char*)grphbmp[ELEVATOR_FRM_BACKGROUND], GInfo[ELEVATOR_FRM_BACKGROUND].width * GInfo[ELEVATOR_FRM_BACKGROUND].height);

    if (grphbmp[ELEVATOR_FRM_PANEL] != ELEVATOR_BACKGROUND_NULL) {
        buf_to_buf((unsigned char*)grphbmp[ELEVATOR_FRM_PANEL],
            GInfo[ELEVATOR_FRM_PANEL].width,
            GInfo[ELEVATOR_FRM_PANEL].height,
            GInfo[ELEVATOR_FRM_PANEL].width,
            win_buf + GInfo[ELEVATOR_FRM_BACKGROUND].width * (GInfo[ELEVATOR_FRM_BACKGROUND].height - GInfo[ELEVATOR_FRM_PANEL].height),
            GInfo[ELEVATOR_FRM_BACKGROUND].width);
    }

    int y = 40;
    for (int level = 0; level < btncnt[elevator]; level++) {
        int btn = win_register_button(elev_win,
            13,
            y,
            GInfo[ELEVATOR_FRM_BUTTON_DOWN].width,
            GInfo[ELEVATOR_FRM_BUTTON_DOWN].height,
            -1,
            -1,
            -1,
            500 + level,
            grphbmp[ELEVATOR_FRM_BUTTON_UP],
            grphbmp[ELEVATOR_FRM_BUTTON_DOWN],
            NULL,
            BUTTON_FLAG_TRANSPARENT);
        if (btn != -1) {
            win_register_button_sound_func(btn, gsound_red_butt_press, NULL);
        }
        y += 60;
    }

    return 0;
}

// 0x4385C4
static void elevator_end()
{
    win_delete(elev_win);

    if (grphbmp[ELEVATOR_FRM_BACKGROUND] != ELEVATOR_BACKGROUND_NULL) {
        art_ptr_unlock(grph_key[ELEVATOR_FRM_BACKGROUND]);
    }

    if (grphbmp[ELEVATOR_FRM_PANEL] != ELEVATOR_BACKGROUND_NULL) {
        art_ptr_unlock(grph_key[ELEVATOR_FRM_PANEL]);
    }

    for (int index = 0; index < ELEVATOR_FRM_STATIC_COUNT; index++) {
        art_ptr_unlock(grph_key[index]);
    }

    if (bk_enable) {
        map_enable_bk_processes();
    }

    cycle_enable();

    gmouse_set_cursor(MOUSE_CURSOR_ARROW);
}

// 0x43862C
static int Check4Keys(int elevator, int keyCode)
{
    // TODO: Check if result is really unused?
    toupper(keyCode);

    for (int index = 0; index < ELEVATOR_LEVEL_MAX; index++) {
        char c = keytable[elevator][index];
        if (c == '\0') {
            break;
        }

        if (c == (char)(keyCode & 0xFF)) {
            return index + 1;
        }
    }
    return 0;
}

} // namespace fallout
