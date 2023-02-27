#include "game/scripts.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "game/actions.h"
#include "game/automap.h"
#include "game/combat.h"
#include "game/critter.h"
#include "game/elevator.h"
#include "game/endgame.h"
#include "game/game.h"
#include "game/gdialog.h"
#include "game/gmouse.h"
#include "game/gmovie.h"
#include "game/object.h"
#include "game/protinst.h"
#include "game/proto.h"
#include "game/queue.h"
#include "game/tile.h"
#include "game/worldmap.h"
#include "int/dialog.h"
#include "int/export.h"
#include "int/window.h"
#include "platform_compat.h"
#include "plib/gnw/debug.h"
#include "plib/gnw/gnw.h"
#include "plib/gnw/input.h"
#include "plib/gnw/intrface.h"
#include "plib/gnw/memory.h"

namespace fallout {

#define GAME_TIME_START_YEAR 2161
#define GAME_TIME_START_MONTH 12
#define GAME_TIME_START_DAY 5
#define GAME_TIME_START_HOUR 7
#define GAME_TIME_START_MINUTE 21

#define SCRIPT_LIST_EXTENT_SIZE 16

typedef struct ScriptListExtent {
    Script scripts[SCRIPT_LIST_EXTENT_SIZE];
    // Number of scripts in the extent
    int length;
    struct ScriptListExtent* next;
} ScriptListExtent;

typedef struct ScriptList {
    ScriptListExtent* head;
    ScriptListExtent* tail;
    // Number of extents in the script list.
    int length;
    int nextScriptId;
} ScriptList;

typedef struct ScriptState {
    unsigned int requests;
    STRUCT_664980 combatState1;
    STRUCT_664980 combatState2;
    int elevatorType;
    int explosionTile;
    int explosionElevation;
    int explosionMinDamage;
    int explosionMaxDamage;
    Object* dialogTarget;
    Object* lootingBy;
    Object* lootingFrom;
    Object* stealingBy;
    Object* stealingFrom;
} ScriptState;

static void doBkProcesses();
static void script_chk_critters();
static void script_chk_timed_events();
static int scr_build_lookup_table(Script* scr);
static int scr_index_to_name(int scriptIndex, char* name, size_t size);
static int scr_header_load();
static int scr_write_ScriptSubNode(Script* scr, DB_FILE* stream);
static int scr_write_ScriptNode(ScriptListExtent* a1, DB_FILE* stream);
static int scr_read_ScriptSubNode(Script* scr, DB_FILE* stream);
static int scr_read_ScriptNode(ScriptListExtent* a1, DB_FILE* stream);
static int scr_new_id(int scriptType);
static void scrExecMapProcScripts(int a1);

// Number of lines in scripts.lst
//
// 0x50784C
int num_script_indexes = 0;

// 0x507850
static int scr_find_first_idx = 0;

// 0x507854
static ScriptListExtent* scr_find_first_ptr = NULL;

// 0x507858
static int scr_find_first_elev = 0;

// 0x50785C
static bool scrSpatialsEnabled = true;

// 0x507860
static ScriptList scriptlists[SCRIPT_TYPE_COUNT];

// 0x5078B0
static char script_path_base[] = "scripts\\";

// 0x5078B4
static bool script_engine_running = false;

// 0x5078B8
static int script_engine_run_critters = 0;

// 0x5078BC
static int script_engine_game_mode = 0;

// Game time in ticks (1/10 second).
//
// 0x5078C0
static int fallout_game_time = (GAME_TIME_START_HOUR * 60 * 60 + GAME_TIME_START_MINUTE * 60) * 10;

// 0x5078C4
static int days_in_month[12] = {
    31, // Jan
    28, // Feb
    31, // Mar
    30, // Apr
    31, // May
    30, // Jun
    31, // Jul
    31, // Aug
    30, // Sep
    31, // Oct
    30, // Nov
    31, // Dec
};

// 0x5078F8
static const char* procTableStrs[SCRIPT_PROC_COUNT] = {
    "no_p_proc",
    "start",
    "spatial_p_proc",
    "description_p_proc",
    "pickup_p_proc",
    "drop_p_proc",
    "use_p_proc",
    "use_obj_on_p_proc",
    "use_skill_on_p_proc",
    "none_x_bad",
    "none_x_bad",
    "talk_p_proc",
    "critter_p_proc",
    "combat_p_proc",
    "damage_p_proc",
    "map_enter_p_proc",
    "map_exit_p_proc",
    "create_p_proc",
    "destroy_p_proc",
    "none_x_bad",
    "none_x_bad",
    "look_at_p_proc",
    "timed_event_p_proc",
    "map_update_p_proc",
};

// 0x507958
static unsigned char water_movie_play_flag = 0;

// 0x664F1C
static ScriptState scriptState;

// 0x662FD4
MessageList script_dialog_msgs[SCRIPT_DIALOG_MESSAGE_LIST_CAPACITY];

// scr.msg
//
// 0x664F14
MessageList script_message_file;

// TODO: Make unsigned.
//
// Returns game time in ticks (1/10 second).
//
// 0x491740
int game_time()
{
    return fallout_game_time;
}

// 0x491748
void game_time_date(int* monthPtr, int* dayPtr, int* yearPtr)
{
    int year = (fallout_game_time / GAME_TIME_TICKS_PER_DAY + (GAME_TIME_START_DAY - 1)) / 365 + GAME_TIME_START_YEAR;
    int month = GAME_TIME_START_MONTH - 1;
    int day = (fallout_game_time / GAME_TIME_TICKS_PER_DAY + (GAME_TIME_START_DAY - 1)) % 365;

    while (1) {
        int daysInMonth = days_in_month[month];
        if (day < daysInMonth) {
            break;
        }

        month++;
        day -= daysInMonth;

        if (month == 12) {
            year++;
            month = 0;
        }
    }

    if (dayPtr != NULL) {
        *dayPtr = day + 1;
    }

    if (monthPtr != NULL) {
        *monthPtr = month + 1;
    }

    if (yearPtr != NULL) {
        *yearPtr = year;
    }
}

// Returns game hour/minute in military format (hhmm).
//
// Examples:
// - 8:00 A.M. -> 800
// - 3:00 P.M. -> 1500
// - 11:59 P.M. -> 2359
//
// game_time_hour
// 0x4917D8
int game_time_hour()
{
    return 100 * ((fallout_game_time / 600) / 60 % 24) + (fallout_game_time / 600) % 60;
}

// Returns time string (h:mm)
//
// 0x491830
char* game_time_hour_str()
{
    // 0x66772C
    static char hour_str[7];

    snprintf(hour_str, sizeof(hour_str), "%d:%02d", (fallout_game_time / 600) / 60 % 24, (fallout_game_time / 600) % 60);
    return hour_str;
}

// TODO: Make unsigned.
//
// 0x49188C
void set_game_time(int time)
{
    if (time == 0) {
        time = 1;
    }

    fallout_game_time = time;
}

// 0x4918AC
void set_game_time_in_seconds(int time)
{
    // NOTE: Uninline.
    set_game_time(time * 10);
}

// 0x4918DC
void inc_game_time(int inc)
{
    fallout_game_time += inc;
}

// 0x4918E4
void inc_game_time_in_seconds(int inc)
{
    // NOTE: Uninline.
    inc_game_time(inc * 10);
}

// 0x491900
int gtime_q_add()
{
    int delay;

    delay = 10 * (60 * (60 - (fallout_game_time / 600) % 60 - 1) + 3600 * (24 - (fallout_game_time / 600) / 60 % 24 - 1) + 60);
    if (queue_add(delay, NULL, NULL, EVENT_TYPE_GAME_TIME) == -1) {
        return -1;
    }

    if (map_data.name[0] != '\0') {
        if (queue_add(600, NULL, NULL, EVENT_TYPE_MAP_UPDATE_EVENT) == -1) {
            return -1;
        }
    }

    return 0;
}

// 0x4919B0
int gtime_q_process(Object* obj, void* data)
{
    // 0x50795C
    static int moviePlaying = 0;

    int movie;
    int rc;

    movie = -1;

    debug_printf("\nQUEUE PROCESS: Midnight!");

    if (moviePlaying == 1) {
        return 0;
    }

    obj_unjam_all_locks();

    if (game_global_vars[GVAR_FIND_WATER_CHIP] != 2) {
        if (game_global_vars[GVAR_VAULT_WATER] > 0) {
            game_global_vars[GVAR_VAULT_WATER] -= 1;
            if (!dialog_active()) {
                if (game_global_vars[GVAR_VAULT_WATER] <= 100 && (water_movie_play_flag & 0x2) == 0) {
                    water_movie_play_flag |= 0x2;
                    movie = MOVIE_BOIL1;
                } else if (game_global_vars[GVAR_VAULT_WATER] <= 50 && (water_movie_play_flag & 0x4) == 0) {
                    water_movie_play_flag |= 0x4;
                    movie = MOVIE_BOIL2;
                } else if (game_global_vars[GVAR_VAULT_WATER] == 0) {
                    movie = MOVIE_BOIL3;
                }
            }
        }
    }

    if (!dialog_active()) {
        if (movie != -1) {
            if (!gmovie_has_been_played(movie)) {
                int flags = movie == MOVIE_BOIL3
                    ? GAME_MOVIE_FADE_IN | GAME_MOVIE_STOP_MUSIC
                    : GAME_MOVIE_FADE_IN | GAME_MOVIE_FADE_OUT | GAME_MOVIE_STOP_MUSIC;

                moviePlaying = 1;
                if (gmovie_play(movie, flags) == -1) {
                    debug_printf("\nError playing movie!");
                }
                moviePlaying = 0;

                tile_refresh_display();

                if (movie == MOVIE_BOIL3) {
                    game_user_wants_to_quit = 2;
                } else {
                    movie = -1;
                }
            } else {
                movie = -1;
            }
        }
    }

    rc = critter_check_rads(obj_dude);

    queue_clear_type(EVENT_TYPE_GAME_TIME, NULL);

    gtime_q_add();

    if (movie != -1) {
        rc = 1;
    }

    return rc;
}

// 0x491B24
int scr_map_q_process(Object* obj, void* data)
{
    scr_exec_map_update_scripts();

    queue_clear_type(EVENT_TYPE_MAP_UPDATE_EVENT, NULL);

    if (map_data.name[0] == '\0') {
        return 0;
    }

    if (queue_add(600, NULL, NULL, EVENT_TYPE_MAP_UPDATE_EVENT) != -1) {
        return 0;
    }

    return -1;
}

// 0x491B60
int new_obj_id()
{
    // 0x51C7D4
    static int cur_id = 4;

    Object* ptr;

    do {
        cur_id++;
        ptr = obj_find_first();

        while (ptr) {
            if (cur_id == ptr->id) {
                break;
            }

            ptr = obj_find_next();
        }
    } while (ptr);

    if (cur_id >= 18000) {
        debug_printf("\n    ERROR: new_obj_id() !!!! Picked PLAYER ID!!!!");
    }

    cur_id++;

    return cur_id;
}

// 0x491C00
int scr_find_sid_from_program(Program* program)
{
    for (int type = 0; type < SCRIPT_TYPE_COUNT; type++) {
        ScriptListExtent* extent = scriptlists[type].head;
        while (extent != NULL) {
            for (int index = 0; index < extent->length; index++) {
                Script* script = &(extent->scripts[index]);
                if (script->program == program) {
                    return script->scr_id;
                }
            }
            extent = extent->next;
        }
    }

    return -1;
}

// 0x491CA4
Object* scr_find_obj_from_program(Program* program)
{
    int sid = scr_find_sid_from_program(program);

    Script* script;
    if (scr_ptr(sid, &script) == -1) {
        return NULL;
    }

    if (script->owner != NULL) {
        return script->owner;
    }

    if (SID_TYPE(sid) != SCRIPT_TYPE_SPATIAL) {
        return NULL;
    }

    Object* object;
    int fid = art_id(OBJ_TYPE_INTERFACE, 3, 0, 0, 0);
    obj_new(&object, fid, -1);
    obj_turn_off(object, NULL);
    obj_toggle_flat(object, NULL);
    object->sid = sid;

    // NOTE: Redundant, we've already obtained script earlier. Probably
    // inlining.
    Script* v1;
    if (scr_ptr(sid, &v1) == -1) {
        // FIXME: this is clearly an error, but I guess it's never reached since
        // we've already obtained script for given sid earlier.
        return (Object*)-1;
    }

    object->id = new_obj_id();
    v1->scr_oid = object->id;
    v1->owner = object;

    for (int elevation = 0; elevation < ELEVATION_COUNT; elevation++) {
        Script* spatialScript = scr_find_first_at(elevation);
        while (spatialScript != NULL) {
            if (spatialScript == script) {
                obj_move_to_tile(object, builtTileGetTile(script->sp.built_tile), elevation, NULL);
                return object;
            }
            spatialScript = scr_find_next_at();
        }
    }

    return object;
}

// 0x491E00
int scr_set_objs(int sid, Object* source, Object* target)
{
    Script* script;
    if (scr_ptr(sid, &script) == -1) {
        return -1;
    }

    script->source = source;
    script->target = target;

    return 0;
}

// 0x491E28
void scr_set_ext_param(int sid, int value)
{
    Script* script;
    if (scr_ptr(sid, &script) != -1) {
        script->fixedParam = value;
    }
}

// 0x491E48
int scr_set_action_num(int sid, int value)
{
    Script* scr;

    if (scr_ptr(sid, &scr) == -1) {
        return -1;
    }

    scr->actionBeingUsed = value;

    return 0;
}

// 0x491E68
Program* loadProgram(const char* name)
{
    char path[COMPAT_MAX_PATH];

    strcpy(path, cd_path_base);
    strcat(path, script_path_base);
    strcat(path, name);
    strcat(path, ".int");

    return allocateProgram(path);
}

// 0x491F20
static void doBkProcesses()
{
    // 0x66774C
    static bool set;

    // 0x667748
    static int lasttime;

    if (!set) {
        lasttime = get_bk_time();
        set = 1;
    }

    int v0 = get_bk_time();
    if (script_engine_running) {
        lasttime = v0;

        // NOTE: There is a loop at 0x4A3C64, consisting of one iteration, going
        // downwards from 1.
        for (int index = 0; index < 1; index++) {
            updatePrograms();
        }
    }

    updateWindows();

    if (script_engine_running && script_engine_run_critters) {
        if (!dialog_active()) {
            script_chk_critters();
            script_chk_timed_events();
        }
    }
}

// 0x491F94
static void script_chk_critters()
{
    // 0x51C7DC
    static int count = 0;

    if (!dialog_active() && !isInCombat()) {
        ScriptList* scriptList;
        ScriptListExtent* scriptListExtent;

        int scriptsCount = 0;

        scriptList = &(scriptlists[SCRIPT_TYPE_CRITTER]);
        scriptListExtent = scriptList->head;
        while (scriptListExtent != NULL) {
            scriptsCount += scriptListExtent->length;
            scriptListExtent = scriptListExtent->next;
        }

        count += 1;
        if (count >= scriptsCount) {
            count = 0;
        }

        if (count < scriptsCount) {
            int proc = isInCombat() ? SCRIPT_PROC_COMBAT : SCRIPT_PROC_CRITTER;
            int extentIndex = count / SCRIPT_LIST_EXTENT_SIZE;
            int scriptIndex = count % SCRIPT_LIST_EXTENT_SIZE;

            scriptList = &(scriptlists[SCRIPT_TYPE_CRITTER]);
            scriptListExtent = scriptList->head;
            while (scriptListExtent != NULL && extentIndex != 0) {
                extentIndex -= 1;
                scriptListExtent = scriptListExtent->next;
            }

            if (scriptListExtent != NULL) {
                Script* script = &(scriptListExtent->scripts[scriptIndex]);
                exec_script_proc(script->scr_id, proc);
            }
        }
    }
}

// TODO: Check.
//
// 0x49207C
static void script_chk_timed_events()
{
    // 0x51C7E0
    static int last_time = 0;

    // 0x51C7E4
    static int last_light_time = 0;

    int now;
    bool should_process_queue;

    now = get_bk_time();
    should_process_queue = false;

    if (!isInCombat()) {
        should_process_queue = true;
    }

    if (game_state() != GAME_STATE_4) {
        if (elapsed_tocks(now, last_light_time) >= 30000) {
            last_light_time = now;
            scr_exec_map_update_scripts();
        }
    } else {
        should_process_queue = false;
    }

    if (elapsed_tocks(now, last_time) >= 100) {
        last_time = now;
        if (!isInCombat()) {
            fallout_game_time += 1;
        }
        should_process_queue = true;
    }

    if (should_process_queue) {
        queue_process();
    }
}

// 0x492100
int script_q_add(int sid, int delay, int param)
{
    ScriptEvent* scriptEvent = (ScriptEvent*)mem_malloc(sizeof(*scriptEvent));
    if (scriptEvent == NULL) {
        return -1;
    }

    scriptEvent->sid = sid;
    scriptEvent->fixedParam = param;

    Script* script;
    if (scr_ptr(sid, &script) == -1) {
        mem_free(scriptEvent);
        return -1;
    }

    if (queue_add(delay, script->owner, scriptEvent, EVENT_TYPE_SCRIPT) == -1) {
        mem_free(scriptEvent);
        return -1;
    }

    return 0;
}

// 0x49217C
int script_q_save(DB_FILE* stream, void* data)
{
    ScriptEvent* scriptEvent = (ScriptEvent*)data;

    if (db_fwriteInt(stream, scriptEvent->sid) == -1) return -1;
    if (db_fwriteInt(stream, scriptEvent->fixedParam) == -1) return -1;

    return 0;
}

// 0x4921A4
int script_q_load(DB_FILE* stream, void** dataPtr)
{
    ScriptEvent* scriptEvent = (ScriptEvent*)mem_malloc(sizeof(*scriptEvent));
    if (scriptEvent == NULL) {
        return -1;
    }

    if (db_freadInt(stream, &(scriptEvent->sid)) == -1) goto err;
    if (db_freadInt(stream, &(scriptEvent->fixedParam)) == -1) goto err;

    *dataPtr = scriptEvent;

    return 0;

err:

    // there is a memory leak in original code, free is not called
    mem_free(scriptEvent);

    return -1;
}

// 0x4921EC
int script_q_process(Object* obj, void* data)
{
    ScriptEvent* scriptEvent = (ScriptEvent*)data;

    Script* script;
    if (scr_ptr(scriptEvent->sid, &script) == -1) {
        return 0;
    }

    script->fixedParam = scriptEvent->fixedParam;

    exec_script_proc(scriptEvent->sid, SCRIPT_PROC_TIMED);

    return 0;
}

// 0x492220
int scripts_clear_state()
{
    scriptState.requests = 0;
    return 0;
}

// 0x492230
int scripts_clear_combat_requests(Script* script)
{
    if ((scriptState.requests & SCRIPT_REQUEST_COMBAT) != 0 && scriptState.combatState1.attacker == script->owner) {
        scriptState.requests &= ~SCRIPT_REQUEST_COMBAT;
    }
    return 0;
}

// 0x492250
int scripts_check_state()
{
    WorldMapContext ctx;

    if (scriptState.requests == 0) {
        return 0;
    }

    if ((scriptState.requests & SCRIPT_REQUEST_COMBAT) != 0) {
        scriptState.requests &= ~SCRIPT_REQUEST_COMBAT;
        memcpy(&scriptState.combatState2, &scriptState.combatState1, sizeof(scriptState.combatState2));

        if ((scriptState.requests & SCRIPT_REQUEST_NO_INITIAL_COMBAT_STATE) != 0) {
            scriptState.requests &= ~SCRIPT_REQUEST_NO_INITIAL_COMBAT_STATE;
            combat(NULL);
        } else {
            combat(&scriptState.combatState2);
            memset(&scriptState.combatState2, 0, sizeof(scriptState.combatState2));
        }
    }

    if ((scriptState.requests & SCRIPT_REQUEST_TOWN_MAP) != 0) {
        scriptState.requests &= ~SCRIPT_REQUEST_TOWN_MAP;
        ctx.state = 0;
        ctx.town = 0;
        world_map(ctx);
        KillWorldWin();
    }

    if ((scriptState.requests & SCRIPT_REQUEST_WORLD_MAP) != 0) {
        scriptState.requests &= ~SCRIPT_REQUEST_WORLD_MAP;
        ctx.state = 0;
        ctx.town = our_town;
        ctx = town_map(ctx);
        world_map(ctx);
        KillWorldWin();
    }

    if ((scriptState.requests & SCRIPT_REQUEST_ELEVATOR) != 0) {
        int map = map_data.field_34;
        int elevation = map_elevation;
        int tile = -1;

        scriptState.requests &= ~SCRIPT_REQUEST_ELEVATOR;

        if (elevator_select(scriptState.elevatorType, &map, &elevation, &tile) != -1) {
            automap_pip_save();

            if (map == map_data.field_34) {
                if (elevation == map_elevation) {
                    register_clear(obj_dude);
                    obj_set_rotation(obj_dude, ROTATION_SE, 0);
                    obj_attempt_placement(obj_dude, tile, elevation, 0);
                } else {
                    Object* elevatorDoors = obj_find_first_at(obj_dude->elevation);
                    while (elevatorDoors != NULL) {
                        int pid = elevatorDoors->pid;
                        if (PID_TYPE(pid) == OBJ_TYPE_SCENERY
                            && (pid == PROTO_ID_0x2000099 || pid == PROTO_ID_0x20001A5 || pid == PROTO_ID_0x20001D6)
                            && tile_dist(elevatorDoors->tile, obj_dude->tile) <= 4) {
                            break;
                        }
                        elevatorDoors = obj_find_next_at();
                    }

                    register_clear(obj_dude);
                    obj_set_rotation(obj_dude, ROTATION_SE, 0);
                    obj_attempt_placement(obj_dude, tile, elevation, 0);

                    if (elevatorDoors != NULL) {
                        obj_set_frame(elevatorDoors, 0, NULL);
                        obj_move_to_tile(elevatorDoors, elevatorDoors->tile, elevatorDoors->elevation, NULL);
                        elevatorDoors->flags &= ~OBJECT_OPEN_DOOR;
                        elevatorDoors->data.scenery.door.openFlags &= ~0x01;
                        obj_rebuild_all_light();
                    } else {
                        debug_printf("\nWarning: Elevator: Couldn't find old elevator doors!");
                    }
                }
            } else {
                Object* elevatorDoors = obj_find_first_at(obj_dude->elevation);
                while (elevatorDoors != NULL) {
                    int pid = elevatorDoors->pid;
                    if (PID_TYPE(pid) == OBJ_TYPE_SCENERY
                        && (pid == PROTO_ID_0x2000099 || pid == PROTO_ID_0x20001A5 || pid == PROTO_ID_0x20001D6)
                        && tile_dist(elevatorDoors->tile, obj_dude->tile) <= 4) {
                        break;
                    }
                    elevatorDoors = obj_find_next_at();
                }

                if (elevatorDoors != NULL) {
                    obj_set_frame(elevatorDoors, 0, NULL);
                    obj_move_to_tile(elevatorDoors, elevatorDoors->tile, elevatorDoors->elevation, NULL);
                    elevatorDoors->flags &= ~OBJECT_OPEN_DOOR;
                    elevatorDoors->data.scenery.door.openFlags &= ~0x01;
                    obj_rebuild_all_light();
                } else {
                    debug_printf("\nWarning: Elevator: Couldn't find old elevator doors!");
                }

                MapTransition transition;
                memset(&transition, 0, sizeof(transition));

                transition.map = map;
                transition.elevation = elevation;
                transition.tile = tile;
                transition.rotation = ROTATION_SE;

                map_leave_map(&transition);
            }
        }
    }

    if ((scriptState.requests & SCRIPT_REQUEST_EXPLOSION) != 0) {
        scriptState.requests &= ~SCRIPT_REQUEST_EXPLOSION;
        action_explode(scriptState.explosionTile, scriptState.explosionElevation, scriptState.explosionMinDamage, scriptState.explosionMaxDamage, NULL, 1);
    }

    if ((scriptState.requests & SCRIPT_REQUEST_DIALOG) != 0) {
        scriptState.requests &= ~SCRIPT_REQUEST_DIALOG;
        gdialog_enter(scriptState.dialogTarget, 0);
    }

    if ((scriptState.requests & SCRIPT_REQUEST_ENDGAME) != 0) {
        scriptState.requests &= ~SCRIPT_REQUEST_ENDGAME;
        endgame_slideshow();
    }

    if ((scriptState.requests & SCRIPT_REQUEST_LOOTING) != 0) {
        scriptState.requests &= ~SCRIPT_REQUEST_LOOTING;
        loot_container(scriptState.lootingBy, scriptState.lootingFrom);
    }

    if ((scriptState.requests & SCRIPT_REQUEST_STEALING) != 0) {
        scriptState.requests &= ~SCRIPT_REQUEST_STEALING;
        inven_steal_container(scriptState.stealingBy, scriptState.stealingFrom);
    }

    return 0;
}

// 0x4925C0
int scripts_check_state_in_combat()
{
    if ((scriptState.requests & SCRIPT_REQUEST_ELEVATOR) != 0) {
        int map = map_data.field_34;
        int elevation = map_elevation;
        int tile = -1;

        if (elevator_select(scriptState.elevatorType, &map, &elevation, &tile) != -1) {
            automap_pip_save();

            if (map == map_data.field_34) {
                if (elevation == map_elevation) {
                    register_clear(obj_dude);
                    obj_set_rotation(obj_dude, ROTATION_SE, 0);
                    obj_attempt_placement(obj_dude, tile, elevation, 0);
                } else {
                    Object* elevatorDoors = obj_find_first_at(obj_dude->elevation);
                    while (elevatorDoors != NULL) {
                        int pid = elevatorDoors->pid;
                        if (PID_TYPE(pid) == OBJ_TYPE_SCENERY
                            && (pid == PROTO_ID_0x2000099 || pid == PROTO_ID_0x20001A5 || pid == PROTO_ID_0x20001D6)
                            && tile_dist(elevatorDoors->tile, obj_dude->tile) <= 4) {
                            break;
                        }
                        elevatorDoors = obj_find_next_at();
                    }

                    register_clear(obj_dude);
                    obj_set_rotation(obj_dude, ROTATION_SE, 0);
                    obj_attempt_placement(obj_dude, tile, elevation, 0);

                    if (elevatorDoors != NULL) {
                        obj_set_frame(elevatorDoors, 0, NULL);
                        obj_move_to_tile(elevatorDoors, elevatorDoors->tile, elevatorDoors->elevation, NULL);
                        elevatorDoors->flags &= ~OBJECT_OPEN_DOOR;
                        elevatorDoors->data.scenery.door.openFlags &= ~0x01;
                        obj_rebuild_all_light();
                    } else {
                        debug_printf("\nWarning: Elevator: Couldn't find old elevator doors!");
                    }
                }
            } else {
                MapTransition transition;
                memset(&transition, 0, sizeof(transition));

                transition.map = map;
                transition.elevation = elevation;
                transition.tile = tile;
                transition.rotation = ROTATION_SE;

                map_leave_map(&transition);
            }
        }
    }

    if ((scriptState.requests & SCRIPT_REQUEST_LOOTING) != 0) {
        loot_container(scriptState.lootingBy, scriptState.lootingFrom);
    }

    // NOTE: Uninline.
    scripts_clear_state();

    return 0;
}

// 0x492794
int scripts_request_combat(STRUCT_664980* a1)
{
    if (a1) {
        memcpy(&scriptState.combatState1, a1, sizeof(scriptState.combatState1));
    } else {
        scriptState.requests |= SCRIPT_REQUEST_NO_INITIAL_COMBAT_STATE;
    }

    scriptState.requests |= SCRIPT_REQUEST_COMBAT;

    return 0;
}

// 0x4927D8
void scripts_request_townmap()
{
    if (isInCombat()) {
        game_user_wants_to_quit = 1;
    }

    scriptState.requests |= SCRIPT_REQUEST_TOWN_MAP;
}

// 0x492800
void scripts_request_worldmap()
{
    if (isInCombat()) {
        game_user_wants_to_quit = 1;
    }

    scriptState.requests |= SCRIPT_REQUEST_WORLD_MAP;
}

// 0x492828
int scripts_request_elevator(int elevator)
{
    scriptState.elevatorType = elevator;
    scriptState.requests |= SCRIPT_REQUEST_ELEVATOR;

    return 0;
}

// 0x492844
int scripts_request_explosion(int tile, int elevation, int minDamage, int maxDamage)
{
    scriptState.requests |= SCRIPT_REQUEST_EXPLOSION;
    scriptState.explosionTile = tile;
    scriptState.explosionElevation = elevation;
    scriptState.explosionMinDamage = minDamage;
    scriptState.explosionMaxDamage = maxDamage;
    return 0;
}

// 0x492868
void scripts_request_dialog(Object* obj)
{
    scriptState.dialogTarget = obj;
    scriptState.requests |= SCRIPT_REQUEST_DIALOG;
}

// 0x492884
void scripts_request_endgame_slideshow()
{
    scriptState.requests |= SCRIPT_REQUEST_ENDGAME;
}

// 0x492890
int scripts_request_loot_container(Object* a1, Object* a2)
{
    scriptState.lootingBy = a1;
    scriptState.lootingFrom = a2;
    scriptState.requests |= SCRIPT_REQUEST_LOOTING;
    return 0;
}

// 0x4928B0
int scripts_request_steal_container(Object* a1, Object* a2)
{
    scriptState.stealingBy = a1;
    scriptState.stealingFrom = a2;
    scriptState.requests |= SCRIPT_REQUEST_STEALING;
    return 0;
}

// 0x4928D0
void script_make_path(char* path)
{
    strcpy(path, cd_path_base);
    strcat(path, script_path_base);
}

// 0x492924
int exec_script_proc(int sid, int action)
{
    if (!script_engine_running) {
        return -1;
    }

    Script* script;
    if (scr_ptr(sid, &script) == -1) {
        return -1;
    }

    script->action = action;
    script->scriptOverrides = 0;

    bool programLoaded = false;
    if ((script->scr_flags & SCRIPT_FLAG_0x01) == 0) {
        char name[16];
        if (scr_list_str(script->scr_script_idx, name, sizeof(name)) == -1) {
            return -1;
        }

        char* pch = strchr(name, '.');
        if (pch != NULL) {
            *pch = '\0';
        }

        script->program = loadProgram(name);
        if (script->program == NULL) {
            debug_printf("\nError: exec_script_proc: script load failed!");
            return -1;
        }

        programLoaded = true;
        script->scr_flags |= SCRIPT_FLAG_0x01;
    }

    Program* program = script->program;
    if (program == NULL) {
        return -1;
    }

    if ((program->flags & 0x0124) != 0) {
        return 0;
    }

    int proc = script->procs[action];
    if (proc == 0) {
        proc = 1;
    }

    if (proc == -1) {
        debug_printf("\nError: exec_script_proc: Can't Find script procedure!!!");
        return -1;
    }

    if (script->target == NULL) {
        script->target = script->owner;
    }

    script->scr_flags |= SCRIPT_FLAG_0x04;

    if (programLoaded) {
        scr_build_lookup_table(script);
        runProgram(program);
        interpretSetCPUBurstSize(5000);
        updatePrograms();
        interpretSetCPUBurstSize(10);
    } else {
        executeProcedure(program, proc);
    }

    script->source = NULL;

    return 0;
}

// Locate built-in procs for given script.
//
// 0x492AA0
static int scr_build_lookup_table(Script* script)
{
    int action;
    int proc = sizeof(Script);

    for (action = 0; action < SCRIPT_PROC_COUNT; action++) {
        proc = interpretFindProcedure(script->program, procTableStrs[action]);
        if (proc == -1) {
            proc = SCRIPT_PROC_NO_PROC;
        }
        script->procs[action] = proc;
    }

    return 0;
}

// 0x492DEC
int scr_find_str_run_info(int scr_script_idx, int* run_info_flags, int sid)
{
    int rc = -1;
    char path[COMPAT_MAX_PATH];
    DB_FILE* stream;
    int idx;
    char string[COMPAT_MAX_PATH];
    char* sep;
    Script* script;

    if (scr_script_idx < 0) {
        return -1;
    }

    if (run_info_flags == NULL) {
        return -1;
    }

    script_make_path(path);
    strcat(path, "scripts.lst");

    stream = db_fopen(path, "rt");
    if (stream == NULL) {
        return -1;
    }

    for (idx = 0; idx <= scr_script_idx; idx++) {
        if (db_fgets(string, sizeof(string), stream) == NULL) {
            break;
        }
    }

    if (idx - 1 == scr_script_idx) {
        rc = 0;
        sep = strchr(string, '#');
        if (sep != NULL) {
            if (sep[1] != '\0') {
                if (strstr(sep, "map_init") != NULL) {
                    *run_info_flags |= 0x1;
                }

                if (strstr(sep, "map_exit") != NULL) {
                    *run_info_flags |= 0x2;
                }

                sep = strstr(sep, "local_vars=");
                if (sep != NULL) {
                    if (scr_ptr(sid, &script) != -1) {
                        script->scr_num_local_vars = atoi(sep + 11);
                    } else {
                        rc = -1;
                    }
                }
            }
        }
    }

    db_fclose(stream);

    return rc;
}

// 0x492FC4
static int scr_index_to_name(int scr_script_idx, char* name, size_t size)
{
    int rc = -1;
    char path[COMPAT_MAX_PATH];
    DB_FILE* stream;
    int idx;
    char string[COMPAT_MAX_PATH];
    char* sep;

    if (scr_script_idx < 0) {
        return -1;
    }

    if (name == NULL) {
        return -1;
    }

    script_make_path(path);
    strcat(path, "scripts.lst");

    stream = db_fopen(path, "rt");
    if (stream == NULL) {
        return -1;
    }

    for (idx = 0; idx <= scr_script_idx; idx++) {
        if (db_fgets(string, sizeof(string), stream) == NULL) {
            break;
        }
    }

    if (idx - 1 == scr_script_idx) {
        sep = strchr(string, '.');
        if (sep != NULL) {
            *sep = '\0';
            snprintf(name, size, "%s.%s", string, "int");
            rc = 0;
        }
    }

    db_fclose(stream);

    return rc;
}

// 0x492FBC
int scr_list_str(int index, char* name, size_t size)
{
    name[0] = '\0';
    return scr_index_to_name(index & 0xFFFFFF, name, size);
}

// 0x493118
int scr_set_dude_script()
{
    if (scr_clear_dude_script() == -1) {
        return -1;
    }

    if (obj_dude == NULL) {
        debug_printf("Error in scr_set_dude_script: obj_dude uninitialized!");
        return -1;
    }

    Proto* proto;
    if (proto_ptr(0x1000000, &proto) == -1) {
        debug_printf("Error in scr_set_dude_script: can't find obj_dude proto!");
        return -1;
    }

    proto->critter.sid = 0x4000000;

    obj_new_sid(obj_dude, &(obj_dude->sid));

    Script* script;
    if (scr_ptr(obj_dude->sid, &script) == -1) {
        debug_printf("Error in scr_set_dude_script: can't find obj_dude script!");
        return -1;
    }

    script->scr_flags |= (SCRIPT_FLAG_0x08 | SCRIPT_FLAG_0x10);

    return 0;
}

// 0x4931CC
int scr_clear_dude_script()
{
    if (obj_dude == NULL) {
        debug_printf("\nError in scr_clear_dude_script: obj_dude uninitialized!");
        return -1;
    }

    if (obj_dude->sid != -1) {
        Script* script;
        if (scr_ptr(obj_dude->sid, &script) != -1) {
            script->scr_flags &= ~(SCRIPT_FLAG_0x08 | SCRIPT_FLAG_0x10);
        }

        scr_remove(obj_dude->sid);

        obj_dude->sid = -1;
    }

    return 0;
}

// 0x493230
int scr_init()
{
    int index;

    if (!message_init(&script_message_file)) {
        return -1;
    }

    for (index = 0; index < SCRIPT_DIALOG_MESSAGE_LIST_CAPACITY; index++) {
        if (!message_init(&(script_dialog_msgs[index]))) {
            return -1;
        }
    }

    scr_remove_all();
    interpretOutputFunc(win_debug);
    initInterpreter();
    scr_header_load();

    // NOTE: Uninline.
    scripts_clear_state();

    partyMemberClear();

    return 0;
}

// 0x49329C
int scr_reset()
{
    scr_remove_all();

    // NOTE: Uninline.
    scripts_clear_state();

    partyMemberClear();

    return 0;
}

// 0x4932B4
int scr_game_init()
{
    int index;
    char path[COMPAT_MAX_PATH];

    if (!message_init(&script_message_file)) {
        debug_printf("\nError initing script message file!");
        return -1;
    }

    for (index = 0; index < SCRIPT_DIALOG_MESSAGE_LIST_CAPACITY; index++) {
        if (!message_init(&(script_dialog_msgs[index]))) {
            debug_printf("\nERROR IN SCRIPT_DIALOG_MSGS!");
            return -1;
        }
    }

    snprintf(path, sizeof(path), "%s%s", msg_path, "script.msg");
    if (!message_load(&script_message_file, path)) {
        debug_printf("\nError loading script message file!");
        return -1;
    }

    script_engine_running = true;
    script_engine_game_mode = 1;
    fallout_game_time = 1;
    water_movie_play_flag = 0;
    set_game_time_in_seconds(GAME_TIME_START_HOUR * 60 * 60 + GAME_TIME_START_MINUTE * 60);
    add_bk_process(doBkProcesses);

    if (scr_set_dude_script() == -1) {
        return -1;
    }

    // NOTE: Uninline.
    scripts_clear_state();

    // NOTE: Uninline.
    scr_spatials_enable();

    return 0;
}

// 0x4933C4
int scr_game_reset()
{
    debug_printf("\nScripts: [Game Reset]");
    scr_game_exit();
    scr_game_init();
    partyMemberClear();
    scr_remove_all_force();
    water_movie_play_flag = 0;
    return scr_set_dude_script();
}

// 0x493400
int scr_exit()
{
    script_engine_running = false;
    script_engine_run_critters = 0;
    if (!message_exit(&script_message_file)) {
        debug_printf("\nError exiting script message file!");
        return -1;
    }

    scr_remove_all();
    scr_remove_all_force();
    interpretClose();
    clearPrograms();

    remove_bk_process(doBkProcesses);

    // NOTE: Uninline.
    scripts_clear_state();

    return 0;
}

// 0x49345C
int scr_message_free()
{
    int index;
    MessageList* message_list;

    for (index = 0; index < SCRIPT_DIALOG_MESSAGE_LIST_CAPACITY; index++) {
        message_list = &(script_dialog_msgs[index]);
        if (message_list->entries_num != 0) {
            if (!message_exit(message_list)) {
                debug_printf("\nERROR in scr_message_free!");
                return -1;
            }

            if (!message_init(message_list)) {
                debug_printf("\nERROR in scr_message_free!");
                return -1;
            }
        }
    }

    return 0;
}

// 0x4934C4
int scr_game_exit()
{
    script_engine_game_mode = 0;
    script_engine_running = false;
    script_engine_run_critters = 0;
    scr_message_free();
    scr_remove_all();
    clearPrograms();
    remove_bk_process(doBkProcesses);
    message_exit(&script_message_file);
    if (scr_clear_dude_script() == -1) {
        return -1;
    }

    // NOTE: Uninline.
    scripts_clear_state();

    return 0;
}

// 0x493510
int scr_enable()
{
    if (!script_engine_game_mode) {
        return -1;
    }

    script_engine_run_critters = 1;
    script_engine_running = true;
    return 0;
}

// 0x493538
int scr_disable()
{
    script_engine_running = false;
    return 0;
}

// 0x493548
void scr_enable_critters()
{
    script_engine_run_critters = 1;
}

// 0x493558
void scr_disable_critters()
{
    script_engine_run_critters = 0;
}

// 0x493568
int scr_game_save(DB_FILE* stream)
{
    if (db_fwriteIntCount(stream, game_global_vars, num_game_global_vars) == -1) return -1;
    if (db_fwriteByte(stream, water_movie_play_flag) == -1) return -1;

    return 0;
}

// 0x4935A0
int scr_game_load(DB_FILE* stream)
{
    if (db_freadIntCount(stream, game_global_vars, num_game_global_vars) == -1) return -1;
    if (db_freadByte(stream, &water_movie_play_flag) == -1) return -1;

    return 0;
}

// NOTE: For unknown reason save game files contains two identical sets of game
// global variables (saved with [scr_game_save]). The first set is
// read with [scr_game_load], the second set is simply thrown away
// using this function.
//
// 0x4935D4
int scr_game_load2(DB_FILE* stream)
{
    int* temp_vars;
    unsigned char temp_water_movie_play_flag;

    temp_vars = (int*)mem_malloc(sizeof(*temp_vars) * num_game_global_vars);
    if (temp_vars == NULL) {
        return -1;
    }

    if (db_freadIntCount(stream, temp_vars, num_game_global_vars) == -1) {
        // FIXME: Leaks vars.
        return -1;
    }

    if (db_freadByte(stream, &temp_water_movie_play_flag) == -1) {
        // FIXME: Leaks vars.
        return -1;
    }

    mem_free(temp_vars);

    return 0;
}

// 0x49362C
static int scr_header_load()
{
    char path[COMPAT_MAX_PATH];
    DB_FILE* stream;

    num_script_indexes = 0;

    script_make_path(path);
    strcat(path, "scripts.lst");

    stream = db_fopen(path, "rt");
    if (stream == NULL) {
        return -1;
    }

    while (1) {
        int ch = db_fgetc(stream);
        if (ch == -1) {
            break;
        }

        if (ch == '\n') {
            num_script_indexes++;
        }
    }

    num_script_indexes++;

    db_fclose(stream);

    for (int scriptType = 0; scriptType < SCRIPT_TYPE_COUNT; scriptType++) {
        ScriptList* scriptList = &(scriptlists[scriptType]);
        scriptList->head = NULL;
        scriptList->tail = NULL;
        scriptList->length = 0;
        scriptList->nextScriptId = 0;
    }

    return 0;
}

// 0x49372C
static int scr_write_ScriptSubNode(Script* scr, DB_FILE* stream)
{
    if (db_fwriteInt(stream, scr->scr_id) == -1) return -1;
    if (db_fwriteInt(stream, scr->scr_next) == -1) return -1;

    switch (SID_TYPE(scr->scr_id)) {
    case SCRIPT_TYPE_SPATIAL:
        if (db_fwriteInt(stream, scr->sp.built_tile) == -1) return -1;
        if (db_fwriteInt(stream, scr->sp.radius) == -1) return -1;
        break;
    case SCRIPT_TYPE_TIMED:
        if (db_fwriteInt(stream, scr->tm.time) == -1) return -1;
        break;
    }

    if (db_fwriteInt(stream, scr->scr_flags) == -1) return -1;
    if (db_fwriteInt(stream, scr->scr_script_idx) == -1) return -1;
    // NOTE: Original code writes `scr->program` pointer which is meaningless.
    if (db_fwriteInt(stream, 0) == -1) return -1;
    if (db_fwriteInt(stream, scr->scr_oid) == -1) return -1;
    if (db_fwriteInt(stream, scr->scr_local_var_offset) == -1) return -1;
    if (db_fwriteInt(stream, scr->scr_num_local_vars) == -1) return -1;
    if (db_fwriteInt(stream, scr->field_28) == -1) return -1;
    if (db_fwriteInt(stream, scr->action) == -1) return -1;
    if (db_fwriteInt(stream, scr->fixedParam) == -1) return -1;
    if (db_fwriteInt(stream, scr->actionBeingUsed) == -1) return -1;
    if (db_fwriteInt(stream, scr->scriptOverrides) == -1) return -1;
    if (db_fwriteInt(stream, scr->field_48) == -1) return -1;
    if (db_fwriteInt(stream, scr->howMuch) == -1) return -1;
    if (db_fwriteInt(stream, scr->run_info_flags) == -1) return -1;

    return 0;
}

// 0x4938A0
static int scr_write_ScriptNode(ScriptListExtent* a1, DB_FILE* stream)
{
    for (int index = 0; index < SCRIPT_LIST_EXTENT_SIZE; index++) {
        Script* script = &(a1->scripts[index]);
        if (scr_write_ScriptSubNode(script, stream) != 0) {
            return -1;
        }
    }

    if (db_fwriteInt(stream, a1->length) != 0) {
        return -1;
    }

    // NOTE: Original code writes `a1->next` pointer which is meaningless.
    if (db_fwriteInt(stream, 0) != 0) {
        // FIXME: writing pointer to file
        return -1;
    }

    return 0;
}

// 0x493904
int scr_save(DB_FILE* stream)
{
    for (int scriptType = 0; scriptType < SCRIPT_TYPE_COUNT; scriptType++) {
        ScriptList* scriptList = &(scriptlists[scriptType]);

        int scriptCount = scriptList->length * SCRIPT_LIST_EXTENT_SIZE;
        if (scriptList->tail != NULL) {
            scriptCount += scriptList->tail->length - SCRIPT_LIST_EXTENT_SIZE;
        }

        ScriptListExtent* scriptExtent = scriptList->head;
        ScriptListExtent* lastScriptExtent = NULL;
        while (scriptExtent != NULL) {
            for (int index = 0; index < scriptExtent->length; index++) {
                Script* script = &(scriptExtent->scripts[index]);

                lastScriptExtent = scriptList->tail;
                if ((script->scr_flags & SCRIPT_FLAG_0x08) != 0) {
                    scriptCount--;

                    int backwardsIndex = lastScriptExtent->length - 1;
                    if (lastScriptExtent == scriptExtent && backwardsIndex <= index) {
                        break;
                    }

                    while (lastScriptExtent != scriptExtent || backwardsIndex > index) {
                        Script* backwardsScript = &(lastScriptExtent->scripts[backwardsIndex]);
                        if ((backwardsScript->scr_flags & SCRIPT_FLAG_0x08) == 0) {
                            break;
                        }

                        backwardsIndex--;

                        if (backwardsIndex < 0) {
                            ScriptListExtent* previousScriptExtent = scriptList->head;
                            while (previousScriptExtent->next != lastScriptExtent) {
                                previousScriptExtent = previousScriptExtent->next;
                            }

                            lastScriptExtent = previousScriptExtent;
                            backwardsIndex = lastScriptExtent->length - 1;
                        }
                    }

                    if (lastScriptExtent != scriptExtent || backwardsIndex > index) {
                        Script temp;
                        memcpy(&temp, script, sizeof(Script));
                        memcpy(script, &(lastScriptExtent->scripts[backwardsIndex]), sizeof(Script));
                        memcpy(&(lastScriptExtent->scripts[backwardsIndex]), &temp, sizeof(Script));

                        scriptCount++;
                    }
                }
            }
            scriptExtent = scriptExtent->next;
        }

        if (db_fwriteInt(stream, scriptCount) == -1) {
            return -1;
        }

        if (scriptCount > 0) {
            ScriptListExtent* scriptExtent = scriptList->head;
            while (scriptExtent != lastScriptExtent) {
                if (scr_write_ScriptNode(scriptExtent, stream) == -1) {
                    return -1;
                }
                scriptExtent = scriptExtent->next;
            }

            if (lastScriptExtent != NULL) {
                int index;
                for (index = 0; index < lastScriptExtent->length; index++) {
                    Script* script = &(lastScriptExtent->scripts[index]);
                    if ((script->scr_flags & SCRIPT_FLAG_0x08) != 0) {
                        break;
                    }
                }

                if (index > 0) {
                    int length = lastScriptExtent->length;
                    lastScriptExtent->length = index;
                    if (scr_write_ScriptNode(lastScriptExtent, stream) == -1) {
                        return -1;
                    }
                    lastScriptExtent->length = length;
                }
            }
        }
    }

    return 0;
}

// 0x493BB8
static int scr_read_ScriptSubNode(Script* scr, DB_FILE* stream)
{
    int prg;

    if (db_freadInt(stream, &(scr->scr_id)) == -1) return -1;
    if (db_freadInt(stream, &(scr->scr_next)) == -1) return -1;

    switch (SID_TYPE(scr->scr_id)) {
    case SCRIPT_TYPE_SPATIAL:
        if (db_freadInt(stream, &(scr->sp.built_tile)) == -1) return -1;
        if (db_freadInt(stream, &(scr->sp.radius)) == -1) return -1;
        break;
    case SCRIPT_TYPE_TIMED:
        if (db_freadInt(stream, &(scr->tm.time)) == -1) return -1;
        break;
    }

    if (db_freadInt(stream, &(scr->scr_flags)) == -1) return -1;
    if (db_freadInt(stream, &(scr->scr_script_idx)) == -1) return -1;
    if (db_freadInt(stream, &(prg)) == -1) return -1;
    if (db_freadInt(stream, &(scr->scr_oid)) == -1) return -1;
    if (db_freadInt(stream, &(scr->scr_local_var_offset)) == -1) return -1;
    if (db_freadInt(stream, &(scr->scr_num_local_vars)) == -1) return -1;
    if (db_freadInt(stream, &(scr->field_28)) == -1) return -1;
    if (db_freadInt(stream, &(scr->action)) == -1) return -1;
    if (db_freadInt(stream, &(scr->fixedParam)) == -1) return -1;
    if (db_freadInt(stream, &(scr->actionBeingUsed)) == -1) return -1;
    if (db_freadInt(stream, &(scr->scriptOverrides)) == -1) return -1;
    if (db_freadInt(stream, &(scr->field_48)) == -1) return -1;
    if (db_freadInt(stream, &(scr->howMuch)) == -1) return -1;
    if (db_freadInt(stream, &(scr->run_info_flags)) == -1) return -1;

    scr->program = NULL;
    scr->owner = NULL;
    scr->source = NULL;
    scr->target = NULL;

    for (int index = 0; index < SCRIPT_PROC_COUNT; index++) {
        scr->procs[index] = 0;
    }

    if (!(map_data.flags & 1)) {
        scr->scr_num_local_vars = 0;
    }

    return 0;
}

// 0x493D84
static int scr_read_ScriptNode(ScriptListExtent* scriptExtent, DB_FILE* stream)
{
    for (int index = 0; index < SCRIPT_LIST_EXTENT_SIZE; index++) {
        Script* scr = &(scriptExtent->scripts[index]);
        if (scr_read_ScriptSubNode(scr, stream) != 0) {
            return -1;
        }
    }

    if (db_freadInt(stream, &(scriptExtent->length)) != 0) {
        return -1;
    }

    int next;
    if (db_freadInt(stream, &(next)) != 0) {
        return -1;
    }

    return 0;
}

// 0x493DF4
int scr_load(DB_FILE* stream)
{
    for (int index = 0; index < SCRIPT_TYPE_COUNT; index++) {
        ScriptList* scriptList = &(scriptlists[index]);

        int scriptsCount = 0;
        if (db_freadInt(stream, &scriptsCount) == -1) {
            return -1;
        }

        if (scriptsCount != 0) {
            scriptList->length = scriptsCount / 16;

            if (scriptsCount % 16 != 0) {
                scriptList->length++;
            }

            ScriptListExtent* extent = (ScriptListExtent*)mem_malloc(sizeof(*extent));
            scriptList->head = extent;
            scriptList->tail = extent;
            if (extent == NULL) {
                return -1;
            }

            if (scr_read_ScriptNode(extent, stream) != 0) {
                return -1;
            }

            for (int scriptIndex = 0; scriptIndex < extent->length; scriptIndex++) {
                Script* script = &(extent->scripts[scriptIndex]);
                script->owner = NULL;
                script->source = NULL;
                script->target = NULL;
                script->program = NULL;
                script->scr_flags &= ~SCRIPT_FLAG_0x01;
            }

            extent->next = NULL;

            ScriptListExtent* prevExtent = extent;
            for (int extentIndex = 1; extentIndex < scriptList->length; extentIndex++) {
                ScriptListExtent* extent = (ScriptListExtent*)mem_malloc(sizeof(*extent));
                if (extent == NULL) {
                    return -1;
                }

                if (scr_read_ScriptNode(extent, stream) != 0) {
                    return -1;
                }

                for (int scriptIndex = 0; scriptIndex < extent->length; scriptIndex++) {
                    Script* script = &(extent->scripts[scriptIndex]);
                    script->owner = NULL;
                    script->source = NULL;
                    script->target = NULL;
                    script->program = NULL;
                    script->scr_flags &= ~SCRIPT_FLAG_0x01;
                }

                prevExtent->next = extent;

                extent->next = NULL;
                prevExtent = extent;
            }

            scriptList->tail = prevExtent;
        } else {
            scriptList->head = NULL;
            scriptList->tail = NULL;
            scriptList->length = 0;
        }
    }

    return 0;
}

// 0x493FD8
int scr_ptr(int sid, Script** scriptPtr)
{
    *scriptPtr = NULL;

    if (sid == -1) {
        return -1;
    }

    if (sid == 0xCCCCCCCC) {
        debug_printf("\nERROR: scr_ptr called with UN-SET id #!!!!");
        return -1;
    }

    ScriptList* scriptList = &(scriptlists[SID_TYPE(sid)]);
    ScriptListExtent* scriptListExtent = scriptList->head;

    while (scriptListExtent != NULL) {
        for (int index = 0; index < scriptListExtent->length; index++) {
            Script* script = &(scriptListExtent->scripts[index]);
            if (script->scr_id == sid) {
                *scriptPtr = script;
                return 0;
            }
        }
        scriptListExtent = scriptListExtent->next;
    }

    return -1;
}

// 0x494080
static int scr_new_id(int scriptType)
{
    int scriptId = scriptlists[scriptType].nextScriptId++;
    int v1 = scriptType << 24;

    while (scriptId < 32000) {
        Script* script;
        if (scr_ptr(v1 | scriptId, &script) == -1) {
            break;
        }
        scriptId++;
    }

    return scriptId;
}

// 0x4940D0
int scr_new(int* sidPtr, int scriptType)
{
    ScriptList* scriptList = &(scriptlists[scriptType]);
    ScriptListExtent* scriptListExtent = scriptList->tail;
    if (scriptList->head != NULL) {
        // There is at least one extent available, which means tail is also set.
        if (scriptListExtent->length == SCRIPT_LIST_EXTENT_SIZE) {
            ScriptListExtent* newExtent = scriptListExtent->next = (ScriptListExtent*)mem_malloc(sizeof(*newExtent));
            if (newExtent == NULL) {
                return -1;
            }

            newExtent->length = 0;
            newExtent->next = NULL;

            scriptList->tail = newExtent;
            scriptList->length++;

            scriptListExtent = newExtent;
        }
    } else {
        // Script head
        scriptListExtent = (ScriptListExtent*)mem_malloc(sizeof(ScriptListExtent));
        if (scriptListExtent == NULL) {
            return -1;
        }

        scriptListExtent->length = 0;
        scriptListExtent->next = NULL;

        scriptList->head = scriptListExtent;
        scriptList->tail = scriptListExtent;
        scriptList->length = 1;
    }

    int sid = scr_new_id(scriptType) | (scriptType << 24);

    *sidPtr = sid;

    Script* scr = &(scriptListExtent->scripts[scriptListExtent->length]);
    scr->scr_id = sid;
    scr->sp.built_tile = -1;
    scr->sp.radius = -1;
    scr->scr_flags = 0;
    scr->scr_script_idx = -1;
    scr->program = 0;
    scr->scr_local_var_offset = -1;
    scr->scr_num_local_vars = 0;
    scr->field_28 = 0;
    scr->action = 0;
    scr->fixedParam = 0;
    scr->owner = 0;
    scr->source = 0;
    scr->target = 0;
    scr->actionBeingUsed = -1;
    scr->scriptOverrides = 0;
    scr->field_48 = 0;
    scr->howMuch = 0;
    scr->run_info_flags = 0;

    for (int index = 0; index < SCRIPT_PROC_COUNT; index++) {
        scr->procs[index] = SCRIPT_PROC_NO_PROC;
    }

    scriptListExtent->length++;

    return 0;
}

// 0x494284
int scr_remove_local_vars(Script* script)
{
    if (script == NULL) {
        return -1;
    }

    if (script->scr_num_local_vars != 0) {
        int oldMapLocalVarsCount = num_map_local_vars;
        if (oldMapLocalVarsCount > 0 && script->scr_local_var_offset >= 0) {
            num_map_local_vars -= script->scr_num_local_vars;

            if (oldMapLocalVarsCount - script->scr_num_local_vars != script->scr_local_var_offset && script->scr_local_var_offset != -1) {
                memmove(map_local_vars + script->scr_local_var_offset,
                    map_local_vars + (script->scr_local_var_offset + script->scr_num_local_vars),
                    sizeof(*map_local_vars) * (oldMapLocalVarsCount - script->scr_num_local_vars - script->scr_local_var_offset));

                map_local_vars = (int*)mem_realloc(map_local_vars, sizeof(*map_local_vars) * num_map_local_vars);
                if (map_local_vars == NULL) {
                    debug_printf("\nError in mem_realloc in scr_remove_local_vars!\n");
                }

                for (int index = 0; index < SCRIPT_TYPE_COUNT; index++) {
                    ScriptList* scriptList = &(scriptlists[index]);
                    ScriptListExtent* extent = scriptList->head;
                    while (extent != NULL) {
                        for (int index = 0; index < extent->length; index++) {
                            Script* other = &(extent->scripts[index]);
                            if (other->scr_local_var_offset > script->scr_local_var_offset) {
                                other->scr_local_var_offset -= script->scr_num_local_vars;
                            }
                        }
                        extent = extent->next;
                    }
                }
            }
        }
    }

    return 0;
}

// 0x494384
int scr_remove(int sid)
{
    if (sid == -1) {
        return -1;
    }

    ScriptList* scriptList = &(scriptlists[SID_TYPE(sid)]);

    ScriptListExtent* scriptListExtent = scriptList->head;
    int index;
    while (scriptListExtent != NULL) {
        for (index = 0; index < scriptListExtent->length; index++) {
            Script* script = &(scriptListExtent->scripts[index]);
            if (script->scr_id == sid) {
                break;
            }
        }

        if (index < scriptListExtent->length) {
            break;
        }

        scriptListExtent = scriptListExtent->next;
    }

    if (scriptListExtent == NULL) {
        return -1;
    }

    Script* script = &(scriptListExtent->scripts[index]);
    if ((script->scr_flags & SCRIPT_FLAG_0x02) != 0) {
        if (script->program != NULL) {
            script->program = NULL;
        }
    }

    if ((script->scr_flags & SCRIPT_FLAG_0x10) == 0) {
        // NOTE: Uninline.
        scripts_clear_combat_requests(script);

        if (scr_remove_local_vars(script) == -1) {
            debug_printf("\nERROR Removing local vars on scr_remove!!\n");
        }

        if (queue_remove_this(script->owner, EVENT_TYPE_SCRIPT) == -1) {
            debug_printf("\nERROR Removing Timed Events on scr_remove!!\n");
        }

        if (scriptListExtent == scriptList->tail && index + 1 == scriptListExtent->length) {
            // Removing last script in tail extent
            scriptListExtent->length -= 1;

            if (scriptListExtent->length == 0) {
                scriptList->length--;
                mem_free(scriptListExtent);

                if (scriptList->length != 0) {
                    ScriptListExtent* v13 = scriptList->head;
                    while (scriptList->tail != v13->next) {
                        v13 = v13->next;
                    }
                    v13->next = NULL;
                    scriptList->tail = v13;
                } else {
                    scriptList->head = NULL;
                    scriptList->tail = NULL;
                }
            }
        } else {
            // Relocate last script from tail extent into this script's slot.
            memcpy(&(scriptListExtent->scripts[index]), &(scriptList->tail->scripts[scriptList->tail->length - 1]), sizeof(Script));

            // Decrement number of scripts in tail extent.
            scriptList->tail->length -= 1;

            // Check to see if this extent became empty.
            if (scriptList->tail->length == 0) {
                scriptList->length -= 1;

                // Find previous extent that is about to become a new tail for
                // this script list.
                ScriptListExtent* prev = scriptList->head;
                while (prev->next != scriptList->tail) {
                    prev = prev->next;
                }
                prev->next = NULL;

                mem_free(scriptList->tail);
                scriptList->tail = prev;
            }
        }
    }

    return 0;
}

// 0x4945AC
int scr_remove_all()
{
    queue_clear_type(EVENT_TYPE_SCRIPT, NULL);
    scr_message_free();

    for (int scriptType = 0; scriptType < SCRIPT_TYPE_COUNT; scriptType++) {
        ScriptList* scriptList = &(scriptlists[scriptType]);

        ScriptListExtent* scriptListExtent = scriptList->head;
        while (scriptListExtent != NULL) {
            int index = 0;
            while (scriptListExtent != NULL && index < scriptListExtent->length) {
                Script* script = &(scriptListExtent->scripts[index]);

                if ((script->scr_flags & SCRIPT_FLAG_0x10) != 0) {
                    index++;
                } else {
                    if (index == 0 && scriptListExtent->length == 1) {
                        scriptListExtent = scriptListExtent->next;
                        scr_remove(script->scr_id);
                    } else {
                        scr_remove(script->scr_id);
                    }
                }
            }

            if (scriptListExtent != NULL) {
                scriptListExtent = scriptListExtent->next;
            }
        }
    }

    scr_find_first_idx = 0;
    scr_find_first_ptr = NULL;
    scr_find_first_elev = 0;
    map_script_id = -1;

    clearPrograms();
    exportClearAllVariables();

    return 0;
}

// 0x494674
int scr_remove_all_force()
{
    queue_clear_type(EVENT_TYPE_SCRIPT, NULL);
    scr_message_free();

    for (int type = 0; type < SCRIPT_TYPE_COUNT; type++) {
        ScriptList* scriptList = &(scriptlists[type]);
        ScriptListExtent* extent = scriptList->head;
        while (extent != NULL) {
            ScriptListExtent* next = extent->next;
            mem_free(extent);
            extent = next;
        }

        scriptList->head = NULL;
        scriptList->tail = NULL;
        scriptList->length = 0;
    }

    scr_find_first_idx = 0;
    scr_find_first_ptr = 0;
    scr_find_first_elev = 0;
    map_script_id = -1;
    clearPrograms();
    exportClearAllVariables();

    return 0;
}

// 0x4946F0
Script* scr_find_first_at(int elevation)
{
    scr_find_first_elev = elevation;
    scr_find_first_idx = 0;
    scr_find_first_ptr = scriptlists[SCRIPT_TYPE_SPATIAL].head;

    if (scr_find_first_ptr == NULL) {
        return NULL;
    }

    Script* script = &(scr_find_first_ptr->scripts[0]);
    if ((script->scr_flags & SCRIPT_FLAG_0x02) != 0 || builtTileGetElevation(script->sp.built_tile) != elevation) {
        script = scr_find_next_at();
    }

    return script;
}

// 0x494730
Script* scr_find_next_at()
{
    ScriptListExtent* scriptListExtent = scr_find_first_ptr;
    int scriptIndex = scr_find_first_idx;

    if (scriptListExtent == NULL) {
        return NULL;
    }

    for (;;) {
        scriptIndex++;

        if (scriptIndex == SCRIPT_LIST_EXTENT_SIZE) {
            scriptListExtent = scriptListExtent->next;
            scriptIndex = 0;
        } else if (scriptIndex >= scriptListExtent->length) {
            scriptListExtent = NULL;
        }

        if (scriptListExtent == NULL) {
            break;
        }

        Script* script = &(scriptListExtent->scripts[scriptIndex]);
        if ((script->scr_flags & SCRIPT_FLAG_0x02) == 0 && builtTileGetElevation(script->sp.built_tile) == scr_find_first_elev) {
            break;
        }
    }

    Script* script;
    if (scriptListExtent != NULL) {
        script = &(scriptListExtent->scripts[scriptIndex]);
    } else {
        script = NULL;
    }

    scr_find_first_idx = scriptIndex;
    scr_find_first_ptr = scriptListExtent;

    return script;
}

// 0x4947BC
bool scr_spatials_enabled()
{
    return scrSpatialsEnabled;
}

// 0x4947C4
void scr_spatials_enable()
{
    scrSpatialsEnabled = true;
}

// 0x4947D4
void scr_spatials_disable()
{
    scrSpatialsEnabled = false;
}

// 0x4947E4
bool scr_chk_spatials_in(Object* object, int tile, int elevation)
{
    Script* script;
    int built_tile;

    if (object == obj_mouse) {
        return false;
    }

    if (object == obj_mouse_flat) {
        return false;
    }

    if ((object->flags & OBJECT_HIDDEN) != 0 || (object->flags & OBJECT_FLAT) != 0) {
        return false;
    }

    if (tile < 10) {
        return false;
    }

    if (!scr_spatials_enabled()) {
        return false;
    }

    scr_spatials_disable();

    built_tile = builtTileCreate(tile, elevation);

    script = scr_find_first_at(elevation);
    while (script != NULL) {
        if (built_tile == script->sp.built_tile) {
            // NOTE: Uninline.
            scr_set_objs(script->scr_id, object, NULL);
            exec_script_proc(script->scr_id, SCRIPT_PROC_SPATIAL);
        } else {
            if (script->sp.radius != 0) {
                if (tile_in_tile_bound(builtTileGetTile(script->sp.built_tile), script->sp.radius, tile)) {
                    // NOTE: Uninline.
                    scr_set_objs(script->scr_id, object, NULL);
                    exec_script_proc(script->scr_id, SCRIPT_PROC_SPATIAL);
                }
            }
        }

        script = scr_find_next_at();
    }

    scr_spatials_enable();

    return true;
}

// 0x4948F8
bool tile_in_tile_bound(int tile1, int radius, int tile2)
{
    return tile_dist(tile1, tile2) <= radius;
}

// 0x494960
int scr_load_all_scripts()
{
    for (int scriptListIndex = 0; scriptListIndex < SCRIPT_TYPE_COUNT; scriptListIndex++) {
        ScriptList* scriptList = &(scriptlists[scriptListIndex]);
        ScriptListExtent* extent = scriptList->head;
        while (extent != NULL) {
            for (int scriptIndex = 0; scriptIndex < extent->length; scriptIndex++) {
                Script* script = &(extent->scripts[scriptIndex]);
                exec_script_proc(script->scr_id, SCRIPT_PROC_START);
            }
            extent = extent->next;
        }
    }

    return 0;
}

// 0x4949C0
void scr_exec_map_enter_scripts()
{
    int script_type;
    ScriptListExtent* script_list_extent;
    int script_index;
    int sid;

    scr_spatials_disable();

    for (script_type = 0; script_type < SCRIPT_TYPE_COUNT; script_type++) {
        script_list_extent = scriptlists[script_type].head;
        while (script_list_extent != NULL) {
            for (script_index = 0; script_index < script_list_extent->length; script_index++) {
                if (script_list_extent->scripts[script_index].procs[SCRIPT_PROC_MAP_ENTER] > 0) {
                    sid = script_list_extent->scripts[script_index].scr_id;
                    if (sid != map_script_id) {
                        scr_set_ext_param(sid, (map_data.flags & 0x1) == 0);
                        exec_script_proc(sid, SCRIPT_PROC_MAP_ENTER);
                    }
                }
            }
            script_list_extent = script_list_extent->next;
        }
    }

    scr_spatials_enable();
}

// 0x494A70
void scr_exec_map_update_scripts()
{
    int script_type;
    ScriptListExtent* script_list_extent;
    int script_index;
    int sid;

    scr_spatials_disable();

    exec_script_proc(map_script_id, SCRIPT_PROC_MAP_UPDATE);

    for (script_type = 0; script_type < SCRIPT_TYPE_COUNT; script_type++) {
        script_list_extent = scriptlists[script_type].head;
        while (script_list_extent != NULL) {
            for (script_index = 0; script_index < script_list_extent->length; script_index++) {
                if (script_list_extent->scripts[script_index].procs[SCRIPT_PROC_MAP_UPDATE] > 0) {
                    sid = script_list_extent->scripts[script_index].scr_id;
                    if (sid != map_script_id) {
                        exec_script_proc(sid, SCRIPT_PROC_MAP_UPDATE);
                    }
                }
            }
            script_list_extent = script_list_extent->next;
        }
    }

    scr_spatials_enable();
}

// 0x494AFC
void scr_exec_map_exit_scripts()
{
    exec_script_proc(map_script_id, SCRIPT_PROC_MAP_EXIT);
}

// 0x494CC8
int scr_get_dialog_msg_file(int a1, MessageList** messageListPtr)
{
    if (a1 == -1) {
        return -1;
    }

    int messageListIndex = a1 - 1;
    MessageList* messageList = &(script_dialog_msgs[messageListIndex]);
    if (messageList->entries_num == 0) {
        char scriptName[16];
        scr_list_str(messageListIndex, scriptName, sizeof(scriptName));

        char* pch = strrchr(scriptName, '.');
        if (pch != NULL) {
            *pch = '\0';
        }

        char path[COMPAT_MAX_PATH];
        snprintf(path, sizeof(path), "dialog\\%s.msg", scriptName);

        if (!message_load(messageList, path)) {
            debug_printf("\nError loading script dialog message file!");
            return -1;
        }

        if (!message_filter(messageList)) {
            debug_printf("\nError filtering script dialog message file!");
            return -1;
        }
    }

    *messageListPtr = messageList;

    return 0;
}

// 0x494DB4
char* scr_get_msg_str(int messageListId, int messageId)
{
    return scr_get_msg_str_speech(messageListId, messageId, 0);
}

// 0x494DC0
char* scr_get_msg_str_speech(int messageListId, int messageId, int a3)
{
    // 0x507974
    static char err_str[] = "Error";

    // 0x507978
    static char blank_str[] = "";

    if (messageListId == 0 && messageId == 0) {
        return blank_str;
    }

    if (messageListId == -1 && messageId == -1) {
        return blank_str;
    }

    if (messageListId == -2 && messageId == -2) {
        MessageListItem messageListItem;
        return getmsg(&proto_main_msg_file, &messageListItem, 650);
    }

    MessageList* messageList;
    if (scr_get_dialog_msg_file(messageListId, &messageList) == -1) {
        debug_printf("\nERROR: message_str: can't find message file: List: %d!", messageListId);
        return NULL;
    }

    if (FID_TYPE(dialogue_head) != OBJ_TYPE_HEAD) {
        a3 = 0;
    }

    MessageListItem messageListItem;
    messageListItem.num = messageId;
    if (!message_search(messageList, &messageListItem)) {
        debug_printf("\nError: can't find message: List: %d, Num: %d!", messageListId, messageId);
        return err_str;
    }

    if (a3) {
        if (dialog_active()) {
            if (messageListItem.audio != NULL && messageListItem.audio[0] != '\0') {
                gdialog_setup_speech(messageListItem.audio);
            } else {
                debug_printf("Missing speech name: %d\n", messageListItem.num);
            }
        }
    }

    return messageListItem.text;
}

// 0x494EA4
int scr_get_local_var(int sid, int variable, ProgramValue& value)
{
    Script* script;

    if (SID_TYPE(sid) == SCRIPT_TYPE_SYSTEM) {
        debug_printf("\nError! System scripts/Map scripts not allowed local_vars!\n");
        value.opcode = VALUE_TYPE_INT;
        value.integerValue = -1;
        return -1;
    }

    if (scr_ptr(sid, &script) == -1) {
        value.opcode = VALUE_TYPE_INT;
        value.integerValue = -1;
        return -1;
    }

    if (script->scr_num_local_vars == 0) {
        // NOTE: Uninline.
        scr_find_str_run_info(script->scr_script_idx, &(script->run_info_flags), sid);
    }

    if (script->scr_num_local_vars > 0) {
        if (script->scr_local_var_offset == -1) {
            script->scr_local_var_offset = map_malloc_local_var(script->scr_num_local_vars);
        }

        if (map_get_local_var(script->scr_local_var_offset + variable, value) == -1) {
            value.opcode = VALUE_TYPE_INT;
            value.integerValue = -1;
            return -1;
        }
    }

    return 0;
}

// 0x494F50
int scr_set_local_var(int sid, int variable, ProgramValue& value)
{
    Script* script;

    if (scr_ptr(sid, &script) == -1) {
        return -1;
    }

    if (script->scr_num_local_vars == 0) {
        // NOTE: Uninline.
        scr_find_str_run_info(script->scr_script_idx, &(script->run_info_flags), sid);
    }

    if (script->scr_num_local_vars <= 0) {
        return -1;
    }

    if (script->scr_local_var_offset == -1) {
        script->scr_local_var_offset = map_malloc_local_var(script->scr_num_local_vars);
    }

    map_set_local_var(script->scr_local_var_offset + variable, value);

    return 0;
}

// Performs combat script and returns true if default action has been overriden
// by script.
//
// 0x494FD4
bool scr_end_combat()
{
    if (map_script_id == 0 || map_script_id == -1) {
        return false;
    }

    int team = combat_player_knocked_out_by();
    if (team == -1) {
        return false;
    }

    Script* before;
    if (scr_ptr(map_script_id, &before) != -1) {
        before->fixedParam = team;
    }

    exec_script_proc(map_script_id, SCRIPT_PROC_COMBAT);

    bool success = false;

    Script* after;
    if (scr_ptr(map_script_id, &after) != -1) {
        if (after->scriptOverrides != 0) {
            success = true;
        }
    }

    return success;
}

// 0x495048
int scr_explode_scenery(Object* a1, int tile, int radius, int elevation)
{
    int scriptExtentsCount = scriptlists[SCRIPT_TYPE_SPATIAL].length + scriptlists[SCRIPT_TYPE_ITEM].length;
    if (scriptExtentsCount == 0) {
        return 0;
    }

    int* scriptIds = (int*)mem_malloc(sizeof(*scriptIds) * scriptExtentsCount * SCRIPT_LIST_EXTENT_SIZE);
    if (scriptIds == NULL) {
        return -1;
    }

    ScriptListExtent* extent;
    int scriptsCount = 0;

    scr_spatials_disable();

    extent = scriptlists[SCRIPT_TYPE_ITEM].head;
    while (extent != NULL) {
        for (int index = 0; index < extent->length; index++) {
            Script* script = &(extent->scripts[index]);
            if (script->procs[SCRIPT_PROC_DAMAGE] <= 0 && script->program == NULL) {
                exec_script_proc(script->scr_id, SCRIPT_PROC_START);
            }

            if (script->procs[SCRIPT_PROC_DAMAGE] > 0) {
                Object* self = script->owner;
                if (self != NULL) {
                    if (self->elevation == elevation && tile_dist(self->tile, tile) <= radius) {
                        scriptIds[scriptsCount] = script->scr_id;
                        scriptsCount += 1;
                    }
                }
            }
        }
        extent = extent->next;
    }

    extent = scriptlists[SCRIPT_TYPE_SPATIAL].head;
    while (extent != NULL) {
        for (int index = 0; index < extent->length; index++) {
            Script* script = &(extent->scripts[index]);
            if (script->procs[SCRIPT_PROC_DAMAGE] <= 0 && script->program == NULL) {
                exec_script_proc(script->scr_id, SCRIPT_PROC_START);
            }

            if (script->procs[SCRIPT_PROC_DAMAGE] > 0
                && builtTileGetElevation(script->sp.built_tile) == elevation
                && tile_dist(builtTileGetTile(script->sp.built_tile), tile) <= radius) {
                scriptIds[scriptsCount] = script->scr_id;
                scriptsCount += 1;
            }
        }
        extent = extent->next;
    }

    for (int index = 0; index < scriptsCount; index++) {
        exec_script_proc(scriptIds[index], SCRIPT_PROC_DAMAGE);
    }

    // TODO: Redundant, we already know `scriptIds` is not NULL.
    if (scriptIds != NULL) {
        mem_free(scriptIds);
    }

    scr_spatials_enable();

    return 0;
}

} // namespace fallout
