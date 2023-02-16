#include "game/editor.h"

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "game/art.h"
#include "game/bmpdlog.h"
#include "game/critter.h"
#include "game/cycle.h"
#include "game/game.h"
#include "game/gmouse.h"
#include "game/graphlib.h"
#include "game/gsound.h"
#include "game/intface.h"
#include "game/item.h"
#include "game/map.h"
#include "game/message.h"
#include "game/object.h"
#include "game/palette.h"
#include "game/perk.h"
#include "game/proto.h"
#include "game/scripts.h"
#include "game/skill.h"
#include "game/stat.h"
#include "game/trait.h"
#include "game/wordwrap.h"
#include "game/worldmap.h"
#include "platform_compat.h"
#include "plib/color/color.h"
#include "plib/db/db.h"
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

#define RENDER_ALL_STATS 7

#define EDITOR_WINDOW_WIDTH 640
#define EDITOR_WINDOW_HEIGHT 480

#define NAME_BUTTON_X 9
#define NAME_BUTTON_Y 0

#define TAG_SKILLS_BUTTON_X 347
#define TAG_SKILLS_BUTTON_Y 26
#define TAG_SKILLS_BUTTON_CODE 536

#define PRINT_BTN_X 363
#define PRINT_BTN_Y 454

#define DONE_BTN_X 475
#define DONE_BTN_Y 454

#define CANCEL_BTN_X 571
#define CANCEL_BTN_Y 454

#define NAME_BTN_CODE 517
#define AGE_BTN_CODE 519
#define SEX_BTN_CODE 520

#define OPTIONAL_TRAITS_LEFT_BTN_X 23
#define OPTIONAL_TRAITS_RIGHT_BTN_X 298
#define OPTIONAL_TRAITS_BTN_Y 352

#define OPTIONAL_TRAITS_BTN_CODE 555

#define OPTIONAL_TRAITS_BTN_SPACE 2

#define SPECIAL_STATS_BTN_X 149

#define PERK_WINDOW_X 33
#define PERK_WINDOW_Y 91
#define PERK_WINDOW_WIDTH 573
#define PERK_WINDOW_HEIGHT 230

#define PERK_WINDOW_LIST_X 45
#define PERK_WINDOW_LIST_Y 43
#define PERK_WINDOW_LIST_WIDTH 192
#define PERK_WINDOW_LIST_HEIGHT 129

#define ANIMATE 0x01
#define RED_NUMBERS 0x02
#define BIG_NUM_WIDTH 14
#define BIG_NUM_HEIGHT 24
#define BIG_NUM_ANIMATION_DELAY 123
#define DIALOG_PICKER_NUM_OPTIONS 72

typedef enum EditorFolder {
    EDITOR_FOLDER_PERKS,
    EDITOR_FOLDER_KARMA,
    EDITOR_FOLDER_KILLS,
} EditorFolder;

enum {
    EDITOR_DERIVED_STAT_ARMOR_CLASS,
    EDITOR_DERIVED_STAT_ACTION_POINTS,
    EDITOR_DERIVED_STAT_CARRY_WEIGHT,
    EDITOR_DERIVED_STAT_MELEE_DAMAGE,
    EDITOR_DERIVED_STAT_DAMAGE_RESISTANCE,
    EDITOR_DERIVED_STAT_POISON_RESISTANCE,
    EDITOR_DERIVED_STAT_RADIATION_RESISTANCE,
    EDITOR_DERIVED_STAT_SEQUENCE,
    EDITOR_DERIVED_STAT_HEALING_RATE,
    EDITOR_DERIVED_STAT_CRITICAL_CHANCE,
    EDITOR_DERIVED_STAT_COUNT,
};

enum {
    EDITOR_FIRST_PRIMARY_STAT,
    EDITOR_HIT_POINTS = 43,
    EDITOR_POISONED,
    EDITOR_RADIATED,
    EDITOR_EYE_DAMAGE,
    EDITOR_CRIPPLED_RIGHT_ARM,
    EDITOR_CRIPPLED_LEFT_ARM,
    EDITOR_CRIPPLED_RIGHT_LEG,
    EDITOR_CRIPPLED_LEFT_LEG,
    EDITOR_FIRST_DERIVED_STAT,
    EDITOR_FIRST_SKILL = EDITOR_FIRST_DERIVED_STAT + EDITOR_DERIVED_STAT_COUNT,
    EDITOR_TAG_SKILL = EDITOR_FIRST_SKILL + SKILL_COUNT,
    EDITOR_SKILLS,
    EDITOR_OPTIONAL_TRAITS,
    EDITOR_FIRST_TRAIT,
    EDITOR_BUTTONS_COUNT = EDITOR_FIRST_TRAIT + TRAIT_COUNT,
};

enum {
    EDITOR_GRAPHIC_BIG_NUMBERS,
    EDITOR_GRAPHIC_AGE_MASK,
    EDITOR_GRAPHIC_AGE_OFF,
    EDITOR_GRAPHIC_DOWN_ARROW_OFF,
    EDITOR_GRAPHIC_DOWN_ARROW_ON,
    EDITOR_GRAPHIC_NAME_MASK,
    EDITOR_GRAPHIC_NAME_ON,
    EDITOR_GRAPHIC_NAME_OFF,
    EDITOR_GRAPHIC_FOLDER_MASK, // mask for all three folders
    EDITOR_GRAPHIC_SEX_MASK,
    EDITOR_GRAPHIC_SEX_OFF,
    EDITOR_GRAPHIC_SEX_ON,
    EDITOR_GRAPHIC_SLIDER, // image containing small plus/minus buttons appeared near selected skill
    EDITOR_GRAPHIC_SLIDER_MINUS_OFF,
    EDITOR_GRAPHIC_SLIDER_MINUS_ON,
    EDITOR_GRAPHIC_SLIDER_PLUS_OFF,
    EDITOR_GRAPHIC_SLIDER_PLUS_ON,
    EDITOR_GRAPHIC_SLIDER_TRANS_MINUS_OFF,
    EDITOR_GRAPHIC_SLIDER_TRANS_MINUS_ON,
    EDITOR_GRAPHIC_SLIDER_TRANS_PLUS_OFF,
    EDITOR_GRAPHIC_SLIDER_TRANS_PLUS_ON,
    EDITOR_GRAPHIC_UP_ARROW_OFF,
    EDITOR_GRAPHIC_UP_ARROW_ON,
    EDITOR_GRAPHIC_LITTLE_RED_BUTTON_UP,
    EDITOR_GRAPHIC_LILTTLE_RED_BUTTON_DOWN,
    EDITOR_GRAPHIC_AGE_ON,
    EDITOR_GRAPHIC_AGE_BOX, // image containing right and left buttons with age stepper in the middle
    EDITOR_GRAPHIC_ATTRIBOX, // ??? black image with two little arrows (up and down) in the right-top corner
    EDITOR_GRAPHIC_ATTRIBWN, // ??? not sure where and when it's used
    EDITOR_GRAPHIC_CHARWIN, // ??? looks like metal plate
    EDITOR_GRAPHIC_DONE_BOX, // metal plate holding DONE button
    EDITOR_GRAPHIC_FEMALE_OFF,
    EDITOR_GRAPHIC_FEMALE_ON,
    EDITOR_GRAPHIC_MALE_OFF,
    EDITOR_GRAPHIC_MALE_ON,
    EDITOR_GRAPHIC_NAME_BOX, // placeholder for name
    EDITOR_GRAPHIC_LEFT_ARROW_UP,
    EDITOR_GRAPHIC_LEFT_ARROW_DOWN,
    EDITOR_GRAPHIC_RIGHT_ARROW_UP,
    EDITOR_GRAPHIC_RIGHT_ARROW_DOWN,
    EDITOR_GRAPHIC_BARARRWS, // ??? two arrows up/down with some strange knob at the top, probably for scrollbar
    EDITOR_GRAPHIC_OPTIONS_BASE, // options metal plate
    EDITOR_GRAPHIC_OPTIONS_BUTTON_OFF,
    EDITOR_GRAPHIC_OPTIONS_BUTTON_ON,
    EDITOR_GRAPHIC_KARMA_FOLDER_SELECTED, // all three folders with middle folder selected (karma)
    EDITOR_GRAPHIC_KILLS_FOLDER_SELECTED, // all theee folders with right folder selected (kills)
    EDITOR_GRAPHIC_PERKS_FOLDER_SELECTED, // all three folders with left folder selected (perks)
    EDITOR_GRAPHIC_KARMAFDR_PLACEOLDER, // ??? placeholder for traits folder image <- this is comment from intrface.lst
    EDITOR_GRAPHIC_TAG_SKILL_BUTTON_OFF,
    EDITOR_GRAPHIC_TAG_SKILL_BUTTON_ON,
    EDITOR_GRAPHIC_COUNT,
};

typedef struct EditorSortableEntry {
    // Depending on the current mode this value is the id of either
    // perk, trait (handling Mutate perk), or skill (handling Tag perk).
    int value;
    char* name;
} EditorSortableEntry;

static int CharEditStart();
static void CharEditEnd();
static void RstrBckgProc();
static void DrawFolder();
static int ListKills();
static void PrintBigNum(int x, int y, int flags, int value, int previousValue, int windowHandle);
static void PrintLevelWin();
static void PrintBasicStat(int stat, bool animate, int previousValue);
static void PrintGender();
static void PrintAgeBig();
static void PrintBigname();
static void ListDrvdStats();
static void ListSkills(int a1);
static void DrawInfoWin();
static int NameWindow();
static void PrintName(unsigned char* buf, int pitch);
static int AgeWindow();
static void SexWindow();
static void StatButton(int eventCode);
static int OptionWindow();
static int Save_as_ASCII(const char* fileName);
static char* AddDots(char* string, int length);
static void ResetScreen();
static void RegInfoAreas();
static int CheckValidPlayer();
static void SavePlayer();
static void RestorePlayer();
static int DrawCard(int graphicId, const char* name, const char* attributes, char* description);
static int XltPerk(int search);
static void FldrButton();
static void InfoButton(int eventCode);
static void SliderBtn(int a1);
static int tagskl_free();
static void TagSkillSelect(int skill);
static void ListTraits();
static int get_trait_count();
static void TraitSelect(int trait);
static int ListKarma();
static int XlateKarma(int search);
static int UpdateLevel();
static void RedrwDPrks();
static int perks_dialog();
static int InputPDLoop(int count, void (*refreshProc)());
static int ListDPerks();
static bool GetMutateTrait();
static void RedrwDMTagSkl();
static bool Add4thTagSkill();
static void ListNewTagSkills();
static int ListMyTraits(int a1);
static int name_sort_comp(const void* a1, const void* a2);
static int DrawCard2(int frmId, const char* name, const char* rank, char* description);
static void push_perks();
static void pop_perks();
static int PerkCount();
static int is_supper_bonus();

// 0x431C40
static const int grph_id[EDITOR_GRAPHIC_COUNT] = {
    170,
    175,
    176,
    181,
    182,
    183,
    184,
    185,
    186,
    187,
    188,
    189,
    190,
    191,
    192,
    193,
    194,
    195,
    196,
    197,
    198,
    199,
    200,
    8,
    9,
    204,
    205,
    206,
    207,
    208,
    209,
    210,
    211,
    212,
    213,
    214,
    122,
    123,
    124,
    125,
    219,
    220,
    221,
    222,
    178,
    179,
    180,
    38,
    215,
    216,
};

// flags to preload fid
//
// 0x431D08
static const unsigned char copyflag[EDITOR_GRAPHIC_COUNT] = {
    0,
    0,
    1,
    0,
    0,
    0,
    1,
    1,
    0,
    0,
    1,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    1,
    1,
    1,
    0,
    0,
};

// graphic ids for derived stats panel
//
// 0x431D3A
static const short ndrvd[EDITOR_DERIVED_STAT_COUNT] = {
    18,
    19,
    20,
    21,
    22,
    23,
    83,
    24,
    25,
    26,
};

// y offsets for stats +/- buttons
//
// 0x431D50
static const int StatYpos[7] = {
    37,
    70,
    103,
    136,
    169,
    202,
    235,
};

// 0x42C33C
static const int karma_var_table[9] = {
    GVAR_BERSERKER_REPUTATION,
    GVAR_CHAMPION_REPUTATION,
    GVAR_CHILDKILLER_REPUATION,
    GVAR_NUKA_COLA_ADDICT,
    GVAR_BUFF_OUT_ADDICT,
    GVAR_MENTATS_ADDICT,
    GVAR_PSYCHO_ADDICT,
    GVAR_RADAWAY_ADDICT,
    GVAR_ALCOHOL_ADDICT,
};

// 0x42C360
static const int karma_pic_table[10] = {
    48,
    49,
    51,
    50,
    52,
    53,
    53,
    53,
    53,
    52,
};

// stat ids for derived stats panel
//
// 0x431D6
static const short ndinfoxlt[EDITOR_DERIVED_STAT_COUNT] = {
    STAT_ARMOR_CLASS,
    STAT_MAXIMUM_ACTION_POINTS,
    STAT_CARRY_WEIGHT,
    STAT_MELEE_DAMAGE,
    STAT_DAMAGE_RESISTANCE,
    STAT_POISON_RESISTANCE,
    STAT_RADIATION_RESISTANCE,
    STAT_SEQUENCE,
    STAT_HEALING_RATE,
    STAT_CRITICAL_CHANCE,
};

// TODO: Remove.
// 0x431D93
char byte_431D93[64];

// 0x504F28
static bool bk_enable = false;

// 0x504F2C
static int skill_cursor = 0;

// 0x504F34
static int slider_y = 27;

// 0x504F38
int character_points = 0;

// 0x56E2F0
static int skillsav[SKILL_COUNT];

// 0x56E338
static MessageList editor_message_file;

// 0x56E340
static EditorSortableEntry name_sort_list[DIALOG_PICKER_NUM_OPTIONS];

// 0x56E580
static char old_str1[48];

// 0x56E5B0
static char old_str2[48];

// pc name
//
// 0x56E5E0
static char name_save[32];

// 0x56E600
static CritterProtoData dude_data;

// 0x56E770
static int perk_back[PERK_COUNT];

// 0x56E870
static Size GInfo[EDITOR_GRAPHIC_COUNT];

// 0x56EA00
static CacheEntry* grph_key[EDITOR_GRAPHIC_COUNT];

// 0x56EAC8
static unsigned char* grphcpy[EDITOR_GRAPHIC_COUNT];

// 0x56EB90
static unsigned char* grphbmp[EDITOR_GRAPHIC_COUNT];

// 0x56EC58
static unsigned char* pbckgnd;

// 0x56EC5C
static int pwin;

// 0x56EC60
static int SliderPlusID;

// 0x56EC64
static int SliderNegID;

// 0x56EC68
static MessageListItem mesg;

// 0x56EC74
static int edit_win;

// 0x56EC78
static unsigned char* pwin_buf;

// 0x56EC7C
static unsigned char* bckgnd;

// 0x56EC80
static int cline;

// 0x56EC84
static int oldsline;

// unspent skill points
//
// 0x56EC88
static int upsent_points_back;

// 0x56EC8C
static int last_level;

// 0x56EC90
static int karma_count;

// 0x56EC94
static int fontsave;

// 0x56EC98
static int kills_count;

// character editor background
//
// 0x56EC9C
static CacheEntry* bck_key;

// 0x56ECA0
static unsigned char* win_buf;

// current hit points
//
// 0x56ECA4
static int hp_back;

// 0x56ECA8
static int mouse_ypos; // mouse y

// 0x56ECAC
static int mouse_xpos; // mouse x

// 0x56ECB0
static int folder;

// 0x56ECB4
static int crow;

// 0x56ECB8
static unsigned int repFtime;

// 0x56ECBC
static unsigned int frame_time;

// 0x56ECC0
static int old_tags;

// 0x56ECC4
static int last_level_back;

// 0x56ECC8
static bool glblmode;

// 0x56ECCC
static int tag_skill_back[NUM_TAGGED_SKILLS];

// 0x56ECE0
static int old_fid2;

// 0x56ECE4
static int old_fid1;

// 0x56ECE8
static int trait_back[3];

// buttons for selecting traits
//
// 0x5700A8
static int trait_bids[TRAIT_COUNT];

// 0x56ECF4
static int info_line;

// 0x56ECF8
static bool frstc_draw1;

// 0x56ECFC
static bool frstc_draw2;

// current index for selecting new trait
//
// 0x56ED00
static int trait_count;

// 0x56ED04
static int optrt_count;

// 0x56ED08
static int temp_trait[3];

// 0x56ED14
static int tagskill_count;

// 0x56ED18
static int temp_tag_skill[NUM_TAGGED_SKILLS];

// 0x56ED2C
static char free_perk_back;

// 0x56ED2D
static unsigned char free_perk;

// 0x56ED2E
static unsigned char first_skill_list;

