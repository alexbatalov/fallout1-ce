#include "game/map.h"

#include <stdio.h>
#include <string.h>

#include <vector>

#include "game/anim.h"
#include "game/automap.h"
#include "game/combat.h"
#include "game/critter.h"
#include "game/cycle.h"
#include "game/editor.h"
#include "game/game.h"
#include "game/gconfig.h"
#include "game/gmouse.h"
#include "game/gmovie.h"
#include "game/gsound.h"
#include "game/intface.h"
#include "game/item.h"
#include "game/light.h"
#include "game/loadsave.h"
#include "game/object.h"
#include "game/palette.h"
#include "game/pipboy.h"
#include "game/protinst.h"
#include "game/proto.h"
#include "game/queue.h"
#include "game/roll.h"
#include "game/scripts.h"
#include "game/textobj.h"
#include "game/tile.h"
#include "game/worldmap.h"
#include "platform_compat.h"
#include "plib/color/color.h"
#include "plib/gnw/debug.h"
#include "plib/gnw/gnw.h"
#include "plib/gnw/grbuf.h"
#include "plib/gnw/input.h"
#include "plib/gnw/intrface.h"
#include "plib/gnw/memory.h"
#include "plib/gnw/svga.h"

namespace fallout {

static int map_age_dead_critters();
static void map_match_map_number();
static void map_display_draw(Rect* rect);
static void map_scroll_refresh_game(Rect* rect);
static void map_scroll_refresh_mapper(Rect* rect);
static int map_allocate_global_vars(int count);
static void map_free_global_vars();
static int map_load_global_vars(DB_FILE* stream);
static int map_allocate_local_vars(int count);
static void map_free_local_vars();
static int map_load_local_vars(DB_FILE* stream);
static void map_place_dude_and_mouse();
static void square_init();
static void square_reset();
static int square_load(DB_FILE* stream, int a2);
static int map_write_MapData(MapHeader* ptr, DB_FILE* stream);
static int map_read_MapData(MapHeader* ptr, DB_FILE* stream);

// 0x4735CE
static const short city_vs_city_idx_table[MAP_COUNT][5] = {
    /*  DESERT1 */ { -1, -1, -1, -1, -1 },
    /*  DESERT2 */ { -1, -1, -1, -1, -1 },
    /*  DESERT3 */ { -1, -1, -1, -1, -1 },
    /*  HALLDED */ { MAP_VAULTNEC, MAP_HOTEL, MAP_WATRSHD, -1, -1 },
    /*    HOTEL */ { MAP_HALLDED, MAP_VAULTNEC, MAP_WATRSHD, -1, -1 },
    /*  WATRSHD */ { MAP_HALLDED, MAP_HOTEL, MAP_VAULTNEC, -1, -1 },
    /*  VAULT13 */ { MAP_V13ENT, -1, -1, -1, -1 },
    /* VAULTENT */ { MAP_VAULTBUR, -1, -1, -1, -1 },
    /* VAULTBUR */ { MAP_VAULTENT, -1, -1, -1, -1 },
    /* VAULTNEC */ { MAP_HALLDED, MAP_HOTEL, MAP_WATRSHD, -1, -1 },
    /*  JUNKENT */ { MAP_JUNKCSNO, MAP_JUNKKILL, -1, -1, -1 },
    /* JUNKCSNO */ { MAP_JUNKENT, MAP_JUNKKILL, -1, -1, -1 },
    /* JUNKKILL */ { MAP_JUNKENT, MAP_JUNKCSNO, -1, -1, -1 },
    /* BROHDENT */ { MAP_BROHD12, MAP_BROHD34, MAP_BRODEAD, -1, -1 },
    /*  BROHD12 */ { MAP_BROHDENT, MAP_BROHD34, MAP_BRODEAD, -1, -1 },
    /*  BROHD34 */ { MAP_BROHDENT, MAP_BROHD12, MAP_BRODEAD, -1, -1 },
    /*    CAVES */ { MAP_SHADYE, MAP_SHADYW, -1, -1, -1 },
    /* CHILDRN1 */ { MAP_CHILDRN2, MAP_CHILDEAD, MAP_MSTRLR12, MAP_MSTRLR34, -1 },
    /* CHILDRN2 */ { MAP_CHILDRN1, MAP_CHILDEAD, MAP_MSTRLR12, MAP_MSTRLR34, -1 },
    /*    CITY1 */ { -1, -1, -1, -1, -1 },
    /*   COAST1 */ { -1, -1, -1, -1, -1 },
    /*   COAST2 */ { -1, -1, -1, -1, -1 },
    /* COLATRUK */ { -1, -1, -1, -1, -1 },
    /*  FSAUSER */ { -1, -1, -1, -1, -1 },
    /*  RAIDERS */ { -1, -1, -1, -1, -1 },
    /*   SHADYE */ { MAP_CAVES, MAP_SHADYW, -1, -1, -1 },
    /*   SHADYW */ { MAP_CAVES, MAP_SHADYE, -1, -1, -1 },
    /*  GLOWENT */ { MAP_GLOW1, MAP_GLOW2, -1, -1, -1 },
    /* LAADYTUM */ { MAP_LAFOLLWR, MAP_LABLADES, MAP_LARIPPER, MAP_LAGUNRUN, -1 },
    /* LAFOLLWR */ { MAP_LAADYTUM, MAP_LABLADES, MAP_LARIPPER, MAP_LAGUNRUN, -1 },
    /*    MBENT */ { MAP_MBSTRG12, MAP_MBVATS12, MAP_MBDEAD, -1, -1 },
    /* MBSTRG12 */ { MAP_MBENT, MAP_MBVATS12, MAP_MBDEAD, -1, -1 },
    /* MBVATS12 */ { MAP_MBENT, MAP_MBSTRG12, MAP_MBDEAD, -1, -1 },
    /* MSTRLR12 */ { MAP_MSTRLR34, MAP_CHILDEAD, MAP_CHILDRN1, MAP_CHILDRN2, -1 },
    /* MSTRLR34 */ { MAP_MSTRLR12, MAP_CHILDEAD, MAP_CHILDRN1, MAP_CHILDRN2, -1 },
    /*   V13ENT */ { MAP_VAULT13, -1, -1, -1, -1 },
    /*   HUBENT */ { MAP_DETHCLAW, MAP_HUBDWNTN, MAP_HUBHEIGT, MAP_HUBOLDTN, MAP_HUBWATER },
    /* DETHCLAW */ { MAP_HUBENT, MAP_HUBDWNTN, MAP_HUBHEIGT, MAP_HUBOLDTN, MAP_HUBWATER },
    /* HUBDWNTN */ { MAP_HUBENT, MAP_DETHCLAW, MAP_HUBHEIGT, MAP_HUBOLDTN, MAP_HUBWATER },
    /* HUBHEIGT */ { MAP_HUBENT, MAP_DETHCLAW, MAP_HUBDWNTN, MAP_HUBOLDTN, MAP_HUBWATER },
    /* HUBOLDTN */ { MAP_HUBENT, MAP_DETHCLAW, MAP_HUBDWNTN, MAP_HUBHEIGT, MAP_HUBWATER },
    /* HUBWATER */ { MAP_HUBENT, MAP_DETHCLAW, MAP_HUBDWNTN, MAP_HUBHEIGT, MAP_HUBOLDTN },
    /*    GLOW1 */ { MAP_GLOWENT, MAP_GLOW2, -1, -1, -1 },
    /*    GLOW2 */ { MAP_GLOWENT, MAP_GLOW1, -1, -1, -1 },
    /* LABLADES */ { MAP_LAADYTUM, MAP_LAFOLLWR, MAP_LARIPPER, MAP_LAGUNRUN, -1 },
    /* LARIPPER */ { MAP_LAADYTUM, MAP_LAFOLLWR, MAP_LABLADES, MAP_LAGUNRUN, -1 },
    /* LAGUNRUN */ { MAP_LAADYTUM, MAP_LAFOLLWR, MAP_LABLADES, MAP_LARIPPER, -1 },
    /* CHILDEAD */ { MAP_CHILDRN1, MAP_CHILDRN2, MAP_MSTRLR12, MAP_MSTRLR34, -1 },
    /*   MBDEAD */ { MAP_MBENT, MAP_MBSTRG12, MAP_MBVATS12, -1, -1 },
    /*  MOUNTN1 */ { -1, -1, -1, -1, -1 },
    /*  MOUNTN2 */ { -1, -1, -1, -1, -1 },
    /*     FOOT */ { -1, -1, -1, -1, -1 },
    /*   TARDIS */ { -1, -1, -1, -1, -1 },
    /*  TALKCOW */ { -1, -1, -1, -1, -1 },
    /*  USEDCAR */ { -1, -1, -1, -1, -1 },
    /*  BRODEAD */ { MAP_BROHDENT, MAP_BROHD12, MAP_BROHD34, -1, -1 },
    /* DESCRVN1 */ { -1, -1, -1, -1, -1 },
    /* DESCRVN2 */ { -1, -1, -1, -1, -1 },
    /* MNTCRVN1 */ { -1, -1, -1, -1, -1 },
    /* MNTCRVN2 */ { -1, -1, -1, -1, -1 },
    /*   VIPERS */ { -1, -1, -1, -1, -1 },
    /* DESCRVN3 */ { -1, -1, -1, -1, -1 },
    /* MNTCRVN3 */ { -1, -1, -1, -1, -1 },
    /* DESCRVN4 */ { -1, -1, -1, -1, -1 },
    /* MNTCRVN4 */ { -1, -1, -1, -1, -1 },
    /*  HUBMIS1 */ { -1, -1, -1, -1, -1 },
};

// 0x473864
static const short shrtnames[MAP_COUNT] = {
    /*  DESERT1 */ 100,
    /*  DESERT2 */ 100,
    /*  DESERT3 */ 100,
    /*  HALLDED */ 505,
    /*    HOTEL */ 505,
    /*  WATRSHD */ 505,
    /*  VAULT13 */ 500,
    /* VAULTENT */ 501,
    /* VAULTBUR */ 501,
    /* VAULTNEC */ 505,
    /*  JUNKENT */ 503,
    /* JUNKCSNO */ 503,
    /* JUNKKILL */ 503,
    /* BROHDENT */ 507,
    /*  BROHD12 */ 507,
    /*  BROHD34 */ 507,
    /*    CAVES */ 116,
    /* CHILDRN1 */ 511,
    /* CHILDRN2 */ 511,
    /*    CITY1 */ 119,
    /*   COAST1 */ 120,
    /*   COAST2 */ 120,
    /* COLATRUK */ 100,
    /*  FSAUSER */ 100,
    /*  RAIDERS */ 504,
    /*   SHADYE */ 502,
    /*   SHADYW */ 502,
    /*  GLOWENT */ 509,
    /* LAADYTUM */ 510,
    /* LAFOLLWR */ 510,
    /*    MBENT */ 508,
    /* MBSTRG12 */ 508,
    /* MBVATS12 */ 508,
    /* MSTRLR12 */ 511,
    /* MSTRLR34 */ 511,
    /*   V13ENT */ 500,
    /*   HUBENT */ 506,
    /* DETHCLAW */ 506,
    /* HUBDWNTN */ 506,
    /* HUBHEIGT */ 506,
    /* HUBOLDTN */ 506,
    /* HUBWATER */ 506,
    /*    GLOW1 */ 509,
    /*    GLOW2 */ 509,
    /* LABLADES */ 510,
    /* LARIPPER */ 510,
    /* LAGUNRUN */ 510,
    /* CHILDEAD */ 511,
    /*   MBDEAD */ 508,
    /*  MOUNTN1 */ 149,
    /*  MOUNTN2 */ 149,
    /*     FOOT */ 100,
    /*   TARDIS */ 100,
    /*  TALKCOW */ 100,
    /*  USEDCAR */ 100,
    /*  BRODEAD */ 507,
    /* DESCRVN1 */ 100,
    /* DESCRVN2 */ 100,
    /* MNTCRVN1 */ 149,
    /* MNTCRVN2 */ 149,
    /*   VIPERS */ 160,
    /* DESCRVN3 */ 100,
    /* MNTCRVN3 */ 149,
    /* DESCRVN4 */ 100,
    /* MNTCRVN4 */ 149,
    /*  HUBMIS1 */ 506,
};

// 0x50B058
char byte_50B058[] = "";

// 0x50B30C
char _aErrorF2[] = "ERROR! F2";

// 0x505ACC
static IsoWindowRefreshProc* map_scroll_refresh = map_scroll_refresh_game;

// 0x505AD0
static int map_data_elev_flags[ELEVATION_COUNT] = {
    2,
    4,
    8,
};

// 0x505ADC
static unsigned int map_last_scroll_time = 0;

// 0x505AE0
static bool map_bk_enabled = false;

// 0x505AE4
int map_script_id = -1;

// 0x505AE8
int* map_local_vars = NULL;

// 0x505AEC
int* map_global_vars = NULL;

// 0x505AF0
int num_map_local_vars = 0;

// 0x505AF4
int num_map_global_vars = 0;

// 0x505AF8
int map_elevation = 0;

// 0x612DE4
TileData square_data[ELEVATION_COUNT];

// 0x6302A4
static MapTransition map_state;

// 0x6302B4
static Rect map_display_rect;

// map.msg
//
// 0x6302C4
MessageList map_msg_file;

// 0x6302CC
static unsigned char* display_buf;

// 0x6302D0
MapHeader map_data;

// 0x6303BC
TileData* square[ELEVATION_COUNT];

// 0x6303C8
int display_win;

static std::vector<void*> map_global_pointers;
static std::vector<void*> map_local_pointers;

// 0x4738E8
int iso_init()
{
    tile_disable_scroll_limiting();
    tile_disable_scroll_blocking();

    // NOTE: Uninline.
    square_init();

    display_win = win_add(0, 0, screenGetWidth(), screenGetHeight() - INTERFACE_BAR_HEIGHT, 256, 10);
    if (display_win == -1) {
        debug_printf("win_add failed in iso_init\n");
        return -1;
    }

    display_buf = win_get_buf(display_win);
    if (display_buf == NULL) {
        debug_printf("win_get_buf failed in iso_init\n");
        return -1;
    }

    if (win_get_rect(display_win, &map_display_rect) != 0) {
        debug_printf("win_get_rect failed in iso_init\n");
        return -1;
    }

    if (art_init() != 0) {
        debug_printf("art_init failed in iso_init\n");
        return -1;
    }

    debug_printf(">art_init\t\t");

    if (tile_init(square, SQUARE_GRID_WIDTH, SQUARE_GRID_HEIGHT, HEX_GRID_WIDTH, HEX_GRID_HEIGHT, display_buf, scr_size.lrx - scr_size.ulx + 1, scr_size.lry - scr_size.uly - 99, scr_size.lrx - scr_size.ulx + 1, map_display_draw) != 0) {
        debug_printf("tile_init failed in iso_init\n");
        return -1;
    }

    debug_printf(">tile_init\t\t");

    if (obj_init(display_buf, scr_size.lrx - scr_size.ulx + 1, scr_size.lry - scr_size.uly - 99, scr_size.lrx - scr_size.ulx + 1) != 0) {
        debug_printf("obj_init failed in iso_init\n");
        return -1;
    }

    debug_printf(">obj_init\t\t");

    cycle_init();
    debug_printf(">cycle_init\t\t");

    tile_enable_scroll_blocking();
    tile_enable_scroll_limiting();

    if (intface_init() != 0) {
        debug_printf("intface_init failed in iso_init\n");
        return -1;
    }

    debug_printf(">intface_init\t\t");

    map_setup_paths();

    return 0;
}

// 0x473B04
void iso_reset()
{
    // NOTE: Uninline.
    map_free_global_vars();

    // NOTE: Uninline.
    map_free_local_vars();

    art_reset();
    tile_reset();
    obj_reset();
    cycle_reset();
    intface_reset();
}

// 0x473B64
void iso_exit()
{
    intface_exit();
    cycle_exit();
    obj_exit();
    tile_exit();
    art_exit();

    win_delete(display_win);

    // NOTE: Uninline.
    map_free_global_vars();

    // NOTE: Uninline.
    map_free_local_vars();
}

// 0x473BD0
void map_init()
{
    char* executable;
    config_get_string(&game_config, GAME_CONFIG_SYSTEM_KEY, "executable", &executable);
    if (compat_stricmp(executable, "mapper") == 0) {
        map_scroll_refresh = map_scroll_refresh_mapper;
    }

    if (message_init(&map_msg_file)) {
        char path[COMPAT_MAX_PATH];
        snprintf(path, sizeof(path), "%smap.msg", msg_path);

        if (!message_load(&map_msg_file, path)) {
            debug_printf("\nError loading map_msg_file!");
        }
    } else {
        debug_printf("\nError initing map_msg_file!");
    }

    // NOTE: Uninline.
    map_reset();
}

// 0x473C80
void map_reset()
{
    map_new_map();
    add_bk_process(gmouse_bk_process);
    gmouse_disable(0);
    win_show(display_win);
}

// 0x473CA0
void map_exit()
{
    win_hide(display_win);
    gmouse_set_cursor(MOUSE_CURSOR_ARROW);
    remove_bk_process(gmouse_bk_process);
    if (!message_exit(&map_msg_file)) {
        debug_printf("\nError exiting map_msg_file!");
    }
}

// 0x473CDC
void map_enable_bk_processes()
{
    if (!map_bk_enabled) {
        text_object_enable();
        gmouse_enable();
        add_bk_process(object_animate);
        add_bk_process(dude_fidget);
        scr_enable_critters();
        map_bk_enabled = true;
    }
}

// 0x473D18
bool map_disable_bk_processes()
{
    if (!map_bk_enabled) {
        return false;
    }

    scr_disable_critters();
    remove_bk_process(dude_fidget);
    remove_bk_process(object_animate);
    gmouse_disable(0);
    text_object_disable();

    map_bk_enabled = false;

    return true;
}

// 0x473D5C
int map_set_elevation(int elevation)
{
    if (elevation < 0 || elevation >= ELEVATION_COUNT) {
        return -1;
    }

    gmouse_3d_off();
    gmouse_set_cursor(MOUSE_CURSOR_NONE);
    map_elevation = elevation;

    // CE: Recalculate bounds.
    tile_update_bounds_base();

    register_clear(obj_dude);
    dude_stand(obj_dude, obj_dude->rotation, obj_dude->fid);
    partyMemberSyncPosition();

    if (map_script_id != -1) {
        scr_exec_map_update_scripts();
    }

    gmouse_3d_on();

    return 0;
}

// 0x473DBC
bool map_is_elevation_empty(int elevation)
{
    return elevation < 0
        || elevation >= ELEVATION_COUNT
        || (map_data.flags & map_data_elev_flags[elevation]) != 0;
}

// 0x473DE8
int map_set_global_var(int var, ProgramValue& value)
{
    if (var < 0 || var >= num_map_global_vars) {
        debug_printf("ERROR: attempt to reference map var out of range: %d", var);
        return -1;
    }

    if (value.opcode == VALUE_TYPE_PTR) {
        map_global_vars[var] = 0;
        map_global_pointers[var] = value.pointerValue;
    } else {
        map_global_vars[var] = value.integerValue;
        map_global_pointers[var] = nullptr;
    }

    return 0;
}

// 0x473E18
int map_get_global_var(int var, ProgramValue& value)
{
    if (var < 0 || var >= num_map_global_vars) {
        debug_printf("ERROR: attempt to reference map var out of range: %d", var);
        return -1;
    }

    if (map_global_pointers[var] != nullptr) {
        value.opcode = VALUE_TYPE_PTR;
        value.pointerValue = map_global_pointers[var];
    } else {
        value.opcode = VALUE_TYPE_INT;
        value.integerValue = map_global_vars[var];
    }

    return 0;
}

// 0x473E48
int map_set_local_var(int var, ProgramValue& value)
{
    if (var < 0 || var >= num_map_local_vars) {
        debug_printf("ERROR: attempt to reference local var out of range: %d", var);
        return -1;
    }

    if (value.opcode == VALUE_TYPE_PTR) {
        map_local_vars[var] = 0;
        map_local_pointers[var] = value.pointerValue;
    } else {
        map_local_vars[var] = value.integerValue;
        map_local_pointers[var] = nullptr;
    }

    return 0;
}

// 0x473E78
int map_get_local_var(int var, ProgramValue& value)
{
    if (var < 0 || var >= num_map_local_vars) {
        debug_printf("ERROR: attempt to reference local var out of range: %d", var);
        return -1;
    }

    if (map_local_pointers[var] != nullptr) {
        value.opcode = VALUE_TYPE_PTR;
        value.pointerValue = map_local_pointers[var];
    } else {
        value.opcode = VALUE_TYPE_INT;
        value.integerValue = map_local_vars[var];
    }

    return 0;
}

// Make a room to store more local variables.
//
// 0x473EA8
int map_malloc_local_var(int a1)
{
    int oldMapLocalVarsLength = num_map_local_vars;
    num_map_local_vars += a1;

    int* vars = (int*)mem_realloc(map_local_vars, sizeof(*vars) * num_map_local_vars);
    if (vars == NULL) {
        debug_printf("\nError: Ran out of memory!");
    }

    map_local_vars = vars;
    memset((unsigned char*)vars + sizeof(*vars) * oldMapLocalVarsLength, 0, sizeof(*vars) * a1);

    map_local_pointers.resize(num_map_local_vars);

    return oldMapLocalVarsLength;
}

// 0x473F14
void map_set_entrance_hex(int tile, int elevation, int rotation)
{
    map_data.enteringTile = tile;
    map_data.enteringElevation = elevation;
    map_data.enteringRotation = rotation;
}

// 0x474044
void map_set_name(const char* name)
{
    strcpy(map_data.name, name);
}

// 0x47406C
void map_get_name(char* name)
{
    strcpy(name, map_data.name);
}

// 0x474094
int map_get_name_idx(char* name, int map)
{
    MessageListItem mesg;

    if (name == NULL) {
        return -1;
    }

    name[0] = '\0';

    // FIXME: Bad check.
    if (map == -1 || map > MAP_COUNT) {
        return -1;
    }

    mesg.num = map;
    if (message_search(&map_msg_file, &mesg) != 1) {
        return -1;
    }

    strcpy(name, mesg.text);

    return 0;
}

// 0x474104
char* map_get_elev_idx(int map, int elevation)
{
    MessageListItem mesg;

    if (map < 0 || map >= MAP_COUNT) {
        return NULL;
    }

    if (elevation < 0 || elevation >= ELEVATION_COUNT) {
        return NULL;
    }

    return getmsg(&map_msg_file, &mesg, map * 3 + elevation + 200);
}

// 0x474158
bool is_map_idx_same(int map1, int map2)
{
    int index;

    if (map1 < 0 || map1 >= MAP_COUNT) {
        return 0;
    }

    if (map2 < 0 || map2 >= MAP_COUNT) {
        return 0;
    }

    for (index = 0; index < 5; index++) {
        if (city_vs_city_idx_table[map1][index] == map2) {
            return true;
        }
    }

    return false;
}

// 0x4741B8
int get_map_idx_same(int map, int index)
{
    if (index < 0 || index >= 5) {
        return -1;
    }

    if (map < 0 || map >= MAP_COUNT) {
        return -1;
    }

    return city_vs_city_idx_table[map][index];
}

// 0x4741F4
char* map_get_short_name(int map)
{
    MessageListItem mesg;

    return getmsg(&map_msg_file, &mesg, shrtnames[map]);
}

// 0x474218
char* map_get_description_idx(int map)
{
    MessageListItem mesg;

    if (map > 0) {
        mesg.num = map + 100;
        if (message_search(&map_msg_file, &mesg) == 1) {
            return mesg.text;
        }
    }

    return NULL;
}

// 0x474248
char* map_get_description()
{
    return map_get_description_idx(map_data.field_34);
}

// 0x47427C
int map_get_index_number()
{
    return map_data.field_34;
}

// 0x474284
int map_scroll(int dx, int dy)
{
    if (elapsed_time(map_last_scroll_time) < 33) {
        return -2;
    }

    map_last_scroll_time = get_time();

    int screenDx = dx * 32;
    int screenDy = dy * 24;

    if (screenDx == 0 && screenDy == 0) {
        return -1;
    }

    gmouse_3d_off();

    int centerScreenX;
    int centerScreenY;
    tile_coord(tile_center_tile, &centerScreenX, &centerScreenY, map_elevation);
    centerScreenX += screenDx + 16;
    centerScreenY += screenDy + 8;

    int newCenterTile = tile_num(centerScreenX, centerScreenY, map_elevation);
    if (newCenterTile == -1) {
        return -1;
    }

    if (tile_set_center(newCenterTile, 0) == -1) {
        return -1;
    }

    Rect r1;
    rectCopy(&r1, &map_display_rect);

    Rect r2;
    rectCopy(&r2, &r1);

    int width = scr_size.lrx - scr_size.ulx + 1;
    int pitch = width;
    int height = scr_size.lry - scr_size.uly - 99;

    if (screenDx != 0) {
        width -= 32;
    }

    if (screenDy != 0) {
        height -= 24;
    }

    if (screenDx < 0) {
        r2.lrx = r2.ulx - screenDx;
    } else {
        r2.ulx = r2.lrx - screenDx;
    }

    unsigned char* src;
    unsigned char* dest;
    int step;
    if (screenDy < 0) {
        r1.lry = r1.uly - screenDy;
        src = display_buf + pitch * (height - 1);
        dest = display_buf + pitch * (scr_size.lry - scr_size.uly - 100);
        if (screenDx < 0) {
            dest -= screenDx;
        } else {
            src += screenDx;
        }
        step = -pitch;
    } else {
        r1.uly = r1.lry - screenDy;
        dest = display_buf;
        src = display_buf + pitch * screenDy;

        if (screenDx < 0) {
            dest -= screenDx;
        } else {
            src += screenDx;
        }
        step = pitch;
    }

    for (int y = 0; y < height; y++) {
        memmove(dest, src, width);
        dest += step;
        src += step;
    }

    if (screenDx != 0) {
        map_scroll_refresh(&r2);
    }

    if (screenDy != 0) {
        map_scroll_refresh(&r1);
    }

    win_draw(display_win);

    return 0;
}

// 0x4744C0
char* map_file_path(char* name)
{
    // 0x6303CC
    static char map_path[COMPAT_MAX_PATH];

    if (*name != '\\') {
        // NOTE: Uppercased from "maps".
        snprintf(map_path, sizeof(map_path), "MAPS\\%s", name);
        return map_path;
    }
    return name;
}

// 0x4744E4
void map_new_map()
{
    map_set_elevation(0);
    tile_set_center(20100, TILE_SET_CENTER_FLAG_IGNORE_SCROLL_RESTRICTIONS);
    memset(&map_state, 0, sizeof(map_state));
    map_data.enteringElevation = 0;
    map_data.enteringRotation = 0;
    map_data.localVariablesCount = 0;
    map_data.version = 19;
    map_data.name[0] = '\0';
    map_data.enteringTile = 20100;
    obj_remove_all();
    anim_stop();

    // NOTE: Uninline.
    map_free_global_vars();

    // NOTE: Uninline.
    map_free_local_vars();

    square_reset();
    map_place_dude_and_mouse();
    tile_refresh_display();
}

// 0x474614
int map_load(char* file_name)
{
    int rc;
    DB_FILE* stream;
    char* extension;
    char* file_path;

    compat_strupr(file_name);

    rc = -1;

    extension = strstr(file_name, ".MAP");
    if (extension != NULL) {
        strcpy(extension, ".SAV");

        file_path = map_file_path(file_name);

        stream = db_fopen(file_path, "rb");
        strcpy(extension, ".MAP");
        db_fclose(stream);

        if (stream != NULL) {
            rc = map_load_in_game(file_name);
            PlayCityMapMusic();
        }
    }

    if (rc == -1) {
        file_path = map_file_path(file_name);
        stream = db_fopen(file_path, "rb");
        if (stream != NULL) {
            rc = map_load_file(stream);
            db_fclose(stream);
        }

        if (rc == 0) {
            strcpy(map_data.name, file_name);
            obj_dude->data.critter.combat.whoHitMe = NULL;
        }
    }

    return rc;
}

// 0x4746E4
int map_load_idx(int map)
{
    char name[16];
    int rc;

    scr_set_ext_param(map_script_id, map);

    if (map_get_name_idx(name, map) == -1) {
        return -1;
    }

    rc = map_load(name);

    PlayCityMapMusic();

    return rc;
}

// 0x47471C
int map_load_file(DB_FILE* stream)
{
    int rc = 0;
    const char* error;

    map_save_in_game(true);
    gsound_background_play("wind2", 12, 13, 16);
    map_disable_bk_processes();
    partyMemberPrepLoad();
    gmouse_disable_scrolling();
    gmouse_set_cursor(MOUSE_CURSOR_WAIT_PLANET);
    db_register_callback(gameMouseRefreshImmediately, 8192);
    tile_disable_refresh();
    anim_stop();
    scr_disable();

    map_script_id = -1;

    do {
        error = "Invalid file handle";
        if (stream == NULL) break;

        error = "Error reading header";
        if (map_read_MapData(&map_data, stream) != 0) break;

        error = "Invalid map version";
        if (map_data.version != 19) break;

        obj_remove_all();

        if (map_data.globalVariablesCount < 0) {
            map_data.globalVariablesCount = 0;
        }

        if (map_data.localVariablesCount < 0) {
            map_data.localVariablesCount = 0;
        }

        error = "Error allocating global vars";
        // NOTE: Uninline.
        if (map_allocate_global_vars(map_data.globalVariablesCount) != 0) break;

        error = "Error loading global vars";
        // NOTE: Uninline.
        if (map_load_global_vars(stream) != 0) break;

        error = "Error allocating local vars";
        // NOTE: Uninline.
        if (map_allocate_local_vars(map_data.localVariablesCount) != 0) break;

        error = "Error loading local vars";
        if (map_load_local_vars(stream) != 0) break;

        if (square_load(stream, map_data.flags) != 0) break;

        error = "Error reading scripts";
        if (scr_load(stream) != 0) break;

        error = "Error reading objects";
        if (obj_load(stream) != 0) break;

        if ((map_data.flags & 1) == 0) {
            map_fix_critter_combat_data();
        }

        error = "Error setting map elevation";
        if (map_set_elevation(map_data.enteringElevation) != 0) break;

        error = "Error setting tile center";
        if (tile_set_center(map_data.enteringTile, TILE_SET_CENTER_FLAG_IGNORE_SCROLL_RESTRICTIONS) != 0) break;

        light_set_ambient(LIGHT_LEVEL_MAX, false);
        obj_move_to_tile(obj_dude, tile_center_tile, map_elevation, NULL);
        obj_set_rotation(obj_dude, map_data.enteringRotation, NULL);
        map_match_map_number();

        if ((map_data.flags & 1) == 0) {
            char path[COMPAT_MAX_PATH];
            snprintf(path, sizeof(path), "maps\\%s", map_data.name);

            char* extension = strstr(path, ".MAP");
            if (extension == NULL) {
                extension = strstr(path, ".map");
            }

            if (extension != NULL) {
                *extension = '\0';
            }

            strcat(path, ".GAM");
            game_load_info_vars(path, "MAP_GLOBAL_VARS:", &num_map_global_vars, &map_global_vars);
            map_data.globalVariablesCount = num_map_global_vars;
        }

        scr_enable();

        if (map_data.scriptIndex > 0) {
            error = "Error creating new map script";
            if (scr_new(&map_script_id, SCRIPT_TYPE_SYSTEM) == -1) break;

            Object* object;
            int fid = art_id(OBJ_TYPE_MISC, 12, 0, 0, 0);
            obj_new(&object, fid, -1);
            object->flags |= (OBJECT_LIGHT_THRU | OBJECT_NO_SAVE | OBJECT_HIDDEN);
            obj_move_to_tile(object, 1, 0, NULL);
            object->sid = map_script_id;
            scr_set_ext_param(map_script_id, (map_data.flags & 1) == 0);

            Script* script;
            scr_ptr(map_script_id, &script);
            script->scr_script_idx = map_data.scriptIndex - 1;
            script->scr_flags |= SCRIPT_FLAG_0x08;
            object->id = new_obj_id();
            script->scr_oid = object->id;
            script->owner = object;
            scr_spatials_disable();
            exec_script_proc(map_script_id, SCRIPT_PROC_MAP_ENTER);
            scr_spatials_enable();
        }

        error = NULL;
    } while (0);

    if (error != NULL) {
        char message[100]; // TODO: Size is probably wrong.
        snprintf(message, sizeof(message), "%s while loading map, version = %d", error, map_data.version);
        debug_printf(message);
        map_new_map();
        rc = -1;
    } else {
        obj_preload_art_cache(map_data.flags);
    }

    partyMemberRecoverLoad();
    intface_show();
    map_place_dude_and_mouse();
    map_enable_bk_processes();
    gmouse_disable_scrolling();
    gmouse_set_cursor(MOUSE_CURSOR_WAIT_PLANET);

    if (scr_load_all_scripts() == -1) {
        debug_printf("\n   Error: scr_load_all_scripts failed!");
    }

    scr_exec_map_enter_scripts();
    scr_exec_map_update_scripts();
    tile_enable_refresh();

    if (map_state.map > 0) {
        if (map_state.rotation >= 0) {
            obj_set_rotation(obj_dude, map_state.rotation, NULL);
        }
    } else {
        tile_refresh_display();
    }

    gtime_q_add();
    db_register_callback(NULL, 0);
    gmouse_enable_scrolling();
    gmouse_set_cursor(MOUSE_CURSOR_NONE);

    return rc;
}

// 0x474D14
int map_load_in_game(char* fileName)
{
    debug_printf("\nMAP: Loading SAVED map.");

    char mapName[16]; // TODO: Size is probably wrong.
    strmfe(mapName, fileName, "SAV");

    int rc = map_load(mapName);

    if (game_time() >= map_data.lastVisitTime) {
        if (((game_time() - map_data.lastVisitTime) / GAME_TIME_TICKS_PER_HOUR) >= 24) {
            obj_unjam_all_locks();
        }

        if (map_age_dead_critters() == -1) {
            debug_printf("\nError: Critter aging failed on map load!");
            return -1;
        }
    }

    return rc;
}

// 0x474DB0
static int map_age_dead_critters()
{
    // if (!wmMapDeadBodiesAge()) {
    //     return 0;
    // }

    int hoursSinceLastVisit = (game_time() - map_data.lastVisitTime) / GAME_TIME_TICKS_PER_HOUR;
    if (hoursSinceLastVisit == 0) {
        return 0;
    }

    Object* obj = obj_find_first();
    while (obj != NULL) {
        if (PID_TYPE(obj->pid) == OBJ_TYPE_CRITTER
            && obj != obj_dude
            && !isPartyMember(obj)
            && !critter_is_dead(obj)) {
            obj->data.critter.combat.maneuver &= ~CRITTER_MANUEVER_FLEEING;
            if (critter_kill_count_type(obj) != KILL_TYPE_ROBOT) {
                critter_heal_hours(obj, hoursSinceLastVisit);
            }
        }
        obj = obj_find_next();
    }

    int agingType;
    if (hoursSinceLastVisit > 6 * 24) {
        agingType = 1;
    } else if (hoursSinceLastVisit > 14 * 24) {
        agingType = 2;
    } else {
        return 0;
    }

    int capacity = 100;
    int count = 0;
    Object** objects = (Object**)mem_malloc(sizeof(*objects) * capacity);

    obj = obj_find_first();
    while (obj != NULL) {
        int type = PID_TYPE(obj->pid);
        if (type == OBJ_TYPE_CRITTER) {
            if (obj != obj_dude && critter_is_dead(obj)) {
                if (critter_kill_count_type(obj) != KILL_TYPE_ROBOT) {
                    objects[count++] = obj;

                    if (count >= capacity) {
                        capacity *= 2;
                        objects = (Object**)mem_realloc(objects, sizeof(*objects) * capacity);
                        if (objects == NULL) {
                            debug_printf("\nError: Out of Memory!");
                            return -1;
                        }
                    }
                }
            }
        } else if (agingType == 2 && type == OBJ_TYPE_MISC && obj->pid == 0x500000B) {
            objects[count++] = obj;
            if (count >= capacity) {
                capacity *= 2;
                objects = (Object**)mem_realloc(objects, sizeof(*objects) * capacity);
                if (objects == NULL) {
                    debug_printf("\nError: Out of Memory!");
                    return -1;
                }
            }
        }
        obj = obj_find_next();
    }

    int rc = 0;
    for (int index = 0; index < count; index++) {
        Object* obj = objects[index];
        if (PID_TYPE(obj->pid) == OBJ_TYPE_CRITTER) {
            int blood_pid;
            if (obj->pid != 16777265 && obj->pid != 213 && obj->pid != 214) {
                blood_pid = 0x5000004;
                item_drop_all(obj, obj->tile);
            } else {
                blood_pid = 213;
            }

            Object* blood;
            if (obj_pid_new(&blood, blood_pid) == -1) {
                rc = -1;
                break;
            }

            obj_move_to_tile(blood, obj->tile, obj->elevation, NULL);

            Proto* proto;
            proto_ptr(obj->pid, &proto);

            int frame = roll_random(0, 3);
            if ((proto->critter.flags & 0x800)) {
                frame += 6;
            } else {
                if (critter_kill_count_type(obj) != KILL_TYPE_RAT
                    && critter_kill_count_type(obj) != KILL_TYPE_MANTIS) {
                    frame += 3;
                }
            }

            obj_set_frame(blood, frame, NULL);
        }

        register_clear(obj);
        obj_erase_object(obj, NULL);
    }

    mem_free(objects);

    return rc;
}

// 0x475160
int map_leave_map(MapTransition* transition)
{
    if (transition == NULL) {
        return -1;
    }

    memcpy(&map_state, transition, sizeof(map_state));

    if (map_state.map == 0) {
        map_state.map = -2;
    }

    if (isInCombat()) {
        game_user_wants_to_quit = 1;
    }

    return 0;
}

// 0x4751A4
int map_check_state()
{
    int town;
    WorldMapContext ctx;

    if (map_state.map == 0) {
        return 0;
    }

    gmouse_3d_off();
    gmouse_set_cursor(MOUSE_CURSOR_NONE);

    if (map_state.map == -1) {
        if (!isInCombat()) {
            anim_stop();

            ctx.state = 0;
            ctx.town = our_town;
            ctx = town_map(ctx);

            if (ctx.state == -1) {
                ctx.town = our_town;
            }

            world_map(ctx);
            KillWorldWin();
            memset(&map_state, 0, sizeof(map_state));
        }
    } else if (map_state.map == -2) {
        if (!isInCombat()) {
            anim_stop();
            ctx.state = 0;
            ctx.town = our_town;
            world_map(ctx);
            KillWorldWin();
            memset(&map_state, 0, sizeof(map_state));
        }
    } else {
        if (!isInCombat()) {
            // NOTE: Uninline.
            map_load_idx(map_state.map);

            if (map_state.tile != -1 && map_state.tile != 0
                && map_state.elevation >= 0 && map_state.elevation < ELEVATION_COUNT) {
                obj_move_to_tile(obj_dude, map_state.tile, map_state.elevation, NULL);
                map_set_elevation(map_state.elevation);
                obj_set_rotation(obj_dude, map_state.rotation, NULL);
            }

            if (tile_set_center(obj_dude->tile, TILE_SET_CENTER_REFRESH_WINDOW) == -1) {
                debug_printf("\nError: map: attempt to center out-of-bounds!");
            }

            memset(&map_state, 0, sizeof(map_state));

            town = xlate_mapidx_to_town(map_data.field_34);
            if (worldmap_script_jump(town, 0) == -1) {
                debug_printf("\nError: couldn't make jump on worldmap for map jump!");
            }
        }
    }

    return 0;
}

// 0x475394
void map_fix_critter_combat_data()
{
    for (Object* object = obj_find_first(); object != NULL; object = obj_find_next()) {
        if (object->pid == -1) {
            continue;
        }

        if (PID_TYPE(object->pid) != OBJ_TYPE_CRITTER) {
            continue;
        }

        if (object->data.critter.combat.whoHitMeCid == -1) {
            object->data.critter.combat.whoHitMe = NULL;
        }
    }
}

// 0x475460
int map_save()
{
    char temp[80];
    temp[0] = '\0';

    char* masterPatchesPath;
    if (config_get_string(&game_config, GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_MASTER_PATCHES_KEY, &masterPatchesPath)) {
        strcat(temp, masterPatchesPath);
        compat_mkdir(temp);

        strcat(temp, "\\MAPS");
        compat_mkdir(temp);
    }

    int rc = -1;
    if (map_data.name[0] != '\0') {
        char* mapFileName = map_file_path(map_data.name);
        DB_FILE* stream = db_fopen(mapFileName, "wb");
        if (stream != NULL) {
            rc = map_save_file(stream);
            db_fclose(stream);
        } else {
            snprintf(temp, sizeof(temp), "Unable to open %s to write!", map_data.name);
            debug_printf(temp);
        }

        if (rc == 0) {
            snprintf(temp, sizeof(temp), "%s saved.", map_data.name);
            debug_printf(temp);
        }
    } else {
        debug_printf("\nError: map_save: map header corrupt!");
    }

    return rc;
}

// 0x475590
int map_save_file(DB_FILE* stream)
{
    if (stream == NULL) {
        return -1;
    }

    scr_disable();

    for (int elevation = 0; elevation < ELEVATION_COUNT; elevation++) {
        int tile;
        for (tile = 0; tile < SQUARE_GRID_SIZE; tile++) {
            int fid;

            fid = art_id(OBJ_TYPE_TILE, square[elevation]->field_0[tile] & 0xFFF, 0, 0, 0);
            if (fid != art_id(OBJ_TYPE_TILE, 1, 0, 0, 0)) {
                break;
            }

            fid = art_id(OBJ_TYPE_TILE, (square[elevation]->field_0[tile] >> 16) & 0xFFF, 0, 0, 0);
            if (fid != art_id(OBJ_TYPE_TILE, 1, 0, 0, 0)) {
                break;
            }
        }

        if (tile == SQUARE_GRID_SIZE) {
            Object* object = obj_find_first_at(elevation);
            if (object != NULL) {
                // TODO: Implementation is slightly different, check in debugger.
                while (object != NULL && (object->flags & OBJECT_NO_SAVE)) {
                    object = obj_find_next_at();
                }

                if (object != NULL) {
                    map_data.flags &= ~map_data_elev_flags[elevation];
                } else {
                    map_data.flags |= map_data_elev_flags[elevation];
                }
            } else {
                map_data.flags |= map_data_elev_flags[elevation];
            }
        } else {
            map_data.flags &= ~map_data_elev_flags[elevation];
        }
    }

    map_data.localVariablesCount = num_map_local_vars;
    map_data.globalVariablesCount = num_map_global_vars;
    map_data.darkness = 1;

    map_write_MapData(&map_data, stream);

    if (map_data.globalVariablesCount != 0) {
        db_fwriteInt32List(stream, map_global_vars, map_data.globalVariablesCount);
    }

    if (map_data.localVariablesCount != 0) {
        db_fwriteInt32List(stream, map_local_vars, map_data.localVariablesCount);
    }

    for (int elevation = 0; elevation < ELEVATION_COUNT; elevation++) {
        if ((map_data.flags & map_data_elev_flags[elevation]) == 0) {
            db_fwriteInt32List(stream, square[elevation]->field_0, SQUARE_GRID_SIZE);
        }
    }

    char err[80];

    if (scr_save(stream) == -1) {
        snprintf(err, sizeof(err), "Error saving scripts in %s", map_data.name);
        win_msg(err, 80, 80, colorTable[31744]);
    }

    if (obj_save(stream) == -1) {
        snprintf(err, sizeof(err), "Error saving objects in %s", map_data.name);
        win_msg(err, 80, 80, colorTable[31744]);
    }

    scr_enable();

    return 0;
}

// 0x4758A8
int map_save_in_game(bool a1)
{
    if (map_data.name[0] == '\0') {
        return 0;
    }

    anim_stop();

    if (a1) {
        partyMemberPrepLoad();
        partyMemberPrepItemSaveAll();
        queue_leaving_map();
        scr_exec_map_exit_scripts();

        if (map_script_id != -1) {
            Script* script;
            scr_ptr(map_script_id, &script);
        }

        gtime_q_add();
        obj_reset_roof();
    }

    map_data.flags |= MAP_SAVED;
    map_data.lastVisitTime = game_time();

    char name[16];

    if (a1 && !YesWriteIndex(map_get_index_number(), map_elevation)) {
        debug_printf("\nNot saving RANDOM encounter map.");

        strcpy(name, map_data.name);
        strmfe(map_data.name, name, "SAV");
        MapDirEraseFile("MAPS\\", map_data.name);
        strcpy(map_data.name, name);
    } else {
        debug_printf("\n Saving \".SAV\" map.");

        strcpy(name, map_data.name);
        strmfe(map_data.name, name, "SAV");
        if (map_save() == -1) {
            return -1;
        }

        strcpy(map_data.name, name);

        automap_pip_save();
    }

    if (a1) {
        map_data.name[0] = '\0';
        obj_remove_all();
        proto_remove_all();
        square_reset();
        gtime_q_add();
    }

    return 0;
}

// 0x475A44
void map_setup_paths()
{
    char path[COMPAT_MAX_PATH];

    char* masterPatchesPath;
    if (config_get_string(&game_config, GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_MASTER_PATCHES_KEY, &masterPatchesPath)) {
        strcpy(path, masterPatchesPath);
    } else {
        strcpy(path, "DATA");
    }

    compat_mkdir(path);

    strcat(path, "\\MAPS");
    compat_mkdir(path);
}

// 0x475AEC
int map_match_map_name(const char* name)
{
    char temp[16];
    char* extension;
    int index;
    char candidate[16];

    strcpy(temp, name);
    compat_strupr(temp);

    extension = strstr(temp, ".SAV");
    if (extension != NULL) {
        strcpy(extension, ".MAP");
    }

    for (index = 0; index < MAP_COUNT; index++) {
        if (map_get_name_idx(candidate, index) == 0) {
            if (strcmp(temp, candidate) == 0) {
                return index;
            }
        }
    }

    return -1;
}

// 0x475B70
static void map_match_map_number()
{
    char temp[16];
    char* extension;
    int index;
    char candidate[16];

    if (strstr(map_data.name, ".sav") != NULL) {
        return;
    }

    if (strcmp(map_data.name, "TMP$MAP#.MAP") == 0) {
        return;
    }

    strcpy(temp, map_data.name);
    compat_strupr(temp);

    extension = strstr(temp, ".SAV");
    if (extension != NULL) {
        strcpy(extension, ".MAP");
    }

    for (index = 0; index < MAP_COUNT; index++) {
        if (map_get_name_idx(candidate, index) == 0) {
            if (strcmp(temp, candidate) == 0) {
                map_data.field_34 = index;
                return;
            }
        }
    }

    debug_printf("\nNote: Couldn't match name for map!");
    map_data.field_34 = -1;
}

// 0x475C3C
static void map_display_draw(Rect* rect)
{
    win_draw_rect(display_win, rect);
}

// 0x475C50
static void map_scroll_refresh_game(Rect* rect)
{
    Rect rectToUpdate;
    if (rect_inside_bound(rect, &map_display_rect, &rectToUpdate) == -1) {
        return;
    }

    // CE: Clear dirty rect to prevent most of the visual artifacts near map
    // edges.
    buf_fill(display_buf + rectToUpdate.uly * rectGetWidth(&map_display_rect) + rectToUpdate.ulx,
        rectGetWidth(&rectToUpdate),
        rectGetHeight(&rectToUpdate),
        rectGetWidth(&map_display_rect),
        0);

    square_render_floor(&rectToUpdate, map_elevation);
    grid_render(&rectToUpdate, map_elevation);
    obj_render_pre_roof(&rectToUpdate, map_elevation);
    square_render_roof(&rectToUpdate, map_elevation);
    bounds_render(&rectToUpdate, map_elevation);
    obj_render_post_roof(&rectToUpdate, map_elevation);
}

// 0x475CB0
static void map_scroll_refresh_mapper(Rect* rect)
{
    Rect rectToUpdate;
    if (rect_inside_bound(rect, &map_display_rect, &rectToUpdate) == -1) {
        return;
    }

    buf_fill(display_buf + rectToUpdate.uly * rectGetWidth(&map_display_rect) + rectToUpdate.ulx,
        rectGetWidth(&rectToUpdate),
        rectGetHeight(&rectToUpdate),
        rectGetWidth(&map_display_rect),
        0);

    square_render_floor(&rectToUpdate, map_elevation);
    grid_render(&rectToUpdate, map_elevation);
    obj_render_pre_roof(&rectToUpdate, map_elevation);
    square_render_roof(&rectToUpdate, map_elevation);
    obj_render_post_roof(&rectToUpdate, map_elevation);
}

// 0x475D50
static int map_allocate_global_vars(int count)
{
    map_free_global_vars();

    if (count != 0) {
        map_global_vars = (int*)mem_malloc(sizeof(*map_global_vars) * count);
        if (map_global_vars == NULL) {
            return -1;
        }

        map_global_pointers.resize(count);
    }

    num_map_global_vars = count;

    return 0;
}

// 0x475DA4
static void map_free_global_vars()
{
    if (map_global_vars != NULL) {
        mem_free(map_global_vars);
        map_global_vars = NULL;
        num_map_global_vars = 0;
    }

    map_global_pointers.clear();
}

// 0x475DC8
static int map_load_global_vars(DB_FILE* stream)
{
    if (db_freadInt32List(stream, map_global_vars, num_map_global_vars) != 0) {
        return -1;
    }

    return 0;
}

// 0x475DEC
static int map_allocate_local_vars(int count)
{
    map_free_local_vars();

    if (count != 0) {
        map_local_vars = (int*)mem_malloc(sizeof(*map_local_vars) * count);
        if (map_local_vars == NULL) {
            return -1;
        }

        map_local_pointers.resize(count);
    }

    num_map_local_vars = count;

    return 0;
}

// 0x475E40
static void map_free_local_vars()
{
    if (map_local_vars != NULL) {
        mem_free(map_local_vars);
        map_local_vars = NULL;
        num_map_local_vars = 0;
    }

    map_local_pointers.clear();
}

// 0x475E64
static int map_load_local_vars(DB_FILE* stream)
{
    if (db_freadInt32List(stream, map_local_vars, num_map_local_vars) != 0) {
        return -1;
    }

    return 0;
}

// 0x475E88
static void map_place_dude_and_mouse()
{
    if (obj_dude != NULL) {
        if (FID_ANIM_TYPE(obj_dude->fid) != ANIM_STAND) {
            obj_set_frame(obj_dude, 0, 0);
            obj_dude->fid = art_id(OBJ_TYPE_CRITTER, obj_dude->fid & 0xFFF, ANIM_STAND, (obj_dude->fid & 0xF000) >> 12, obj_dude->rotation + 1);
        }

        if (obj_dude->tile == -1) {
            obj_move_to_tile(obj_dude, tile_center_tile, map_elevation, NULL);
            obj_set_rotation(obj_dude, map_data.enteringRotation, 0);
        }

        obj_set_light(obj_dude, 4, 0x10000, 0);
        obj_dude->flags |= OBJECT_NO_SAVE;

        dude_stand(obj_dude, obj_dude->rotation, obj_dude->fid);
        partyMemberSyncPosition();
    }

    gmouse_3d_reset_fid();
    gmouse_3d_on();
}

// 0x475F58
static void square_init()
{
    int elevation;

    for (elevation = 0; elevation < ELEVATION_COUNT; elevation++) {
        square[elevation] = &(square_data[elevation]);
    }
}

// 0x475F78
static void square_reset()
{
    for (int elevation = 0; elevation < ELEVATION_COUNT; elevation++) {
        int* p = square[elevation]->field_0;
        for (int y = 0; y < SQUARE_GRID_HEIGHT; y++) {
            for (int x = 0; x < SQUARE_GRID_WIDTH; x++) {
                // TODO: Strange math, initially right, but need to figure it out and
                // check subsequent calls.
                int fid = *p;
                fid &= ~0xFFFF;
                *p = (((art_id(OBJ_TYPE_TILE, 1, 0, 0, 0) & 0xFFF) | (((fid >> 16) & 0xF000) >> 12)) << 16) | (fid & 0xFFFF);

                fid = *p;
                int v3 = (fid & 0xF000) >> 12;
                int v4 = (art_id(OBJ_TYPE_TILE, 1, 0, 0, 0) & 0xFFF) | v3;

                fid &= ~0xFFFF;

                *p = v4 | ((fid >> 16) << 16);

                p++;
            }
        }
    }
}

// 0x476084
static int square_load(DB_FILE* stream, int flags)
{
    int v6;
    int v7;
    int v8;
    int v9;

    square_reset();

    for (int elevation = 0; elevation < ELEVATION_COUNT; elevation++) {
        if ((flags & map_data_elev_flags[elevation]) == 0) {
            int* arr = square[elevation]->field_0;
            if (db_freadInt32List(stream, arr, SQUARE_GRID_SIZE) != 0) {
                return -1;
            }

            for (int tile = 0; tile < SQUARE_GRID_SIZE; tile++) {
                v6 = arr[tile];
                v6 &= ~(0xFFFF);
                v6 >>= 16;

                v7 = (v6 & 0xF000) >> 12;
                v7 &= ~(0x01);

                v8 = v6 & 0xFFF;
                v9 = arr[tile] & 0xFFFF;
                arr[tile] = ((v8 | (v7 << 12)) << 16) | v9;
            }
        }
    }

    return 0;
}

// 0x476120
static int map_write_MapData(MapHeader* ptr, DB_FILE* stream)
{
    if (db_fwriteInt32(stream, ptr->version) == -1) return -1;
    if (db_fwriteInt8List(stream, ptr->name, 16) == -1) return -1;
    if (db_fwriteInt32(stream, ptr->enteringTile) == -1) return -1;
    if (db_fwriteInt32(stream, ptr->enteringElevation) == -1) return -1;
    if (db_fwriteInt32(stream, ptr->enteringRotation) == -1) return -1;
    if (db_fwriteInt32(stream, ptr->localVariablesCount) == -1) return -1;
    if (db_fwriteInt32(stream, ptr->scriptIndex) == -1) return -1;
    if (db_fwriteInt32(stream, ptr->flags) == -1) return -1;
    if (db_fwriteInt32(stream, ptr->darkness) == -1) return -1;
    if (db_fwriteInt32(stream, ptr->globalVariablesCount) == -1) return -1;
    if (db_fwriteInt32(stream, ptr->field_34) == -1) return -1;
    if (db_fwriteInt32(stream, ptr->lastVisitTime) == -1) return -1;
    if (db_fwriteInt32List(stream, ptr->field_3C, 44) == -1) return -1;

    return 0;
}

// 0x47621C
static int map_read_MapData(MapHeader* ptr, DB_FILE* stream)
{
    if (db_freadInt32(stream, &(ptr->version)) == -1) return -1;
    if (db_freadInt8List(stream, ptr->name, 16) == -1) return -1;
    if (db_freadInt32(stream, &(ptr->enteringTile)) == -1) return -1;
    if (db_freadInt32(stream, &(ptr->enteringElevation)) == -1) return -1;
    if (db_freadInt32(stream, &(ptr->enteringRotation)) == -1) return -1;
    if (db_freadInt32(stream, &(ptr->localVariablesCount)) == -1) return -1;
    if (db_freadInt32(stream, &(ptr->scriptIndex)) == -1) return -1;
    if (db_freadInt32(stream, &(ptr->flags)) == -1) return -1;
    if (db_freadInt32(stream, &(ptr->darkness)) == -1) return -1;
    if (db_freadInt32(stream, &(ptr->globalVariablesCount)) == -1) return -1;
    if (db_freadInt32(stream, &(ptr->field_34)) == -1) return -1;
    if (db_freadInt32(stream, &(ptr->lastVisitTime)) == -1) return -1;
    if (db_freadInt32List(stream, ptr->field_3C, 44) == -1) return -1;

    return 0;
}

} // namespace fallout
