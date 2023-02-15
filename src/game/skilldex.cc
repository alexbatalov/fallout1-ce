#include "game/skilldex.h"

#include <stdio.h>
#include <string.h>

#include "game/art.h"
#include "game/cycle.h"
#include "game/game.h"
#include "game/gmouse.h"
#include "game/gsound.h"
#include "game/intface.h"
#include "game/map.h"
#include "game/message.h"
#include "game/object.h"
#include "game/skill.h"
#include "platform_compat.h"
#include "plib/color/color.h"
#include "plib/gnw/button.h"
#include "plib/gnw/debug.h"
#include "plib/gnw/gnw.h"
#include "plib/gnw/grbuf.h"
#include "plib/gnw/input.h"
#include "plib/gnw/memory.h"
#include "plib/gnw/rect.h"
#include "plib/gnw/svga.h"
#include "plib/gnw/text.h"

namespace fallout {

#define SKILLDEX_WINDOW_RIGHT_MARGIN 4
#define SKILLDEX_WINDOW_BOTTOM_MARGIN 6

#define SKILLDEX_SKILL_BUTTON_BUFFER_COUNT (SKILLDEX_SKILL_COUNT * 2)

typedef enum SkilldexFrm {
    SKILLDEX_FRM_BACKGROUND,
    SKILLDEX_FRM_BUTTON_ON,
    SKILLDEX_FRM_BUTTON_OFF,
    SKILLDEX_FRM_LITTLE_RED_BUTTON_UP,
    SKILLDEX_FRM_LITTLE_RED_BUTTON_DOWN,
    SKILLDEX_FRM_BIG_NUMBERS,
    SKILLDEX_FRM_COUNT,
} SkilldexFrm;

typedef enum SkilldexSkill {
    SKILLDEX_SKILL_SNEAK,
    SKILLDEX_SKILL_LOCKPICK,
    SKILLDEX_SKILL_STEAL,
    SKILLDEX_SKILL_TRAPS,
    SKILLDEX_SKILL_FIRST_AID,
    SKILLDEX_SKILL_DOCTOR,
    SKILLDEX_SKILL_SCIENCE,
    SKILLDEX_SKILL_REPAIR,
    SKILLDEX_SKILL_COUNT,
} SkilldexSkill;

static int skilldex_start();
static void skilldex_end();

// 0x507DC8
static bool bk_enable = false;

// 0x507DCC
static int grphfid[SKILLDEX_FRM_COUNT] = {
    121,
    119,
    120,
    8,
    9,
    170,
};

// Maps Skilldex options into skills.
//
// 0x507DE4
static int sklxref[SKILLDEX_SKILL_COUNT] = {
    SKILL_SNEAK,
    SKILL_LOCKPICK,
    SKILL_STEAL,
    SKILL_TRAPS,
    SKILL_FIRST_AID,
    SKILL_DOCTOR,
    SKILL_SCIENCE,
    SKILL_REPAIR,
};

// 0x6650E0
static Size ginfo[SKILLDEX_FRM_COUNT];

// 0x665110
static unsigned char* skldxbtn[SKILLDEX_SKILL_BUTTON_BUFFER_COUNT];

// skilldex.msg
//
// 0x665150
static MessageList skldxmsg;

// 0x665190
static MessageListItem mesg;

// 0x665158
static unsigned char* skldxbmp[SKILLDEX_FRM_COUNT];

// 0x665170
static CacheEntry* grphkey[SKILLDEX_FRM_COUNT];

// 0x665188
static int skldxwin;

// 0x66518C
static unsigned char* winbuf;

// 0x66519C
static int fontsave;

// 0x499560
int skilldex_select()
{
    if (skilldex_start() == -1) {
        debug_printf("\n ** Error loading skilldex dialog data! **\n");
        return -1;
    }

    int rc = -1;
    while (rc == -1) {
        sharedFpsLimiter.mark();

        int keyCode = get_input();

        if (keyCode == KEY_ESCAPE || keyCode == KEY_UPPERCASE_S || keyCode == KEY_LOWERCASE_S || keyCode == 500 || game_user_wants_to_quit != 0) {
            rc = 0;
        } else if (keyCode == KEY_RETURN) {
            gsound_play_sfx_file("ib1p1xx1");
            rc = 0;
        } else if (keyCode >= 501 && keyCode <= 509) {
            rc = keyCode - 500;
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    if (rc != 0) {
        block_for_tocks(1000 / 9);
    }

    skilldex_end();

    return rc;
}

// 0x4995E4
static int skilldex_start()
{
    fontsave = text_curr();
    bk_enable = false;

    gmouse_3d_off();
    gmouse_set_cursor(MOUSE_CURSOR_ARROW);

    if (!message_init(&skldxmsg)) {
        return -1;
    }

    char path[COMPAT_MAX_PATH];
    snprintf(path, sizeof(path), "%s%s", msg_path, "skilldex.msg");

    if (!message_load(&skldxmsg, path)) {
        return -1;
    }

    int frmIndex;
    for (frmIndex = 0; frmIndex < SKILLDEX_FRM_COUNT; frmIndex++) {
        int fid = art_id(OBJ_TYPE_INTERFACE, grphfid[frmIndex], 0, 0, 0);
        skldxbmp[frmIndex] = art_lock(fid, &(grphkey[frmIndex]), &(ginfo[frmIndex].width), &(ginfo[frmIndex].height));
        if (skldxbmp[frmIndex] == NULL) {
            break;
        }
    }

    if (frmIndex < SKILLDEX_FRM_COUNT) {
        while (--frmIndex >= 0) {
            art_ptr_unlock(grphkey[frmIndex]);
        }

        message_exit(&skldxmsg);

        return -1;
    }

    bool cycle = false;
    int buttonDataIndex;
    for (buttonDataIndex = 0; buttonDataIndex < SKILLDEX_SKILL_BUTTON_BUFFER_COUNT; buttonDataIndex++) {
        skldxbtn[buttonDataIndex] = (unsigned char*)mem_malloc(ginfo[SKILLDEX_FRM_BUTTON_ON].height * ginfo[SKILLDEX_FRM_BUTTON_ON].width + 512);
        if (skldxbtn[buttonDataIndex] == NULL) {
            break;
        }

        // NOTE: Original code uses bitwise XOR.
        cycle = !cycle;

        unsigned char* data;
        int size;
        if (cycle) {
            size = ginfo[SKILLDEX_FRM_BUTTON_OFF].width * ginfo[SKILLDEX_FRM_BUTTON_OFF].height;
            data = skldxbmp[SKILLDEX_FRM_BUTTON_OFF];
        } else {
            size = ginfo[SKILLDEX_FRM_BUTTON_ON].width * ginfo[SKILLDEX_FRM_BUTTON_ON].height;
            data = skldxbmp[SKILLDEX_FRM_BUTTON_ON];
        }

        memcpy(skldxbtn[buttonDataIndex], data, size);
    }

    if (buttonDataIndex < SKILLDEX_SKILL_BUTTON_BUFFER_COUNT) {
        while (--buttonDataIndex >= 0) {
            mem_free(skldxbtn[buttonDataIndex]);
        }

        for (int index = 0; index < SKILLDEX_FRM_COUNT; index++) {
            art_ptr_unlock(grphkey[index]);
        }

        message_exit(&skldxmsg);

        return -1;
    }

    int skilldexWindowX = (screenGetWidth() - INTERFACE_BAR_WIDTH) / 2 + INTERFACE_BAR_WIDTH - ginfo[SKILLDEX_FRM_BACKGROUND].width - SKILLDEX_WINDOW_RIGHT_MARGIN;
    int skilldexWindowY = screenGetHeight() - INTERFACE_BAR_HEIGHT - 1 - ginfo[SKILLDEX_FRM_BACKGROUND].height - SKILLDEX_WINDOW_BOTTOM_MARGIN;
    skldxwin = win_add(skilldexWindowX,
        skilldexWindowY,
        ginfo[SKILLDEX_FRM_BACKGROUND].width,
        ginfo[SKILLDEX_FRM_BACKGROUND].height,
        256,
        WINDOW_MODAL | WINDOW_DONT_MOVE_TOP);
    if (skldxwin == -1) {
        for (int index = 0; index < SKILLDEX_SKILL_BUTTON_BUFFER_COUNT; index++) {
            mem_free(skldxbtn[index]);
        }

        for (int index = 0; index < SKILLDEX_FRM_COUNT; index++) {
            art_ptr_unlock(grphkey[index]);
        }

        message_exit(&skldxmsg);

        return -1;
    }

    bk_enable = map_disable_bk_processes();

    cycle_disable();
    gmouse_set_cursor(MOUSE_CURSOR_ARROW);

    winbuf = win_get_buf(skldxwin);
    memcpy(winbuf,
        skldxbmp[SKILLDEX_FRM_BACKGROUND],
        ginfo[SKILLDEX_FRM_BACKGROUND].width * ginfo[SKILLDEX_FRM_BACKGROUND].height);

    text_font(103);

    // Render "SKILLDEX" title.
    char* title = getmsg(&skldxmsg, &mesg, 100);
    text_to_buf(winbuf + 14 * ginfo[SKILLDEX_FRM_BACKGROUND].width + 55,
        title,
        ginfo[SKILLDEX_FRM_BACKGROUND].width,
        ginfo[SKILLDEX_FRM_BACKGROUND].width,
        colorTable[18979]);

    // Render skill values.
    int valueY = 48;
    for (int index = 0; index < SKILLDEX_SKILL_COUNT; index++) {
        int value = skill_level(obj_dude, sklxref[index]);
        if (value == -1) {
            value = 0;
        }

        int hundreds = value / 100;
        buf_to_buf(skldxbmp[SKILLDEX_FRM_BIG_NUMBERS] + 14 * hundreds,
            14,
            24,
            336,
            winbuf + ginfo[SKILLDEX_FRM_BACKGROUND].width * valueY + 110,
            ginfo[SKILLDEX_FRM_BACKGROUND].width);

        int tens = (value % 100) / 10;
        buf_to_buf(skldxbmp[SKILLDEX_FRM_BIG_NUMBERS] + 14 * tens,
            14,
            24,
            336,
            winbuf + ginfo[SKILLDEX_FRM_BACKGROUND].width * valueY + 124,
            ginfo[SKILLDEX_FRM_BACKGROUND].width);

        int ones = (value % 100) % 10;
        buf_to_buf(skldxbmp[SKILLDEX_FRM_BIG_NUMBERS] + 14 * ones,
            14,
            24,
            336,
            winbuf + ginfo[SKILLDEX_FRM_BACKGROUND].width * valueY + 138,
            ginfo[SKILLDEX_FRM_BACKGROUND].width);

        valueY += 36;
    }

    // Render skill buttons.
    int lineHeight = text_height();

    int buttonY = 45;
    int nameY = ((ginfo[SKILLDEX_FRM_BUTTON_OFF].height - lineHeight) / 2) + 1;
    for (int index = 0; index < SKILLDEX_SKILL_COUNT; index++) {
        char name[MESSAGE_LIST_ITEM_FIELD_MAX_SIZE];
        strcpy(name, getmsg(&skldxmsg, &mesg, 102 + index));

        int nameX = ((ginfo[SKILLDEX_FRM_BUTTON_OFF].width - text_width(name)) / 2) + 1;
        if (nameX < 0) {
            nameX = 0;
        }

        text_to_buf(skldxbtn[index * 2] + ginfo[SKILLDEX_FRM_BUTTON_ON].width * nameY + nameX,
            name,
            ginfo[SKILLDEX_FRM_BUTTON_ON].width,
            ginfo[SKILLDEX_FRM_BUTTON_ON].width,
            colorTable[18979]);

        text_to_buf(skldxbtn[index * 2 + 1] + ginfo[SKILLDEX_FRM_BUTTON_OFF].width * nameY + nameX,
            name,
            ginfo[SKILLDEX_FRM_BUTTON_OFF].width,
            ginfo[SKILLDEX_FRM_BUTTON_OFF].width,
            colorTable[14723]);

        int btn = win_register_button(skldxwin,
            15,
            buttonY,
            ginfo[SKILLDEX_FRM_BUTTON_OFF].width,
            ginfo[SKILLDEX_FRM_BUTTON_OFF].height,
            -1,
            -1,
            -1,
            501 + index,
            skldxbtn[index * 2],
            skldxbtn[index * 2 + 1],
            NULL,
            BUTTON_FLAG_TRANSPARENT);
        if (btn != -1) {
            win_register_button_sound_func(btn, gsound_lrg_butt_press, gsound_lrg_butt_release);
        }

        buttonY += 36;
    }

    // Render "CANCEL" button.
    char* cancel = getmsg(&skldxmsg, &mesg, 101);
    text_to_buf(winbuf + ginfo[SKILLDEX_FRM_BACKGROUND].width * 337 + 72,
        cancel,
        ginfo[SKILLDEX_FRM_BACKGROUND].width,
        ginfo[SKILLDEX_FRM_BACKGROUND].width,
        colorTable[18979]);

    int cancelBtn = win_register_button(skldxwin,
        48,
        338,
        ginfo[SKILLDEX_FRM_LITTLE_RED_BUTTON_UP].width,
        ginfo[SKILLDEX_FRM_LITTLE_RED_BUTTON_UP].height,
        -1,
        -1,
        -1,
        500,
        skldxbmp[SKILLDEX_FRM_LITTLE_RED_BUTTON_UP],
        skldxbmp[SKILLDEX_FRM_LITTLE_RED_BUTTON_DOWN],
        NULL,
        BUTTON_FLAG_TRANSPARENT);
    if (cancelBtn != -1) {
        win_register_button_sound_func(cancelBtn, gsound_red_butt_press, gsound_red_butt_release);
    }

    win_draw(skldxwin);

    return 0;
}

// 0x499C0C
static void skilldex_end()
{
    win_delete(skldxwin);

    for (int index = 0; index < SKILLDEX_SKILL_BUTTON_BUFFER_COUNT; index++) {
        mem_free(skldxbtn[index]);
    }

    for (int index = 0; index < SKILLDEX_FRM_COUNT; index++) {
        art_ptr_unlock(grphkey[index]);
    }

    message_exit(&skldxmsg);

    text_font(fontsave);

    if (bk_enable) {
        map_enable_bk_processes();
    }

    cycle_enable();

    gmouse_set_cursor(MOUSE_CURSOR_ARROW);
}

} // namespace fallout