// 0x42C40C
int editor_design(bool isCreationMode)
{
    char* messageListItemText;
    char line1[128];
    char line2[128];
    const char* lines[] = { line2 };

    glblmode = isCreationMode;

    SavePlayer();

    if (CharEditStart() == -1) {
        debug_printf("\n ** Error loading character editor data! **\n");
        return -1;
    }

    if (!glblmode) {
        if (UpdateLevel()) {
            stat_recalc_derived(obj_dude);
            ListTraits();
            ListSkills(0);
            PrintBasicStat(RENDER_ALL_STATS, 0, 0);
            ListDrvdStats();
            DrawInfoWin();
        }
    }

    int rc = -1;
    while (rc == -1) {
        sharedFpsLimiter.mark();

        frame_time = get_time();
        int keyCode = get_input();

        bool done = false;
        if (keyCode == 500) {
            done = true;
        }

        if (keyCode == KEY_RETURN || keyCode == KEY_UPPERCASE_D || keyCode == KEY_LOWERCASE_D) {
            done = true;
            gsound_play_sfx_file("ib1p1xx1");
        }

        if (done) {
            if (glblmode) {
                if (character_points != 0) {
                    gsound_play_sfx_file("iisxxxx1");

                    // You must use all character points
                    messageListItemText = getmsg(&editor_message_file, &mesg, 118);
                    strcpy(line1, messageListItemText);

                    // before starting the game!
                    messageListItemText = getmsg(&editor_message_file, &mesg, 119);
                    strcpy(line2, messageListItemText);

                    dialog_out(line1, lines, 1, 192, 126, colorTable[32328], 0, colorTable[32328], 0);
                    win_draw(edit_win);

                    rc = -1;
                } else if (tagskill_count > 0) {
                    gsound_play_sfx_file("iisxxxx1");

                    // You must select all tag skills
                    messageListItemText = getmsg(&editor_message_file, &mesg, 142);
                    strcpy(line1, messageListItemText);

                    // before starting the game!
                    messageListItemText = getmsg(&editor_message_file, &mesg, 143);
                    strcpy(line2, messageListItemText);

                    dialog_out(line1, lines, 1, 192, 126, colorTable[32328], 0, colorTable[32328], 0);
                    win_draw(edit_win);

                    rc = -1;
                } else if (is_supper_bonus()) {
                    gsound_play_sfx_file("iisxxxx1");

                    // All stats must be between 1 and 10
                    messageListItemText = getmsg(&editor_message_file, &mesg, 157);
                    strcpy(line1, messageListItemText);

                    // before starting the game!
                    messageListItemText = getmsg(&editor_message_file, &mesg, 158);
                    strcpy(line2, messageListItemText);

                    dialog_out(line1, lines, 1, 192, 126, colorTable[32328], 0, colorTable[32328], 0);
                    win_draw(edit_win);

                    rc = -1;
                } else {
                    rc = 0;
                }
            } else {
                rc = 0;
            }
        } else if (keyCode == KEY_CTRL_Q || keyCode == KEY_CTRL_X || keyCode == KEY_F10) {
            game_quit_with_confirm();
        } else if (keyCode == 502 || keyCode == KEY_ESCAPE || keyCode == KEY_UPPERCASE_C || keyCode == KEY_LOWERCASE_C || game_user_wants_to_quit != 0) {
            rc = 1;
        } else if (glblmode && (keyCode == 517 || keyCode == KEY_UPPERCASE_N || keyCode == KEY_LOWERCASE_N)) {
            NameWindow();
        } else if (glblmode && (keyCode == 519 || keyCode == KEY_UPPERCASE_A || keyCode == KEY_LOWERCASE_A)) {
            AgeWindow();
        } else if (glblmode && (keyCode == 520 || keyCode == KEY_UPPERCASE_S || keyCode == KEY_LOWERCASE_S)) {
            SexWindow();
        } else if (glblmode && (keyCode >= 503 && keyCode < 517)) {
            StatButton(keyCode);
        } else if ((glblmode && (keyCode == 501 || keyCode == KEY_UPPERCASE_O || keyCode == KEY_LOWERCASE_O))
            || (!glblmode && (keyCode == 501 || keyCode == KEY_UPPERCASE_P || keyCode == KEY_LOWERCASE_P))) {
            OptionWindow();
        } else if (keyCode >= 525 && keyCode < 535) {
            InfoButton(keyCode);
        } else if (keyCode == 535) {
            FldrButton();
        } else if (keyCode == 521
            || keyCode == 523
            || keyCode == KEY_ARROW_RIGHT
            || keyCode == KEY_PLUS
            || keyCode == KEY_UPPERCASE_N
            || keyCode == KEY_ARROW_LEFT
            || keyCode == KEY_MINUS
            || keyCode == KEY_UPPERCASE_J) {
            SliderBtn(keyCode);
        } else if (glblmode == 1 && keyCode >= 536 && keyCode < 555) {
            TagSkillSelect(keyCode - 536);
        } else if (glblmode == 1 && keyCode >= 555 && keyCode < 571) {
            TraitSelect(keyCode - 555);
        } else if (keyCode == 390) {
            dump_screen();
        }

        win_draw(edit_win);

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    if (rc == 0) {
        if (isCreationMode) {
            proto_dude_update_gender();
            palette_fade_to(black_palette);
        }
    }

    CharEditEnd();

    if (rc == 1) {
        RestorePlayer();
    }

    if (is_pc_flag(PC_FLAG_LEVEL_UP_AVAILABLE)) {
        pc_flag_off(PC_FLAG_LEVEL_UP_AVAILABLE);
    }

    intface_update_hit_points(false);

    return rc;
}

// 0x42C9F8
static int CharEditStart()
{
    int i;
    char path[COMPAT_MAX_PATH];
    int fid;
    char* str;
    int len;
    int btn;
    int x;
    int y;
    char perks[32];
    char karma[32];
    char kills[32];

    fontsave = text_curr();
    old_tags = 0;
    karma_count = 0;
    bk_enable = 0;
    old_fid2 = -1;
    old_fid1 = -1;
    frstc_draw2 = false;
    frstc_draw1 = false;
    first_skill_list = 1;
    old_str2[0] = '\0';
    old_str1[0] = '\0';

    text_font(101);

    slider_y = skill_cursor * (text_height() + 1) + 27;

    // skills
    skill_get_tags(temp_tag_skill, NUM_TAGGED_SKILLS);

    // NOTE: Uninline.
    tagskill_count = tagskl_free();

    // traits
    trait_get(&(temp_trait[0]), &(temp_trait[1]));

    // NOTE: Uninline.
    trait_count = get_trait_count();

    if (!glblmode) {
        bk_enable = map_disable_bk_processes();
    }

    cycle_disable();
    gmouse_set_cursor(MOUSE_CURSOR_ARROW);

    if (!message_init(&editor_message_file)) {
        return -1;
    }

    snprintf(path, sizeof(path), "%s%s", msg_path, "editor.msg");

    if (!message_load(&editor_message_file, path)) {
        return -1;
    }

    fid = art_id(OBJ_TYPE_INTERFACE, (glblmode ? 169 : 177), 0, 0, 0);
    bckgnd = art_lock(fid, &bck_key, &(GInfo[0].width), &(GInfo[0].height));
    if (bckgnd == NULL) {
        message_exit(&editor_message_file);
        return -1;
    }

    soundUpdate();

    for (i = 0; i < EDITOR_GRAPHIC_COUNT; i++) {
        fid = art_id(OBJ_TYPE_INTERFACE, grph_id[i], 0, 0, 0);
        grphbmp[i] = art_lock(fid, &(grph_key[i]), &(GInfo[i].width), &(GInfo[i].height));
        if (grphbmp[i] == NULL) {
            break;
        }
    }

    if (i != EDITOR_GRAPHIC_COUNT) {
        while (--i >= 0) {
            art_ptr_unlock(grph_key[i]);
        }
        return -1;

        art_ptr_unlock(bck_key);

        message_exit(&editor_message_file);

        // NOTE: Uninline.
        RstrBckgProc();

        return -1;
    }

    soundUpdate();

    for (i = 0; i < EDITOR_GRAPHIC_COUNT; i++) {
        if (copyflag[i]) {
            grphcpy[i] = (unsigned char*)mem_malloc(GInfo[i].width * GInfo[i].height);
            if (grphcpy[i] == NULL) {
                break;
            }
            memcpy(grphcpy[i], grphbmp[i], GInfo[i].width * GInfo[i].height);
        } else {
            grphcpy[i] = (unsigned char*)-1;
        }
    }

    if (i != EDITOR_GRAPHIC_COUNT) {
        while (--i >= 0) {
            if (copyflag[i]) {
                mem_free(grphcpy[i]);
            }
        }

        for (i = 0; i < EDITOR_GRAPHIC_COUNT; i++) {
            art_ptr_unlock(grph_key[i]);
        }

        art_ptr_unlock(bck_key);

        message_exit(&editor_message_file);

        // NOTE: Uninline.
        RstrBckgProc();

        return -1;
    }

    int editorWindowX = (screenGetWidth() - EDITOR_WINDOW_WIDTH) / 2;
    int editorWindowY = (screenGetHeight() - EDITOR_WINDOW_HEIGHT) / 2;
    edit_win = win_add(editorWindowX,
        editorWindowY,
        EDITOR_WINDOW_WIDTH,
        EDITOR_WINDOW_HEIGHT,
        256,
        WINDOW_MODAL | WINDOW_DONT_MOVE_TOP);
    if (edit_win == -1) {
        for (i = 0; i < EDITOR_GRAPHIC_COUNT; i++) {
            if (copyflag[i]) {
                mem_free(grphcpy[i]);
            }
            art_ptr_unlock(grph_key[i]);
        }

        art_ptr_unlock(bck_key);

        message_exit(&editor_message_file);

        // NOTE: Uninline.
        RstrBckgProc();

        return -1;
    }

    win_buf = win_get_buf(edit_win);
    memcpy(win_buf, bckgnd, 640 * 480);

    if (glblmode) {
        text_font(103);

        // CHAR POINTS
        str = getmsg(&editor_message_file, &mesg, 116);
        text_to_buf(win_buf + (286 * 640) + 14, str, 640, 640, colorTable[18979]);
        PrintBigNum(126, 282, 0, character_points, 0, edit_win);

        // OPTIONS
        str = getmsg(&editor_message_file, &mesg, 101);
        text_to_buf(win_buf + (454 * 640) + 363, str, 640, 640, colorTable[18979]);

        // OPTIONAL TRAITS
        str = getmsg(&editor_message_file, &mesg, 139);
        text_to_buf(win_buf + (326 * 640) + 52, str, 640, 640, colorTable[18979]);
        PrintBigNum(522, 228, 0, optrt_count, 0, edit_win);

        // TAG SKILLS
        str = getmsg(&editor_message_file, &mesg, 138);
        text_to_buf(win_buf + (233 * 640) + 422, str, 640, 640, colorTable[18979]);
        PrintBigNum(522, 228, 0, tagskill_count, 0, edit_win);
    } else {
        text_font(103);

        str = getmsg(&editor_message_file, &mesg, 109);
        strcpy(perks, str);

        str = getmsg(&editor_message_file, &mesg, 110);
        strcpy(karma, str);

        str = getmsg(&editor_message_file, &mesg, 111);
        strcpy(kills, str);

        // perks selected
        len = text_width(perks);
        text_to_buf(
            grphcpy[46] + 5 * GInfo[EDITOR_GRAPHIC_PERKS_FOLDER_SELECTED].width + 61 - len / 2,
            perks,
            GInfo[EDITOR_GRAPHIC_PERKS_FOLDER_SELECTED].width,
            GInfo[EDITOR_GRAPHIC_PERKS_FOLDER_SELECTED].width,
            colorTable[18979]);

        len = text_width(karma);
        text_to_buf(grphcpy[EDITOR_GRAPHIC_PERKS_FOLDER_SELECTED] + 5 * GInfo[EDITOR_GRAPHIC_PERKS_FOLDER_SELECTED].width + 159 - len / 2,
            karma,
            GInfo[EDITOR_GRAPHIC_PERKS_FOLDER_SELECTED].width,
            GInfo[EDITOR_GRAPHIC_PERKS_FOLDER_SELECTED].width,
            colorTable[14723]);

        len = text_width(kills);
        text_to_buf(grphcpy[EDITOR_GRAPHIC_PERKS_FOLDER_SELECTED] + 5 * GInfo[EDITOR_GRAPHIC_PERKS_FOLDER_SELECTED].width + 257 - len / 2,
            kills,
            GInfo[EDITOR_GRAPHIC_PERKS_FOLDER_SELECTED].width,
            GInfo[EDITOR_GRAPHIC_PERKS_FOLDER_SELECTED].width,
            colorTable[14723]);

        // karma selected
        len = text_width(perks);
        text_to_buf(grphcpy[EDITOR_GRAPHIC_KARMA_FOLDER_SELECTED] + 5 * GInfo[EDITOR_GRAPHIC_PERKS_FOLDER_SELECTED].width + 61 - len / 2,
            perks,
            GInfo[EDITOR_GRAPHIC_PERKS_FOLDER_SELECTED].width,
            GInfo[EDITOR_GRAPHIC_PERKS_FOLDER_SELECTED].width,
            colorTable[14723]);

        len = text_width(karma);
        text_to_buf(grphcpy[EDITOR_GRAPHIC_KARMA_FOLDER_SELECTED] + 5 * GInfo[EDITOR_GRAPHIC_PERKS_FOLDER_SELECTED].width + 159 - len / 2,
            karma,
            GInfo[EDITOR_GRAPHIC_PERKS_FOLDER_SELECTED].width,
            GInfo[EDITOR_GRAPHIC_PERKS_FOLDER_SELECTED].width,
            colorTable[18979]);

        len = text_width(kills);
        text_to_buf(grphcpy[EDITOR_GRAPHIC_KARMA_FOLDER_SELECTED] + 5 * GInfo[EDITOR_GRAPHIC_PERKS_FOLDER_SELECTED].width + 257 - len / 2,
            kills,
            GInfo[46].width,
            GInfo[46].width,
            colorTable[14723]);

        // kills selected
        len = text_width(perks);
        text_to_buf(grphcpy[EDITOR_GRAPHIC_KILLS_FOLDER_SELECTED] + 5 * GInfo[EDITOR_GRAPHIC_PERKS_FOLDER_SELECTED].width + 61 - len / 2,
            perks,
            GInfo[46].width,
            GInfo[46].width,
            colorTable[14723]);

        len = text_width(karma);
        text_to_buf(grphcpy[EDITOR_GRAPHIC_KILLS_FOLDER_SELECTED] + 5 * GInfo[EDITOR_GRAPHIC_PERKS_FOLDER_SELECTED].width + 159 - len / 2,
            karma,
            GInfo[46].width,
            GInfo[46].width,
            colorTable[14723]);

        len = text_width(kills);
        text_to_buf(grphcpy[EDITOR_GRAPHIC_KILLS_FOLDER_SELECTED] + 5 * GInfo[EDITOR_GRAPHIC_PERKS_FOLDER_SELECTED].width + 257 - len / 2,
            kills,
            GInfo[EDITOR_GRAPHIC_PERKS_FOLDER_SELECTED].width,
            GInfo[EDITOR_GRAPHIC_PERKS_FOLDER_SELECTED].width,
            colorTable[18979]);

        DrawFolder();

        text_font(103);

        // PRINT
        str = getmsg(&editor_message_file, &mesg, 103);
        text_to_buf(win_buf + (EDITOR_WINDOW_WIDTH * PRINT_BTN_Y) + PRINT_BTN_X, str, EDITOR_WINDOW_WIDTH, EDITOR_WINDOW_WIDTH, colorTable[18979]);

        PrintLevelWin();
    }

    text_font(103);

    // CANCEL
    str = getmsg(&editor_message_file, &mesg, 102);
    text_to_buf(win_buf + (EDITOR_WINDOW_WIDTH * CANCEL_BTN_Y) + CANCEL_BTN_X, str, EDITOR_WINDOW_WIDTH, EDITOR_WINDOW_WIDTH, colorTable[18979]);

    // DONE
    str = getmsg(&editor_message_file, &mesg, 100);
    text_to_buf(win_buf + (EDITOR_WINDOW_WIDTH * DONE_BTN_Y) + DONE_BTN_X, str, EDITOR_WINDOW_WIDTH, EDITOR_WINDOW_WIDTH, colorTable[18979]);

    PrintBasicStat(RENDER_ALL_STATS, 0, 0);
    ListDrvdStats();

    if (!glblmode) {
        SliderPlusID = win_register_button(
            edit_win,
            614,
            20,
            GInfo[EDITOR_GRAPHIC_SLIDER_PLUS_ON].width,
            GInfo[EDITOR_GRAPHIC_SLIDER_PLUS_ON].height,
            -1,
            522,
            521,
            522,
            grphbmp[EDITOR_GRAPHIC_SLIDER_PLUS_OFF],
            grphbmp[EDITOR_GRAPHIC_SLIDER_PLUS_ON],
            0,
            BUTTON_FLAG_TRANSPARENT | BUTTON_FLAG_0x40);
        SliderNegID = win_register_button(
            edit_win,
            614,
            20 + GInfo[EDITOR_GRAPHIC_SLIDER_MINUS_ON].height - 1,
            GInfo[EDITOR_GRAPHIC_SLIDER_MINUS_ON].width,
            GInfo[EDITOR_GRAPHIC_SLIDER_MINUS_OFF].height,
            -1,
            524,
            523,
            524,
            grphbmp[EDITOR_GRAPHIC_SLIDER_MINUS_OFF],
            grphbmp[EDITOR_GRAPHIC_SLIDER_MINUS_ON],
            0,
            BUTTON_FLAG_TRANSPARENT | BUTTON_FLAG_0x40);
        win_register_button_sound_func(SliderPlusID, gsound_red_butt_press, NULL);
        win_register_button_sound_func(SliderNegID, gsound_red_butt_press, NULL);
    }

    ListSkills(0);
    DrawInfoWin();
    soundUpdate();
    PrintBigname();
    PrintAgeBig();
    PrintGender();

    if (glblmode) {
        x = NAME_BUTTON_X;
        btn = win_register_button(
            edit_win,
            x,
            NAME_BUTTON_Y,
            GInfo[EDITOR_GRAPHIC_NAME_ON].width,
            GInfo[EDITOR_GRAPHIC_NAME_ON].height,
            -1,
            -1,
            -1,
            NAME_BTN_CODE,
            grphcpy[EDITOR_GRAPHIC_NAME_OFF],
            grphcpy[EDITOR_GRAPHIC_NAME_ON],
            0,
            BUTTON_FLAG_TRANSPARENT);
        if (btn != -1) {
            win_register_button_mask(btn, grphbmp[EDITOR_GRAPHIC_NAME_MASK]);
            win_register_button_sound_func(btn, gsound_lrg_butt_press, NULL);
        }

        x += GInfo[EDITOR_GRAPHIC_NAME_ON].width;
        btn = win_register_button(
            edit_win,
            x,
            NAME_BUTTON_Y,
            GInfo[EDITOR_GRAPHIC_AGE_ON].width,
            GInfo[EDITOR_GRAPHIC_AGE_ON].height,
            -1,
            -1,
            -1,
            AGE_BTN_CODE,
            grphcpy[EDITOR_GRAPHIC_AGE_OFF],
            grphcpy[EDITOR_GRAPHIC_AGE_ON],
            0,
            BUTTON_FLAG_TRANSPARENT);
        if (btn != -1) {
            win_register_button_mask(btn, grphbmp[EDITOR_GRAPHIC_AGE_MASK]);
            win_register_button_sound_func(btn, gsound_lrg_butt_press, NULL);
        }

        x += GInfo[EDITOR_GRAPHIC_AGE_ON].width;
        btn = win_register_button(
            edit_win,
            x,
            NAME_BUTTON_Y,
            GInfo[EDITOR_GRAPHIC_SEX_ON].width,
            GInfo[EDITOR_GRAPHIC_SEX_ON].height,
            -1,
            -1,
            -1,
            SEX_BTN_CODE,
            grphcpy[EDITOR_GRAPHIC_SEX_OFF],
            grphcpy[EDITOR_GRAPHIC_SEX_ON],
            0,
            BUTTON_FLAG_TRANSPARENT);
        if (btn != -1) {
            win_register_button_mask(btn, grphbmp[EDITOR_GRAPHIC_SEX_MASK]);
            win_register_button_sound_func(btn, gsound_lrg_butt_press, NULL);
        }

        y = TAG_SKILLS_BUTTON_Y;
        for (i = 0; i < SKILL_COUNT; i++) {
            btn = win_register_button(
                edit_win,
                TAG_SKILLS_BUTTON_X,
                y,
                GInfo[EDITOR_GRAPHIC_TAG_SKILL_BUTTON_ON].width,
                GInfo[EDITOR_GRAPHIC_TAG_SKILL_BUTTON_ON].height,
                -1,
                -1,
                -1,
                TAG_SKILLS_BUTTON_CODE + i,
                grphbmp[EDITOR_GRAPHIC_TAG_SKILL_BUTTON_OFF],
                grphbmp[EDITOR_GRAPHIC_TAG_SKILL_BUTTON_ON],
                NULL,
                BUTTON_FLAG_TRANSPARENT);
            if (btn != -1) {
                win_register_button_sound_func(btn, gsound_red_butt_press, NULL);
            }
            y += GInfo[EDITOR_GRAPHIC_TAG_SKILL_BUTTON_ON].height;
        }

        y = OPTIONAL_TRAITS_BTN_Y;
        for (i = 0; i < TRAIT_COUNT / 2; i++) {
            btn = win_register_button(
                edit_win,
                OPTIONAL_TRAITS_LEFT_BTN_X,
                y,
                GInfo[EDITOR_GRAPHIC_TAG_SKILL_BUTTON_ON].width,
                GInfo[EDITOR_GRAPHIC_TAG_SKILL_BUTTON_ON].height,
                -1,
                -1,
                -1,
                OPTIONAL_TRAITS_BTN_CODE + i,
                grphbmp[EDITOR_GRAPHIC_TAG_SKILL_BUTTON_OFF],
                grphbmp[EDITOR_GRAPHIC_TAG_SKILL_BUTTON_ON],
                NULL,
                BUTTON_FLAG_TRANSPARENT);
            if (btn != -1) {
                win_register_button_sound_func(btn, gsound_red_butt_press, NULL);
            }
            y += GInfo[EDITOR_GRAPHIC_TAG_SKILL_BUTTON_ON].height + OPTIONAL_TRAITS_BTN_SPACE;
        }

        y = OPTIONAL_TRAITS_BTN_Y;
        for (i = TRAIT_COUNT / 2; i < TRAIT_COUNT; i++) {
            btn = win_register_button(
                edit_win,
                OPTIONAL_TRAITS_RIGHT_BTN_X,
                y,
                GInfo[EDITOR_GRAPHIC_TAG_SKILL_BUTTON_ON].width,
                GInfo[EDITOR_GRAPHIC_TAG_SKILL_BUTTON_ON].height,
                -1,
                -1,
                -1,
                OPTIONAL_TRAITS_BTN_CODE + i,
                grphbmp[EDITOR_GRAPHIC_TAG_SKILL_BUTTON_OFF],
                grphbmp[EDITOR_GRAPHIC_TAG_SKILL_BUTTON_ON],
                NULL,
                BUTTON_FLAG_TRANSPARENT);
            if (btn != -1) {
                win_register_button_sound_func(btn, gsound_red_butt_press, NULL);
            }
            y += GInfo[EDITOR_GRAPHIC_TAG_SKILL_BUTTON_ON].height + OPTIONAL_TRAITS_BTN_SPACE;
        }

        ListTraits();
    } else {
        x = NAME_BUTTON_X;
        trans_buf_to_buf(grphcpy[EDITOR_GRAPHIC_NAME_OFF],
            GInfo[EDITOR_GRAPHIC_NAME_ON].width,
            GInfo[EDITOR_GRAPHIC_NAME_ON].height,
            GInfo[EDITOR_GRAPHIC_NAME_ON].width,
            win_buf + (EDITOR_WINDOW_WIDTH * NAME_BUTTON_Y) + x,
            EDITOR_WINDOW_WIDTH);

        x += GInfo[EDITOR_GRAPHIC_NAME_ON].width;
        trans_buf_to_buf(grphcpy[EDITOR_GRAPHIC_AGE_OFF],
            GInfo[EDITOR_GRAPHIC_AGE_ON].width,
            GInfo[EDITOR_GRAPHIC_AGE_ON].height,
            GInfo[EDITOR_GRAPHIC_AGE_ON].width,
            win_buf + (EDITOR_WINDOW_WIDTH * NAME_BUTTON_Y) + x,
            EDITOR_WINDOW_WIDTH);

        x += GInfo[EDITOR_GRAPHIC_AGE_ON].width;
        trans_buf_to_buf(grphcpy[EDITOR_GRAPHIC_SEX_OFF],
            GInfo[EDITOR_GRAPHIC_SEX_ON].width,
            GInfo[EDITOR_GRAPHIC_SEX_ON].height,
            GInfo[EDITOR_GRAPHIC_SEX_ON].width,
            win_buf + (EDITOR_WINDOW_WIDTH * NAME_BUTTON_Y) + x,
            EDITOR_WINDOW_WIDTH);

        btn = win_register_button(edit_win,
            11,
            327,
            GInfo[EDITOR_GRAPHIC_FOLDER_MASK].width,
            GInfo[EDITOR_GRAPHIC_FOLDER_MASK].height,
            -1,
            -1,
            -1,
            535,
            NULL,
            NULL,
            NULL,
            BUTTON_FLAG_TRANSPARENT);
        if (btn != -1) {
            win_register_button_mask(btn, grphbmp[EDITOR_GRAPHIC_FOLDER_MASK]);
        }
    }

    if (glblmode) {
        // +/- buttons for stats
        for (i = 0; i < 7; i++) {
            btn = win_register_button(edit_win,
                SPECIAL_STATS_BTN_X,
                StatYpos[i],
                GInfo[EDITOR_GRAPHIC_SLIDER_PLUS_ON].width,
                GInfo[EDITOR_GRAPHIC_SLIDER_PLUS_ON].height,
                -1,
                518,
                503 + i,
                518,
                grphbmp[EDITOR_GRAPHIC_SLIDER_PLUS_OFF],
                grphbmp[EDITOR_GRAPHIC_SLIDER_PLUS_ON],
                NULL,
                BUTTON_FLAG_TRANSPARENT);
            if (btn != -1) {
                win_register_button_sound_func(btn, gsound_red_butt_press, NULL);
            }

            btn = win_register_button(edit_win,
                SPECIAL_STATS_BTN_X,
                StatYpos[i] + GInfo[EDITOR_GRAPHIC_SLIDER_PLUS_ON].height - 1,
                GInfo[EDITOR_GRAPHIC_SLIDER_MINUS_ON].width,
                GInfo[EDITOR_GRAPHIC_SLIDER_MINUS_ON].height,
                -1,
                518,
                510 + i,
                518,
                grphbmp[EDITOR_GRAPHIC_SLIDER_MINUS_OFF],
                grphbmp[EDITOR_GRAPHIC_SLIDER_MINUS_ON],
                NULL,
                BUTTON_FLAG_TRANSPARENT);
            if (btn != -1) {
                win_register_button_sound_func(btn, gsound_red_butt_press, NULL);
            }
        }
    }

    RegInfoAreas();
    soundUpdate();

    btn = win_register_button(
        edit_win,
        343,
        454,
        GInfo[EDITOR_GRAPHIC_LITTLE_RED_BUTTON_UP].width,
        GInfo[EDITOR_GRAPHIC_LITTLE_RED_BUTTON_UP].height,
        -1,
        -1,
        -1,
        501,
        grphbmp[EDITOR_GRAPHIC_LITTLE_RED_BUTTON_UP],
        grphbmp[EDITOR_GRAPHIC_LILTTLE_RED_BUTTON_DOWN],
        NULL,
        BUTTON_FLAG_TRANSPARENT);
    if (btn != -1) {
        win_register_button_sound_func(btn, gsound_red_butt_press, gsound_red_butt_release);
    }

    btn = win_register_button(
        edit_win,
        552,
        454,
        GInfo[EDITOR_GRAPHIC_LITTLE_RED_BUTTON_UP].width,
        GInfo[EDITOR_GRAPHIC_LITTLE_RED_BUTTON_UP].height,
        -1,
        -1,
        -1,
        502,
        grphbmp[EDITOR_GRAPHIC_LITTLE_RED_BUTTON_UP],
        grphbmp[EDITOR_GRAPHIC_LILTTLE_RED_BUTTON_DOWN],
        0,
        BUTTON_FLAG_TRANSPARENT);
    if (btn != -1) {
        win_register_button_sound_func(btn, gsound_red_butt_press, gsound_red_butt_release);
    }

    btn = win_register_button(
        edit_win,
        455,
        454,
        GInfo[23].width,
        GInfo[23].height,
        -1,
        -1,
        -1,
        500,
        grphbmp[23],
        grphbmp[24],
        0,
        BUTTON_FLAG_TRANSPARENT);
    if (btn != -1) {
        win_register_button_sound_func(btn, gsound_red_butt_press, gsound_red_butt_release);
    }

    win_draw(edit_win);
    disable_box_bar_win();

    return 0;
}

// 0x42DA38
static void CharEditEnd()
{
    win_delete(edit_win);

    for (int index = 0; index < EDITOR_GRAPHIC_COUNT; index++) {
        art_ptr_unlock(grph_key[index]);

        if (copyflag[index]) {
            mem_free(grphcpy[index]);
        }
    }

    art_ptr_unlock(bck_key);

    message_exit(&editor_message_file);

    intface_redraw();

    // NOTE: Uninline.
    RstrBckgProc();

    text_font(fontsave);

    if (glblmode == 1) {
        skill_set_tags(temp_tag_skill, 3);
        trait_set(temp_trait[0], temp_trait[1]);
        info_line = 0;
        critter_adjust_hits(obj_dude, 1000);
    }

    enable_box_bar_win();
}

// 0x42DAF4
static void RstrBckgProc()
{
    if (bk_enable) {
        map_enable_bk_processes();
    }

    cycle_enable();

    gmouse_set_cursor(MOUSE_CURSOR_ARROW);
}

// 0x42DB14
void CharEditInit()
{
    int i;

    info_line = 0;
    skill_cursor = 0;
    slider_y = 27;
    free_perk = 0;
    folder = EDITOR_FOLDER_PERKS;

    for (i = 0; i < 2; i++) {
        temp_trait[i] = -1;
        trait_back[i] = -1;
    }

    character_points = 5;
    last_level = 1;
}

// 0x0x42DB74
int get_input_str(int win, int cancelKeyCode, char* text, int maxLength, int x, int y, int textColor, int backgroundColor, int flags)
{
    int cursorWidth = text_width("_") - 4;
    int windowWidth = win_width(win);
    int v60 = text_height();
    unsigned char* windowBuffer = win_get_buf(win);
    if (maxLength > 255) {
        maxLength = 255;
    }

    char copy[257];
    strcpy(copy, text);

    int nameLength = strlen(text);
    copy[nameLength] = ' ';
    copy[nameLength + 1] = '\0';

    int nameWidth = text_width(copy);

    buf_fill(windowBuffer + windowWidth * y + x, nameWidth, text_height(), windowWidth, backgroundColor);
    text_to_buf(windowBuffer + windowWidth * y + x, copy, windowWidth, windowWidth, textColor);

    win_draw(win);

    beginTextInput();

    int blinkingCounter = 3;
    bool blink = false;

    int rc = 1;
    while (rc == 1) {
        sharedFpsLimiter.mark();

        frame_time = get_time();

        int keyCode = get_input();
        if (keyCode == cancelKeyCode) {
            rc = 0;
        } else if (keyCode == KEY_RETURN) {
            gsound_play_sfx_file("ib1p1xx1");
            rc = 0;
        } else if (keyCode == KEY_ESCAPE || game_user_wants_to_quit != 0) {
            rc = -1;
        } else {
            if ((keyCode == KEY_DELETE || keyCode == KEY_BACKSPACE) && nameLength >= 1) {
                buf_fill(windowBuffer + windowWidth * y + x, text_width(copy), v60, windowWidth, backgroundColor);
                copy[nameLength - 1] = ' ';
                copy[nameLength] = '\0';
                text_to_buf(windowBuffer + windowWidth * y + x, copy, windowWidth, windowWidth, textColor);
                nameLength--;

                win_draw(win);
            } else if ((keyCode >= KEY_FIRST_INPUT_CHARACTER && keyCode <= KEY_LAST_INPUT_CHARACTER) && nameLength < maxLength) {
                if ((flags & 0x01) != 0) {
                    if (!isdoschar(keyCode)) {
                        break;
                    }
                }

                buf_fill(windowBuffer + windowWidth * y + x, text_width(copy), v60, windowWidth, backgroundColor);

                copy[nameLength] = keyCode & 0xFF;
                copy[nameLength + 1] = ' ';
                copy[nameLength + 2] = '\0';
                text_to_buf(windowBuffer + windowWidth * y + x, copy, windowWidth, windowWidth, textColor);
                nameLength++;

                win_draw(win);
            }
        }

        blinkingCounter -= 1;
        if (blinkingCounter == 0) {
            blinkingCounter = 3;

            int color = blink ? backgroundColor : textColor;
            blink = !blink;

            buf_fill(windowBuffer + windowWidth * y + x + text_width(copy) - cursorWidth, cursorWidth, v60 - 2, windowWidth, color);
        }

        win_draw(win);

        while (elapsed_time(frame_time) < 1000 / 24) { }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    endTextInput();

    if (rc == 0 || nameLength > 0) {
        copy[nameLength] = '\0';
        strcpy(text, copy);
    }

    return rc;
}

// 0x42DF68
bool isdoschar(int ch)
{
    const char* punctuations = "#@!$`'~^&()-_=[]{}";

    if (isalnum(ch)) {
        return true;
    }

    int length = strlen(punctuations);
    for (int index = 0; index < length; index++) {
        if (punctuations[index] == ch) {
            return true;
        }
    }

    return false;
}

// copy filename replacing extension
//
// 0x42DFD8
char* strmfe(char* dest, const char* name, const char* ext)
{
    char* save = dest;

    while (*name != '\0' && *name != '.') {
        *dest++ = *name++;
    }

    *dest++ = '.';

    strcpy(dest, ext);

    return save;
}

// 0x42E014
static void DrawFolder()
{
    char perkName[80];
    int perk;
    int perkLevel;
    int selected_perk_line;
    int count;
    int color;
    int y;
    int v2;
    int v3;
    int v4;

    if (glblmode) {
        return;
    }

    buf_to_buf(bckgnd + (360 * 640) + 34, 280, 120, 640, win_buf + (360 * 640) + 34, 640);

    text_font(101);

    switch (folder) {
    case EDITOR_FOLDER_PERKS:
        buf_to_buf(grphcpy[EDITOR_GRAPHIC_PERKS_FOLDER_SELECTED],
            GInfo[EDITOR_GRAPHIC_PERKS_FOLDER_SELECTED].width,
            GInfo[EDITOR_GRAPHIC_PERKS_FOLDER_SELECTED].height,
            GInfo[EDITOR_GRAPHIC_PERKS_FOLDER_SELECTED].width,
            win_buf + (327 * 640) + 11,
            640);

        // NOTE: This value is not initialized, leading to crash in MSVC.
        selected_perk_line = -1;

        if (info_line >= 10 && info_line < 43) {
            selected_perk_line = info_line - 10;
        }

        count = 0;
        y = 364;
        for (perk = 0; perk < PERK_COUNT; perk++) {
            perkLevel = perk_level(perk);
            if (perkLevel != 0) {
                count++;
                if (count > 7) {
                    break;
                }

                if (perkLevel > 1) {
                    snprintf(perkName, sizeof(perkName), "%s (%d)", perk_name(perk), perkLevel);
                } else {
                    strcpy(perkName, perk_name(perk));
                }

                if (count - 1 == selected_perk_line) {
                    color = colorTable[32747];
                } else {
                    color = colorTable[992];
                }

                text_to_buf(win_buf + 640 * y + 34,
                    perkName,
                    640,
                    640,
                    color);

                y += 1 + text_height();
            }
        }

        if (count > 7) {
            debug_printf("\n ** To many perks! %d total **\n", count);
        }

        y = 362 + 7 * (text_height() + 1);
        v2 = text_width(getmsg(&editor_message_file, &mesg, 156));
        v3 = (280 - v2) / 2 + 34;
        v4 = (246 - v2) / 2;

        text_to_buf(win_buf + 640 * y + v3 - 3,
            getmsg(&editor_message_file, &mesg, 156),
            640,
            640,
            colorTable[992]);

        win_line(edit_win,
            34,
            y + text_height() / 2,
            v4,
            y + text_height() / 2,
            colorTable[992]);

        win_line(edit_win,
            v3 + v2 + 34 - 23,
            y + text_height() / 2,
            v3 + v2 + 34 + v4 - 23,
            y + text_height() / 2,
            colorTable[992]);

        y += 1 + text_height();
        if (temp_trait[0] != -1) {
            if (selected_perk_line == 8) {
                color = colorTable[32747];
            } else {
                color = colorTable[992];
            }

            text_to_buf(win_buf + 640 * y + 34,
                trait_name(temp_trait[0]),
                640,
                640, color);

            y += 1 + text_height();
        }

        if (temp_trait[1] != -1) {
            if (selected_perk_line == 9) {
                color = colorTable[32747];
            } else {
                color = colorTable[992];
            }

            text_to_buf(win_buf + 640 * y + 34,
                trait_name(temp_trait[1]),
                640,
                640, color);
        }
        break;
    case EDITOR_FOLDER_KARMA:
        buf_to_buf(grphcpy[EDITOR_GRAPHIC_KARMA_FOLDER_SELECTED],
            GInfo[EDITOR_GRAPHIC_PERKS_FOLDER_SELECTED].width,
            GInfo[EDITOR_GRAPHIC_PERKS_FOLDER_SELECTED].height,
            GInfo[EDITOR_GRAPHIC_PERKS_FOLDER_SELECTED].width,
            win_buf + (327 * 640) + 11,
            640);
        karma_count = ListKarma();
        break;
    case EDITOR_FOLDER_KILLS:
        buf_to_buf(grphcpy[EDITOR_GRAPHIC_KILLS_FOLDER_SELECTED],
            GInfo[EDITOR_GRAPHIC_PERKS_FOLDER_SELECTED].width,
            GInfo[EDITOR_GRAPHIC_PERKS_FOLDER_SELECTED].height,
            GInfo[EDITOR_GRAPHIC_PERKS_FOLDER_SELECTED].width,
            win_buf + (327 * 640) + 11,
            640);
        kills_count = ListKills();
        break;
    default:
        debug_printf("\n ** Unknown folder type! **\n");
        break;
    }
}

// 0x42E434
static int ListKills()
{
    int selected_kill_line = -1;
    int index;
    int count;
    int color;
    int y;
    char temp[16];

    text_font(101);

    if (info_line >= 10 && info_line < 43) {
        selected_kill_line = info_line - 10;
    }

    count = 0;
    for (index = 0; index < KILL_TYPE_COUNT; index++) {
        if (critter_kill_count(index) != 0) {
            name_sort_list[count].name = critter_kill_name(index);
            name_sort_list[count].value = index;
            count++;
        }
    }

    if (count > 1) {
        qsort(name_sort_list, count, sizeof(*name_sort_list), name_sort_comp);
    }

    y = 362;
    for (index = 0; index < count && index < 10; index++) {
        if (index == selected_kill_line) {
            color = colorTable[32747];
        } else {
            color = colorTable[992];
        }

        text_to_buf(win_buf + 640 * y + 34,
            name_sort_list[index].name,
            640,
            640,
            color);

        text_to_buf(win_buf + 640 * y + 136,
            compat_itoa(critter_kill_count(name_sort_list[index].value), temp, 10),
            640,
            640,
            color);

        y += 1 + text_height();
    }

    if (count - 10 > 0) {
        y = 362;
        for (index = 0; index < count - 10; index++) {
            if (index + 10 == selected_kill_line) {
                color = colorTable[32747];
            } else {
                color = colorTable[992];
            }

            text_to_buf(win_buf + 640 * y + 191,
                name_sort_list[index + 10].name,
                640,
                640,
                color);

            text_to_buf(win_buf + 640 * y + 293,
                compat_itoa(critter_kill_count(name_sort_list[index + 10].value), temp, 10),
                640,
                640,
                color);

            y += 1 + text_height();
        }
    }

    return count;
}

// 0x42E684
static void PrintBigNum(int x, int y, int flags, int value, int previousValue, int windowHandle)
{
    Rect rect;
    int windowWidth;
    unsigned char* windowBuf;
    int tens;
    int ones;
    unsigned char* tensBufferPtr;
    unsigned char* onesBufferPtr;
    unsigned char* numbersGraphicBufferPtr;

    windowWidth = win_width(windowHandle);
    windowBuf = win_get_buf(windowHandle);

    rect.ulx = x;
    rect.uly = y;
    rect.lrx = x + BIG_NUM_WIDTH * 2;
    rect.lry = y + BIG_NUM_HEIGHT;

    numbersGraphicBufferPtr = grphbmp[0];

    if (flags & RED_NUMBERS) {
        // First half of the bignum.frm is white,
        // second half is red.
        numbersGraphicBufferPtr += GInfo[EDITOR_GRAPHIC_BIG_NUMBERS].width / 2;
    }

    tensBufferPtr = windowBuf + windowWidth * y + x;
    onesBufferPtr = tensBufferPtr + BIG_NUM_WIDTH;

    if (value >= 0 && value <= 99 && previousValue >= 0 && previousValue <= 99) {
        tens = value / 10;
        ones = value % 10;

        if (flags & ANIMATE) {
            if (previousValue % 10 != ones) {
                frame_time = get_time();
                buf_to_buf(numbersGraphicBufferPtr + BIG_NUM_WIDTH * 11,
                    BIG_NUM_WIDTH,
                    BIG_NUM_HEIGHT,
                    GInfo[EDITOR_GRAPHIC_BIG_NUMBERS].width,
                    onesBufferPtr,
                    windowWidth);
                win_draw_rect(windowHandle, &rect);
                renderPresent();
                while (elapsed_time(frame_time) < BIG_NUM_ANIMATION_DELAY)
                    ;
            }

            buf_to_buf(numbersGraphicBufferPtr + BIG_NUM_WIDTH * ones,
                BIG_NUM_WIDTH,
                BIG_NUM_HEIGHT,
                GInfo[EDITOR_GRAPHIC_BIG_NUMBERS].width,
                onesBufferPtr,
                windowWidth);
            win_draw_rect(windowHandle, &rect);
            renderPresent();

            if (previousValue / 10 != tens) {
                frame_time = get_time();
                buf_to_buf(numbersGraphicBufferPtr + BIG_NUM_WIDTH * 11,
                    BIG_NUM_WIDTH,
                    BIG_NUM_HEIGHT,
                    GInfo[EDITOR_GRAPHIC_BIG_NUMBERS].width,
                    tensBufferPtr,
                    windowWidth);
                win_draw_rect(windowHandle, &rect);
                while (elapsed_time(frame_time) < BIG_NUM_ANIMATION_DELAY)
                    ;
            }

            buf_to_buf(numbersGraphicBufferPtr + BIG_NUM_WIDTH * tens,
                BIG_NUM_WIDTH,
                BIG_NUM_HEIGHT,
                GInfo[EDITOR_GRAPHIC_BIG_NUMBERS].width,
                tensBufferPtr,
                windowWidth);
            win_draw_rect(windowHandle, &rect);
            renderPresent();
        } else {
            buf_to_buf(numbersGraphicBufferPtr + BIG_NUM_WIDTH * tens,
                BIG_NUM_WIDTH,
                BIG_NUM_HEIGHT,
                GInfo[EDITOR_GRAPHIC_BIG_NUMBERS].width,
                tensBufferPtr,
                windowWidth);
            buf_to_buf(numbersGraphicBufferPtr + BIG_NUM_WIDTH * ones,
                BIG_NUM_WIDTH,
                BIG_NUM_HEIGHT,
                GInfo[EDITOR_GRAPHIC_BIG_NUMBERS].width,
                onesBufferPtr,
                windowWidth);
        }
    } else {

        buf_to_buf(numbersGraphicBufferPtr + BIG_NUM_WIDTH * 9,
            BIG_NUM_WIDTH,
            BIG_NUM_HEIGHT,
            GInfo[EDITOR_GRAPHIC_BIG_NUMBERS].width,
            tensBufferPtr,
            windowWidth);
        buf_to_buf(numbersGraphicBufferPtr + BIG_NUM_WIDTH * 9,
            BIG_NUM_WIDTH,
            BIG_NUM_HEIGHT,
            GInfo[EDITOR_GRAPHIC_BIG_NUMBERS].width,
            onesBufferPtr,
            windowWidth);
    }
}

// 0x42E9C8
static void PrintLevelWin()
{
    int color;
    int y;
    char formattedValueBuffer[8];
    char stringBuffer[128];

    if (glblmode == 1) {
        return;
    }

    text_font(101);

    buf_to_buf(bckgnd + 640 * 280 + 32, 124, 32, 640, win_buf + 640 * 280 + 32, 640);

    // LEVEL
    y = 280;
    if (info_line != 7) {
        color = colorTable[992];
    } else {
        color = colorTable[32747];
    }

    int level = stat_pc_get(PC_STAT_LEVEL);
    snprintf(stringBuffer, sizeof(stringBuffer), "%s %d",
        getmsg(&editor_message_file, &mesg, 113),
        level);
    text_to_buf(win_buf + 640 * y + 32, stringBuffer, 640, 640, color);

    // EXPERIENCE
    y += text_height() + 1;
    if (info_line != 8) {
        color = colorTable[992];
    } else {
        color = colorTable[32747];
    }

    int exp = stat_pc_get(PC_STAT_EXPERIENCE);
    snprintf(stringBuffer, sizeof(stringBuffer), "%s %s",
        getmsg(&editor_message_file, &mesg, 114),
        itostndn(exp, formattedValueBuffer));
    text_to_buf(win_buf + 640 * y + 32, stringBuffer, 640, 640, color);

    // EXP NEEDED TO NEXT LEVEL
    y += text_height() + 1;
    if (info_line != 9) {
        color = colorTable[992];
    } else {
        color = colorTable[32747];
    }

    int expToNextLevel = stat_pc_min_exp();
    if (expToNextLevel == -1) {
        snprintf(stringBuffer, sizeof(stringBuffer), "%s %s",
            getmsg(&editor_message_file, &mesg, 115),
            "------");
    } else {
        snprintf(stringBuffer, sizeof(stringBuffer), "%s %s",
            getmsg(&editor_message_file, &mesg, 115),
            itostndn(expToNextLevel, formattedValueBuffer));
    }

    text_to_buf(win_buf + 640 * y + 32, stringBuffer, 640, 640, color);
}

// 0x42EBD0
static void PrintBasicStat(int stat, bool animate, int previousValue)
{
    int off;
    int color;
    const char* description;
    int value;
    int flags;
    int messageListItemId;

    text_font(101);

    if (stat == RENDER_ALL_STATS) {
        // NOTE: Original code is different, looks like tail recursion
        // optimization.
        for (stat = 0; stat < 7; stat++) {
            PrintBasicStat(stat, false, 0);
        }
        return;
    }

    if (info_line == stat) {
        color = colorTable[32747];
    } else {
        color = colorTable[992];
    }

    off = 640 * (StatYpos[stat] + 8) + 103;

    // TODO: The original code is different.
    if (glblmode) {
        value = stat_get_base(obj_dude, stat) + stat_get_bonus(obj_dude, stat);

        flags = 0;

        if (animate) {
            flags |= ANIMATE;
        }

        if (value > 10) {
            flags |= RED_NUMBERS;
        }

        PrintBigNum(58, StatYpos[stat], flags, value, previousValue, edit_win);

        buf_to_buf(bckgnd + off, 40, text_height(), 640, win_buf + off, 640);

        messageListItemId = stat_level(obj_dude, stat) + 199;
        if (messageListItemId > 210) {
            messageListItemId = 210;
        }

        description = getmsg(&editor_message_file, &mesg, messageListItemId);
        text_to_buf(win_buf + 640 * (StatYpos[stat] + 8) + 103, description, 640, 640, color);
    } else {
        value = stat_level(obj_dude, stat);
        PrintBigNum(58, StatYpos[stat], 0, value, 0, edit_win);
        buf_to_buf(bckgnd + off, 40, text_height(), 640, win_buf + off, 640);

        value = stat_level(obj_dude, stat);
        if (value > 10) {
            value = 10;
        }

        description = stat_level_description(value);
        text_to_buf(win_buf + off, description, 640, 640, color);
    }
}

// 0x42EFC0
static void PrintGender()
{
    int gender;
    char* str;
    char text[32];
    int x, width;

    text_font(103);

    gender = stat_level(obj_dude, STAT_GENDER);
    str = getmsg(&editor_message_file, &mesg, 107 + gender);

    strcpy(text, str);

    width = GInfo[EDITOR_GRAPHIC_SEX_ON].width;
    x = (width / 2) - (text_width(text) / 2);

    memcpy(grphcpy[11],
        grphbmp[EDITOR_GRAPHIC_SEX_ON],
        width * GInfo[EDITOR_GRAPHIC_SEX_ON].height);
    memcpy(grphcpy[EDITOR_GRAPHIC_SEX_OFF],
        grphbmp[10],
        width * GInfo[EDITOR_GRAPHIC_SEX_OFF].height);

    x += 6 * width;
    text_to_buf(grphcpy[EDITOR_GRAPHIC_SEX_ON] + x, text, width, width, colorTable[14723]);
    text_to_buf(grphcpy[EDITOR_GRAPHIC_SEX_OFF] + x, text, width, width, colorTable[18979]);
}

// 0x42F0C4
static void PrintAgeBig()
{
    int age;
    char* str;
    char text[32];
    int x, width;

    text_font(103);

    age = stat_level(obj_dude, STAT_AGE);
    str = getmsg(&editor_message_file, &mesg, 104);

    snprintf(text, sizeof(text), "%s %d", str, age);

    width = GInfo[EDITOR_GRAPHIC_AGE_ON].width;
    x = (width / 2) + 1 - (text_width(text) / 2);

    memcpy(grphcpy[EDITOR_GRAPHIC_AGE_ON],
        grphbmp[EDITOR_GRAPHIC_AGE_ON],
        width * GInfo[EDITOR_GRAPHIC_AGE_ON].height);
    memcpy(grphcpy[EDITOR_GRAPHIC_AGE_OFF],
        grphbmp[EDITOR_GRAPHIC_AGE_OFF],
        width * GInfo[EDITOR_GRAPHIC_AGE_ON].height);

    x += 6 * width;
    text_to_buf(grphcpy[EDITOR_GRAPHIC_AGE_ON] + x, text, width, width, colorTable[14723]);
    text_to_buf(grphcpy[EDITOR_GRAPHIC_AGE_OFF] + x, text, width, width, colorTable[18979]);
}

// 0x42F1C0
static void PrintBigname()
{
    char* str;
    char text[32];
    int x, width;
    char *pch, tmp;
    bool has_space;

    text_font(103);

    str = critter_name(obj_dude);
    strcpy(text, str);

    if (text_width(text) > 100) {
        pch = text;
        has_space = false;
        while (*pch != '\0') {
            tmp = *pch;
            *pch = '\0';
            if (tmp == ' ') {
                has_space = true;
            }

            if (text_width(text) > 100) {
                break;
            }

            *pch = tmp;
            pch++;
        }

        if (has_space) {
            pch = text + strlen(text);
            while (pch != text && *pch != ' ') {
                *pch = '\0';
                pch--;
            }
        }
    }

    width = GInfo[EDITOR_GRAPHIC_NAME_ON].width;
    x = (width / 2) + 3 - (text_width(text) / 2);

    memcpy(grphcpy[EDITOR_GRAPHIC_NAME_ON],
        grphbmp[EDITOR_GRAPHIC_NAME_ON],
        GInfo[EDITOR_GRAPHIC_NAME_ON].width * GInfo[EDITOR_GRAPHIC_NAME_ON].height);
    memcpy(grphcpy[EDITOR_GRAPHIC_NAME_OFF],
        grphbmp[EDITOR_GRAPHIC_NAME_OFF],
        GInfo[EDITOR_GRAPHIC_NAME_OFF].width * GInfo[EDITOR_GRAPHIC_NAME_OFF].height);

    x += 6 * width;
    text_to_buf(grphcpy[EDITOR_GRAPHIC_NAME_ON] + x, text, width, width, colorTable[14723]);
    text_to_buf(grphcpy[EDITOR_GRAPHIC_NAME_OFF] + x, text, width, width, colorTable[18979]);
}

// 0x42F324
static void ListDrvdStats()
{
    int conditions;
    int color;
    const char* messageListItemText;
    char t[420]; // TODO: Size is wrong.
    int y;

    conditions = obj_dude->data.critter.combat.results;

    text_font(101);

    y = 46;

    buf_to_buf(bckgnd + 640 * y + 194, 118, 108, 640, win_buf + 640 * y + 194, 640);

    // Hit Points
    if (info_line == EDITOR_HIT_POINTS) {
        color = colorTable[32747];
    } else {
        color = colorTable[992];
    }

    int currHp;
    int maxHp;
    if (glblmode) {
        maxHp = stat_level(obj_dude, STAT_MAXIMUM_HIT_POINTS);
        currHp = maxHp;
    } else {
        maxHp = stat_level(obj_dude, STAT_MAXIMUM_HIT_POINTS);
        currHp = critter_get_hits(obj_dude);
    }

    messageListItemText = getmsg(&editor_message_file, &mesg, 300);
    snprintf(t, sizeof(t), "%s %d/%d", messageListItemText, currHp, maxHp);
    text_to_buf(win_buf + 640 * y + 194, t, 640, 640, color);

    // Poisoned
    y += text_height() + 3;

    if (info_line == EDITOR_POISONED) {
        color = critter_get_poison(obj_dude) != 0 ? colorTable[32747] : colorTable[15845];
    } else {
        color = critter_get_poison(obj_dude) != 0 ? colorTable[992] : colorTable[1313];
    }

    messageListItemText = getmsg(&editor_message_file, &mesg, 312);
    snprintf(t, sizeof(t), "%s", messageListItemText);
    text_to_buf(win_buf + 640 * y + 194, t, 640, 640, color);

    // Radiated
    y += text_height() + 3;

    if (info_line == EDITOR_RADIATED) {
        color = critter_get_rads(obj_dude) != 0 ? colorTable[32747] : colorTable[15845];
    } else {
        color = critter_get_rads(obj_dude) != 0 ? colorTable[992] : colorTable[1313];
    }

    messageListItemText = getmsg(&editor_message_file, &mesg, 313);
    snprintf(t, sizeof(t), "%s", messageListItemText);
    text_to_buf(win_buf + 640 * y + 194, t, 640, 640, color);

    // Eye Damage
    y += text_height() + 3;

    if (info_line == EDITOR_EYE_DAMAGE) {
        color = (conditions & DAM_BLIND) ? colorTable[32747] : colorTable[15845];
    } else {
        color = (conditions & DAM_BLIND) ? colorTable[992] : colorTable[1313];
    }

    messageListItemText = getmsg(&editor_message_file, &mesg, 314);
    snprintf(t, sizeof(t), "%s", messageListItemText);
    text_to_buf(win_buf + 640 * y + 194, t, 640, 640, color);

    // Crippled Right Arm
    y += text_height() + 3;

    if (info_line == EDITOR_CRIPPLED_RIGHT_ARM) {
        color = (conditions & DAM_CRIP_ARM_RIGHT) ? colorTable[32747] : colorTable[15845];
    } else {
        color = (conditions & DAM_CRIP_ARM_RIGHT) ? colorTable[992] : colorTable[1313];
    }

    messageListItemText = getmsg(&editor_message_file, &mesg, 315);
    snprintf(t, sizeof(t), "%s", messageListItemText);
    text_to_buf(win_buf + 640 * y + 194, t, 640, 640, color);

    // Crippled Left Arm
    y += text_height() + 3;

    if (info_line == EDITOR_CRIPPLED_LEFT_ARM) {
        color = (conditions & DAM_CRIP_ARM_LEFT) ? colorTable[32747] : colorTable[15845];
    } else {
        color = (conditions & DAM_CRIP_ARM_LEFT) ? colorTable[992] : colorTable[1313];
    }

    messageListItemText = getmsg(&editor_message_file, &mesg, 316);
    snprintf(t, sizeof(t), "%s", messageListItemText);
    text_to_buf(win_buf + 640 * y + 194, t, 640, 640, color);

    // Crippled Right Leg
    y += text_height() + 3;

    if (info_line == EDITOR_CRIPPLED_RIGHT_LEG) {
        color = (conditions & DAM_CRIP_LEG_RIGHT) ? colorTable[32747] : colorTable[15845];
    } else {
        color = (conditions & DAM_CRIP_LEG_RIGHT) ? colorTable[992] : colorTable[1313];
    }

    messageListItemText = getmsg(&editor_message_file, &mesg, 317);
    snprintf(t, sizeof(t), "%s", messageListItemText);
    text_to_buf(win_buf + 640 * y + 194, t, 640, 640, color);

    // Crippled Left Leg
    y += text_height() + 3;

    if (info_line == EDITOR_CRIPPLED_LEFT_LEG) {
        color = (conditions & DAM_CRIP_LEG_LEFT) ? colorTable[32747] : colorTable[15845];
    } else {
        color = (conditions & DAM_CRIP_LEG_LEFT) ? colorTable[992] : colorTable[1313];
    }

    messageListItemText = getmsg(&editor_message_file, &mesg, 318);
    snprintf(t, sizeof(t), "%s", messageListItemText);
    text_to_buf(win_buf + 640 * y + 194, t, 640, 640, color);

    y = 179;

    buf_to_buf(bckgnd + 640 * y + 194, 116, 130, 640, win_buf + 640 * y + 194, 640);

    // Armor Class
    if (info_line == EDITOR_FIRST_DERIVED_STAT + EDITOR_DERIVED_STAT_ARMOR_CLASS) {
        color = colorTable[32747];
    } else {
        color = colorTable[992];
    }

    messageListItemText = getmsg(&editor_message_file, &mesg, 302);
    snprintf(t, sizeof(t), "%s", messageListItemText);
    text_to_buf(win_buf + 640 * y + 194, t, 640, 640, color);

    compat_itoa(stat_level(obj_dude, STAT_ARMOR_CLASS), t, 10);
    text_to_buf(win_buf + 640 * y + 288, t, 640, 640, color);

    // Action Points
    y += text_height() + 3;

    if (info_line == EDITOR_FIRST_DERIVED_STAT + EDITOR_DERIVED_STAT_ACTION_POINTS) {
        color = colorTable[32747];
    } else {
        color = colorTable[992];
    }

    messageListItemText = getmsg(&editor_message_file, &mesg, 301);
    snprintf(t, sizeof(t), "%s", messageListItemText);
    text_to_buf(win_buf + 640 * y + 194, t, 640, 640, color);

    compat_itoa(stat_level(obj_dude, STAT_MAXIMUM_ACTION_POINTS), t, 10);
    text_to_buf(win_buf + 640 * y + 288, t, 640, 640, color);

    // Carry Weight
    y += text_height() + 3;

    if (info_line == EDITOR_FIRST_DERIVED_STAT + EDITOR_DERIVED_STAT_CARRY_WEIGHT) {
        color = colorTable[32747];
    } else {
        color = colorTable[992];
    }

    messageListItemText = getmsg(&editor_message_file, &mesg, 311);
    snprintf(t, sizeof(t), "%s", messageListItemText);
    text_to_buf(win_buf + 640 * y + 194, t, 640, 640, color);

    compat_itoa(stat_level(obj_dude, STAT_CARRY_WEIGHT), t, 10);
    text_to_buf(win_buf + 640 * y + 288, t, 640, 640, color);

    // Melee Damage
    y += text_height() + 3;

    if (info_line == EDITOR_FIRST_DERIVED_STAT + EDITOR_DERIVED_STAT_MELEE_DAMAGE) {
        color = colorTable[32747];
    } else {
        color = colorTable[992];
    }

    messageListItemText = getmsg(&editor_message_file, &mesg, 304);
    snprintf(t, sizeof(t), "%s", messageListItemText);
    text_to_buf(win_buf + 640 * y + 194, t, 640, 640, color);

    compat_itoa(stat_level(obj_dude, STAT_MELEE_DAMAGE), t, 10);
    text_to_buf(win_buf + 640 * y + 288, t, 640, 640, color);

    // Damage Resistance
    y += text_height() + 3;

    if (info_line == EDITOR_FIRST_DERIVED_STAT + EDITOR_DERIVED_STAT_DAMAGE_RESISTANCE) {
        color = colorTable[32747];
    } else {
        color = colorTable[992];
    }

    messageListItemText = getmsg(&editor_message_file, &mesg, 305);
    snprintf(t, sizeof(t), "%s", messageListItemText);
    text_to_buf(win_buf + 640 * y + 194, t, 640, 640, color);

    snprintf(t, sizeof(t), "%d%%", stat_level(obj_dude, STAT_DAMAGE_RESISTANCE));
    text_to_buf(win_buf + 640 * y + 288, t, 640, 640, color);

    // Poison Resistance
    y += text_height() + 3;

    if (info_line == EDITOR_FIRST_DERIVED_STAT + EDITOR_DERIVED_STAT_POISON_RESISTANCE) {
        color = colorTable[32747];
    } else {
        color = colorTable[992];
    }

    messageListItemText = getmsg(&editor_message_file, &mesg, 306);
    snprintf(t, sizeof(t), "%s", messageListItemText);
    text_to_buf(win_buf + 640 * y + 194, t, 640, 640, color);

    snprintf(t, sizeof(t), "%d%%", stat_level(obj_dude, STAT_POISON_RESISTANCE));
    text_to_buf(win_buf + 640 * y + 288, t, 640, 640, color);

    // Radiation Resistance
    y += text_height() + 3;

    if (info_line == EDITOR_FIRST_DERIVED_STAT + EDITOR_DERIVED_STAT_RADIATION_RESISTANCE) {
        color = colorTable[32747];
    } else {
        color = colorTable[992];
    }

    messageListItemText = getmsg(&editor_message_file, &mesg, 307);
    snprintf(t, sizeof(t), "%s", messageListItemText);
    text_to_buf(win_buf + 640 * y + 194, t, 640, 640, color);

    snprintf(t, sizeof(t), "%d%%", stat_level(obj_dude, STAT_RADIATION_RESISTANCE));
    text_to_buf(win_buf + 640 * y + 288, t, 640, 640, color);

    // Sequence
    y += text_height() + 3;

    if (info_line == EDITOR_FIRST_DERIVED_STAT + EDITOR_DERIVED_STAT_SEQUENCE) {
        color = colorTable[32747];
    } else {
        color = colorTable[992];
    }

    messageListItemText = getmsg(&editor_message_file, &mesg, 308);
    snprintf(t, sizeof(t), "%s", messageListItemText);
    text_to_buf(win_buf + 640 * y + 194, t, 640, 640, color);

    compat_itoa(stat_level(obj_dude, STAT_SEQUENCE), t, 10);
    text_to_buf(win_buf + 640 * y + 288, t, 640, 640, color);

    // Healing Rate
    y += text_height() + 3;

    if (info_line == EDITOR_FIRST_DERIVED_STAT + EDITOR_DERIVED_STAT_HEALING_RATE) {
        color = colorTable[32747];
    } else {
        color = colorTable[992];
    }

    messageListItemText = getmsg(&editor_message_file, &mesg, 309);
    snprintf(t, sizeof(t), "%s", messageListItemText);
    text_to_buf(win_buf + 640 * y + 194, t, 640, 640, color);

    compat_itoa(stat_level(obj_dude, STAT_HEALING_RATE), t, 10);
    text_to_buf(win_buf + 640 * y + 288, t, 640, 640, color);

    // Critical Chance
    y += text_height() + 3;

    if (info_line == EDITOR_FIRST_DERIVED_STAT + EDITOR_DERIVED_STAT_CRITICAL_CHANCE) {
        color = colorTable[32747];
    } else {
        color = colorTable[992];
    }

    messageListItemText = getmsg(&editor_message_file, &mesg, 310);
    snprintf(t, sizeof(t), "%s", messageListItemText);
    text_to_buf(win_buf + 640 * y + 194, t, 640, 640, color);

    snprintf(t, sizeof(t), "%d%%", stat_level(obj_dude, STAT_CRITICAL_CHANCE));
    text_to_buf(win_buf + 640 * y + 288, t, 640, 640, color);
}

// 0x430184
static void ListSkills(int a1)
{
    int selectedSkill = -1;
    const char* str;
    int i;
    int color;
    int y;
    int value;
    char valueString[32];

    if (info_line >= EDITOR_FIRST_SKILL && info_line < 79) {
        selectedSkill = info_line - EDITOR_FIRST_SKILL;
    }

    if (glblmode == 0 && a1 == 0) {
        win_delete_button(SliderPlusID);
        win_delete_button(SliderNegID);
        SliderNegID = -1;
        SliderPlusID = -1;
    }

    buf_to_buf(bckgnd + 370, 270, 252, 640, win_buf + 370, 640);

    text_font(103);

    // SKILLS
    str = getmsg(&editor_message_file, &mesg, 117);
    text_to_buf(win_buf + 640 * 5 + 380, str, 640, 640, colorTable[18979]);

    if (!glblmode) {
        // SKILL POINTS
        str = getmsg(&editor_message_file, &mesg, 112);
        text_to_buf(win_buf + 640 * 233 + 400, str, 640, 640, colorTable[18979]);

        value = stat_pc_get(PC_STAT_UNSPENT_SKILL_POINTS);
        PrintBigNum(522, 228, 0, value, 0, edit_win);
    } else {
        // TAG SKILLS
        str = getmsg(&editor_message_file, &mesg, 138);
        text_to_buf(win_buf + 640 * 233 + 422, str, 640, 640, colorTable[18979]);

        if (a1 == 2 && !first_skill_list) {
            PrintBigNum(522, 228, ANIMATE, tagskill_count, old_tags, edit_win);
        } else {
            PrintBigNum(522, 228, 0, tagskill_count, 0, edit_win);
            first_skill_list = 0;
        }
    }

    skill_set_tags(temp_tag_skill, NUM_TAGGED_SKILLS);

    text_font(101);

    y = 27;
    for (i = 0; i < SKILL_COUNT; i++) {
        if (i == selectedSkill) {
            if (i != temp_tag_skill[0] && i != temp_tag_skill[1] && i != temp_tag_skill[2] && i != temp_tag_skill[3]) {
                color = colorTable[32747];
            } else {
                color = colorTable[32767];
            }
        } else {
            if (i != temp_tag_skill[0] && i != temp_tag_skill[1] && i != temp_tag_skill[2] && i != temp_tag_skill[3]) {
                color = colorTable[992];
            } else {
                color = colorTable[21140];
            }
        }

        str = skill_name(i);
        text_to_buf(win_buf + 640 * y + 380, str, 640, 640, color);

        value = skill_level(obj_dude, i);
        snprintf(valueString, sizeof(valueString), "%d%%", value);

        text_to_buf(win_buf + 640 * y + 573, valueString, 640, 640, color);

        y += text_height() + 1;
    }

    if (!glblmode) {
        y = skill_cursor * (text_height() + 1);
        slider_y = y + 27;

        trans_buf_to_buf(
            grphbmp[EDITOR_GRAPHIC_SLIDER],
            GInfo[EDITOR_GRAPHIC_SLIDER].width,
            GInfo[EDITOR_GRAPHIC_SLIDER].height,
            GInfo[EDITOR_GRAPHIC_SLIDER].width,
            win_buf + 640 * (y + 16) + 592,
            640);

        if (a1 == 0) {
            if (SliderPlusID == -1) {
                SliderPlusID = win_register_button(
                    edit_win,
                    614,
                    slider_y - 7,
                    GInfo[EDITOR_GRAPHIC_SLIDER_PLUS_ON].width,
                    GInfo[EDITOR_GRAPHIC_SLIDER_PLUS_ON].height,
                    -1,
                    522,
                    521,
                    522,
                    grphbmp[EDITOR_GRAPHIC_SLIDER_PLUS_OFF],
                    grphbmp[EDITOR_GRAPHIC_SLIDER_PLUS_ON],
                    NULL,
                    96);
                win_register_button_sound_func(SliderPlusID, gsound_red_butt_press, NULL);
            }

            if (SliderNegID == -1) {
                SliderNegID = win_register_button(
                    edit_win,
                    614,
                    slider_y + 4 - 12 + GInfo[EDITOR_GRAPHIC_SLIDER_MINUS_ON].height,
                    GInfo[EDITOR_GRAPHIC_SLIDER_MINUS_ON].width,
                    GInfo[EDITOR_GRAPHIC_SLIDER_MINUS_OFF].height,
                    -1,
                    524,
                    523,
                    524,
                    grphbmp[EDITOR_GRAPHIC_SLIDER_MINUS_OFF],
                    grphbmp[EDITOR_GRAPHIC_SLIDER_MINUS_ON],
                    NULL,
                    96);
                win_register_button_sound_func(SliderNegID, gsound_red_butt_press, NULL);
            }
        }
    }
}

// 0x4305DC
static void DrawInfoWin()
{
    int graphicId;
    char* title;
    char* description;
    char buffer[128];

    if (info_line < 0 || info_line >= 98) {
        return;
    }

    buf_to_buf(bckgnd + (640 * 267) + 345, 277, 170, 640, win_buf + (267 * 640) + 345, 640);

    if (info_line >= 0 && info_line < 7) {
        description = stat_description(info_line);
        title = stat_name(info_line);
        graphicId = stat_picture(info_line);
        DrawCard(graphicId, title, NULL, description);
    } else if (info_line >= 7 && info_line < 10) {
        if (glblmode) {
            switch (info_line) {
            case 7:
                // Character Points
                description = getmsg(&editor_message_file, &mesg, 121);
                title = getmsg(&editor_message_file, &mesg, 120);
                DrawCard(7, title, NULL, description);
                break;
            }
        } else {
            switch (info_line) {
            case 7:
                description = stat_pc_description(PC_STAT_LEVEL);
                title = stat_pc_name(PC_STAT_LEVEL);
                DrawCard(7, title, NULL, description);
                break;
            case 8:
                description = stat_pc_description(PC_STAT_EXPERIENCE);
                title = stat_pc_name(PC_STAT_EXPERIENCE);
                DrawCard(8, title, NULL, description);
                break;
            case 9:
                // Next Level
                description = getmsg(&editor_message_file, &mesg, 123);
                title = getmsg(&editor_message_file, &mesg, 122);
                DrawCard(9, title, NULL, description);
                break;
            }
        }
    } else if (info_line >= 10 && info_line < 43) {
        trait_count = get_trait_count();

        switch (folder) {
        case EDITOR_FOLDER_PERKS:
            if (PerkCount() != 0 && info_line - 10 < PerkCount()) {
                int perk = XltPerk(info_line - 10);
                if (perk != -1) {
                    title = perk_name(perk);
                    description = perk_description(perk);
                    graphicId = perk + 72;
                    DrawCard(graphicId, title, NULL, description);
                }
            } else if (info_line - 10 >= 7 && info_line - 10 < 11) {
                if (trait_count < 2 && info_line - 10 >= 8 && info_line - 10 <= 9) {
                    title = trait_name(temp_trait[info_line - 10 - 8]);
                    description = trait_description(temp_trait[info_line - 10 - 8]);
                    graphicId = trait_pic(temp_trait[info_line - 10 - 8]);
                    DrawCard(graphicId, title, NULL, description);
                } else {
                    title = getmsg(&editor_message_file, &mesg, 146);
                    description = getmsg(&editor_message_file, &mesg, 147);
                    DrawCard(54, title, NULL, description);
                }
            } else {
                title = getmsg(&editor_message_file, &mesg, 124);
                description = getmsg(&editor_message_file, &mesg, 127);
                DrawCard(71, title, NULL, description);
            }
            break;
        case EDITOR_FOLDER_KARMA:
            if (info_line - 10 < karma_count) {
                int karma = info_line - 10;
                if (karma > 0) {
                    karma = XlateKarma(info_line - 11) + 1;
                }

                graphicId = karma_pic_table[karma];
                title = getmsg(&editor_message_file, &mesg, 1000 + karma);
                description = getmsg(&editor_message_file, &mesg, 1100 + karma);
                DrawCard(graphicId, title, NULL, description);
            } else {
                graphicId = 47;
                title = getmsg(&editor_message_file, &mesg, 125);
                description = getmsg(&editor_message_file, &mesg, 128);
                DrawCard(graphicId, title, NULL, description);
            }
            break;
        case EDITOR_FOLDER_KILLS:
            if (info_line - 10 < kills_count) {
                DrawFolder();
                snprintf(buffer, sizeof(buffer), "%s %s",
                    name_sort_list[info_line - 10].name,
                    getmsg(&editor_message_file, &mesg, 126));

                graphicId = 46;
                title = buffer;
                description = critter_kill_info(name_sort_list[info_line - 10].value);
                DrawCard(graphicId, title, NULL, description);
            } else {
                graphicId = 46;
                title = getmsg(&editor_message_file, &mesg, 126);
                description = getmsg(&editor_message_file, &mesg, 129);
                DrawCard(graphicId, title, NULL, description);
            }
            break;
        }
    } else if (info_line >= 82 && info_line < 98) {
        graphicId = trait_pic(info_line - 82);
        title = trait_name(info_line - 82);
        description = trait_description(info_line - 82);
        DrawCard(graphicId, title, NULL, description);
    } else if (info_line >= 43 && info_line < 51) {
        switch (info_line) {
        case EDITOR_HIT_POINTS:
            description = stat_description(STAT_MAXIMUM_HIT_POINTS);
            title = getmsg(&editor_message_file, &mesg, 300);
            graphicId = stat_picture(STAT_MAXIMUM_HIT_POINTS);
            DrawCard(graphicId, title, NULL, description);
            break;
        case EDITOR_POISONED:
            description = getmsg(&editor_message_file, &mesg, 400);
            title = getmsg(&editor_message_file, &mesg, 312);
            DrawCard(11, title, NULL, description);
            break;
        case EDITOR_RADIATED:
            description = getmsg(&editor_message_file, &mesg, 401);
            title = getmsg(&editor_message_file, &mesg, 313);
            DrawCard(12, title, NULL, description);
            break;
        case EDITOR_EYE_DAMAGE:
            description = getmsg(&editor_message_file, &mesg, 402);
            title = getmsg(&editor_message_file, &mesg, 314);
            DrawCard(13, title, NULL, description);
            break;
        case EDITOR_CRIPPLED_RIGHT_ARM:
            description = getmsg(&editor_message_file, &mesg, 403);
            title = getmsg(&editor_message_file, &mesg, 315);
            DrawCard(14, title, NULL, description);
            break;
        case EDITOR_CRIPPLED_LEFT_ARM:
            description = getmsg(&editor_message_file, &mesg, 404);
            title = getmsg(&editor_message_file, &mesg, 316);
            DrawCard(15, title, NULL, description);
            break;
        case EDITOR_CRIPPLED_RIGHT_LEG:
            description = getmsg(&editor_message_file, &mesg, 405);
            title = getmsg(&editor_message_file, &mesg, 317);
            DrawCard(16, title, NULL, description);
            break;
        case EDITOR_CRIPPLED_LEFT_LEG:
            description = getmsg(&editor_message_file, &mesg, 406);
            title = getmsg(&editor_message_file, &mesg, 318);
            DrawCard(17, title, NULL, description);
            break;
        }
    } else if (info_line >= EDITOR_FIRST_DERIVED_STAT && info_line < 61) {
        int derivedStatIndex = info_line - 51;
        int stat = ndinfoxlt[derivedStatIndex];
        description = stat_description(stat);
        title = stat_name(stat);
        graphicId = ndrvd[derivedStatIndex];
        DrawCard(graphicId, title, NULL, description);
    } else if (info_line >= EDITOR_FIRST_SKILL && info_line < 79) {
        int skill = info_line - 61;
        const char* attributesDescription = skill_attribute(skill);

        char formatted[150]; // TODO: Size is probably wrong.
        const char* base = getmsg(&editor_message_file, &mesg, 137);
        int defaultValue = skill_base(skill);
        snprintf(formatted, sizeof(formatted), "%s %d%% %s", base, defaultValue, attributesDescription);

        graphicId = skill_pic(skill);
        title = skill_name(skill);
        description = skill_description(skill);
        DrawCard(graphicId, title, formatted, description);
    } else if (info_line >= 79 && info_line < 82) {
        switch (info_line) {
        case EDITOR_TAG_SKILL:
            if (glblmode) {
                // Tag Skill
                description = getmsg(&editor_message_file, &mesg, 145);
                title = getmsg(&editor_message_file, &mesg, 144);
                DrawCard(27, title, NULL, description);
            } else {
                // Skill Points
                description = getmsg(&editor_message_file, &mesg, 131);
                title = getmsg(&editor_message_file, &mesg, 130);
                DrawCard(27, title, NULL, description);
            }
            break;
        case EDITOR_SKILLS:
            // Skills
            description = getmsg(&editor_message_file, &mesg, 151);
            title = getmsg(&editor_message_file, &mesg, 150);
            DrawCard(27, title, NULL, description);
            break;
        case EDITOR_OPTIONAL_TRAITS:
            // Optional Traits
            description = getmsg(&editor_message_file, &mesg, 147);
            title = getmsg(&editor_message_file, &mesg, 146);
            DrawCard(27, title, NULL, description);
            break;
        }
    }
}

// 0x430F20
static int NameWindow()
{
    char* text;

    int windowWidth = GInfo[EDITOR_GRAPHIC_CHARWIN].width;
    int windowHeight = GInfo[EDITOR_GRAPHIC_CHARWIN].height;

    int nameWindowX = (screenGetWidth() - EDITOR_WINDOW_WIDTH) / 2 + 17;
    int nameWindowY = (screenGetHeight() - EDITOR_WINDOW_HEIGHT) / 2;
    int win = win_add(nameWindowX, nameWindowY, windowWidth, windowHeight, 256, WINDOW_MODAL | WINDOW_DONT_MOVE_TOP);
    if (win == -1) {
        return -1;
    }

    unsigned char* windowBuf = win_get_buf(win);

    // Copy background
    memcpy(windowBuf, grphbmp[EDITOR_GRAPHIC_CHARWIN], windowWidth * windowHeight);

    trans_buf_to_buf(
        grphbmp[EDITOR_GRAPHIC_NAME_BOX],
        GInfo[EDITOR_GRAPHIC_NAME_BOX].width,
        GInfo[EDITOR_GRAPHIC_NAME_BOX].height,
        GInfo[EDITOR_GRAPHIC_NAME_BOX].width,
        windowBuf + windowWidth * 13 + 13,
        windowWidth);
    trans_buf_to_buf(grphbmp[EDITOR_GRAPHIC_DONE_BOX],
        GInfo[EDITOR_GRAPHIC_DONE_BOX].width,
        GInfo[EDITOR_GRAPHIC_DONE_BOX].height,
        GInfo[EDITOR_GRAPHIC_DONE_BOX].width,
        windowBuf + windowWidth * 40 + 13,
        windowWidth);

    text_font(103);

    text = getmsg(&editor_message_file, &mesg, 100);
    text_to_buf(windowBuf + windowWidth * 44 + 50, text, windowWidth, windowWidth, colorTable[18979]);

    int doneBtn = win_register_button(win,
        26,
        44,
        GInfo[EDITOR_GRAPHIC_LITTLE_RED_BUTTON_UP].width,
        GInfo[EDITOR_GRAPHIC_LITTLE_RED_BUTTON_UP].height,
        -1,
        -1,
        -1,
        500,
        grphbmp[EDITOR_GRAPHIC_LITTLE_RED_BUTTON_UP],
        grphbmp[EDITOR_GRAPHIC_LILTTLE_RED_BUTTON_DOWN],
        NULL,
        BUTTON_FLAG_TRANSPARENT);
    if (doneBtn != -1) {
        win_register_button_sound_func(doneBtn, gsound_red_butt_press, gsound_red_butt_release);
    }

    win_draw(win);

    text_font(101);

    char name[64];
    strcpy(name, critter_name(obj_dude));

    if (strcmp(name, "None") == 0) {
        name[0] = '\0';
    }

    // NOTE: I don't understand the nameCopy, not sure what it is used for. It's
    // definitely there, but I just don' get it.
    char nameCopy[64];
    strcpy(nameCopy, name);

    if (get_input_str(win, 500, nameCopy, 11, 23, 19, colorTable[992], 100, 0) != -1) {
        if (nameCopy[0] != '\0') {
            critter_pc_set_name(nameCopy);
            PrintBigname();
            win_delete(win);
            return 0;
        }
    }

    // NOTE: original code is a bit different, the following chunk of code written two times.

    text_font(101);
    buf_to_buf(grphbmp[EDITOR_GRAPHIC_NAME_BOX],
        GInfo[EDITOR_GRAPHIC_NAME_BOX].width,
        GInfo[EDITOR_GRAPHIC_NAME_BOX].height,
        GInfo[EDITOR_GRAPHIC_NAME_BOX].width,
        windowBuf + GInfo[EDITOR_GRAPHIC_CHARWIN].width * 13 + 13,
        GInfo[EDITOR_GRAPHIC_CHARWIN].width);

    PrintName(windowBuf, GInfo[EDITOR_GRAPHIC_CHARWIN].width);

    strcpy(nameCopy, name);

    win_delete(win);

    return 0;
}

// 0x431244
static void PrintName(unsigned char* buf, int pitch)
{
    char str[64];
    char* v4;

    memcpy(str, byte_431D93, 64);

    text_font(101);

    v4 = critter_name(obj_dude);

    // TODO: Check.
    strcpy(str, v4);

    text_to_buf(buf + 19 * pitch + 21, str, pitch, pitch, colorTable[992]);
}

// 0x4312C0
static int AgeWindow()
{
    int win;
    unsigned char* windowBuf;
    int windowWidth;
    int windowHeight;
    const char* messageListItemText;
    int previousAge;
    int age;
    int doneBtn;
    int prevBtn;
    int nextBtn;
    int keyCode;
    int change;
    int flags;

    int savedAge = stat_level(obj_dude, STAT_AGE);

    windowWidth = GInfo[EDITOR_GRAPHIC_CHARWIN].width;
    windowHeight = GInfo[EDITOR_GRAPHIC_CHARWIN].height;

    int ageWindowX = (screenGetWidth() - EDITOR_WINDOW_WIDTH) / 2 + GInfo[EDITOR_GRAPHIC_NAME_ON].width + 9;
    int ageWindowY = (screenGetHeight() - EDITOR_WINDOW_HEIGHT) / 2;
    win = win_add(ageWindowX,
        ageWindowY,
        windowWidth,
        windowHeight,
        256,
        WINDOW_MODAL | WINDOW_DONT_MOVE_TOP);
    if (win == -1) {
        return -1;
    }

    windowBuf = win_get_buf(win);

    memcpy(windowBuf, grphbmp[EDITOR_GRAPHIC_CHARWIN], windowWidth * windowHeight);

    trans_buf_to_buf(
        grphbmp[EDITOR_GRAPHIC_AGE_BOX],
        GInfo[EDITOR_GRAPHIC_AGE_BOX].width,
        GInfo[EDITOR_GRAPHIC_AGE_BOX].height,
        GInfo[EDITOR_GRAPHIC_AGE_BOX].width,
        windowBuf + windowWidth * 7 + 8,
        windowWidth);
    trans_buf_to_buf(
        grphbmp[EDITOR_GRAPHIC_DONE_BOX],
        GInfo[EDITOR_GRAPHIC_DONE_BOX].width,
        GInfo[EDITOR_GRAPHIC_DONE_BOX].height,
        GInfo[EDITOR_GRAPHIC_DONE_BOX].width,
        windowBuf + windowWidth * 40 + 13,
        GInfo[EDITOR_GRAPHIC_CHARWIN].width);

    text_font(103);

    messageListItemText = getmsg(&editor_message_file, &mesg, 100);
    text_to_buf(windowBuf + windowWidth * 44 + 50, messageListItemText, windowWidth, windowWidth, colorTable[18979]);

    age = stat_level(obj_dude, STAT_AGE);
    PrintBigNum(55, 10, 0, age, 0, win);

    doneBtn = win_register_button(win,
        26,
        44,
        GInfo[EDITOR_GRAPHIC_LITTLE_RED_BUTTON_UP].width,
        GInfo[EDITOR_GRAPHIC_LITTLE_RED_BUTTON_UP].height,
        -1,
        -1,
        -1,
        500,
        grphbmp[EDITOR_GRAPHIC_LITTLE_RED_BUTTON_UP],
        grphbmp[EDITOR_GRAPHIC_LILTTLE_RED_BUTTON_DOWN],
        NULL,
        BUTTON_FLAG_TRANSPARENT);
    if (doneBtn != -1) {
        win_register_button_sound_func(doneBtn, gsound_red_butt_press, gsound_red_butt_release);
    }

    nextBtn = win_register_button(win,
        105,
        13,
        GInfo[EDITOR_GRAPHIC_LEFT_ARROW_DOWN].width,
        GInfo[EDITOR_GRAPHIC_LEFT_ARROW_DOWN].height,
        -1,
        503,
        501,
        503,
        grphbmp[EDITOR_GRAPHIC_RIGHT_ARROW_UP],
        grphbmp[EDITOR_GRAPHIC_RIGHT_ARROW_DOWN],
        NULL,
        BUTTON_FLAG_TRANSPARENT);
    if (nextBtn != -1) {
        win_register_button_sound_func(nextBtn, gsound_med_butt_press, NULL);
    }

    prevBtn = win_register_button(win,
        19,
        13,
        GInfo[EDITOR_GRAPHIC_RIGHT_ARROW_DOWN].width,
        GInfo[EDITOR_GRAPHIC_RIGHT_ARROW_DOWN].height,
        -1,
        504,
        502,
        504,
        grphbmp[EDITOR_GRAPHIC_LEFT_ARROW_UP],
        grphbmp[EDITOR_GRAPHIC_LEFT_ARROW_DOWN],
        NULL,
        BUTTON_FLAG_TRANSPARENT);
    if (prevBtn != -1) {
        win_register_button_sound_func(prevBtn, gsound_med_butt_press, NULL);
    }

    while (true) {
        sharedFpsLimiter.mark();

        frame_time = get_time();
        change = 0;
        int v32 = 0;

        keyCode = get_input();

        convertMouseWheelToArrowKey(&keyCode);

        if (keyCode == KEY_RETURN || keyCode == 500) {
            if (keyCode != 500) {
                gsound_play_sfx_file("ib1p1xx1");
            }

            win_delete(win);
            return 0;
        } else if (keyCode == KEY_ESCAPE || game_user_wants_to_quit != 0) {
            break;
        } else if (keyCode == 501) {
            change = 1;
        } else if (keyCode == 502) {
            change = -1;
        } else if (keyCode == KEY_PLUS || keyCode == KEY_UPPERCASE_N || keyCode == KEY_ARROW_RIGHT) {
            previousAge = stat_level(obj_dude, STAT_AGE);
            flags = inc_stat(obj_dude, STAT_AGE) >= 0;
            age = stat_level(obj_dude, STAT_AGE);
            PrintBigNum(55, 10, flags, age, previousAge, win);

            if (flags == ANIMATE) {
                PrintAgeBig();
                PrintBasicStat(RENDER_ALL_STATS, 0, 0);
                ListDrvdStats();
                win_draw(edit_win);
                win_draw(win);
            }
        } else if (keyCode == KEY_MINUS || keyCode == KEY_UPPERCASE_J || keyCode == KEY_ARROW_LEFT) {
            previousAge = stat_level(obj_dude, STAT_AGE);
            flags = dec_stat(obj_dude, STAT_AGE) >= 0;
            age = stat_level(obj_dude, STAT_AGE);

            PrintBigNum(55, 10, flags, age, previousAge, win);

            if (flags == ANIMATE) {
                PrintAgeBig();
                PrintBasicStat(RENDER_ALL_STATS, 0, 0);
                ListDrvdStats();
                win_draw(edit_win);
                win_draw(win);
            }
        }

        if (change != 0) {
            int v33 = 0;

            repFtime = 4;

            while (true) {
                sharedFpsLimiter.mark();

                frame_time = get_time();

                v33++;

                if ((!v32 && v33 == 1) || (v32 && v33 > 14.4)) {
                    v32 = true;

                    if (v33 > 14.4) {
                        repFtime++;
                        if (repFtime > 24) {
                            repFtime = 24;
                        }
                    }

                    flags = ANIMATE;
                    previousAge = stat_level(obj_dude, STAT_AGE);

                    if (change == 1) {
                        if (inc_stat(obj_dude, STAT_AGE) < 0) {
                            flags = 0;
                        }
                    } else {
                        if (dec_stat(obj_dude, STAT_AGE) < 0) {
                            flags = 0;
                        }
                    }

                    age = stat_level(obj_dude, STAT_AGE);
                    PrintBigNum(55, 10, flags, age, previousAge, win);
                    if (flags == ANIMATE) {
                        PrintAgeBig();
                        PrintBasicStat(RENDER_ALL_STATS, 0, 0);
                        ListDrvdStats();
                        win_draw(edit_win);
                        win_draw(win);
                    }
                }

                if (v33 > 14.4) {
                    while (elapsed_time(frame_time) < 1000 / repFtime) {
                    }
                } else {
                    while (elapsed_time(frame_time) < 1000 / 24) {
                    }
                }

                keyCode = get_input();
                if (keyCode == 503 || keyCode == 504 || game_user_wants_to_quit != 0) {
                    break;
                }

                renderPresent();
                sharedFpsLimiter.throttle();
            }
        } else {
            win_draw(win);

            while (elapsed_time(frame_time) < 1000 / 24) {
            }

            renderPresent();
            sharedFpsLimiter.throttle();
        }
    }

    stat_set_base(obj_dude, STAT_AGE, savedAge);
    PrintAgeBig();
    PrintBasicStat(RENDER_ALL_STATS, 0, 0);
    ListDrvdStats();
    win_draw(edit_win);
    win_draw(win);
    win_delete(win);
    return 0;
}

// 0x431838
static void SexWindow()
{
    char* text;

    int windowWidth = GInfo[EDITOR_GRAPHIC_CHARWIN].width;
    int windowHeight = GInfo[EDITOR_GRAPHIC_CHARWIN].height;

    int genderWindowX = (screenGetWidth() - EDITOR_WINDOW_WIDTH) / 2 + 9
        + GInfo[EDITOR_GRAPHIC_NAME_ON].width
        + GInfo[EDITOR_GRAPHIC_AGE_ON].width;
    int genderWindowY = (screenGetHeight() - EDITOR_WINDOW_HEIGHT) / 2;
    int win = win_add(genderWindowX, genderWindowY, windowWidth, windowHeight, 256, WINDOW_MODAL | WINDOW_DONT_MOVE_TOP);

    if (win == -1) {
        return;
    }

    unsigned char* windowBuf = win_get_buf(win);

    // Copy background
    memcpy(windowBuf, grphbmp[EDITOR_GRAPHIC_CHARWIN], windowWidth * windowHeight);

    trans_buf_to_buf(grphbmp[EDITOR_GRAPHIC_DONE_BOX],
        GInfo[EDITOR_GRAPHIC_DONE_BOX].width,
        GInfo[EDITOR_GRAPHIC_DONE_BOX].height,
        GInfo[EDITOR_GRAPHIC_DONE_BOX].width,
        windowBuf + windowWidth * 44 + 15,
        windowWidth);

    text_font(103);

    text = getmsg(&editor_message_file, &mesg, 100);
    text_to_buf(windowBuf + windowWidth * 48 + 52, text, windowWidth, windowWidth, colorTable[18979]);

    int doneBtn = win_register_button(win,
        28,
        48,
        GInfo[EDITOR_GRAPHIC_LITTLE_RED_BUTTON_UP].width,
        GInfo[EDITOR_GRAPHIC_LITTLE_RED_BUTTON_UP].height,
        -1,
        -1,
        -1,
        500,
        grphbmp[EDITOR_GRAPHIC_LITTLE_RED_BUTTON_UP],
        grphbmp[EDITOR_GRAPHIC_LILTTLE_RED_BUTTON_DOWN],
        NULL,
        BUTTON_FLAG_TRANSPARENT);
    if (doneBtn != -1) {
        win_register_button_sound_func(doneBtn, gsound_red_butt_press, gsound_red_butt_release);
    }

    int btns[2];
    btns[0] = win_register_button(win,
        22,
        2,
        GInfo[EDITOR_GRAPHIC_MALE_ON].width,
        GInfo[EDITOR_GRAPHIC_MALE_ON].height,
        -1,
        -1,
        501,
        -1,
        grphbmp[EDITOR_GRAPHIC_MALE_OFF],
        grphbmp[EDITOR_GRAPHIC_MALE_ON],
        NULL,
        BUTTON_FLAG_TRANSPARENT | BUTTON_FLAG_0x04 | BUTTON_FLAG_0x02 | BUTTON_FLAG_0x01);
    if (btns[0] != -1) {
        win_register_button_sound_func(doneBtn, gsound_red_butt_press, NULL);
    }

    btns[1] = win_register_button(win,
        71,
        3,
        GInfo[EDITOR_GRAPHIC_FEMALE_ON].width,
        GInfo[EDITOR_GRAPHIC_FEMALE_ON].height,
        -1,
        -1,
        502,
        -1,
        grphbmp[EDITOR_GRAPHIC_FEMALE_OFF],
        grphbmp[EDITOR_GRAPHIC_FEMALE_ON],
        NULL,
        BUTTON_FLAG_TRANSPARENT | BUTTON_FLAG_0x04 | BUTTON_FLAG_0x02 | BUTTON_FLAG_0x01);
    if (btns[1] != -1) {
        win_group_radio_buttons(2, btns);
        win_register_button_sound_func(doneBtn, gsound_red_butt_press, NULL);
    }

    int savedGender = stat_level(obj_dude, STAT_GENDER);
    win_set_button_rest_state(btns[savedGender], 1, 0);

    while (true) {
        sharedFpsLimiter.mark();

        frame_time = get_time();

        int eventCode = get_input();

        if (eventCode == 500) {
            break;
        }

        if (eventCode == KEY_RETURN) {
            gsound_play_sfx_file("ib1p1xx1");
            break;
        }

        if (eventCode == KEY_ESCAPE || game_user_wants_to_quit != 0) {
            break;
        }

        switch (eventCode) {
        case 501:
        case 502:
            // TODO: Original code is slightly different.
            stat_set_base(obj_dude, STAT_GENDER, eventCode - 501);
            PrintBasicStat(RENDER_ALL_STATS, 0, 0);
            ListDrvdStats();
            break;
        }

        win_draw(win);

        while (elapsed_time(frame_time) < 41) {
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    PrintGender();
    win_delete(win);
}

// 0x431B08
static void StatButton(int eventCode)
{
    repFtime = 4;

    int savedRemainingCharacterPoints = character_points;

    if (!glblmode) {
        return;
    }

    int incrementingStat = eventCode - 503;
    int decrementingStat = eventCode - 510;

    int v11 = 0;

    bool cont = true;
    do {
        sharedFpsLimiter.mark();

        frame_time = get_time();
        if (v11 <= 19.2) {
            v11++;
        }

        if (v11 == 1 || v11 > 19.2) {
            if (v11 > 19.2) {
                repFtime++;
                if (repFtime > 24) {
                    repFtime = 24;
                }
            }

            if (eventCode >= 510) {
                int previousValue = stat_level(obj_dude, decrementingStat);
                if (dec_stat(obj_dude, decrementingStat) == 0) {
                    character_points++;
                } else {
                    cont = false;
                }

                PrintBasicStat(decrementingStat, cont ? ANIMATE : 0, previousValue);
                PrintBigNum(126, 282, cont ? ANIMATE : 0, character_points, savedRemainingCharacterPoints, edit_win);
                stat_recalc_derived(obj_dude);
                ListDrvdStats();
                ListSkills(0);
                info_line = decrementingStat;
            } else {
                int previousValue = stat_get_base(obj_dude, incrementingStat);
                previousValue += stat_get_bonus(obj_dude, incrementingStat);
                if (character_points > 0 && previousValue < 10 && inc_stat(obj_dude, incrementingStat) == 0) {
                    character_points--;
                } else {
                    cont = false;
                }

                PrintBasicStat(incrementingStat, cont ? ANIMATE : 0, previousValue);
                PrintBigNum(126, 282, cont ? ANIMATE : 0, character_points, savedRemainingCharacterPoints, edit_win);
                stat_recalc_derived(obj_dude);
                ListDrvdStats();
                ListSkills(0);
                info_line = incrementingStat;
            }

            win_draw(edit_win);
        }

        if (v11 >= 19.2) {
            unsigned int delay = 1000 / repFtime;
            while (elapsed_time(frame_time) < delay) {
            }
        } else {
            while (elapsed_time(frame_time) < 1000 / 24) {
            }
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    } while (get_input() != 518 && cont);

    DrawInfoWin();
}

// handle options dialog
//
// 0x431D44
static int OptionWindow()
{
    int width = GInfo[43].width;
    int height = GInfo[43].height;

    // NOTE: The following is a block of general purpose string buffers used in
    // this function. They are either store path, or strings from .msg files. I
    // don't know if such usage was intentional in the original code or it's a
    // result of some kind of compiler optimization.
    char string1[512];
    char string2[512];
    char string3[512];
    char string4[512];
    char string5[512];

    // Only two of the these blocks are used as a dialog body. Depending on the
    // dialog either 1 or 2 strings used from this array.
    const char* dialogBody[2] = {
        string5,
        string2,
    };

    if (glblmode) {
        int optionsWindowX = (screenGetWidth() != 640)
            ? (screenGetWidth() - GInfo[41].width) / 2
            : 238;
        int optionsWindowY = (screenGetHeight() != 480)
            ? (screenGetHeight() - GInfo[41].height) / 2
            : 90;
        int win = win_add(optionsWindowX, optionsWindowY, GInfo[41].width, GInfo[41].height, 256, WINDOW_MODAL | WINDOW_DONT_MOVE_TOP);
        if (win == -1) {
            return -1;
        }

        unsigned char* windowBuffer = win_get_buf(win);
        memcpy(windowBuffer, grphbmp[41], GInfo[41].width * GInfo[41].height);

        text_font(103);

        int err = 0;
        unsigned char* down[5];
        unsigned char* up[5];
        int size = width * height;
        int y = 17;
        int index;

        for (index = 0; index < 5; index++) {
            if (err != 0) {
                break;
            }

            do {
                down[index] = (unsigned char*)mem_malloc(size);
                if (down[index] == NULL) {
                    err = 1;
                    break;
                }

                up[index] = (unsigned char*)mem_malloc(size);
                if (up[index] == NULL) {
                    err = 2;
                    break;
                }

                memcpy(down[index], grphbmp[43], size);
                memcpy(up[index], grphbmp[42], size);

                strcpy(string4, getmsg(&editor_message_file, &mesg, 600 + index));

                int offset = width * 7 + width / 2 - text_width(string4) / 2;
                text_to_buf(up[index] + offset, string4, width, width, colorTable[18979]);
                text_to_buf(down[index] + offset, string4, width, width, colorTable[14723]);

                int btn = win_register_button(win, 13, y, width, height, -1, -1, -1, 500 + index, up[index], down[index], NULL, BUTTON_FLAG_TRANSPARENT);
                if (btn != -1) {
                    win_register_button_sound_func(btn, gsound_lrg_butt_press, NULL);
                }
            } while (0);

            y += height + 3;
        }

        if (err != 0) {
            if (err == 2) {
                mem_free(down[index]);
            }

            while (--index >= 0) {
                mem_free(up[index]);
                mem_free(down[index]);
            }

            return -1;
        }

        text_font(101);

        int rc = 0;
        while (rc == 0) {
            sharedFpsLimiter.mark();

            int keyCode = get_input();

            if (game_user_wants_to_quit != 0) {
                rc = 2;
            } else if (keyCode == 504) {
                rc = 2;
            } else if (keyCode == KEY_RETURN || keyCode == KEY_UPPERCASE_D || keyCode == KEY_LOWERCASE_D) {
                // DONE
                rc = 2;
                gsound_play_sfx_file("ib1p1xx1");
            } else if (keyCode == KEY_ESCAPE) {
                rc = 2;
            } else if (keyCode == 503 || keyCode == KEY_UPPERCASE_E || keyCode == KEY_LOWERCASE_E) {
                // ERASE
                strcpy(string5, getmsg(&editor_message_file, &mesg, 605));
                strcpy(string2, getmsg(&editor_message_file, &mesg, 606));

                if (dialog_out(NULL, dialogBody, 2, 169, 126, colorTable[992], NULL, colorTable[992], DIALOG_BOX_YES_NO) != 0) {
                    ResetPlayer();
                    skill_get_tags(temp_tag_skill, NUM_TAGGED_SKILLS);

                    // NOTE: Uninline.
                    tagskill_count = tagskl_free();

                    trait_get(&temp_trait[0], &temp_trait[1]);

                    // NOTE: Uninline.
                    trait_count = get_trait_count();
                    stat_recalc_derived(obj_dude);
                    ResetScreen();
                }
            } else if (keyCode == 502 || keyCode == KEY_UPPERCASE_P || keyCode == KEY_LOWERCASE_P) {
                // PRINT TO FILE
                string4[0] = '\0';

                strcat(string4, "*.");
                strcat(string4, "TXT");

                char** fileList;
                int fileListLength = db_get_file_list(string4, &fileList, NULL, 0);
                if (fileListLength != -1) {
                    // PRINT
                    strcpy(string1, getmsg(&editor_message_file, &mesg, 616));

                    // PRINT TO FILE
                    strcpy(string4, getmsg(&editor_message_file, &mesg, 602));

                    if (save_file_dialog(string4, fileList, string1, fileListLength, 168, 80, 0) == 0) {
                        strcat(string1, ".");
                        strcat(string1, "TXT");

                        string4[0] = '\0';
                        strcat(string4, string1);

                        if (db_access(string4)) {
                            // already exists
                            snprintf(string4, sizeof(string4),
                                "%s %s",
                                compat_strupr(string1),
                                getmsg(&editor_message_file, &mesg, 609));

                            strcpy(string5, getmsg(&editor_message_file, &mesg, 610));

                            if (dialog_out(string4, dialogBody, 1, 169, 126, colorTable[32328], NULL, colorTable[32328], 0x10) != 0) {
                                rc = 1;
                            } else {
                                rc = 0;
                            }
                        } else {
                            rc = 1;
                        }

                        if (rc != 0) {
                            string4[0] = '\0';
                            strcat(string4, string1);

                            if (Save_as_ASCII(string4) == 0) {
                                snprintf(string4, sizeof(string4),
                                    "%s%s",
                                    compat_strupr(string1),
                                    getmsg(&editor_message_file, &mesg, 607));
                                dialog_out(string4, NULL, 0, 169, 126, colorTable[992], NULL, colorTable[992], 0);
                            } else {
                                gsound_play_sfx_file("iisxxxx1");

                                snprintf(string4, sizeof(string4),
                                    "%s%s%s",
                                    getmsg(&editor_message_file, &mesg, 611),
                                    compat_strupr(string1),
                                    "!");
                                dialog_out(string4, NULL, 0, 169, 126, colorTable[32328], NULL, colorTable[992], 0x01);
                            }
                        }
                    }

                    db_free_file_list(&fileList, NULL);
                } else {
                    gsound_play_sfx_file("iisxxxx1");

                    strcpy(string4, getmsg(&editor_message_file, &mesg, 615));
                    dialog_out(string4, NULL, 0, 169, 126, colorTable[32328], NULL, colorTable[32328], 0);

                    rc = 0;
                }
            } else if (keyCode == 501 || keyCode == KEY_UPPERCASE_L || keyCode == KEY_LOWERCASE_L) {
                // LOAD
                string4[0] = '\0';
                strcat(string4, "*.");
                strcat(string4, "GCD");

                char** fileNameList;
                int fileNameListLength = db_get_file_list(string4, &fileNameList, NULL, 0);
                if (fileNameListLength != -1) {
                    // NOTE: This value is not copied as in save dialog.
                    char* title = getmsg(&editor_message_file, &mesg, 601);
                    int loadFileDialogRc = file_dialog(title, fileNameList, string3, fileNameListLength, 168, 80, 0);
                    if (loadFileDialogRc == -1) {
                        db_free_file_list(&fileNameList, NULL);
                        // FIXME: This branch ignores cleanup at the end of the loop.
                        return -1;
                    }

                    if (loadFileDialogRc == 0) {
                        string4[0] = '\0';
                        strcat(string4, string3);

                        int oldRemainingCharacterPoints = character_points;

                        ResetPlayer();

                        if (pc_load_data(string4) == 0) {
                            // NOTE: Uninline.
                            CheckValidPlayer();

                            skill_get_tags(temp_tag_skill, 4);

                            // NOTE: Uninline.
                            tagskill_count = tagskl_free();

                            trait_get(&(temp_trait[0]), &(temp_trait[1]));

                            // NOTE: Uninline.
                            trait_count = get_trait_count();

                            stat_recalc_derived(obj_dude);

                            critter_adjust_hits(obj_dude, 1000);

                            rc = 1;
                        } else {
                            RestorePlayer();
                            character_points = oldRemainingCharacterPoints;
                            critter_adjust_hits(obj_dude, 1000);
                            gsound_play_sfx_file("iisxxxx1");

                            strcpy(string4, getmsg(&editor_message_file, &mesg, 612));
                            strcat(string4, string3);
                            strcat(string4, "!");

                            dialog_out(string4, NULL, 0, 169, 126, colorTable[32328], NULL, colorTable[32328], 0);
                        }

                        ResetScreen();
                    }

                    db_free_file_list(&fileNameList, NULL);
                } else {
                    gsound_play_sfx_file("iisxxxx1");

                    // Error reading file list!
                    strcpy(string4, getmsg(&editor_message_file, &mesg, 615));
                    rc = 0;

                    dialog_out(string4, NULL, 0, 169, 126, colorTable[32328], NULL, colorTable[32328], 0);
                }
            } else if (keyCode == 500 || keyCode == KEY_UPPERCASE_S || keyCode == KEY_LOWERCASE_S) {
                // SAVE
                string4[0] = '\0';
                strcat(string4, "*.");
                strcat(string4, "GCD");

                char** fileNameList;
                int fileNameListLength = db_get_file_list(string4, &fileNameList, NULL, 0);
                if (fileNameListLength != -1) {
                    strcpy(string1, getmsg(&editor_message_file, &mesg, 617));
                    strcpy(string4, getmsg(&editor_message_file, &mesg, 600));

                    if (save_file_dialog(string4, fileNameList, string1, fileNameListLength, 168, 80, 0) == 0) {
                        strcat(string1, ".");
                        strcat(string1, "GCD");

                        string4[0] = '\0';
                        strcat(string4, string1);

                        bool shouldSave;
                        if (db_access(string4)) {
                            snprintf(string4, sizeof(string4), "%s %s",
                                compat_strupr(string1),
                                getmsg(&editor_message_file, &mesg, 609));
                            strcpy(string5, getmsg(&editor_message_file, &mesg, 610));

                            if (dialog_out(string4, dialogBody, 1, 169, 126, colorTable[32328], NULL, colorTable[32328], DIALOG_BOX_YES_NO) != 0) {
                                shouldSave = true;
                            } else {
                                shouldSave = false;
                            }
                        } else {
                            shouldSave = true;
                        }

                        if (shouldSave) {
                            skill_set_tags(temp_tag_skill, 4);
                            trait_set(temp_trait[0], temp_trait[1]);

                            string4[0] = '\0';
                            strcat(string4, string1);

                            if (pc_save_data(string4) != 0) {
                                gsound_play_sfx_file("iisxxxx1");
                                snprintf(string4, sizeof(string4), "%s%s!",
                                    compat_strupr(string1),
                                    getmsg(&editor_message_file, &mesg, 611));
                                dialog_out(string4, NULL, 0, 169, 126, colorTable[32328], NULL, colorTable[32328], DIALOG_BOX_LARGE);
                                rc = 0;
                            } else {
                                snprintf(string4, sizeof(string4), "%s%s",
                                    compat_strupr(string1),
                                    getmsg(&editor_message_file, &mesg, 607));
                                dialog_out(string4, NULL, 0, 169, 126, colorTable[992], NULL, colorTable[992], DIALOG_BOX_LARGE);
                                rc = 1;
                            }
                        }
                    }

                    db_free_file_list(&fileNameList, NULL);
                } else {
                    gsound_play_sfx_file("iisxxxx1");

                    // Error reading file list!
                    char* msg = getmsg(&editor_message_file, &mesg, 615);
                    dialog_out(msg, NULL, 0, 169, 126, colorTable[32328], NULL, colorTable[32328], 0);

                    rc = 0;
                }
            }

            win_draw(win);

            renderPresent();
            sharedFpsLimiter.throttle();
        }

        win_delete(win);

        for (index = 0; index < 5; index++) {
            mem_free(up[index]);
            mem_free(down[index]);
        }

        return 0;
    }

    // Character Editor is not in creation mode - this button is only for
    // printing character details.

    char pattern[512];
    strcpy(pattern, "*.TXT");

    char** fileNames;
    int filesCount = db_get_file_list(pattern, &fileNames, NULL, 0);
    if (filesCount == -1) {
        gsound_play_sfx_file("iisxxxx1");

        // Error reading file list!
        strcpy(pattern, getmsg(&editor_message_file, &mesg, 615));
        dialog_out(pattern, NULL, 0, 169, 126, colorTable[32328], NULL, colorTable[32328], 0);
        return 0;
    }

    // PRINT
    char fileName[512];
    strcpy(fileName, getmsg(&editor_message_file, &mesg, 616));

    char title[512];
    strcpy(title, getmsg(&editor_message_file, &mesg, 602));

    if (save_file_dialog(title, fileNames, fileName, filesCount, 168, 80, 0) == 0) {
        strcat(fileName, ".TXT");

        title[0] = '\0';
        strcat(title, fileName);

        int v42 = 0;
        if (db_access(title)) {
            snprintf(title, sizeof(title),
                "%s %s",
                compat_strupr(fileName),
                getmsg(&editor_message_file, &mesg, 609));

            char line2[512];
            strcpy(line2, getmsg(&editor_message_file, &mesg, 610));

            const char* lines[] = { line2 };
            v42 = dialog_out(title, lines, 1, 169, 126, colorTable[32328], NULL, colorTable[32328], 0x10);
            if (v42) {
                v42 = 1;
            }
        } else {
            v42 = 1;
        }

        if (v42) {
            title[0] = '\0';
            strcpy(title, fileName);

            if (Save_as_ASCII(title) != 0) {
                gsound_play_sfx_file("iisxxxx1");

                snprintf(title, sizeof(title),
                    "%s%s%s",
                    getmsg(&editor_message_file, &mesg, 611),
                    compat_strupr(fileName),
                    "!");
                dialog_out(title, NULL, 0, 169, 126, colorTable[32328], NULL, colorTable[32328], 1);
            }
        }
    }

    db_free_file_list(&fileNames, NULL);

    return 0;
}

// 0x433214
bool db_access(const char* fname)
{
    DB_FILE* stream = db_fopen(fname, "rb");
    if (stream == NULL) {
        return false;
    }

    db_fclose(stream);
    return true;
}

// 0x433230
static int Save_as_ASCII(const char* fileName)
{
    DB_FILE* stream = db_fopen(fileName, "wt");
    if (stream == NULL) {
        return -1;
    }

    db_fputs("\n", stream);
    db_fputs("\n", stream);

    char title1[256];
    char title2[256];
    char title3[256];
    char padding[256];

    // FALLOUT
    strcpy(title1, getmsg(&editor_message_file, &mesg, 620));

    // NOTE: Uninline.
    padding[0] = '\0';
    AddSpaces(padding, (80 - strlen(title1)) / 2 - 2);

    strcat(padding, title1);
    strcat(padding, "\n");
    db_fputs(padding, stream);

    // VAULT-13 PERSONNEL RECORD
    strcpy(title1, getmsg(&editor_message_file, &mesg, 621));

    // NOTE: Uninline.
    padding[0] = '\0';
    AddSpaces(padding, (80 - strlen(title1)) / 2 - 2);

    strcat(padding, title1);
    strcat(padding, "\n");
    db_fputs(padding, stream);

    int month;
    int day;
    int year;
    game_time_date(&month, &day, &year);

    snprintf(title1, sizeof(title1), "%.2d %s %d  %.4d %s",
        day,
        getmsg(&editor_message_file, &mesg, 500 + month - 1),
        year,
        game_time_hour(),
        getmsg(&editor_message_file, &mesg, 622));

    // NOTE: Uninline.
    padding[0] = '\0';
    AddSpaces(padding, (80 - strlen(title1)) / 2 - 2);

    strcat(padding, title1);
    strcat(padding, "\n");
    db_fputs(padding, stream);

    // Blank line
    db_fputs("\n", stream);

    // Name
    snprintf(title1, sizeof(title1),
        "%s %s",
        getmsg(&editor_message_file, &mesg, 642),
        critter_name(obj_dude));

    int paddingLength = 27 - strlen(title1);
    if (paddingLength > 0) {
        // NOTE: Uninline.
        padding[0] = '\0';
        AddSpaces(padding, paddingLength);

        strcat(title1, padding);
    }

    // Age
    snprintf(title2, sizeof(title2),
        "%s%s %d",
        title1,
        getmsg(&editor_message_file, &mesg, 643),
        stat_level(obj_dude, STAT_AGE));

    // Gender
    snprintf(title3, sizeof(title3),
        "%s%s %s",
        title2,
        getmsg(&editor_message_file, &mesg, 644),
        getmsg(&editor_message_file, &mesg, 645 + stat_level(obj_dude, STAT_GENDER)));

    db_fputs(title3, stream);
    db_fputs("\n", stream);

    snprintf(title1, sizeof(title1),
        "%s %.2d %s %s ",
        getmsg(&editor_message_file, &mesg, 647),
        stat_pc_get(PC_STAT_LEVEL),
        getmsg(&editor_message_file, &mesg, 648),
        itostndn(stat_pc_get(PC_STAT_EXPERIENCE), title3));

    paddingLength = 12 - strlen(title3);
    if (paddingLength > 0) {
        // NOTE: Uninline.
        padding[0] = '\0';
        AddSpaces(padding, paddingLength);

        strcat(title1, padding);
    }

    snprintf(title2, sizeof(title2),
        "%s%s %s",
        title1,
        getmsg(&editor_message_file, &mesg, 649),
        itostndn(stat_pc_min_exp(), title3));
    db_fputs(title2, stream);
    db_fputs("\n", stream);
    db_fputs("\n", stream);

    // Statistics
    snprintf(title1, sizeof(title1), "%s\n", getmsg(&editor_message_file, &mesg, 623));

    // Strength / Hit Points / Sequence
    //
    // FIXME: There is bug - it shows strength instead of sequence.
    snprintf(title1, sizeof(title1),
        "%s %.2d %s %.3d/%.3d %s %.2d",
        getmsg(&editor_message_file, &mesg, 624),
        stat_level(obj_dude, STAT_STRENGTH),
        getmsg(&editor_message_file, &mesg, 625),
        critter_get_hits(obj_dude),
        stat_level(obj_dude, STAT_MAXIMUM_HIT_POINTS),
        getmsg(&editor_message_file, &mesg, 626),
        stat_level(obj_dude, STAT_STRENGTH));
    db_fputs(title1, stream);
    db_fputs("\n", stream);

    // Perception / Armor Class / Healing Rate
    snprintf(title1, sizeof(title1),
        "%s %.2d %s %.3d %s %.2d",
        getmsg(&editor_message_file, &mesg, 627),
        stat_level(obj_dude, STAT_PERCEPTION),
        getmsg(&editor_message_file, &mesg, 628),
        stat_level(obj_dude, STAT_ARMOR_CLASS),
        getmsg(&editor_message_file, &mesg, 629),
        stat_level(obj_dude, STAT_HEALING_RATE));
    db_fputs(title1, stream);
    db_fputs("\n", stream);

    // Endurance / Action Points / Critical Chance
    snprintf(title1, sizeof(title1),
        "%s %.2d %s %.2d %s %.3d%%",
        getmsg(&editor_message_file, &mesg, 630),
        stat_level(obj_dude, STAT_ENDURANCE),
        getmsg(&editor_message_file, &mesg, 631),
        stat_level(obj_dude, STAT_MAXIMUM_ACTION_POINTS),
        getmsg(&editor_message_file, &mesg, 632),
        stat_level(obj_dude, STAT_CRITICAL_CHANCE));
    db_fputs(title1, stream);
    db_fputs("\n", stream);

    // Charisma / Melee Damage / Carry Weight
    snprintf(title1, sizeof(title1),
        "%s %.2d %s %.2d %s %.3d lbs.",
        getmsg(&editor_message_file, &mesg, 633),
        stat_level(obj_dude, STAT_CHARISMA),
        getmsg(&editor_message_file, &mesg, 634),
        stat_level(obj_dude, STAT_MELEE_DAMAGE),
        getmsg(&editor_message_file, &mesg, 635),
        stat_level(obj_dude, STAT_CARRY_WEIGHT));
    db_fputs(title1, stream);
    db_fputs("\n", stream);

    // Intelligence / Damage Resistance
    snprintf(title1, sizeof(title1),
        "%s %.2d %s %.3d%%",
        getmsg(&editor_message_file, &mesg, 636),
        stat_level(obj_dude, STAT_INTELLIGENCE),
        getmsg(&editor_message_file, &mesg, 637),
        stat_level(obj_dude, STAT_DAMAGE_RESISTANCE));
    db_fputs(title1, stream);
    db_fputs("\n", stream);

    // Agility / Radiation Resistance
    snprintf(title1, sizeof(title1),
        "%s %.2d %s %.3d%%",
        getmsg(&editor_message_file, &mesg, 638),
        stat_level(obj_dude, STAT_AGILITY),
        getmsg(&editor_message_file, &mesg, 639),
        stat_level(obj_dude, STAT_RADIATION_RESISTANCE));
    db_fputs(title1, stream);
    db_fputs("\n", stream);

    // Luck / Poison Resistance
    snprintf(title1, sizeof(title1),
        "%s %.2d %s %.3d%%",
        getmsg(&editor_message_file, &mesg, 640),
        stat_level(obj_dude, STAT_LUCK),
        getmsg(&editor_message_file, &mesg, 641),
        stat_level(obj_dude, STAT_POISON_RESISTANCE));
    db_fputs(title1, stream);
    db_fputs("\n", stream);

    db_fputs("\n", stream);
    db_fputs("\n", stream);

    if (temp_trait[0] != -1) {
        // ::: Traits :::
        snprintf(title1, sizeof(title1), "%s\n", getmsg(&editor_message_file, &mesg, 650));
        db_fputs(title1, stream);

        // NOTE: The original code does not use loop, or it was optimized away.
        for (int index = 0; index < PC_TRAIT_MAX; index++) {
            if (temp_trait[index] != -1) {
                snprintf(title1, sizeof(title1), "  %s", trait_name(temp_trait[index]));
                db_fputs(title1, stream);
                db_fputs("\n", stream);
            }
        }
    }

    int perk = 0;
    for (; perk < PERK_COUNT; perk++) {
        if (perk_level(perk) != 0) {
            break;
        }
    }

    if (perk < PERK_COUNT) {
        // ::: Perks :::
        snprintf(title1, sizeof(title1), "%s\n", getmsg(&editor_message_file, &mesg, 651));
        db_fputs(title1, stream);

        for (perk = 0; perk < PERK_COUNT; perk++) {
            int rank = perk_level(perk);
            if (rank != 0) {
                if (rank == 1) {
                    snprintf(title1, sizeof(title1), "  %s", perk_name(perk));
                } else {
                    snprintf(title1, sizeof(title1), "  %s (%d)", perk_name(perk), rank);
                }

                db_fputs(title1, stream);
                db_fputs("\n", stream);
            }
        }
    }

    db_fputs("\n", stream);

    // ::: Karma :::
    snprintf(title1, sizeof(title1), "%s\n", getmsg(&editor_message_file, &mesg, 652));
    db_fputs(title1, stream);

    // for (int index = 0; index < karma_vars_count; index++) {
    //     KarmaEntry* karmaEntry = &(karma_vars[index]);
    //     if (karmaEntry->gvar == GVAR_PLAYER_REPUTATION) {
    //         int reputation = 0;
    //         for (; reputation < general_reps_count; reputation++) {
    //             GenericReputationEntry* reputationDescription = &(general_reps[reputation]);
    //             if (game_global_vars[GVAR_PLAYER_REPUTATION] >= reputationDescription->threshold) {
    //                 break;
    //             }
    //         }

    //         if (reputation < general_reps_count) {
    //             GenericReputationEntry* reputationDescription = &(general_reps[reputation]);
    //             snprintf(title1, sizeof(title1),
    //                 "  %s: %s (%s)",
    //                 getmsg(&editor_message_file, &mesg, 125),
    //                 compat_itoa(game_global_vars[GVAR_PLAYER_REPUTATION], title2, 10),
    //                 getmsg(&editor_message_file, &mesg, reputationDescription->name));
    //             db_fputs(title1, stream);
    //             db_fputs("\n", stream);
    //         }
    //     } else {
    //         if (game_global_vars[karmaEntry->gvar] != 0) {
    //             snprintf(title1, sizeof(title1), "  %s", getmsg(&editor_message_file, &mesg, karmaEntry->name));
    //             db_fputs(title1, stream);
    //             db_fputs("\n", stream);
    //         }
    //     }
    // }

    // bool hasTownReputationHeading = false;
    // for (int index = 0; index < TOWN_REPUTATION_COUNT; index++) {
    //     const TownReputationEntry* pair = &(town_rep_info[index]);
    //     if (false /* wmAreaIsKnown(pair->city) */) {
    //         if (!hasTownReputationHeading) {
    //             db_fputs("\n", stream);

    //             // ::: Reputation :::
    //             snprintf(title1, sizeof(title1), "%s\n", getmsg(&editor_message_file, &mesg, 657));
    //             db_fputs(title1, stream);
    //             hasTownReputationHeading = true;
    //         }

    //         // wmGetAreaIdxName(pair->city, title2);

    //         int townReputation = game_global_vars[pair->gvar];

    //         int townReputationMessageId;

    //         if (townReputation < -30) {
    //             townReputationMessageId = 2006; // Vilified
    //         } else if (townReputation < -15) {
    //             townReputationMessageId = 2005; // Hated
    //         } else if (townReputation < 0) {
    //             townReputationMessageId = 2004; // Antipathy
    //         } else if (townReputation == 0) {
    //             townReputationMessageId = 2003; // Neutral
    //         } else if (townReputation < 15) {
    //             townReputationMessageId = 2002; // Accepted
    //         } else if (townReputation < 30) {
    //             townReputationMessageId = 2001; // Liked
    //         } else {
    //             townReputationMessageId = 2000; // Idolized
    //         }

    //         snprintf(title1, sizeof(title1),
    //             "  %s: %s",
    //             title2,
    //             getmsg(&editor_message_file, &mesg, townReputationMessageId));
    //         db_fputs(title1, stream);
    //         db_fputs("\n", stream);
    //     }
    // }

    // bool hasAddictionsHeading = false;
    // for (int index = 0; index < ADDICTION_REPUTATION_COUNT; index++) {
    //     if (game_global_vars[addiction_vars[index]] != 0) {
    //         if (!hasAddictionsHeading) {
    //             db_fputs("\n", stream);

    //             // ::: Addictions :::
    //             snprintf(title1, sizeof(title1), "%s\n", getmsg(&editor_message_file, &mesg, 656));
    //             db_fputs(title1, stream);
    //             hasAddictionsHeading = true;
    //         }

    //         snprintf(title1, sizeof(title1),
    //             "  %s",
    //             getmsg(&editor_message_file, &mesg, 1004 + index));
    //         db_fputs(title1, stream);
    //         db_fputs("\n", stream);
    //     }
    // }

    db_fputs("\n", stream);

    // ::: Skills ::: / ::: Kills :::
    snprintf(title1, sizeof(title1), "%s\n", getmsg(&editor_message_file, &mesg, 653));
    db_fputs(title1, stream);

    int killType = 0;
    for (int skill = 0; skill < SKILL_COUNT; skill++) {
        snprintf(title1, sizeof(title1), "%s ", skill_name(skill));

        // NOTE: Uninline.
        AddDots(title1 + strlen(title1), 16 - strlen(title1));

        bool hasKillType = false;

        for (; killType < KILL_TYPE_COUNT; killType++) {
            int killsCount = critter_kill_count(killType);
            if (killsCount > 0) {
                snprintf(title2, sizeof(title2), "%s ", critter_kill_name(killType));

                // NOTE: Uninline.
                AddDots(title2 + strlen(title2), 16 - strlen(title2));

                snprintf(title3, sizeof(title3),
                    "  %s %.3d%%        %s %.3d\n",
                    title1,
                    skill_level(obj_dude, skill),
                    title2,
                    killsCount);
                hasKillType = true;
                break;
            }
        }

        if (!hasKillType) {
            snprintf(title3, sizeof(title3),
                "  %s %.3d%%\n",
                title1,
                skill_level(obj_dude, skill));
        }
    }

    db_fputs("\n", stream);
    db_fputs("\n", stream);

    // ::: Inventory :::
    snprintf(title1, sizeof(title1), "%s\n", getmsg(&editor_message_file, &mesg, 654));
    db_fputs(title1, stream);

    Inventory* inventory = &(obj_dude->data.inventory);
    for (int index = 0; index < inventory->length; index += 3) {
        title1[0] = '\0';

        for (int column = 0; column < 3; column++) {
            int inventoryItemIndex = index + column;
            if (inventoryItemIndex >= inventory->length) {
                break;
            }

            InventoryItem* inventoryItem = &(inventory->items[inventoryItemIndex]);

            snprintf(title2, sizeof(title2),
                "  %sx %s",
                itostndn(inventoryItem->quantity, title3),
                object_name(inventoryItem->item));

            int length = 25 - strlen(title2);
            if (length < 0) {
                length = 0;
            }

            AddSpaces(title2, length);

            strcat(title1, title2);
        }

        strcat(title1, "\n");
        db_fputs(title1, stream);
    }

    db_fputs("\n", stream);

    // Total Weight:
    snprintf(title1, sizeof(title1),
        "%s %d lbs.",
        getmsg(&editor_message_file, &mesg, 655),
        item_total_weight(obj_dude));
    db_fputs(title1, stream);

    db_fputs("\n", stream);
    db_fputs("\n", stream);
    db_fputs("\n", stream);
    db_fclose(stream);

    return 0;
}

// 0x4344A0
static void ResetScreen()
{
    info_line = 0;
    skill_cursor = 0;
    slider_y = 27;
    folder = 0;

    if (glblmode) {
        PrintBigNum(126, 282, 0, character_points, 0, edit_win);
    } else {
        DrawFolder();
        PrintLevelWin();
    }

    PrintBigname();
    PrintAgeBig();
    PrintGender();
    ListTraits();
    ListSkills(0);
    PrintBasicStat(7, 0, 0);
    ListDrvdStats();
    DrawInfoWin();
    win_draw(edit_win);
}

// 0x434540
char* AddSpaces(char* string, int length)
{
    char* pch = string + strlen(string);

    for (int index = 0; index < length; index++) {
        *pch++ = ' ';
    }

    *pch = '\0';

    return string;
}

// 0x434570
static char* AddDots(char* string, int length)
{
    char* pch = string + strlen(string);

    for (int index = 0; index < length; index++) {
        *pch++ = '.';
    }

    *pch = '\0';

    return string;
}

// 0x4345A0
static void RegInfoAreas()
{
    win_register_button(edit_win, 19, 38, 125, 227, -1, -1, 525, -1, NULL, NULL, NULL, 0);
    win_register_button(edit_win, 28, 280, 124, 32, -1, -1, 526, -1, NULL, NULL, NULL, 0);

    if (glblmode) {
        win_register_button(edit_win, 52, 324, 169, 20, -1, -1, 533, -1, NULL, NULL, NULL, 0);
        win_register_button(edit_win, 47, 353, 245, 100, -1, -1, 534, -1, NULL, NULL, NULL, 0);
    } else {
        win_register_button(edit_win, 28, 363, 283, 105, -1, -1, 527, -1, NULL, NULL, NULL, 0);
    }

    win_register_button(edit_win, 191, 41, 122, 110, -1, -1, 528, -1, NULL, NULL, NULL, 0);
    win_register_button(edit_win, 191, 175, 122, 135, -1, -1, 529, -1, NULL, NULL, NULL, 0);
    win_register_button(edit_win, 376, 5, 223, 20, -1, -1, 530, -1, NULL, NULL, NULL, 0);
    win_register_button(edit_win, 370, 27, 223, 195, -1, -1, 531, -1, NULL, NULL, NULL, 0);
    win_register_button(edit_win, 396, 228, 171, 25, -1, -1, 532, -1, NULL, NULL, NULL, 0);
}

// 0x434780
static int CheckValidPlayer()
{
    int stat;

    stat_recalc_derived(obj_dude);
    stat_pc_set_defaults();

    for (stat = 0; stat < SAVEABLE_STAT_COUNT; stat++) {
        stat_set_bonus(obj_dude, stat, 0);
    }

    perk_reset();
    stat_recalc_derived(obj_dude);

    return 1;
}

// 0x4347C0
static void SavePlayer()
{
    Proto* proto;
    proto_ptr(obj_dude->pid, &proto);
    critter_copy(&dude_data, &(proto->critter.data));

    hp_back = critter_get_hits(obj_dude);

    strncpy(name_save, critter_name(obj_dude), 32);

    last_level_back = last_level;

    // NOTE: Uninline.
    push_perks();

    free_perk_back = free_perk;

    upsent_points_back = stat_pc_get(PC_STAT_UNSPENT_SKILL_POINTS);

    skill_get_tags(tag_skill_back, NUM_TAGGED_SKILLS);

    trait_get(&(trait_back[0]), &(trait_back[1]));

    if (!glblmode) {
        for (int skill = 0; skill < SKILL_COUNT; skill++) {
            skillsav[skill] = skill_level(obj_dude, skill);
        }
    }
}

// 0x434898
static void RestorePlayer()
{
    Proto* proto;
    int cur_hp;

    pop_perks();

    proto_ptr(obj_dude->pid, &proto);
    critter_copy(&(proto->critter.data), &dude_data);

    critter_pc_set_name(name_save);

    last_level = last_level_back;
    free_perk = free_perk_back;

    stat_pc_set(PC_STAT_UNSPENT_SKILL_POINTS, upsent_points_back);

    skill_set_tags(tag_skill_back, NUM_TAGGED_SKILLS);

    trait_set(trait_back[0], trait_back[1]);

    skill_get_tags(temp_tag_skill, NUM_TAGGED_SKILLS);

    // NOTE: Uninline.
    tagskill_count = tagskl_free();

    trait_get(&(temp_trait[0]), &(temp_trait[1]));

    // NOTE: Uninline.
    trait_count = get_trait_count();

    stat_recalc_derived(obj_dude);

    cur_hp = critter_get_hits(obj_dude);
    critter_adjust_hits(obj_dude, hp_back - cur_hp);
}

// 0x4349A8
char* itostndn(int value, char* dest)
{
    // 0x42C3F0
    static const int v16[7] = {
        1000000,
        100000,
        10000,
        1000,
        100,
        10,
        1,
    };

    char* savedDest = dest;

    if (value != 0) {
        *dest = '\0';

        bool v3 = false;
        for (int index = 0; index < 7; index++) {
            int v18 = value / v16[index];
            if (v18 > 0 || v3) {
                char temp[64]; // TODO: Size is probably wrong.
                compat_itoa(v18, temp, 10);
                strcat(dest, temp);

                v3 = true;

                value -= v16[index] * v18;

                if (index == 0 || index == 3) {
                    strcat(dest, ",");
                }
            }
        }
    } else {
        strcpy(dest, "0");
    }

    return savedDest;
}

// 0x434AC8
static int DrawCard(int graphicId, const char* name, const char* attributes, char* description)
{
    CacheEntry* graphicHandle;
    Size size;
    int fid;
    unsigned char* buf;
    unsigned char* ptr;
    int v9;
    int x;
    int y;
    short beginnings[WORD_WRAP_MAX_COUNT];
    short beginningsCount;

    fid = art_id(OBJ_TYPE_SKILLDEX, graphicId, 0, 0, 0);
    buf = art_lock(fid, &graphicHandle, &(size.width), &(size.height));
    if (buf == NULL) {
        return -1;
    }

    buf_to_buf(buf, size.width, size.height, size.width, win_buf + 640 * 309 + 484, 640);

    v9 = 150;
    ptr = buf;
    for (y = 0; y < size.height; y++) {
        for (x = 0; x < size.width; x++) {
            if (HighRGB(*ptr) < 2 && v9 >= x) {
                v9 = x;
            }
            ptr++;
        }
    }

    v9 -= 8;
    if (v9 < 0) {
        v9 = 0;
    }

    text_font(102);

    text_to_buf(win_buf + 640 * 272 + 348, name, 640, 640, colorTable[0]);
    int nameFontLineHeight = text_height();
    if (attributes != NULL) {
        int nameWidth = text_width(name);

        text_font(101);
        int attributesFontLineHeight = text_height();
        text_to_buf(win_buf + 640 * (268 + nameFontLineHeight - attributesFontLineHeight) + 348 + nameWidth + 8, attributes, 640, 640, colorTable[0]);
    }

    y = nameFontLineHeight;
    win_line(edit_win, 348, y + 272, 613, y + 272, colorTable[0]);
    win_line(edit_win, 348, y + 273, 613, y + 273, colorTable[0]);

    text_font(101);

    int descriptionFontLineHeight = text_height();

    if (word_wrap(description, v9 + 136, beginnings, &beginningsCount) != 0) {
        // TODO: Leaking graphic handle.
        return -1;
    }

    y = 315;
    for (short i = 0; i < beginningsCount - 1; i++) {
        short beginning = beginnings[i];
        short ending = beginnings[i + 1];
        char c = description[ending];
        description[ending] = '\0';
        text_to_buf(win_buf + 640 * y + 348, description + beginning, 640, 640, colorTable[0]);
        description[ending] = c;
        y += descriptionFontLineHeight;
    }

    if (graphicId != old_fid1 || strcmp(name, old_str1) != 0) {
        if (frstc_draw1) {
            gsound_play_sfx_file("isdxxxx1");
        }
    }

    strcpy(old_str1, name);
    old_fid1 = graphicId;
    frstc_draw1 = true;

    art_ptr_unlock(graphicHandle);

    return 0;
}

// 0x434E18
static int XltPerk(int search)
{
    int perk;
    int valid_perk = 0;

    for (perk = 0; perk < PERK_COUNT; perk++) {
        if (perk_level(perk)) {
            if (valid_perk == search) {
                return perk;
            }
            valid_perk++;
        }
    }

    debug_printf("\n ** Perk not found in translate! **\n");

    return -1;
}

// 0x434E58
static void FldrButton()
{
    mouseGetPositionInWindow(edit_win, &mouse_xpos, &mouse_ypos);
    gsound_play_sfx_file("ib3p1xx1");

    if (mouse_xpos >= 208) {
        info_line = 41;
        folder = EDITOR_FOLDER_KILLS;
    } else if (mouse_xpos > 110) {
        info_line = 42;
        folder = EDITOR_FOLDER_KARMA;
    } else {
        info_line = 40;
        folder = EDITOR_FOLDER_PERKS;
    }

    DrawFolder();
    DrawInfoWin();
}

// 0x434F14
static void InfoButton(int eventCode)
{
    mouseGetPositionInWindow(edit_win, &mouse_xpos, &mouse_ypos);

    switch (eventCode) {
    case 525:
        if (1) {
            // TODO: Original code is slightly different.
            double mouseY = mouse_ypos;
            for (int index = 0; index < 7; index++) {
                double buttonTop = StatYpos[index];
                double buttonBottom = StatYpos[index] + 22;
                double allowance = 5.0 - index * 0.25;
                if (mouseY >= buttonTop - allowance && mouseY <= buttonBottom + allowance) {
                    info_line = index;
                    break;
                }
            }
        }
        break;
    case 526:
        if (glblmode) {
            info_line = 7;
        } else {
            int offset = mouse_ypos - 280;
            if (offset < 0) {
                offset = 0;
            }

            info_line = offset / 10 + 7;
        }
        break;
    case 527:
        if (!glblmode) {
            text_font(101);
            int offset = mouse_ypos - 364;
            if (offset < 0) {
                offset = 0;
            }
            info_line = offset / (text_height() + 1) + 10;

            if (folder == EDITOR_FOLDER_KILLS && mouse_xpos > 174) {
                info_line += 10;
            }
        }
        break;
    case 528:
        if (1) {
            int offset = mouse_ypos - 41;
            if (offset < 0) {
                offset = 0;
            }

            info_line = offset / 13 + 43;
        }
        break;
    case 529: {
        int offset = mouse_ypos - 175;
        if (offset < 0) {
            offset = 0;
        }

        info_line = offset / 13 + 51;
        break;
    }
    case 530:
        info_line = 80;
        break;
    case 531:
        if (1) {
            int offset = mouse_ypos - 27;
            if (offset < 0) {
                offset = 0;
            }

            skill_cursor = (int)(offset * 0.092307694);
            if (skill_cursor >= 18) {
                skill_cursor = 17;
            }

            info_line = skill_cursor + 61;
        }
        break;
    case 532:
        info_line = 79;
        break;
    case 533:
        info_line = 81;
        break;
    case 534:
        if (1) {
            text_font(101);

            // TODO: Original code is slightly different.
            double mouseY = mouse_ypos;
            double fontLineHeight = text_height();
            double y = 353.0;
            double step = text_height() + 3 + 0.56;
            int index;
            for (index = 0; index < 8; index++) {
                if (mouseY >= y - 4.0 && mouseY <= y + fontLineHeight) {
                    break;
                }
                y += step;
            }

            if (index == 8) {
                index = 7;
            }

            info_line = index + 82;
            if (mouse_xpos >= 169) {
                info_line += 8;
            }
        }
        break;
    }

    ListTraits();
    ListSkills(0);
    PrintLevelWin();
    DrawFolder();
    ListDrvdStats();
    DrawInfoWin();
}

// 0x435220
static void SliderBtn(int keyCode)
{
    if (glblmode) {
        return;
    }

    int unspentSp = stat_pc_get(PC_STAT_UNSPENT_SKILL_POINTS);
    repFtime = 4;

    bool isUsingKeyboard = false;
    int rc = 0;

    switch (keyCode) {
    case KEY_PLUS:
    case KEY_UPPERCASE_N:
    case KEY_ARROW_RIGHT:
        isUsingKeyboard = true;
        keyCode = 521;
        break;
    case KEY_MINUS:
    case KEY_UPPERCASE_J:
    case KEY_ARROW_LEFT:
        isUsingKeyboard = true;
        keyCode = 523;
        break;
    }

    char title[64];
    char body1[64];
    char body2[64];

    const char* body[] = {
        body1,
        body2,
    };

    int repeatDelay = 0;
    for (;;) {
        sharedFpsLimiter.mark();

        frame_time = get_time();
        if (repeatDelay <= 19.2) {
            repeatDelay++;
        }

        if (repeatDelay == 1 || repeatDelay > 19.2) {
            if (repeatDelay > 19.2) {
                repFtime++;
                if (repFtime > 24) {
                    repFtime = 24;
                }
            }

            rc = 1;
            if (keyCode == 521) {
                if (stat_pc_get(PC_STAT_UNSPENT_SKILL_POINTS) > 0) {
                    if (skill_inc_point(obj_dude, skill_cursor) == -3) {
                        gsound_play_sfx_file("iisxxxx1");

                        snprintf(title, sizeof(title), "%s:", skill_name(skill_cursor));
                        // At maximum level.
                        strcpy(body1, getmsg(&editor_message_file, &mesg, 132));
                        // Unable to increment it.
                        strcpy(body2, getmsg(&editor_message_file, &mesg, 133));
                        dialog_out(title, body, 2, 192, 126, colorTable[32328], NULL, colorTable[32328], DIALOG_BOX_LARGE);
                        rc = -1;
                    }
                } else {
                    gsound_play_sfx_file("iisxxxx1");

                    // Not enough skill points available.
                    strcpy(title, getmsg(&editor_message_file, &mesg, 136));
                    dialog_out(title, NULL, 0, 192, 126, colorTable[32328], NULL, colorTable[32328], DIALOG_BOX_LARGE);
                    rc = -1;
                }
            } else if (keyCode == 523) {
                if (skill_level(obj_dude, skill_cursor) <= skillsav[skill_cursor]) {
                    rc = 0;
                } else {
                    if (skill_dec_point(obj_dude, skill_cursor) == -2) {
                        rc = 0;
                    }
                }

                if (rc == 0) {
                    gsound_play_sfx_file("iisxxxx1");

                    snprintf(title, sizeof(title), "%s:", skill_name(skill_cursor));
                    // At minimum level.
                    strcpy(body1, getmsg(&editor_message_file, &mesg, 134));
                    // Unable to decrement it.
                    strcpy(body2, getmsg(&editor_message_file, &mesg, 135));
                    dialog_out(title, body, 2, 192, 126, colorTable[32328], NULL, colorTable[32328], DIALOG_BOX_LARGE);
                    rc = -1;
                }
            }

            info_line = skill_cursor + 61;
            DrawInfoWin();
            ListSkills(1);

            int flags;
            if (rc == 1) {
                flags = ANIMATE;
            } else {
                flags = 0;
            }

            PrintBigNum(522, 228, flags, stat_pc_get(PC_STAT_UNSPENT_SKILL_POINTS), unspentSp, edit_win);

            win_draw(edit_win);
        }

        if (!isUsingKeyboard) {
            unspentSp = stat_pc_get(PC_STAT_UNSPENT_SKILL_POINTS);
            if (repeatDelay >= 19.2) {
                while (elapsed_time(frame_time) < 1000 / repFtime) {
                }
            } else {
                while (elapsed_time(frame_time) < 1000 / 24) {
                }
            }

            int keyCode = get_input();
            if (keyCode != 522 && keyCode != 524 && rc != -1) {
                renderPresent();
                sharedFpsLimiter.throttle();
                continue;
            }
        }
        return;
    }
}

// 0x435618
static int tagskl_free()
{
    int taggedSkillCount;
    int index;

    taggedSkillCount = 0;
    for (index = 3; index >= 0; index--) {
        if (temp_tag_skill[index] != -1) {
            break;
        }

        taggedSkillCount++;
    }

    if (glblmode == 1) {
        taggedSkillCount--;
    }

    return taggedSkillCount;
}

// 0x435648
static void TagSkillSelect(int skill)
{
    int insertionIndex;

    // NOTE: Uninline.
    old_tags = tagskl_free();

    if (skill == temp_tag_skill[0] || skill == temp_tag_skill[1] || skill == temp_tag_skill[2] || skill == temp_tag_skill[3]) {
        if (skill == temp_tag_skill[0]) {
            temp_tag_skill[0] = temp_tag_skill[1];
            temp_tag_skill[1] = temp_tag_skill[2];
            temp_tag_skill[2] = -1;
        } else if (skill == temp_tag_skill[1]) {
            temp_tag_skill[1] = temp_tag_skill[2];
            temp_tag_skill[2] = -1;
        } else {
            temp_tag_skill[2] = -1;
        }
    } else {
        if (tagskill_count > 0) {
            insertionIndex = 0;
            for (int index = 0; index < 3; index++) {
                if (temp_tag_skill[index] == -1) {
                    break;
                }
                insertionIndex++;
            }
            temp_tag_skill[insertionIndex] = skill;
        } else {
            gsound_play_sfx_file("iisxxxx1");

            char line1[128];
            strcpy(line1, getmsg(&editor_message_file, &mesg, 140));

            char line2[128];
            strcpy(line2, getmsg(&editor_message_file, &mesg, 141));

            const char* lines[] = { line2 };
            dialog_out(line1, lines, 1, 192, 126, colorTable[32328], 0, colorTable[32328], 0);
        }
    }

    // NOTE: Uninline.
    tagskill_count = tagskl_free();

    info_line = skill + 61;
    PrintBasicStat(RENDER_ALL_STATS, 0, 0);
    ListDrvdStats();
    ListSkills(2);
    DrawInfoWin();
    win_draw(edit_win);
}

// 0x435874
static void ListTraits()
{
    int selected_trait_line = -1;
    int trait;
    int color;
    double step;
    double y;

    if (glblmode != 1) {
        return;
    }

    if (info_line >= 82 && info_line < 98) {
        selected_trait_line = info_line - 82;
    }

    buf_to_buf(bckgnd + 640 * 353 + 47, 245, 100, 640, win_buf + 640 * 353 + 47, 640);

    text_font(101);

    trait_set(temp_trait[0], temp_trait[1]);

    step = text_height() + 3 + 0.56;
    y = 353;
    for (trait = 0; trait < 8; trait++) {
        if (trait == selected_trait_line) {
            if (trait != temp_trait[0] && trait != temp_trait[1]) {
                color = colorTable[32747];
            } else {
                color = colorTable[32767];
            }
        } else {
            if (trait != temp_trait[0] && trait != temp_trait[1]) {
                color = colorTable[992];
            } else {
                color = colorTable[21140];
            }
        }

        text_to_buf(win_buf + 640 * (int)y + 47, trait_name(trait), 640, 640, color);
        y += step;
    }

    y = 353;
    for (trait = 8; trait < 16; trait++) {
        if (trait == selected_trait_line) {
            if (trait != temp_trait[0] && trait != temp_trait[1]) {
                color = colorTable[32747];
            } else {
                color = colorTable[32767];
            }
        } else {
            if (trait != temp_trait[0] && trait != temp_trait[1]) {
                color = colorTable[992];
            } else {
                color = colorTable[21140];
            }
        }

        text_to_buf(win_buf + 640 * (int)y + 199, trait_name(trait), 640, 640, color);
        y += step;
    }
}

// 0x435A58
static int get_trait_count()
{
    int traitCount;
    int index;

    traitCount = 0;
    for (index = 1; index >= 0; index--) {
        if (temp_trait[index] != -1) {
            break;
        }

        traitCount++;
    }

    return traitCount;
}

// 0x435A7C
static void TraitSelect(int trait)
{
    if (trait == temp_trait[0] || trait == temp_trait[1]) {
        if (trait == temp_trait[0]) {
            temp_trait[0] = temp_trait[1];
            temp_trait[1] = -1;
        } else {
            temp_trait[1] = -1;
        }
    } else {
        if (trait_count == 0) {
            gsound_play_sfx_file("iisxxxx1");

            char line1[128];
            strcpy(line1, getmsg(&editor_message_file, &mesg, 148));

            char line2[128];
            strcpy(line2, getmsg(&editor_message_file, &mesg, 149));

            const char* lines = { line2 };
            dialog_out(line1, &lines, 1, 192, 126, colorTable[32328], 0, colorTable[32328], 0);
        } else {
            for (int index = 0; index < 2; index++) {
                if (temp_trait[index] == -1) {
                    temp_trait[index] = trait;
                    break;
                }
            }
        }
    }

    // NOTE: Uninline.
    trait_count = get_trait_count();

    info_line = trait + EDITOR_FIRST_TRAIT;

    ListTraits();
    ListSkills(0);
    stat_recalc_derived(obj_dude);
    PrintBigNum(126, 282, 0, character_points, 0, edit_win);
    PrintBasicStat(RENDER_ALL_STATS, false, 0);
    ListDrvdStats();
    DrawInfoWin();
    win_draw(edit_win);
}

// 0x435C50
static int ListKarma()
{
    int selected_karma_line = -1;
    int color;
    char text[256];
    char buffer[32];
    int index;
    int count;
    int y;

    if (info_line >= 10 && info_line < 43) {
        selected_karma_line = info_line - 10;
    }

    text_font(101);

    if (selected_karma_line != 0) {
        color = colorTable[992];
    } else {
        color = colorTable[32747];
    }

    strcpy(text, getmsg(&editor_message_file, &mesg, 1000));
    strcat(text, compat_itoa(game_global_vars[GVAR_PLAYER_REPUATION], buffer, 10));
    text_to_buf(win_buf + 640 * 362 + 34, text, 640, 640, color);

    count = 1;
    y = text_height() + 363;
    for (index = 0; index < 9; index++) {
        if (game_global_vars[karma_var_table[index]]) {
            if (count == selected_karma_line) {
                color = colorTable[32747];
            } else {
                color = colorTable[992];
            }

            text_to_buf(win_buf + 640 * y + 34,
                getmsg(&editor_message_file, &mesg, 1001 + index),
                640,
                640,
                color);
            y += 1 + text_height();

            count++;
        }
    }

    return count;
}

// 0x435E1C
static int XlateKarma(int search)
{
    int karma;
    int valid_karma = 0;

    for (karma = 0; karma < 9; karma++) {
        if (game_global_vars[karma_var_table[karma]] != 0) {
            if (valid_karma == search) {
                return karma;
            }
            valid_karma++;
        }
    }

    return 0;
}

// 0x435E64
int editor_save(DB_FILE* stream)
{
    if (db_fwriteInt(stream, last_level) == -1)
        return -1;
    if (db_fwriteByte(stream, free_perk) == -1)
        return -1;

    return 0;
}

// 0x435E94
int editor_load(DB_FILE* stream)
{
    if (db_freadInt(stream, &last_level) == -1)
        return -1;
    if (db_freadByte(stream, &free_perk) == -1)
        return -1;

    return 0;
}

// 0x435EC0
void editor_reset()
{
    character_points = 5;
    last_level = 1;
}

// 0x435EDC
static int UpdateLevel()
{
    int level = stat_pc_get(PC_STAT_LEVEL);
    if (level != last_level && level <= PC_LEVEL_MAX) {
        for (int nextLevel = last_level + 1; nextLevel <= level; nextLevel++) {
            int sp = stat_pc_get(PC_STAT_UNSPENT_SKILL_POINTS);
            sp += 5;
            sp += stat_get_base(obj_dude, STAT_INTELLIGENCE) * 2;
            sp += perk_level(PERK_EDUCATED) * 2;
            if (trait_level(TRAIT_GIFTED)) {
                sp -= 5;
                if (sp < 0) {
                    sp = 0;
                }
            }
            if (sp > 99) {
                sp = 99;
            }

            stat_pc_set(PC_STAT_UNSPENT_SKILL_POINTS, sp);

            // NOTE: Uninline.
            int selectedPerksCount = PerkCount();

            if (selectedPerksCount < 7) {
                int progression = 3;
                if (trait_level(TRAIT_SKILLED)) {
                    progression += 1;
                }

                if (nextLevel % progression == 0) {
                    free_perk = 1;
                }
            }
        }
    }

    if (free_perk != 0) {
        folder = 0;
        DrawFolder();
        win_draw(edit_win);

        int rc = perks_dialog();
        if (rc == -1) {
            debug_printf("\n *** Error running perks dialog! ***\n");
            return -1;
        } else if (rc == 0) {
            DrawFolder();
        } else if (rc == 1) {
            DrawFolder();
            free_perk = 0;
        }
    }

    last_level = level;

    return 1;
}

// 0x436030
static void RedrwDPrks()
{
    char perkRankBuffer[32];

    buf_to_buf(
        pbckgnd + 280,
        293,
        PERK_WINDOW_HEIGHT,
        PERK_WINDOW_WIDTH,
        pwin_buf + 280,
        PERK_WINDOW_WIDTH);

    ListDPerks();

    if (perk_level(name_sort_list[crow + cline].value)) {
        snprintf(perkRankBuffer, sizeof(perkRankBuffer), "(%d)", perk_level(name_sort_list[crow + cline].value));
        DrawCard2(name_sort_list[crow + cline].value + 72,
            perk_name(name_sort_list[crow + cline].value),
            perkRankBuffer,
            perk_description(name_sort_list[crow + cline].value));
    } else {
        DrawCard2(name_sort_list[crow + cline].value + 72,
            perk_name(name_sort_list[crow + cline].value),
            NULL,
            perk_description(name_sort_list[crow + cline].value));
    }

    win_draw(pwin);
}

// 0x436178
static int perks_dialog()
{
    crow = 0;
    cline = 0;
    old_fid2 = -1;
    old_str2[0] = '\0';
    frstc_draw2 = false;

    CacheEntry* backgroundFrmHandle;
    int backgroundWidth;
    int backgroundHeight;
    int fid = art_id(OBJ_TYPE_INTERFACE, 86, 0, 0, 0);
    pbckgnd = art_lock(fid, &backgroundFrmHandle, &backgroundWidth, &backgroundHeight);
    if (pbckgnd == NULL) {
        printf("\n *** Error running perks dialog window ***\n");
        return -1;
    }

    // Maintain original position in original resolution, otherwise center it.
    int perkWindowX = screenGetWidth() != 640
        ? (screenGetWidth() - PERK_WINDOW_WIDTH) / 2
        : PERK_WINDOW_X;
    int perkWindowY = screenGetHeight() != 480
        ? (screenGetHeight() - PERK_WINDOW_HEIGHT) / 2
        : PERK_WINDOW_Y;
    pwin = win_add(perkWindowX,
        perkWindowY,
        PERK_WINDOW_WIDTH,
        PERK_WINDOW_HEIGHT,
        256,
        WINDOW_MODAL | WINDOW_DONT_MOVE_TOP);
    if (pwin == -1) {
        art_ptr_unlock(backgroundFrmHandle);
        printf("\n *** Error running perks dialog window ***\n");
        return -1;
    }

    pwin_buf = win_get_buf(pwin);
    memcpy(pwin_buf, pbckgnd, PERK_WINDOW_WIDTH * PERK_WINDOW_HEIGHT);

    int btn;

    btn = win_register_button(pwin,
        48,
        186,
        GInfo[EDITOR_GRAPHIC_LITTLE_RED_BUTTON_UP].width,
        GInfo[EDITOR_GRAPHIC_LITTLE_RED_BUTTON_UP].height,
        -1,
        -1,
        -1,
        500,
        grphbmp[EDITOR_GRAPHIC_LITTLE_RED_BUTTON_UP],
        grphbmp[EDITOR_GRAPHIC_LILTTLE_RED_BUTTON_DOWN],
        NULL,
        BUTTON_FLAG_TRANSPARENT);
    if (btn != -1) {
        win_register_button_sound_func(btn, gsound_red_butt_press, gsound_red_butt_release);
    }

    btn = win_register_button(pwin,
        153,
        186,
        GInfo[EDITOR_GRAPHIC_LITTLE_RED_BUTTON_UP].width,
        GInfo[EDITOR_GRAPHIC_LITTLE_RED_BUTTON_UP].height,
        -1,
        -1,
        -1,
        502,
        grphbmp[EDITOR_GRAPHIC_LITTLE_RED_BUTTON_UP],
        grphbmp[EDITOR_GRAPHIC_LILTTLE_RED_BUTTON_DOWN],
        NULL,
        BUTTON_FLAG_TRANSPARENT);
    if (btn != -1) {
        win_register_button_sound_func(btn, gsound_red_butt_press, gsound_red_butt_release);
    }

    btn = win_register_button(pwin,
        25,
        46,
        GInfo[EDITOR_GRAPHIC_UP_ARROW_ON].width,
        GInfo[EDITOR_GRAPHIC_UP_ARROW_ON].height,
        -1,
        574,
        572,
        574,
        grphbmp[EDITOR_GRAPHIC_UP_ARROW_OFF],
        grphbmp[EDITOR_GRAPHIC_UP_ARROW_ON],
        NULL,
        BUTTON_FLAG_TRANSPARENT);
    if (btn != -1) {
        win_register_button_sound_func(btn, gsound_red_butt_press, NULL);
    }

    btn = win_register_button(pwin,
        25,
        47 + GInfo[EDITOR_GRAPHIC_UP_ARROW_ON].height,
        GInfo[EDITOR_GRAPHIC_UP_ARROW_ON].width,
        GInfo[EDITOR_GRAPHIC_UP_ARROW_ON].height,
        -1,
        575,
        573,
        575,
        grphbmp[EDITOR_GRAPHIC_DOWN_ARROW_OFF],
        grphbmp[EDITOR_GRAPHIC_DOWN_ARROW_ON],
        NULL,
        BUTTON_FLAG_TRANSPARENT);
    if (btn != -1) {
        win_register_button_sound_func(btn, gsound_red_butt_press, NULL);
    }

    win_register_button(pwin,
        PERK_WINDOW_LIST_X,
        PERK_WINDOW_LIST_Y,
        PERK_WINDOW_LIST_WIDTH,
        PERK_WINDOW_LIST_HEIGHT,
        -1,
        -1,
        -1,
        501,
        NULL,
        NULL,
        NULL,
        BUTTON_FLAG_TRANSPARENT);

    text_font(103);

    const char* msg;

    // PICK A NEW PERK
    msg = getmsg(&editor_message_file, &mesg, 152);
    text_to_buf(pwin_buf + PERK_WINDOW_WIDTH * 16 + 49, msg, PERK_WINDOW_WIDTH, PERK_WINDOW_WIDTH, colorTable[18979]);

    // DONE
    msg = getmsg(&editor_message_file, &mesg, 100);
    text_to_buf(pwin_buf + PERK_WINDOW_WIDTH * 186 + 69, msg, PERK_WINDOW_WIDTH, PERK_WINDOW_WIDTH, colorTable[18979]);

    // CANCEL
    msg = getmsg(&editor_message_file, &mesg, 102);
    text_to_buf(pwin_buf + PERK_WINDOW_WIDTH * 186 + 171, msg, PERK_WINDOW_WIDTH, PERK_WINDOW_WIDTH, colorTable[18979]);

    int count = ListDPerks();

    if (perk_level(name_sort_list[crow + cline].value)) {
        char perkRankBuffer[32];
        snprintf(perkRankBuffer, sizeof(perkRankBuffer), "(%d)", perk_level(name_sort_list[crow + cline].value));
        DrawCard2(name_sort_list[crow + cline].value + 72,
            perk_name(name_sort_list[crow + cline].value),
            perkRankBuffer,
            perk_description(name_sort_list[crow + cline].value));
    } else {
        DrawCard2(name_sort_list[crow + cline].value + 72,
            perk_name(name_sort_list[crow + cline].value),
            NULL,
            perk_description(name_sort_list[crow + cline].value));
    }

    win_draw(pwin);

    int rc = InputPDLoop(count, RedrwDPrks);

    if (rc == 1) {
        if (perk_add(name_sort_list[crow + cline].value) == -1) {
            debug_printf("\n*** Unable to add perk! ***\n");
            rc = 2;
        }
    }

    rc &= 1;

    if (rc != 0) {
        if (perk_level(PERK_TAG) != 0 && perk_back[PERK_TAG] == 0) {
            if (!Add4thTagSkill()) {
                perk_sub(PERK_TAG);
            }
        } else if (perk_level(PERK_MUTATE) != 0 && perk_back[PERK_MUTATE] == 0) {
            if (!GetMutateTrait()) {
                perk_sub(PERK_MUTATE);
            }
        } else if (perk_level(PERK_LIFEGIVER) != perk_back[PERK_LIFEGIVER]) {
            int maxHp = stat_get_bonus(obj_dude, STAT_MAXIMUM_HIT_POINTS);
            stat_set_bonus(obj_dude, STAT_MAXIMUM_HIT_POINTS, maxHp + 4);
            critter_adjust_hits(obj_dude, 4);
        } else if (perk_level(PERK_EDUCATED) != perk_back[PERK_EDUCATED]) {
            int sp = stat_pc_get(PC_STAT_UNSPENT_SKILL_POINTS);
            stat_pc_set(PC_STAT_UNSPENT_SKILL_POINTS, sp + 2);
        }
    }

    ListSkills(0);
    PrintBasicStat(RENDER_ALL_STATS, 0, 0);
    ListDrvdStats();
    DrawFolder();
    DrawInfoWin();
    win_draw(edit_win);

    art_ptr_unlock(backgroundFrmHandle);

    win_delete(pwin);

    return rc;
}

// 0x43671C
static int InputPDLoop(int count, void (*refreshProc)())
{
    text_font(101);

    int v3 = count - 11;

    int height = text_height();
    oldsline = -2;
    int v16 = height + 2;

    int v7 = 0;

    int rc = 0;
    while (rc == 0) {
        sharedFpsLimiter.mark();

        int keyCode = get_input();
        int v19 = 0;

        convertMouseWheelToArrowKey(&keyCode);

        if (keyCode == 500) {
            rc = 1;
        } else if (keyCode == KEY_RETURN) {
            gsound_play_sfx_file("ib1p1xx1");
            rc = 1;
        } else if (keyCode == 501) {
            mouseGetPositionInWindow(pwin, &mouse_xpos, &mouse_ypos);
            cline = (mouse_ypos - (PERK_WINDOW_Y + PERK_WINDOW_LIST_Y)) / v16;
            if (cline >= 0) {
                if (count - 1 < cline)
                    cline = count - 1;
            } else {
                cline = 0;
            }

            if (cline == oldsline) {
                gsound_play_sfx_file("ib1p1xx1");
                rc = 1;
            }
            oldsline = cline;
            refreshProc();
        } else if (keyCode == 502 || keyCode == KEY_ESCAPE || game_user_wants_to_quit != 0) {
            rc = 2;
        } else {
            switch (keyCode) {
            case KEY_ARROW_UP:
                oldsline = -2;

                crow--;
                if (crow < 0) {
                    crow = 0;

                    cline--;
                    if (cline < 0) {
                        cline = 0;
                    }
                }

                refreshProc();
                break;
            case KEY_ARROW_DOWN:
                oldsline = -2;

                if (count > 11) {
                    crow++;
                    if (crow > count - 11) {
                        crow = count - 11;

                        cline++;
                        if (cline > 10) {
                            cline = 10;
                        }
                    }
                } else {
                    cline++;
                    if (cline > count - 1) {
                        cline = count - 1;
                    }
                }

                refreshProc();
                break;
            case 572:
                repFtime = 4;
                oldsline = -2;

                do {
                    sharedFpsLimiter.mark();

                    frame_time = get_time();
                    if (v19 <= 14.4) {
                        v19++;
                    }

                    if (v19 == 1 || v19 > 14.4) {
                        if (v19 > 14.4) {
                            repFtime++;
                            if (repFtime > 24) {
                                repFtime = 24;
                            }
                        }

                        crow--;
                        if (crow < 0) {
                            crow = 0;

                            cline--;
                            if (cline < 0) {
                                cline = 0;
                            }
                        }
                        refreshProc();
                    }

                    if (v19 < 14.4) {
                        while (elapsed_time(frame_time) < 1000 / 24) {
                        }
                    } else {
                        while (elapsed_time(frame_time) < 1000 / repFtime) {
                        }
                    }

                    renderPresent();
                    sharedFpsLimiter.throttle();
                } while (get_input() != 574);

                break;
            case 573:
                oldsline = -2;
                repFtime = 4;

                if (count > 11) {
                    do {
                        sharedFpsLimiter.mark();

                        frame_time = get_time();
                        if (v19 <= 14.4) {
                            v19++;
                        }

                        if (v19 == 1 || v19 > 14.4) {
                            if (v19 > 14.4) {
                                repFtime++;
                                if (repFtime > 24) {
                                    repFtime = 24;
                                }
                            }

                            crow++;
                            if (crow > count - 11) {
                                crow = count - 11;

                                cline++;
                                if (cline > 10) {
                                    cline = 10;
                                }
                            }

                            refreshProc();
                        }

                        if (v19 < 14.4) {
                            while (elapsed_time(frame_time) < 1000 / 24) {
                            }
                        } else {
                            while (elapsed_time(frame_time) < 1000 / repFtime) {
                            }
                        }

                        renderPresent();
                        sharedFpsLimiter.throttle();
                    } while (get_input() != 575);
                } else {
                    do {
                        sharedFpsLimiter.mark();

                        frame_time = get_time();
                        if (v19 <= 14.4) {
                            v19++;
                        }

                        if (v19 == 1 || v19 > 14.4) {
                            if (v19 > 14.4) {
                                repFtime++;
                                if (repFtime > 24) {
                                    repFtime = 24;
                                }
                            }

                            cline++;
                            if (cline > count - 1) {
                                cline = count - 1;
                            }

                            refreshProc();
                        }

                        if (v19 < 14.4) {
                            while (elapsed_time(frame_time) < 1000 / 24) {
                            }
                        } else {
                            while (elapsed_time(frame_time) < 1000 / repFtime) {
                            }
                        }

                        renderPresent();
                        sharedFpsLimiter.throttle();
                    } while (get_input() != 575);
                }
                break;
            case KEY_HOME:
                crow = 0;
                cline = 0;
                oldsline = -2;
                refreshProc();
                break;
            case KEY_END:
                oldsline = -2;
                if (count > 11) {
                    crow = count - 11;
                    cline = 10;
                } else {
                    cline = count - 1;
                }
                refreshProc();
                break;
            default:
                if (elapsed_time(frame_time) > 700) {
                    frame_time = get_time();
                    oldsline = -2;
                }
                break;
            }
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    return rc;
}

// 0x436CE4
static int ListDPerks()
{
    buf_to_buf(
        pbckgnd + PERK_WINDOW_WIDTH * 43 + 45,
        192,
        129,
        PERK_WINDOW_WIDTH,
        pwin_buf + PERK_WINDOW_WIDTH * 43 + 45,
        PERK_WINDOW_WIDTH);

    text_font(101);

    int perks[PERK_COUNT];
    int count = perk_make_list(perks);
    if (count == 0) {
        return 0;
    }

    for (int perk = 0; perk < PERK_COUNT; perk++) {
        name_sort_list[perk].value = 0;
        name_sort_list[perk].name = NULL;
    }

    for (int index = 0; index < count; index++) {
        name_sort_list[index].value = perks[index];
        name_sort_list[index].name = perk_name(perks[index]);
    }

    qsort(name_sort_list, count, sizeof(*name_sort_list), name_sort_comp);

    int v16 = count - crow;
    if (v16 > 11) {
        v16 = 11;
    }

    v16 += crow;

    int y = 43;
    int yStep = text_height() + 2;
    for (int index = crow; index < v16; index++) {
        int color;
        if (index == crow + cline) {
            color = colorTable[32747];
        } else {
            color = colorTable[992];
        }

        text_to_buf(pwin_buf + PERK_WINDOW_WIDTH * y + 45, name_sort_list[index].name, PERK_WINDOW_WIDTH, PERK_WINDOW_WIDTH, color);

        if (perk_level(name_sort_list[index].value) != 0) {
            char rankString[256];
            snprintf(rankString, sizeof(rankString), "(%d)", perk_level(name_sort_list[index].value));
            text_to_buf(pwin_buf + PERK_WINDOW_WIDTH * y + 207, rankString, PERK_WINDOW_WIDTH, PERK_WINDOW_WIDTH, color);
        }

        y += yStep;
    }

    return count;
}

// 0x436F10
void RedrwDMPrk()
{
    buf_to_buf(pbckgnd + 280, 293, PERK_WINDOW_HEIGHT, PERK_WINDOW_WIDTH, pwin_buf + 280, PERK_WINDOW_WIDTH);

    ListMyTraits(optrt_count);

    char* traitName = name_sort_list[crow + cline].name;
    char* tratDescription = trait_description(name_sort_list[crow + cline].value);
    int frmId = trait_pic(name_sort_list[crow + cline].value);
    DrawCard2(frmId, traitName, NULL, tratDescription);

    win_draw(pwin);
}

// 0x436FA4
static bool GetMutateTrait()
{
    old_fid2 = -1;
    old_str2[0] = '\0';
    frstc_draw2 = false;

    // NOTE: Uninline.
    trait_count = PC_TRAIT_MAX - get_trait_count();

    bool result = true;
    if (trait_count >= 1) {
        text_font(103);

        buf_to_buf(pbckgnd + PERK_WINDOW_WIDTH * 14 + 49, 206, text_height() + 2, PERK_WINDOW_WIDTH, pwin_buf + PERK_WINDOW_WIDTH * 15 + 49, PERK_WINDOW_WIDTH);

        // LOSE A TRAIT
        char* msg = getmsg(&editor_message_file, &mesg, 154);
        text_to_buf(pwin_buf + PERK_WINDOW_WIDTH * 16 + 49, msg, PERK_WINDOW_WIDTH, PERK_WINDOW_WIDTH, colorTable[18979]);

        optrt_count = 0;
        cline = 0;
        crow = 0;
        RedrwDMPrk();

        int rc = InputPDLoop(trait_count, RedrwDMPrk);
        if (rc == 1) {
            if (cline == 0) {
                if (trait_count == 1) {
                    temp_trait[0] = -1;
                    temp_trait[1] = -1;
                } else {
                    if (name_sort_list[0].value == temp_trait[0]) {
                        temp_trait[0] = temp_trait[1];
                        temp_trait[1] = -1;
                    } else {
                        temp_trait[1] = -1;
                    }
                }
            } else {
                if (name_sort_list[0].value == temp_trait[0]) {
                    temp_trait[1] = -1;
                } else {
                    temp_trait[0] = temp_trait[1];
                    temp_trait[1] = -1;
                }
            }
        } else {
            result = false;
        }
    }

    if (result) {
        text_font(103);

        buf_to_buf(pbckgnd + PERK_WINDOW_WIDTH * 14 + 49, 206, text_height() + 2, PERK_WINDOW_WIDTH, pwin_buf + PERK_WINDOW_WIDTH * 15 + 49, PERK_WINDOW_WIDTH);

        // PICK A NEW TRAIT
        char* msg = getmsg(&editor_message_file, &mesg, 153);
        text_to_buf(pwin_buf + PERK_WINDOW_WIDTH * 16 + 49, msg, PERK_WINDOW_WIDTH, PERK_WINDOW_WIDTH, colorTable[18979]);

        cline = 0;
        crow = 0;
        optrt_count = 1;

        RedrwDMPrk();

        int count = 16 - trait_count;
        if (count > 16) {
            count = 16;
        }

        int rc = InputPDLoop(count, RedrwDMPrk);
        if (rc == 1) {
            if (trait_count != 0) {
                temp_trait[1] = name_sort_list[cline + crow].value;
            } else {
                temp_trait[0] = name_sort_list[cline + crow].value;
                temp_trait[1] = -1;
            }

            trait_set(temp_trait[0], temp_trait[1]);
        } else {
            result = false;
        }
    }

    if (!result) {
        memcpy(temp_trait, trait_back, sizeof(temp_trait));
    }

    return result;
}

// 0x43727C
static void RedrwDMTagSkl()
{
    buf_to_buf(pbckgnd + 280, 293, PERK_WINDOW_HEIGHT, PERK_WINDOW_WIDTH, pwin_buf + 280, PERK_WINDOW_WIDTH);

    ListNewTagSkills();

    char* name = name_sort_list[crow + cline].name;
    char* description = skill_description(name_sort_list[crow + cline].value);
    int frmId = skill_pic(name_sort_list[crow + cline].value);
    DrawCard2(frmId, name, NULL, description);

    win_draw(pwin);
}

// 0x43730C
static bool Add4thTagSkill()
{
    text_font(103);

    buf_to_buf(pbckgnd + 573 * 14 + 49, 206, text_height() + 2, 573, pwin_buf + 573 * 15 + 49, 573);

    // PICK A NEW TAG SKILL
    char* messageListItemText = getmsg(&editor_message_file, &mesg, 155);
    text_to_buf(pwin_buf + 573 * 16 + 49, messageListItemText, 573, 573, colorTable[18979]);

    cline = 0;
    crow = 0;
    old_fid2 = -1;
    old_str2[0] = '\0';
    frstc_draw2 = false;
    RedrwDMTagSkl();

    int rc = InputPDLoop(optrt_count, RedrwDMTagSkl);
    if (rc != 1) {
        memcpy(temp_tag_skill, tag_skill_back, sizeof(temp_tag_skill));
        skill_set_tags(tag_skill_back, NUM_TAGGED_SKILLS);
        return false;
    }

    temp_tag_skill[3] = name_sort_list[crow + cline].value;
    skill_set_tags(temp_tag_skill, NUM_TAGGED_SKILLS);

    return true;
}

// 0x437430
static void ListNewTagSkills()
{
    buf_to_buf(pbckgnd + PERK_WINDOW_WIDTH * 43 + 45, 192, 129, PERK_WINDOW_WIDTH, pwin_buf + PERK_WINDOW_WIDTH * 43 + 45, PERK_WINDOW_WIDTH);

    text_font(101);

    optrt_count = 0;

    int y = 43;
    int yStep = text_height() + 2;

    for (int skill = 0; skill < SKILL_COUNT; skill++) {
        if (skill != temp_tag_skill[0] && skill != temp_tag_skill[1] && skill != temp_tag_skill[2] && skill != temp_tag_skill[3]) {
            name_sort_list[optrt_count].value = skill;
            name_sort_list[optrt_count].name = skill_name(skill);
            optrt_count++;
        }
    }

    qsort(name_sort_list, optrt_count, sizeof(*name_sort_list), name_sort_comp);

    for (int index = crow; index < crow + 11; index++) {
        int color;
        if (index == cline + crow) {
            color = colorTable[32747];
        } else {
            color = colorTable[992];
        }

        text_to_buf(pwin_buf + PERK_WINDOW_WIDTH * y + 45, name_sort_list[index].name, PERK_WINDOW_WIDTH, PERK_WINDOW_WIDTH, color);
        y += yStep;
    }
}

// 0x437574
static int ListMyTraits(int a1)
{
    buf_to_buf(pbckgnd + PERK_WINDOW_WIDTH * 43 + 45, 192, 129, PERK_WINDOW_WIDTH, pwin_buf + PERK_WINDOW_WIDTH * 43 + 45, PERK_WINDOW_WIDTH);

    text_font(101);

    int y = 43;
    int yStep = text_height() + 2;

    if (a1 != 0) {
        int count = 0;
        for (int trait = 0; trait < TRAIT_COUNT; trait++) {
            if (trait != trait_back[0] && trait != trait_back[1]) {
                name_sort_list[count].value = trait;
                name_sort_list[count].name = trait_name(trait);
                count++;
            }
        }

        qsort(name_sort_list, count, sizeof(*name_sort_list), name_sort_comp);

        for (int index = crow; index < crow + 11; index++) {
            int color;
            if (index == cline + crow) {
                color = colorTable[32747];
            } else {
                color = colorTable[992];
            }

            text_to_buf(pwin_buf + PERK_WINDOW_WIDTH * y + 45, name_sort_list[index].name, PERK_WINDOW_WIDTH, PERK_WINDOW_WIDTH, color);
            y += yStep;
        }
    } else {
        // NOTE: Original code does not use loop.
        for (int index = 0; index < PC_TRAIT_MAX; index++) {
            name_sort_list[index].value = temp_trait[index];
            name_sort_list[index].name = trait_name(temp_trait[index]);
        }

        if (trait_count > 1) {
            qsort(name_sort_list, trait_count, sizeof(*name_sort_list), name_sort_comp);
        }

        for (int index = 0; index < trait_count; index++) {
            int color;
            if (index == cline) {
                color = colorTable[32747];
            } else {
                color = colorTable[992];
            }

            text_to_buf(pwin_buf + PERK_WINDOW_WIDTH * y + 45, name_sort_list[index].name, PERK_WINDOW_WIDTH, PERK_WINDOW_WIDTH, color);
            y += yStep;
        }
    }
    return 0;
}

// 0x43775C
static int name_sort_comp(const void* a1, const void* a2)
{
    EditorSortableEntry* v1 = (EditorSortableEntry*)a1;
    EditorSortableEntry* v2 = (EditorSortableEntry*)a2;
    return strcmp(v1->name, v2->name);
}

// 0x437768
static int DrawCard2(int frmId, const char* name, const char* rank, char* description)
{
    int fid = art_id(OBJ_TYPE_SKILLDEX, frmId, 0, 0, 0);

    CacheEntry* handle;
    int width;
    int height;
    unsigned char* data = art_lock(fid, &handle, &width, &height);
    if (data == NULL) {
        return -1;
    }

    buf_to_buf(data, width, height, width, pwin_buf + PERK_WINDOW_WIDTH * 64 + 413, PERK_WINDOW_WIDTH);

    // Calculate width of transparent pixels on the left side of the image. This
    // space will be occupied by description (in addition to fixed width).
    int extraDescriptionWidth = 150;
    for (int y = 0; y < height; y++) {
        unsigned char* stride = data;
        for (int x = 0; x < width; x++) {
            if (HighRGB(*stride) < 2) {
                if (extraDescriptionWidth > x) {
                    extraDescriptionWidth = x;
                }
            }
            stride++;
        }
        data += width;
    }

    // Add gap between description and image.
    extraDescriptionWidth -= 8;
    if (extraDescriptionWidth < 0) {
        extraDescriptionWidth = 0;
    }

    text_font(102);
    int nameHeight = text_height();

    text_to_buf(pwin_buf + PERK_WINDOW_WIDTH * 27 + 280, name, PERK_WINDOW_WIDTH, PERK_WINDOW_WIDTH, colorTable[0]);

    if (rank != NULL) {
        int rankX = text_width(name) + 280 + 8;
        text_font(101);

        int rankHeight = text_height();
        text_to_buf(pwin_buf + PERK_WINDOW_WIDTH * (23 + nameHeight - rankHeight) + rankX, rank, PERK_WINDOW_WIDTH, PERK_WINDOW_WIDTH, colorTable[0]);
    }

    win_line(pwin, 280, 27 + nameHeight, 545, 27 + nameHeight, colorTable[0]);
    win_line(pwin, 280, 28 + nameHeight, 545, 28 + nameHeight, colorTable[0]);

    text_font(101);

    int yStep = text_height() + 1;
    int y = 70;

    short beginnings[WORD_WRAP_MAX_COUNT];
    short count;
    if (word_wrap(description, 133 + extraDescriptionWidth, beginnings, &count) != 0) {
        // FIXME: Leaks handle.
        return -1;
    }

    for (int index = 0; index < count - 1; index++) {
        char* beginning = description + beginnings[index];
        char* ending = description + beginnings[index + 1];

        char ch = *ending;
        *ending = '\0';

        text_to_buf(pwin_buf + PERK_WINDOW_WIDTH * y + 280, beginning, PERK_WINDOW_WIDTH, PERK_WINDOW_WIDTH, colorTable[0]);

        *ending = ch;

        y += yStep;
    }

    if (frmId != old_fid2 || strcmp(old_str2, name) != 0) {
        if (frstc_draw2) {
            gsound_play_sfx_file("isdxxxx1");
        }
    }

    strcpy(old_str2, name);
    old_fid2 = frmId;
    frstc_draw2 = true;

    art_ptr_unlock(handle);

    return 0;
}

// 0x437AA8
static void push_perks()
{
    int perk;

    for (perk = 0; perk < PERK_COUNT; perk++) {
        perk_back[perk] = perk_level(perk);
    }
}

// 0x437AC8
static void pop_perks()
{
    int perk;

    for (perk = 0; perk < PERK_COUNT; perk++) {
        while (perk_level(perk) > perk_back[perk]) {
            perk_sub(perk);
        }
    }

    for (perk = 0; perk < PERK_COUNT; perk++) {
        while (perk_level(perk) < perk_back[perk]) {
            perk_add(perk);
        }
    }
}

// 0x437B18
static int PerkCount()
{
    int perk;
    int perkCount;

    perkCount = 0;
    for (perk = 0; perk < PERK_COUNT; perk++) {
        if (perk_level(perk) > 0) {
            perkCount++;
            if (perkCount >= 7) {
                break;
            }
        }
    }

    return perkCount;
}

// Validate SPECIAL stats are <= 10.
//
// 0x437B3C
static int is_supper_bonus()
{
    for (int stat = 0; stat < 7; stat++) {
        int v1 = stat_get_base(obj_dude, stat);
        int v2 = stat_get_bonus(obj_dude, stat);
        if (v1 + v2 > 10) {
            return 1;
        }
    }

    return 0;
}

} // namespace fallout
