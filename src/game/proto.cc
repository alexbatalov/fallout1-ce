#include "game/proto.h"

#include <stdio.h>
#include <string.h>

#include "game/art.h"
#include "game/combat.h"
#include "game/config.h"
#include "game/critter.h"
#include "game/editor.h"
#include "game/game.h"
#include "game/gconfig.h"
#include "game/gmovie.h"
#include "game/intface.h"
#include "game/map.h"
#include "game/object.h"
#include "game/perk.h"
#include "game/skill.h"
#include "game/stat.h"
#include "game/trait.h"
#include "int/dialog.h"
#include "platform_compat.h"
#include "plib/gnw/debug.h"
#include "plib/gnw/memory.h"

namespace fallout {

static char* proto_get_msg_info(int pid, int message);
static int proto_read_CombatData(CritterCombatData* data, DB_FILE* stream);
static int proto_write_CombatData(CritterCombatData* data, DB_FILE* stream);
static int proto_read_item_data(ItemProtoData* item_data, int type, DB_FILE* stream);
static int proto_read_scenery_data(SceneryProtoData* scenery_data, int type, DB_FILE* stream);
static int proto_read_protoSubNode(Proto* buf, DB_FILE* stream);
static int proto_write_item_data(ItemProtoData* item_data, int type, DB_FILE* stream);
static int proto_write_scenery_data(SceneryProtoData* scenery_data, int type, DB_FILE* stream);
static int proto_write_protoSubNode(Proto* buf, DB_FILE* stream);
static int proto_new_id(int a1);

// 0x50734C
char cd_path_base[COMPAT_MAX_PATH];

// 0x507450
static ProtoList protolists[11] = {
    { 0, 0, 0, 1 },
    { 0, 0, 0, 1 },
    { 0, 0, 0, 1 },
    { 0, 0, 0, 1 },
    { 0, 0, 0, 1 },
    { 0, 0, 0, 1 },
    { 0, 0, 0, 1 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
};

// 0x507500
static const size_t proto_sizes[11] = {
    sizeof(ItemProto), // 0x84
    sizeof(CritterProto), // 19C
    sizeof(SceneryProto), // 0x38
    sizeof(WallProto), // 0x24
    sizeof(TileProto), // 0x1C
    sizeof(MiscProto), // 0x1C
    0,
    0,
    0,
    0,
    0,
};

// 0x50752C
static int protos_been_initialized = 0;

// 0x507530
static CritterProto pc_proto = {
    0x1000000,
    -1,
    0x1000001,
    0,
    0,
    0x20000000,
    0,
    -1,
    0,
    { 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 18, 0, 0, 0, 0, 0, 0, 0, 0, 100, 0, 0, 0, 23, 0 },
    { 0 },
    { 0 },
    0,
    0,
    0,
    0,
    -1,
    0,
};

// 0x5076CC
static int proto_blocking_list[9] = {
    0x2000043,
    0x2000080,
    0x200008D,
    0x2000158,
    0x300026D,
    0x300026E,
    0x500000C,
    0x5000005,
    0x2000031,
};

// 0x5076F0
char proto_path_base[] = "proto\\";

// 0x50D1B0
static char _aDrugStatSpecia[] = "Drug Stat (Special)";

// 0x50D1C4
static char _aNone_1[] = "None";

// 0x662CA8
char* mp_critter_stats_list[2 + STAT_COUNT];

// Message list by object type
// 0 - pro_item.msg
// 1 - pro_crit.msg
// 2 - pro_scen.msg
// 3 - pro_wall.msg
// 4 - pro_tile.msg
// 5 - pro_misc.msg
//
// 0x662D48
MessageList proto_msg_files[6];

// 0x662D78
char* race_type_strs[RACE_TYPE_COUNT];

// 0x662D80
char* scenery_pro_type[SCENERY_TYPE_COUNT];

// proto.msg
//
// 0x662D98
MessageList proto_main_msg_file;

// 0x662DA0
char* cal_type_strs[CALIBER_TYPE_COUNT];

// 0x662DD8
char* item_pro_material[MATERIAL_TYPE_COUNT];

// 0x662DF8
char* mp_perk_code_strs[1 + PERK_COUNT];

// "<None>" from proto.msg
//
// 0x662EFC
char* proto_none_str;

// 0x662F00
char* body_type_strs[BODY_TYPE_COUNT];

// 0x662F0C
char* item_pro_type[ITEM_TYPE_COUNT];

// 0x662F28
char* damage_code_strs[DAMAGE_TYPE_COUNT];

// Perk names.
//
// 0x662F44
char** perk_code_strs;

// Stat names.
//
// 0x662F48
char** critter_stats_list;

// 0x48C87C
void proto_make_path(char* path, int pid)
{
    strcpy(path, cd_path_base);
    strcat(path, proto_path_base);
    if (pid != -1) {
        strcat(path, art_dir(PID_TYPE(pid)));
    }
}

// Append proto file name to proto_path from proto.lst.
//
// 0x48CD64
int proto_list_str(int pid, char* proto_path)
{
    if (pid == -1) {
        return -1;
    }

    if (proto_path == NULL) {
        return -1;
    }

    char path[COMPAT_MAX_PATH];
    proto_make_path(path, pid);
    strcat(path, "\\");
    strcat(path, art_dir(PID_TYPE(pid)));
    strcat(path, ".lst");

    DB_FILE* stream = db_fopen(path, "rt");

    int i = 1;
    char string[256];
    while (db_fgets(string, sizeof(string), stream)) {
        if (i == (pid & 0xFFFFFF)) {
            break;
        }

        i++;
    }

    db_fclose(stream);

    if (i != (pid & 0xFFFFFF)) {
        return -1;
    }

    char* pch = strchr(string, ' ');
    if (pch != NULL) {
        *pch = '\0';
    }

    pch = strchr(string, '\n');
    if (pch != NULL) {
        *pch = '\0';
    }

    strcpy(proto_path, string);

    return 0;
}

// 0x48CF90
size_t proto_size(int type)
{
    return type >= 0 && type < OBJ_TYPE_COUNT ? proto_sizes[type] : 0;
}

// 0x48CFA8
bool proto_action_can_use(int pid)
{
    Proto* proto;
    if (proto_ptr(pid, &proto) == -1) {
        return false;
    }

    if ((proto->item.extendedFlags & 0x0800) != 0) {
        return true;
    }

    if (PID_TYPE(pid) == OBJ_TYPE_ITEM && proto->item.type == ITEM_TYPE_CONTAINER) {
        return true;
    }

    return false;
}

// 0x48CFE8
bool proto_action_can_use_on(int pid)
{
    Proto* proto;
    if (proto_ptr(pid, &proto) == -1) {
        return false;
    }

    if ((proto->item.extendedFlags & 0x1000) != 0) {
        return true;
    }

    if (PID_TYPE(pid) == OBJ_TYPE_ITEM && proto->item.type == ITEM_TYPE_DRUG) {
        return true;
    }

    return false;
}

// 0x48D028
bool proto_action_can_look_at(int pid)
{
    return 1;
}

// 0x48D030
bool proto_action_can_talk_to(int pid)
{
    Proto* proto;
    if (proto_ptr(pid, &proto) == -1) {
        return false;
    }

    if (PID_TYPE(pid) == OBJ_TYPE_CRITTER) {
        return true;
    }

    if (proto->critter.extendedFlags & 0x4000) {
        return true;
    }

    return false;
}

// Likely returns true if item with given pid can be picked up.
//
// 0x48D068
int proto_action_can_pickup(int pid)
{
    if (PID_TYPE(pid) != OBJ_TYPE_ITEM) {
        return false;
    }

    Proto* proto;
    if (proto_ptr(pid, &proto) == -1) {
        return false;
    }

    if (proto->item.type == ITEM_TYPE_CONTAINER) {
        return (proto->item.extendedFlags & 0x8000) != 0;
    }

    return true;
}

// 0x48D0B0
static char* proto_get_msg_info(int pid, int message)
{
    char* v1 = proto_none_str;

    Proto* proto;
    if (proto_ptr(pid, &proto) != -1) {
        if (proto->messageId != -1) {
            MessageList* messageList = &(proto_msg_files[PID_TYPE(pid)]);

            MessageListItem messageListItem;
            messageListItem.num = proto->messageId + message;
            if (message_search(messageList, &messageListItem)) {
                v1 = messageListItem.text;
            }
        }
    }

    return v1;
}

// 0x48D108
char* proto_name(int pid)
{
    if (pid == 0x1000000) {
        return critter_name(obj_dude);
    }

    return proto_get_msg_info(pid, PROTOTYPE_MESSAGE_NAME);
}

// 0x48D128
char* proto_description(int pid)
{
    return proto_get_msg_info(pid, PROTOTYPE_MESSAGE_DESCRIPTION);
}

// 0x48D3C0
int proto_critter_init(Proto* a1, int a2)
{
    if (!protos_been_initialized) {
        return -1;
    }

    int v1 = a2 & 0xFFFFFF;

    a1->pid = -1;
    a1->messageId = 100 * v1;
    a1->fid = art_id(OBJ_TYPE_CRITTER, v1 - 1, 0, 0, 0);
    a1->critter.lightDistance = 0;
    a1->critter.lightIntensity = 0;
    a1->critter.flags = 0x20000000;
    a1->critter.extendedFlags = 0x6000;
    a1->critter.sid = -1;
    a1->critter.data.flags = 0;
    a1->critter.data.bodyType = 0;
    a1->critter.headFid = -1;
    a1->critter.aiPacket = 1;
    if (!art_exists(a1->fid)) {
        a1->fid = art_id(OBJ_TYPE_CRITTER, 0, 0, 0, 0);
    }

    CritterProtoData* data = &(a1->critter.data);
    data->experience = 60;
    data->killType = 0;
    stat_set_defaults(data);
    skill_set_defaults(data);

    return 0;
}

// 0x48D4A8
void clear_pupdate_data(Object* obj)
{
    // NOTE: Original code is slightly different. It uses loop to zero object
    // data byte by byte.
    memset(&(obj->data), 0, sizeof(obj->data));
}

// 0x48D4BC
static int proto_read_CombatData(CritterCombatData* data, DB_FILE* stream)
{
    if (db_freadInt32(stream, &(data->damageLastTurn)) == -1) return -1;
    if (db_freadInt32(stream, &(data->maneuver)) == -1) return -1;
    if (db_freadInt32(stream, &(data->ap)) == -1) return -1;
    if (db_freadInt32(stream, &(data->results)) == -1) return -1;
    if (db_freadInt32(stream, &(data->aiPacket)) == -1) return -1;
    if (db_freadInt32(stream, &(data->team)) == -1) return -1;
    if (db_freadInt32(stream, &(data->whoHitMeCid)) == -1) return -1;

    return 0;
}

// 0x48D544
static int proto_write_CombatData(CritterCombatData* data, DB_FILE* stream)
{
    if (db_fwriteInt32(stream, data->damageLastTurn) == -1) return -1;
    if (db_fwriteInt32(stream, data->maneuver) == -1) return -1;
    if (db_fwriteInt32(stream, data->ap) == -1) return -1;
    if (db_fwriteInt32(stream, data->results) == -1) return -1;
    if (db_fwriteInt32(stream, data->aiPacket) == -1) return -1;
    if (db_fwriteInt32(stream, data->team) == -1) return -1;
    if (db_fwriteInt32(stream, data->whoHitMeCid) == -1) return -1;

    return 0;
}

// 0x48D608
int proto_read_protoUpdateData(Object* obj, DB_FILE* stream)
{
    Proto* proto;
    int temp;

    Inventory* inventory = &(obj->data.inventory);
    if (db_freadInt32(stream, &(inventory->length)) == -1) return -1;
    if (db_freadInt32(stream, &(inventory->capacity)) == -1) return -1;
    // CE: Original code reads inventory items pointer which is meaningless.
    if (db_freadInt32(stream, &temp) == -1) return -1;

    if (PID_TYPE(obj->pid) == OBJ_TYPE_CRITTER) {
        if (db_freadInt32(stream, &(obj->data.critter.field_0)) == -1) return -1;
        if (proto_read_CombatData(&(obj->data.critter.combat), stream) == -1) return -1;
        if (db_freadInt32(stream, &(obj->data.critter.hp)) == -1) return -1;
        if (db_freadInt32(stream, &(obj->data.critter.radiation)) == -1) return -1;
        if (db_freadInt32(stream, &(obj->data.critter.poison)) == -1) return -1;
    } else {
        if (db_freadInt32(stream, &(obj->data.flags)) == -1) return -1;

        if (obj->data.flags == 0xCCCCCCCC) {
            debug_printf("\nNote: Reading pud: updated_flags was un-Set!");
            obj->data.flags = 0;
        }

        switch (PID_TYPE(obj->pid)) {
        case OBJ_TYPE_ITEM:
            if (proto_ptr(obj->pid, &proto) == -1) return -1;

            switch (proto->item.type) {
            case ITEM_TYPE_WEAPON:
                if (db_freadInt32(stream, &(obj->data.item.weapon.ammoQuantity)) == -1) return -1;
                if (db_freadInt32(stream, &(obj->data.item.weapon.ammoTypePid)) == -1) return -1;
                break;
            case ITEM_TYPE_AMMO:
                if (db_freadInt32(stream, &(obj->data.item.ammo.quantity)) == -1) return -1;
                break;
            case ITEM_TYPE_MISC:
                if (db_freadInt32(stream, &(obj->data.item.misc.charges)) == -1) return -1;
                break;
            case ITEM_TYPE_KEY:
                if (db_freadInt32(stream, &(obj->data.item.key.keyCode)) == -1) return -1;
                break;
            default:
                break;
            }

            break;
        case OBJ_TYPE_SCENERY:
            if (proto_ptr(obj->pid, &proto) == -1) return -1;

            switch (proto->scenery.type) {
            case SCENERY_TYPE_DOOR:
                if (db_freadInt32(stream, &(obj->data.scenery.door.openFlags)) == -1) return -1;
                break;
            case SCENERY_TYPE_STAIRS:
                if (db_freadInt32(stream, &(obj->data.scenery.stairs.destinationMap)) == -1) return -1;
                if (db_freadInt32(stream, &(obj->data.scenery.stairs.destinationBuiltTile)) == -1) return -1;
                break;
            case SCENERY_TYPE_ELEVATOR:
                if (db_freadInt32(stream, &(obj->data.scenery.elevator.type)) == -1) return -1;
                if (db_freadInt32(stream, &(obj->data.scenery.elevator.level)) == -1) return -1;
                break;
            case SCENERY_TYPE_LADDER_UP:
                if (db_freadInt32(stream, &(obj->data.scenery.ladder.destinationBuiltTile)) == -1) return -1;
                break;
            case SCENERY_TYPE_LADDER_DOWN:
                if (db_freadInt32(stream, &(obj->data.scenery.ladder.destinationBuiltTile)) == -1) return -1;
                break;
            }

            break;
        case OBJ_TYPE_MISC:
            if (obj->pid >= 0x5000010 && obj->pid <= 0x5000017) {
                if (db_freadInt32(stream, &(obj->data.misc.map)) == -1) return -1;
                if (db_freadInt32(stream, &(obj->data.misc.tile)) == -1) return -1;
                if (db_freadInt32(stream, &(obj->data.misc.elevation)) == -1) return -1;
                if (db_freadInt32(stream, &(obj->data.misc.rotation)) == -1) return -1;
            }
            break;
        }
    }

    return 0;
}

// 0x48D9B4
int proto_write_protoUpdateData(Object* obj, DB_FILE* stream)
{
    Proto* proto;

    ObjectData* data = &(obj->data);
    if (db_fwriteInt32(stream, data->inventory.length) == -1) return -1;
    if (db_fwriteInt32(stream, data->inventory.capacity) == -1) return -1;
    // CE: Original code writes inventory items pointer, which is meaningless.
    if (db_fwriteInt32(stream, 0) == -1) return -1;

    if (PID_TYPE(obj->pid) == OBJ_TYPE_CRITTER) {
        if (db_fwriteInt32(stream, data->flags) == -1) return -1;
        if (proto_write_CombatData(&(obj->data.critter.combat), stream) == -1) return -1;
        if (db_fwriteInt32(stream, data->critter.hp) == -1) return -1;
        if (db_fwriteInt32(stream, data->critter.radiation) == -1) return -1;
        if (db_fwriteInt32(stream, data->critter.poison) == -1) return -1;
    } else {
        if (db_fwriteInt32(stream, data->flags) == -1) return -1;

        switch (PID_TYPE(obj->pid)) {
        case OBJ_TYPE_ITEM:
            if (proto_ptr(obj->pid, &proto) == -1) return -1;

            switch (proto->item.type) {
            case ITEM_TYPE_WEAPON:
                if (db_fwriteInt32(stream, data->item.weapon.ammoQuantity) == -1) return -1;
                if (db_fwriteInt32(stream, data->item.weapon.ammoTypePid) == -1) return -1;
                break;
            case ITEM_TYPE_AMMO:
                if (db_fwriteInt32(stream, data->item.ammo.quantity) == -1) return -1;
                break;
            case ITEM_TYPE_MISC:
                if (db_fwriteInt32(stream, data->item.misc.charges) == -1) return -1;
                break;
            case ITEM_TYPE_KEY:
                if (db_fwriteInt32(stream, data->item.key.keyCode) == -1) return -1;
                break;
            }
            break;
        case OBJ_TYPE_SCENERY:
            if (proto_ptr(obj->pid, &proto) == -1) return -1;

            switch (proto->scenery.type) {
            case SCENERY_TYPE_DOOR:
                if (db_fwriteInt32(stream, data->scenery.door.openFlags) == -1) return -1;
                break;
            case SCENERY_TYPE_STAIRS:
                if (db_fwriteInt32(stream, data->scenery.stairs.destinationMap) == -1) return -1;
                if (db_fwriteInt32(stream, data->scenery.stairs.destinationBuiltTile) == -1) return -1;
                break;
            case SCENERY_TYPE_ELEVATOR:
                if (db_fwriteInt32(stream, data->scenery.elevator.type) == -1) return -1;
                if (db_fwriteInt32(stream, data->scenery.elevator.level) == -1) return -1;
                break;
            case SCENERY_TYPE_LADDER_UP:
                if (db_fwriteInt32(stream, data->scenery.ladder.destinationBuiltTile) == -1) return -1;
                break;
            case SCENERY_TYPE_LADDER_DOWN:
                if (db_fwriteInt32(stream, data->scenery.ladder.destinationBuiltTile) == -1) return -1;
                break;
            default:
                break;
            }
            break;
        case OBJ_TYPE_MISC:
            if (obj->pid >= 0x5000010 && obj->pid <= 0x5000017) {
                if (db_fwriteInt32(stream, data->misc.map) == -1) return -1;
                if (db_fwriteInt32(stream, data->misc.tile) == -1) return -1;
                if (db_fwriteInt32(stream, data->misc.elevation) == -1) return -1;
                if (db_fwriteInt32(stream, data->misc.rotation) == -1) return -1;
            }
            break;
        default:
            break;
        }
    }

    return 0;
}

// 0x48DCA0
int proto_update_gen(Object* obj)
{
    Proto* proto;

    if (!protos_been_initialized) {
        return -1;
    }

    ObjectData* data = &(obj->data);
    data->inventory.length = 0;
    data->inventory.capacity = 0;
    data->inventory.items = NULL;

    if (proto_ptr(obj->pid, &proto) == -1) {
        return -1;
    }

    switch (PID_TYPE(obj->pid)) {
    case OBJ_TYPE_ITEM:
        switch (proto->item.type) {
        case ITEM_TYPE_CONTAINER:
            data->flags = 0;
            break;
        case ITEM_TYPE_WEAPON:
            data->item.weapon.ammoQuantity = proto->item.data.weapon.ammoCapacity;
            data->item.weapon.ammoTypePid = proto->item.data.weapon.ammoTypePid;
            break;
        case ITEM_TYPE_AMMO:
            data->item.ammo.quantity = proto->item.data.ammo.quantity;
            break;
        case ITEM_TYPE_MISC:
            data->item.misc.charges = proto->item.data.misc.charges;
            break;
        case ITEM_TYPE_KEY:
            data->item.key.keyCode = proto->item.data.key.keyCode;
            break;
        }
        break;
    case OBJ_TYPE_SCENERY:
        switch (proto->scenery.type) {
        case SCENERY_TYPE_DOOR:
            data->scenery.door.openFlags = proto->scenery.data.door.openFlags;
            break;
        case SCENERY_TYPE_STAIRS:
            data->scenery.stairs.destinationMap = proto->scenery.data.stairs.field_0;
            data->scenery.stairs.destinationBuiltTile = proto->scenery.data.stairs.field_4;
            break;
        case SCENERY_TYPE_ELEVATOR:
            data->scenery.elevator.type = proto->scenery.data.elevator.type;
            data->scenery.elevator.level = proto->scenery.data.elevator.level;
            break;
        case SCENERY_TYPE_LADDER_UP:
        case SCENERY_TYPE_LADDER_DOWN:
            data->scenery.ladder.destinationBuiltTile = proto->scenery.data.ladder.field_0;
            break;
        }
        break;
    case OBJ_TYPE_MISC:
        if (obj->pid >= 0x5000010 && obj->pid <= 0x5000017) {
            data->misc.tile = -1;
            data->misc.elevation = 0;
            data->misc.rotation = 0;
            data->misc.map = -1;
        }
        break;
    default:
        break;
    }

    return 0;
}

// 0x48DDE4
int proto_update_init(Object* obj)
{
    if (!protos_been_initialized) {
        return -1;
    }

    if (obj == NULL) {
        return -1;
    }

    if (obj->pid == -1) {
        return -1;
    }

    clear_pupdate_data(obj);

    if (PID_TYPE(obj->pid) != OBJ_TYPE_CRITTER) {
        return proto_update_gen(obj);
    }

    ObjectData* data = &(obj->data);
    data->inventory.length = 0;
    data->inventory.capacity = 0;
    data->inventory.items = NULL;
    combat_data_init(obj);
    data->critter.hp = stat_level(obj, STAT_MAXIMUM_HIT_POINTS);
    data->critter.combat.ap = stat_level(obj, STAT_MAXIMUM_ACTION_POINTS);
    stat_recalc_derived(obj);
    obj->data.critter.combat.whoHitMe = NULL;

    Proto* proto;
    if (proto_ptr(obj->pid, &proto) != -1) {
        data->critter.combat.aiPacket = proto->critter.aiPacket;
        data->critter.combat.team = proto->critter.team;
    }

    return 0;
}

// 0x48DEC8
int proto_dude_update_gender()
{
    Proto* proto;
    if (proto_ptr(0x1000000, &proto) == -1) {
        return -1;
    }

    int art_num;
    if (stat_level(obj_dude, STAT_GENDER) == GENDER_MALE) {
        art_num = art_vault_person_nums[GENDER_MALE];
    } else {
        art_num = art_vault_person_nums[GENDER_FEMALE];
    }

    art_vault_guy_num = art_num;

    if (inven_worn(obj_dude) == NULL) {
        int v1 = 0;
        if (inven_right_hand(obj_dude) != NULL || inven_left_hand(obj_dude) != NULL) {
            v1 = (obj_dude->fid & 0xF000) >> 12;
        }

        int fid = art_id(OBJ_TYPE_CRITTER, art_vault_guy_num, 0, v1, 0);
        obj_change_fid(obj_dude, fid, NULL);
    }

    proto->fid = art_id(OBJ_TYPE_CRITTER, art_vault_guy_num, 0, 0, 0);

    return 0;
}

// 0x48DF90
int proto_dude_init(const char* path)
{
    // 0x51C538
    static int init_true = 0;

    // 0x51C53C
    static int retval = 0;

    pc_proto.fid = art_id(OBJ_TYPE_CRITTER, art_vault_guy_num, 0, 0, 0);

    if (init_true) {
        obj_inven_free(&(obj_dude->data.inventory));
    }

    init_true = 1;

    Proto* proto;
    if (proto_ptr(0x1000000, &proto) == -1) {
        return -1;
    }

    proto_ptr(obj_dude->pid, &proto);

    proto_update_init(obj_dude);
    obj_dude->data.critter.combat.aiPacket = 0;
    obj_dude->data.critter.combat.team = 0;
    ResetPlayer();

    if (pc_load_data(path) == -1) {
        retval = -1;
    }

    proto->critter.data.baseStats[STAT_DAMAGE_RESISTANCE_EMP] = 100;
    proto->critter.data.bodyType = 0;
    proto->critter.data.experience = 0;
    proto->critter.data.killType = 0;

    proto_dude_update_gender();
    inven_reset_dude();

    if ((obj_dude->flags & OBJECT_FLAT) != 0) {
        obj_toggle_flat(obj_dude, NULL);
    }

    if ((obj_dude->flags & OBJECT_NO_BLOCK) != 0) {
        obj_dude->flags &= ~OBJECT_NO_BLOCK;
    }

    stat_recalc_derived(obj_dude);
    critter_adjust_hits(obj_dude, 10000);

    if (retval) {
        debug_printf("\n ** Error in proto_dude_init()! **\n");
    }

    return 0;
}

// 0x48E534
int proto_data_member(int pid, int member, ProtoDataMemberValue* value)
{
    Proto* proto;
    if (proto_ptr(pid, &proto) == -1) {
        return -1;
    }

    switch (PID_TYPE(pid)) {
    case OBJ_TYPE_ITEM:
        switch (member) {
        case ITEM_DATA_MEMBER_PID:
            value->integerValue = proto->pid;
            break;
        case ITEM_DATA_MEMBER_NAME:
            // NOTE: uninline
            value->stringValue = proto_name(proto->scenery.pid);
            return PROTO_DATA_MEMBER_TYPE_STRING;
        case ITEM_DATA_MEMBER_DESCRIPTION:
            // NOTE: Uninline.
            value->stringValue = proto_description(proto->pid);
            return PROTO_DATA_MEMBER_TYPE_STRING;
        case ITEM_DATA_MEMBER_FID:
            value->integerValue = proto->fid;
            break;
        case ITEM_DATA_MEMBER_LIGHT_DISTANCE:
            value->integerValue = proto->item.lightDistance;
            break;
        case ITEM_DATA_MEMBER_LIGHT_INTENSITY:
            value->integerValue = proto->item.lightIntensity;
            break;
        case ITEM_DATA_MEMBER_FLAGS:
            value->integerValue = proto->item.flags;
            break;
        case ITEM_DATA_MEMBER_EXTENDED_FLAGS:
            value->integerValue = proto->item.extendedFlags;
            break;
        case ITEM_DATA_MEMBER_SID:
            value->integerValue = proto->item.sid;
            break;
        case ITEM_DATA_MEMBER_TYPE:
            value->integerValue = proto->item.type;
            break;
        case ITEM_DATA_MEMBER_MATERIAL:
            value->integerValue = proto->item.material;
            break;
        case ITEM_DATA_MEMBER_SIZE:
            value->integerValue = proto->item.size;
            break;
        case ITEM_DATA_MEMBER_WEIGHT:
            value->integerValue = proto->item.weight;
            break;
        case ITEM_DATA_MEMBER_COST:
            value->integerValue = proto->item.cost;
            break;
        case ITEM_DATA_MEMBER_INVENTORY_FID:
            value->integerValue = proto->item.inventoryFid;
            break;
        default:
            debug_printf("\n\tError: Unimp'd data member in member in proto_data_member!");
            break;
        }
        break;
    case OBJ_TYPE_CRITTER:
        switch (member) {
        case CRITTER_DATA_MEMBER_PID:
            value->integerValue = proto->critter.pid;
            break;
        case CRITTER_DATA_MEMBER_NAME:
            // NOTE: Uninline.
            value->stringValue = proto_name(proto->critter.pid);
            return PROTO_DATA_MEMBER_TYPE_STRING;
        case CRITTER_DATA_MEMBER_DESCRIPTION:
            // NOTE: Uninline.
            value->stringValue = proto_description(proto->critter.pid);
            return PROTO_DATA_MEMBER_TYPE_STRING;
        case CRITTER_DATA_MEMBER_FID:
            value->integerValue = proto->critter.fid;
            break;
        case CRITTER_DATA_MEMBER_LIGHT_DISTANCE:
            value->integerValue = proto->critter.lightDistance;
            break;
        case CRITTER_DATA_MEMBER_LIGHT_INTENSITY:
            value->integerValue = proto->critter.lightIntensity;
            break;
        case CRITTER_DATA_MEMBER_FLAGS:
            value->integerValue = proto->critter.flags;
            break;
        case CRITTER_DATA_MEMBER_EXTENDED_FLAGS:
            value->integerValue = proto->critter.extendedFlags;
            break;
        case CRITTER_DATA_MEMBER_SID:
            value->integerValue = proto->critter.sid;
            break;
        case CRITTER_DATA_MEMBER_HEAD_FID:
            value->integerValue = proto->critter.headFid;
            break;
        default:
            debug_printf("\n\tError: Unimp'd data member in member in proto_data_member!");
            break;
        }
        break;
    case OBJ_TYPE_SCENERY:
        switch (member) {
        case SCENERY_DATA_MEMBER_PID:
            value->integerValue = proto->scenery.pid;
            break;
        case SCENERY_DATA_MEMBER_NAME:
            // NOTE: Uninline.
            value->stringValue = proto_name(proto->scenery.pid);
            return PROTO_DATA_MEMBER_TYPE_STRING;
        case SCENERY_DATA_MEMBER_DESCRIPTION:
            // NOTE: Uninline.
            value->stringValue = proto_description(proto->scenery.pid);
            return PROTO_DATA_MEMBER_TYPE_STRING;
        case SCENERY_DATA_MEMBER_FID:
            value->integerValue = proto->scenery.fid;
            break;
        case SCENERY_DATA_MEMBER_LIGHT_DISTANCE:
            value->integerValue = proto->scenery.lightDistance;
            break;
        case SCENERY_DATA_MEMBER_LIGHT_INTENSITY:
            value->integerValue = proto->scenery.lightIntensity;
            break;
        case SCENERY_DATA_MEMBER_FLAGS:
            value->integerValue = proto->scenery.flags;
            break;
        case SCENERY_DATA_MEMBER_EXTENDED_FLAGS:
            value->integerValue = proto->scenery.extendedFlags;
            break;
        case SCENERY_DATA_MEMBER_SID:
            value->integerValue = proto->scenery.sid;
            break;
        case SCENERY_DATA_MEMBER_TYPE:
            value->integerValue = proto->scenery.type;
            break;
        case SCENERY_DATA_MEMBER_MATERIAL:
            value->integerValue = proto->scenery.material;
            break;
        default:
            debug_printf("\n\tError: Unimp'd data member in member in proto_data_member!");
            break;
        }
        break;
    case OBJ_TYPE_WALL:
        switch (member) {
        case WALL_DATA_MEMBER_PID:
            value->integerValue = proto->wall.pid;
            break;
        case WALL_DATA_MEMBER_NAME:
            // NOTE: Uninline.
            value->stringValue = proto_name(proto->wall.pid);
            return PROTO_DATA_MEMBER_TYPE_STRING;
        case WALL_DATA_MEMBER_DESCRIPTION:
            // NOTE: Uninline.
            value->stringValue = proto_description(proto->wall.pid);
            return PROTO_DATA_MEMBER_TYPE_STRING;
        case WALL_DATA_MEMBER_FID:
            value->integerValue = proto->wall.fid;
            break;
        case WALL_DATA_MEMBER_LIGHT_DISTANCE:
            value->integerValue = proto->wall.lightDistance;
            break;
        case WALL_DATA_MEMBER_LIGHT_INTENSITY:
            value->integerValue = proto->wall.lightIntensity;
            break;
        case WALL_DATA_MEMBER_FLAGS:
            value->integerValue = proto->wall.flags;
            break;
        case WALL_DATA_MEMBER_EXTENDED_FLAGS:
            value->integerValue = proto->wall.extendedFlags;
            break;
        case WALL_DATA_MEMBER_SID:
            value->integerValue = proto->wall.sid;
            break;
        case WALL_DATA_MEMBER_MATERIAL:
            value->integerValue = proto->wall.material;
            break;
        default:
            debug_printf("\n\tError: Unimp'd data member in member in proto_data_member!");
            break;
        }
        break;
    case OBJ_TYPE_TILE:
        debug_printf("\n\tError: Unimp'd data member in member in proto_data_member!");
        break;
    case OBJ_TYPE_MISC:
        switch (member) {
        case MISC_DATA_MEMBER_PID:
            value->integerValue = proto->misc.pid;
            break;
        case MISC_DATA_MEMBER_NAME:
            // NOTE: Uninline.
            value->stringValue = proto_name(proto->misc.pid);
            return PROTO_DATA_MEMBER_TYPE_STRING;
        case MISC_DATA_MEMBER_DESCRIPTION:
            // NOTE: Uninline.
            value->stringValue = proto_description(proto->misc.pid);
            // FIXME: Errornously report type as int, should be string.
            return PROTO_DATA_MEMBER_TYPE_INT;
        case MISC_DATA_MEMBER_FID:
            value->integerValue = proto->misc.fid;
            return 1;
        case MISC_DATA_MEMBER_LIGHT_DISTANCE:
            value->integerValue = proto->misc.lightDistance;
            return 1;
        case MISC_DATA_MEMBER_LIGHT_INTENSITY:
            value->integerValue = proto->misc.lightIntensity;
            break;
        case MISC_DATA_MEMBER_FLAGS:
            value->integerValue = proto->misc.flags;
            break;
        case MISC_DATA_MEMBER_EXTENDED_FLAGS:
            value->integerValue = proto->misc.extendedFlags;
            break;
        default:
            debug_printf("\n\tError: Unimp'd data member in member in proto_data_member!");
            break;
        }
        break;
    }

    return PROTO_DATA_MEMBER_TYPE_INT;
}

// 0x48E84C
int proto_init()
{
    MessageListItem messageListItem;
    char path[COMPAT_MAX_PATH];
    int i;

    // TODO: Get rid of cast.
    proto_critter_init((Proto*)&pc_proto, 0x1000000);

    pc_proto.pid = 0x1000000;
    pc_proto.fid = art_id(OBJ_TYPE_CRITTER, 1, 0, 0, 0);

    obj_dude->pid = 0x1000000;
    obj_dude->sid = 1;

    // NOTE: Uninline.
    proto_remove_all();

    proto_header_load();

    protos_been_initialized = 1;

    proto_dude_init("premade\\player.gcd");

    for (i = 0; i < 6; i++) {
        if (!message_init(&(proto_msg_files[i]))) {
            debug_printf("\nError: Initing proto message files!");
            return -1;
        }
    }

    for (i = 0; i < 6; i++) {
        snprintf(path, sizeof(path), "%spro_%.4s%s", msg_path, art_dir(i), ".msg");

        if (!message_load(&(proto_msg_files[i]), path)) {
            debug_printf("\nError: Loading proto message files!");
            return -1;
        }
    }

    mp_critter_stats_list[0] = _aDrugStatSpecia;
    mp_critter_stats_list[1] = _aNone_1;
    critter_stats_list = &(mp_critter_stats_list[2]);
    for (i = 0; i < STAT_COUNT; i++) {
        critter_stats_list[i] = stat_name(i);
        if (critter_stats_list[i] == NULL) {
            debug_printf("\nError: Finding stat names!");
            return -1;
        }
    }

    mp_perk_code_strs[0] = _aNone_1;
    perk_code_strs = &(mp_perk_code_strs[1]);
    for (i = 0; i < PERK_COUNT; i++) {
        mp_perk_code_strs[i] = perk_name(i);
        if (mp_perk_code_strs[i] == NULL) {
            debug_printf("\nError: Finding perk names!");
            return -1;
        }
    }

    if (!message_init(&proto_main_msg_file)) {
        debug_printf("\nError: Initing main proto message file!");
        return -1;
    }

    snprintf(path, sizeof(path), "%sproto.msg", msg_path);

    if (!message_load(&proto_main_msg_file, path)) {
        debug_printf("\nError: Loading main proto message file!");
        return -1;
    }

    proto_none_str = getmsg(&proto_main_msg_file, &messageListItem, 10);

    // material type names
    for (i = 0; i < MATERIAL_TYPE_COUNT; i++) {
        item_pro_material[i] = getmsg(&proto_main_msg_file, &messageListItem, 100 + i);
    }

    // item type names
    for (i = 0; i < ITEM_TYPE_COUNT; i++) {
        item_pro_type[i] = getmsg(&proto_main_msg_file, &messageListItem, 150 + i);
    }

    // scenery type names
    for (i = 0; i < SCENERY_TYPE_COUNT; i++) {
        scenery_pro_type[i] = getmsg(&proto_main_msg_file, &messageListItem, 200 + i);
    }

    // damage code types
    for (i = 0; i < DAMAGE_TYPE_COUNT; i++) {
        damage_code_strs[i] = getmsg(&proto_main_msg_file, &messageListItem, 250 + i);
    }

    // caliber types
    for (i = 0; i < CALIBER_TYPE_COUNT; i++) {
        cal_type_strs[i] = getmsg(&proto_main_msg_file, &messageListItem, 300 + i);
    }

    // race types
    for (i = 0; i < RACE_TYPE_COUNT; i++) {
        race_type_strs[i] = getmsg(&proto_main_msg_file, &messageListItem, 350 + i);
    }

    // body types
    for (i = 0; i < BODY_TYPE_COUNT; i++) {
        body_type_strs[i] = getmsg(&proto_main_msg_file, &messageListItem, 400 + i);
    }

    return 0;
}

// 0x48EC20
void proto_reset()
{
    // TODO: Get rid of cast.
    proto_critter_init((Proto*)&pc_proto, 0x1000000);
    pc_proto.pid = 0x1000000;
    pc_proto.fid = art_id(OBJ_TYPE_CRITTER, 1, 0, 0, 0);

    obj_dude->pid = 0x1000000;
    obj_dude->sid = -1;
    obj_dude->flags &= ~OBJECT_FLAG_0xFC000;

    // NOTE: Uninline.
    proto_remove_all();

    proto_header_load();

    protos_been_initialized = 1;
    proto_dude_init("premade\\player.gcd");
}

// 0x48EC98
void proto_exit()
{
    int i;

    // NOTE: Uninline.
    proto_remove_all();

    protos_been_initialized = 0;

    for (i = 0; i < 6; i++) {
        message_exit(&(proto_msg_files[i]));
    }

    message_exit(&proto_main_msg_file);
}

// Count .pro lines in .lst files.
//
// 0x48ECD8
int proto_header_load()
{
    for (int index = 0; index < 6; index++) {
        ProtoList* ptr = &(protolists[index]);
        ptr->head = NULL;
        ptr->tail = NULL;
        ptr->length = 0;
        ptr->max_entries_num = 1;

        char path[COMPAT_MAX_PATH];
        proto_make_path(path, index << 24);
        strcat(path, "\\");
        strcat(path, art_dir(index));
        strcat(path, ".lst");

        DB_FILE* stream = db_fopen(path, "rt");
        if (stream == NULL) {
            return -1;
        }

        int ch = '\0';
        while (1) {
            ch = db_fgetc(stream);
            if (ch == -1) {
                break;
            }

            if (ch == '\n') {
                ptr->max_entries_num++;
            }
        }

        if (ch != '\n') {
            ptr->max_entries_num++;
        }

        db_fclose(stream);
    }

    return 0;
}

// 0x48EEE4
static int proto_read_item_data(ItemProtoData* item_data, int type, DB_FILE* stream)
{
    switch (type) {
    case ITEM_TYPE_ARMOR:
        if (db_freadInt32(stream, &(item_data->armor.armorClass)) == -1) return -1;
        if (db_freadIntCount(stream, item_data->armor.damageResistance, 7) == -1) return -1;
        if (db_freadIntCount(stream, item_data->armor.damageThreshold, 7) == -1) return -1;
        if (db_freadInt32(stream, &(item_data->armor.perk)) == -1) return -1;
        if (db_freadInt32(stream, &(item_data->armor.maleFid)) == -1) return -1;
        if (db_freadInt32(stream, &(item_data->armor.femaleFid)) == -1) return -1;

        return 0;
    case ITEM_TYPE_CONTAINER:
        if (db_freadInt32(stream, &(item_data->container.maxSize)) == -1) return -1;
        if (db_freadInt32(stream, &(item_data->container.openFlags)) == -1) return -1;

        return 0;
    case ITEM_TYPE_DRUG:
        if (db_freadInt32(stream, &(item_data->drug.stat[0])) == -1) return -1;
        if (db_freadInt32(stream, &(item_data->drug.stat[1])) == -1) return -1;
        if (db_freadInt32(stream, &(item_data->drug.stat[2])) == -1) return -1;
        if (db_freadIntCount(stream, item_data->drug.amount, 3) == -1) return -1;
        if (db_freadInt32(stream, &(item_data->drug.duration1)) == -1) return -1;
        if (db_freadIntCount(stream, item_data->drug.amount1, 3) == -1) return -1;
        if (db_freadInt32(stream, &(item_data->drug.duration2)) == -1) return -1;
        if (db_freadIntCount(stream, item_data->drug.amount2, 3) == -1) return -1;
        if (db_freadInt32(stream, &(item_data->drug.addictionChance)) == -1) return -1;
        if (db_freadInt32(stream, &(item_data->drug.withdrawalEffect)) == -1) return -1;
        if (db_freadInt32(stream, &(item_data->drug.withdrawalOnset)) == -1) return -1;

        return 0;
    case ITEM_TYPE_WEAPON:
        if (db_freadInt32(stream, &(item_data->weapon.animationCode)) == -1) return -1;
        if (db_freadInt32(stream, &(item_data->weapon.minDamage)) == -1) return -1;
        if (db_freadInt32(stream, &(item_data->weapon.maxDamage)) == -1) return -1;
        if (db_freadInt32(stream, &(item_data->weapon.damageType)) == -1) return -1;
        if (db_freadInt32(stream, &(item_data->weapon.maxRange1)) == -1) return -1;
        if (db_freadInt32(stream, &(item_data->weapon.maxRange2)) == -1) return -1;
        if (db_freadInt32(stream, &(item_data->weapon.projectilePid)) == -1) return -1;
        if (db_freadInt32(stream, &(item_data->weapon.minStrength)) == -1) return -1;
        if (db_freadInt32(stream, &(item_data->weapon.actionPointCost1)) == -1) return -1;
        if (db_freadInt32(stream, &(item_data->weapon.actionPointCost2)) == -1) return -1;
        if (db_freadInt32(stream, &(item_data->weapon.criticalFailureType)) == -1) return -1;
        if (db_freadInt32(stream, &(item_data->weapon.perk)) == -1) return -1;
        if (db_freadInt32(stream, &(item_data->weapon.rounds)) == -1) return -1;
        if (db_freadInt32(stream, &(item_data->weapon.caliber)) == -1) return -1;
        if (db_freadInt32(stream, &(item_data->weapon.ammoTypePid)) == -1) return -1;
        if (db_freadInt32(stream, &(item_data->weapon.ammoCapacity)) == -1) return -1;
        if (db_freadByte(stream, &(item_data->weapon.soundCode)) == -1) return -1;

        return 0;
    case ITEM_TYPE_AMMO:
        if (db_freadInt32(stream, &(item_data->ammo.caliber)) == -1) return -1;
        if (db_freadInt32(stream, &(item_data->ammo.quantity)) == -1) return -1;
        if (db_freadInt32(stream, &(item_data->ammo.armorClassModifier)) == -1) return -1;
        if (db_freadInt32(stream, &(item_data->ammo.damageResistanceModifier)) == -1) return -1;
        if (db_freadInt32(stream, &(item_data->ammo.damageMultiplier)) == -1) return -1;
        if (db_freadInt32(stream, &(item_data->ammo.damageDivisor)) == -1) return -1;

        return 0;
    case ITEM_TYPE_MISC:
        if (db_freadInt32(stream, &(item_data->misc.powerTypePid)) == -1) return -1;
        if (db_freadInt32(stream, &(item_data->misc.powerType)) == -1) return -1;
        if (db_freadInt32(stream, &(item_data->misc.charges)) == -1) return -1;

        return 0;
    case ITEM_TYPE_KEY:
        if (db_freadInt32(stream, &(item_data->key.keyCode)) == -1) return -1;

        return 0;
    }

    return 0;
}

// 0x48F2C8
static int proto_read_scenery_data(SceneryProtoData* scenery_data, int type, DB_FILE* stream)
{
    switch (type) {
    case SCENERY_TYPE_DOOR:
        if (db_freadInt32(stream, &(scenery_data->door.openFlags)) == -1) return -1;
        if (db_freadInt32(stream, &(scenery_data->door.keyCode)) == -1) return -1;

        return 0;
    case SCENERY_TYPE_STAIRS:
        if (db_freadInt32(stream, &(scenery_data->stairs.field_0)) == -1) return -1;
        if (db_freadInt32(stream, &(scenery_data->stairs.field_4)) == -1) return -1;

        return 0;
    case SCENERY_TYPE_ELEVATOR:
        if (db_freadInt32(stream, &(scenery_data->elevator.type)) == -1) return -1;
        if (db_freadInt32(stream, &(scenery_data->elevator.level)) == -1) return -1;

        return 0;
    case SCENERY_TYPE_LADDER_UP:
    case SCENERY_TYPE_LADDER_DOWN:
        if (db_freadInt32(stream, &(scenery_data->ladder.field_0)) == -1) return -1;

        return 0;
    case SCENERY_TYPE_GENERIC:
        if (db_freadInt32(stream, &(scenery_data->generic.field_0)) == -1) return -1;

        return 0;
    }

    return 0;
}

// read .pro file
// 0x48F398
static int proto_read_protoSubNode(Proto* proto, DB_FILE* stream)
{
    if (db_freadInt32(stream, &(proto->pid)) == -1) return -1;
    if (db_freadInt32(stream, &(proto->messageId)) == -1) return -1;
    if (db_freadInt32(stream, &(proto->fid)) == -1) return -1;

    switch (PID_TYPE(proto->pid)) {
    case OBJ_TYPE_ITEM:
        if (db_freadInt32(stream, &(proto->item.lightDistance)) == -1) return -1;
        if (db_freadInt32(stream, &(proto->item.lightIntensity)) == -1) return -1;
        if (db_freadInt32(stream, &(proto->item.flags)) == -1) return -1;
        if (db_freadInt32(stream, &(proto->item.extendedFlags)) == -1) return -1;
        if (db_freadInt32(stream, &(proto->item.sid)) == -1) return -1;
        if (db_freadInt32(stream, &(proto->item.type)) == -1) return -1;
        if (db_freadInt32(stream, &(proto->item.material)) == -1) return -1;
        if (db_freadInt32(stream, &(proto->item.size)) == -1) return -1;
        if (db_freadInt32(stream, &(proto->item.weight)) == -1) return -1;
        if (db_freadInt32(stream, &(proto->item.cost)) == -1) return -1;
        if (db_freadInt32(stream, &(proto->item.inventoryFid)) == -1) return -1;
        if (db_freadByte(stream, &(proto->item.field_80)) == -1) return -1;
        if (proto_read_item_data(&(proto->item.data), proto->item.type, stream) == -1) return -1;

        return 0;
    case OBJ_TYPE_CRITTER:
        if (db_freadInt32(stream, &(proto->critter.lightDistance)) == -1) return -1;
        if (db_freadInt32(stream, &(proto->critter.lightIntensity)) == -1) return -1;
        if (db_freadInt32(stream, &(proto->critter.flags)) == -1) return -1;
        if (db_freadInt32(stream, &(proto->critter.extendedFlags)) == -1) return -1;
        if (db_freadInt32(stream, &(proto->critter.sid)) == -1) return -1;
        if (db_freadInt32(stream, &(proto->critter.headFid)) == -1) return -1;
        if (db_freadInt32(stream, &(proto->critter.aiPacket)) == -1) return -1;
        if (db_freadInt32(stream, &(proto->critter.team)) == -1) return -1;

        if (critter_read_data(stream, &(proto->critter.data)) == -1) return -1;

        return 0;
    case OBJ_TYPE_SCENERY:
        if (db_freadInt32(stream, &(proto->scenery.lightDistance)) == -1) return -1;
        if (db_freadInt32(stream, &(proto->scenery.lightIntensity)) == -1) return -1;
        if (db_freadInt32(stream, &(proto->scenery.flags)) == -1) return -1;
        if (db_freadInt32(stream, &(proto->scenery.extendedFlags)) == -1) return -1;
        if (db_freadInt32(stream, &(proto->scenery.sid)) == -1) return -1;
        if (db_freadInt32(stream, &(proto->scenery.type)) == -1) return -1;
        if (db_freadInt32(stream, &(proto->scenery.material)) == -1) return -1;
        if (db_freadByte(stream, &(proto->scenery.field_34)) == -1) return -1;
        if (proto_read_scenery_data(&(proto->scenery.data), proto->scenery.type, stream) == -1) return -1;
        return 0;
    case OBJ_TYPE_WALL:
        if (db_freadInt32(stream, &(proto->wall.lightDistance)) == -1) return -1;
        if (db_freadInt32(stream, &(proto->wall.lightIntensity)) == -1) return -1;
        if (db_freadInt32(stream, &(proto->wall.flags)) == -1) return -1;
        if (db_freadInt32(stream, &(proto->wall.extendedFlags)) == -1) return -1;
        if (db_freadInt32(stream, &(proto->wall.sid)) == -1) return -1;
        if (db_freadInt32(stream, &(proto->wall.material)) == -1) return -1;

        return 0;
    case OBJ_TYPE_TILE:
        if (db_freadInt32(stream, &(proto->tile.flags)) == -1) return -1;
        if (db_freadInt32(stream, &(proto->tile.extendedFlags)) == -1) return -1;
        if (db_freadInt32(stream, &(proto->tile.sid)) == -1) return -1;
        if (db_freadInt32(stream, &(proto->tile.material)) == -1) return -1;

        return 0;
    case OBJ_TYPE_MISC:
        if (db_freadInt32(stream, &(proto->misc.lightDistance)) == -1) return -1;
        if (db_freadInt32(stream, &(proto->misc.lightIntensity)) == -1) return -1;
        if (db_freadInt32(stream, &(proto->misc.flags)) == -1) return -1;
        if (db_freadInt32(stream, &(proto->misc.extendedFlags)) == -1) return -1;

        return 0;
    }

    return -1;
}

// 0x48F788
static int proto_write_item_data(ItemProtoData* item_data, int type, DB_FILE* stream)
{
    switch (type) {
    case ITEM_TYPE_ARMOR:
        if (db_fwriteInt32(stream, item_data->armor.armorClass) == -1) return -1;
        if (db_fwriteInt32List(stream, item_data->armor.damageResistance, 7) == -1) return -1;
        if (db_fwriteInt32List(stream, item_data->armor.damageThreshold, 7) == -1) return -1;
        if (db_fwriteInt32(stream, item_data->armor.perk) == -1) return -1;
        if (db_fwriteInt32(stream, item_data->armor.maleFid) == -1) return -1;
        if (db_fwriteInt32(stream, item_data->armor.femaleFid) == -1) return -1;

        return 0;
    case ITEM_TYPE_CONTAINER:
        if (db_fwriteInt32(stream, item_data->container.maxSize) == -1) return -1;
        if (db_fwriteInt32(stream, item_data->container.openFlags) == -1) return -1;

        return 0;
    case ITEM_TYPE_DRUG:
        if (db_fwriteInt32(stream, item_data->drug.stat[0]) == -1) return -1;
        if (db_fwriteInt32(stream, item_data->drug.stat[1]) == -1) return -1;
        if (db_fwriteInt32(stream, item_data->drug.stat[2]) == -1) return -1;
        if (db_fwriteInt32List(stream, item_data->drug.amount, 3) == -1) return -1;
        if (db_fwriteInt32(stream, item_data->drug.duration1) == -1) return -1;
        if (db_fwriteInt32List(stream, item_data->drug.amount1, 3) == -1) return -1;
        if (db_fwriteInt32(stream, item_data->drug.duration2) == -1) return -1;
        if (db_fwriteInt32List(stream, item_data->drug.amount2, 3) == -1) return -1;
        if (db_fwriteInt32(stream, item_data->drug.addictionChance) == -1) return -1;
        if (db_fwriteInt32(stream, item_data->drug.withdrawalEffect) == -1) return -1;
        if (db_fwriteInt32(stream, item_data->drug.withdrawalOnset) == -1) return -1;

        return 0;
    case ITEM_TYPE_WEAPON:
        if (db_fwriteInt32(stream, item_data->weapon.animationCode) == -1) return -1;
        if (db_fwriteInt32(stream, item_data->weapon.maxDamage) == -1) return -1;
        if (db_fwriteInt32(stream, item_data->weapon.minDamage) == -1) return -1;
        if (db_fwriteInt32(stream, item_data->weapon.damageType) == -1) return -1;
        if (db_fwriteInt32(stream, item_data->weapon.maxRange1) == -1) return -1;
        if (db_fwriteInt32(stream, item_data->weapon.maxRange2) == -1) return -1;
        if (db_fwriteInt32(stream, item_data->weapon.projectilePid) == -1) return -1;
        if (db_fwriteInt32(stream, item_data->weapon.minStrength) == -1) return -1;
        if (db_fwriteInt32(stream, item_data->weapon.actionPointCost1) == -1) return -1;
        if (db_fwriteInt32(stream, item_data->weapon.actionPointCost2) == -1) return -1;
        if (db_fwriteInt32(stream, item_data->weapon.criticalFailureType) == -1) return -1;
        if (db_fwriteInt32(stream, item_data->weapon.perk) == -1) return -1;
        if (db_fwriteInt32(stream, item_data->weapon.rounds) == -1) return -1;
        if (db_fwriteInt32(stream, item_data->weapon.caliber) == -1) return -1;
        if (db_fwriteInt32(stream, item_data->weapon.ammoTypePid) == -1) return -1;
        if (db_fwriteInt32(stream, item_data->weapon.ammoCapacity) == -1) return -1;
        if (db_fwriteByte(stream, item_data->weapon.soundCode) == -1) return -1;

        return 0;
    case ITEM_TYPE_AMMO:
        if (db_fwriteInt32(stream, item_data->ammo.caliber) == -1) return -1;
        if (db_fwriteInt32(stream, item_data->ammo.quantity) == -1) return -1;
        if (db_fwriteInt32(stream, item_data->ammo.armorClassModifier) == -1) return -1;
        if (db_fwriteInt32(stream, item_data->ammo.damageResistanceModifier) == -1) return -1;
        if (db_fwriteInt32(stream, item_data->ammo.damageMultiplier) == -1) return -1;
        if (db_fwriteInt32(stream, item_data->ammo.damageDivisor) == -1) return -1;

        return 0;
    case ITEM_TYPE_MISC:
        if (db_fwriteInt32(stream, item_data->misc.powerTypePid) == -1) return -1;
        if (db_fwriteInt32(stream, item_data->misc.powerType) == -1) return -1;
        if (db_fwriteInt32(stream, item_data->misc.charges) == -1) return -1;

        return 0;
    case ITEM_TYPE_KEY:
        if (db_fwriteInt32(stream, item_data->key.keyCode) == -1) return -1;

        return 0;
    }

    return 0;
}

// 0x48FADC
static int proto_write_scenery_data(SceneryProtoData* scenery_data, int type, DB_FILE* stream)
{
    switch (type) {
    case SCENERY_TYPE_DOOR:
        if (db_fwriteInt32(stream, scenery_data->door.openFlags) == -1) return -1;
        if (db_fwriteInt32(stream, scenery_data->door.keyCode) == -1) return -1;

        return 0;
    case SCENERY_TYPE_STAIRS:
        if (db_fwriteInt32(stream, scenery_data->stairs.field_0) == -1) return -1;
        if (db_fwriteInt32(stream, scenery_data->stairs.field_4) == -1) return -1;

        return 0;
    case SCENERY_TYPE_ELEVATOR:
        if (db_fwriteInt32(stream, scenery_data->elevator.type) == -1) return -1;
        if (db_fwriteInt32(stream, scenery_data->elevator.level) == -1) return -1;

        return 0;
    case SCENERY_TYPE_LADDER_UP:
    case SCENERY_TYPE_LADDER_DOWN:
        if (db_fwriteInt32(stream, scenery_data->ladder.field_0) == -1) return -1;

        return 0;
    case SCENERY_TYPE_GENERIC:
        if (db_fwriteInt32(stream, scenery_data->generic.field_0) == -1) return -1;

        return 0;
    }

    return 0;
}

// 0x48FBAC
static int proto_write_protoSubNode(Proto* proto, DB_FILE* stream)
{
    if (db_fwriteInt32(stream, proto->pid) == -1) return -1;
    if (db_fwriteInt32(stream, proto->messageId) == -1) return -1;
    if (db_fwriteInt32(stream, proto->fid) == -1) return -1;

    switch (PID_TYPE(proto->pid)) {
    case OBJ_TYPE_ITEM:
        if (db_fwriteInt32(stream, proto->item.lightDistance) == -1) return -1;
        if (db_fwriteInt32(stream, proto->item.lightIntensity) == -1) return -1;
        if (db_fwriteInt32(stream, proto->item.flags) == -1) return -1;
        if (db_fwriteInt32(stream, proto->item.extendedFlags) == -1) return -1;
        if (db_fwriteInt32(stream, proto->item.sid) == -1) return -1;
        if (db_fwriteInt32(stream, proto->item.type) == -1) return -1;
        if (db_fwriteInt32(stream, proto->item.material) == -1) return -1;
        if (db_fwriteInt32(stream, proto->item.size) == -1) return -1;
        if (db_fwriteInt32(stream, proto->item.weight) == -1) return -1;
        if (db_fwriteInt32(stream, proto->item.cost) == -1) return -1;
        if (db_fwriteInt32(stream, proto->item.inventoryFid) == -1) return -1;
        if (db_fwriteByte(stream, proto->item.field_80) == -1) return -1;
        if (proto_write_item_data(&(proto->item.data), proto->item.type, stream) == -1) return -1;

        return 0;
    case OBJ_TYPE_CRITTER:
        if (db_fwriteInt32(stream, proto->critter.lightDistance) == -1) return -1;
        if (db_fwriteInt32(stream, proto->critter.lightIntensity) == -1) return -1;
        if (db_fwriteInt32(stream, proto->critter.flags) == -1) return -1;
        if (db_fwriteInt32(stream, proto->critter.extendedFlags) == -1) return -1;
        if (db_fwriteInt32(stream, proto->critter.sid) == -1) return -1;
        if (db_fwriteInt32(stream, proto->critter.headFid) == -1) return -1;
        if (db_fwriteInt32(stream, proto->critter.aiPacket) == -1) return -1;
        if (db_fwriteInt32(stream, proto->critter.team) == -1) return -1;
        if (critter_write_data(stream, &(proto->critter.data)) == -1) return -1;

        return 0;
    case OBJ_TYPE_SCENERY:
        if (db_fwriteInt32(stream, proto->scenery.lightDistance) == -1) return -1;
        if (db_fwriteInt32(stream, proto->scenery.lightIntensity) == -1) return -1;
        if (db_fwriteInt32(stream, proto->scenery.flags) == -1) return -1;
        if (db_fwriteInt32(stream, proto->scenery.extendedFlags) == -1) return -1;
        if (db_fwriteInt32(stream, proto->scenery.sid) == -1) return -1;
        if (db_fwriteInt32(stream, proto->scenery.type) == -1) return -1;
        if (db_fwriteInt32(stream, proto->scenery.material) == -1) return -1;
        if (db_fwriteByte(stream, proto->scenery.field_34) == -1) return -1;
        if (proto_write_scenery_data(&(proto->scenery.data), proto->scenery.type, stream) == -1) return -1;
    case OBJ_TYPE_WALL:
        if (db_fwriteInt32(stream, proto->wall.lightDistance) == -1) return -1;
        if (db_fwriteInt32(stream, proto->wall.lightIntensity) == -1) return -1;
        if (db_fwriteInt32(stream, proto->wall.flags) == -1) return -1;
        if (db_fwriteInt32(stream, proto->wall.extendedFlags) == -1) return -1;
        if (db_fwriteInt32(stream, proto->wall.sid) == -1) return -1;
        if (db_fwriteInt32(stream, proto->wall.material) == -1) return -1;

        return 0;
    case OBJ_TYPE_TILE:
        if (db_fwriteInt32(stream, proto->tile.flags) == -1) return -1;
        if (db_fwriteInt32(stream, proto->tile.extendedFlags) == -1) return -1;
        if (db_fwriteInt32(stream, proto->tile.sid) == -1) return -1;
        if (db_fwriteInt32(stream, proto->tile.material) == -1) return -1;

        return 0;
    case OBJ_TYPE_MISC:
        if (db_fwriteInt32(stream, proto->misc.lightDistance) == -1) return -1;
        if (db_fwriteInt32(stream, proto->misc.lightIntensity) == -1) return -1;
        if (db_fwriteInt32(stream, proto->misc.flags) == -1) return -1;
        if (db_fwriteInt32(stream, proto->misc.extendedFlags) == -1) return -1;

        return 0;
    }

    return -1;
}

// 0x48FF28
int proto_save_pid(int pid)
{
    Proto* proto;
    if (proto_ptr(pid, &proto) == -1) {
        return -1;
    }

    char path[COMPAT_MAX_PATH];
    proto_make_path(path, pid);
    strcat(path, "\\");

    proto_list_str(pid, path + strlen(path));

    DB_FILE* stream = db_fopen(path, "wb");
    if (stream == NULL) {
        return -1;
    }

    int rc = proto_write_protoSubNode(proto, stream);

    db_fclose(stream);

    return rc;
}

// 0x490034
int proto_load_pid(int pid, Proto** protoPtr)
{
    char path[COMPAT_MAX_PATH];
    proto_make_path(path, pid);
    strcat(path, "\\");

    if (proto_list_str(pid, path + strlen(path)) == -1) {
        return -1;
    }

    DB_FILE* stream = db_fopen(path, "rb");
    if (stream == NULL) {
        debug_printf("\nError: Can't fopen proto!\n");
        *protoPtr = NULL;
        return -1;
    }

    if (proto_find_free_subnode(PID_TYPE(pid), protoPtr) == -1) {
        db_fclose(stream);
        return -1;
    }

    if (proto_read_protoSubNode(*protoPtr, stream) != 0) {
        db_fclose(stream);
        return -1;
    }

    db_fclose(stream);
    return 0;
}

// 0x490190
int proto_find_free_subnode(int type, Proto** protoPtr)
{
    size_t size = (type >= 0 && type < 11) ? proto_sizes[type] : 0;

    Proto* proto = (Proto*)mem_malloc(size);
    *protoPtr = proto;
    if (proto == NULL) {
        return -1;
    }

    ProtoList* protoList = &(protolists[type]);
    ProtoListExtent* protoListExtent = protoList->tail;

    if (protoList->head != NULL) {
        if (protoListExtent->length == PROTO_LIST_EXTENT_SIZE) {
            ProtoListExtent* newExtent = protoListExtent->next = (ProtoListExtent*)mem_malloc(sizeof(ProtoListExtent));
            if (protoListExtent == NULL) {
                mem_free(proto);
                *protoPtr = NULL;
                return -1;
            }

            newExtent->length = 0;
            newExtent->next = NULL;

            protoList->tail = newExtent;
            protoList->length++;

            protoListExtent = newExtent;
        }
    } else {
        protoListExtent = (ProtoListExtent*)mem_malloc(sizeof(ProtoListExtent));
        if (protoListExtent == NULL) {
            mem_free(proto);
            *protoPtr = NULL;
            return -1;
        }

        protoListExtent->next = NULL;
        protoListExtent->length = 0;

        protoList->length = 1;
        protoList->tail = protoListExtent;
        protoList->head = protoListExtent;
    }

    protoListExtent->proto[protoListExtent->length] = proto;
    protoListExtent->length++;

    return 0;
}

// Clear all proto cache.
//
// 0x490438
void proto_remove_all()
{
    for (int type = 0; type < 6; type++) {
        ProtoList* protoList = &(protolists[type]);

        ProtoListExtent* curr = protoList->head;
        while (curr != NULL) {
            ProtoListExtent* next = curr->next;
            for (int index = 0; index < curr->length; index++) {
                mem_free(curr->proto[index]);
            }
            mem_free(curr);
            curr = next;
        }

        protoList->head = NULL;
        protoList->tail = NULL;
        protoList->length = 0;
    }
}

// 0x4904AC
int proto_ptr(int pid, Proto** protoPtr)
{
    *protoPtr = NULL;

    if (pid == -1) {
        return -1;
    }

    if (pid == 0x1000000) {
        *protoPtr = (Proto*)&pc_proto;
        return 0;
    }

    ProtoList* protoList = &(protolists[PID_TYPE(pid)]);
    ProtoListExtent* protoListExtent = protoList->head;
    while (protoListExtent != NULL) {
        for (int index = 0; index < protoListExtent->length; index++) {
            Proto* proto = (Proto*)protoListExtent->proto[index];
            if (pid == proto->pid) {
                *protoPtr = proto;
                return 0;
            }
        }
        protoListExtent = protoListExtent->next;
    }

    return proto_load_pid(pid, protoPtr);
}

// 0x490530
static int proto_new_id(int type)
{
    return protolists[type].max_entries_num++;
}

// 0x49054C
int proto_undo_new_id(int type)
{
    return protolists[type].max_entries_num--;
}

// 0x490568
int proto_max_id(int a1)
{
    return protolists[a1].max_entries_num;
}

// 0x490614
int ResetPlayer()
{
    Proto* proto;
    proto_ptr(obj_dude->pid, &proto);

    stat_pc_set_defaults();
    stat_set_defaults(&(proto->critter.data));
    critter_reset();
    editor_reset();
    skill_set_defaults(&(proto->critter.data));
    skill_reset();
    perk_reset();
    trait_reset();
    stat_recalc_derived(obj_dude);
    return 0;
}

} // namespace fallout
