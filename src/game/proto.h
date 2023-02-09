#ifndef FALLOUT_GAME_PROTO_H_
#define FALLOUT_GAME_PROTO_H_

#include "game/message.h"
#include "game/object_types.h"
#include "game/perk_defs.h"
#include "game/proto_types.h"
#include "game/skill_defs.h"
#include "game/stat_defs.h"
#include "plib/db/db.h"

namespace fallout {

typedef enum ItemDataMember {
    ITEM_DATA_MEMBER_PID = 0,
    ITEM_DATA_MEMBER_NAME = 1,
    ITEM_DATA_MEMBER_DESCRIPTION = 2,
    ITEM_DATA_MEMBER_FID = 3,
    ITEM_DATA_MEMBER_LIGHT_DISTANCE = 4,
    ITEM_DATA_MEMBER_LIGHT_INTENSITY = 5,
    ITEM_DATA_MEMBER_FLAGS = 6,
    ITEM_DATA_MEMBER_EXTENDED_FLAGS = 7,
    ITEM_DATA_MEMBER_SID = 8,
    ITEM_DATA_MEMBER_TYPE = 9,
    ITEM_DATA_MEMBER_MATERIAL = 11,
    ITEM_DATA_MEMBER_SIZE = 12,
    ITEM_DATA_MEMBER_WEIGHT = 13,
    ITEM_DATA_MEMBER_COST = 14,
    ITEM_DATA_MEMBER_INVENTORY_FID = 15,
} ItemDataMember;

typedef enum CritterDataMember {
    CRITTER_DATA_MEMBER_PID = 0,
    CRITTER_DATA_MEMBER_NAME = 1,
    CRITTER_DATA_MEMBER_DESCRIPTION = 2,
    CRITTER_DATA_MEMBER_FID = 3,
    CRITTER_DATA_MEMBER_LIGHT_DISTANCE = 4,
    CRITTER_DATA_MEMBER_LIGHT_INTENSITY = 5,
    CRITTER_DATA_MEMBER_FLAGS = 6,
    CRITTER_DATA_MEMBER_EXTENDED_FLAGS = 7,
    CRITTER_DATA_MEMBER_SID = 8,
    CRITTER_DATA_MEMBER_DATA = 9,
    CRITTER_DATA_MEMBER_HEAD_FID = 10,
} CritterDataMember;

typedef enum SceneryDataMember {
    SCENERY_DATA_MEMBER_PID = 0,
    SCENERY_DATA_MEMBER_NAME = 1,
    SCENERY_DATA_MEMBER_DESCRIPTION = 2,
    SCENERY_DATA_MEMBER_FID = 3,
    SCENERY_DATA_MEMBER_LIGHT_DISTANCE = 4,
    SCENERY_DATA_MEMBER_LIGHT_INTENSITY = 5,
    SCENERY_DATA_MEMBER_FLAGS = 6,
    SCENERY_DATA_MEMBER_EXTENDED_FLAGS = 7,
    SCENERY_DATA_MEMBER_SID = 8,
    SCENERY_DATA_MEMBER_TYPE = 9,
    SCENERY_DATA_MEMBER_DATA = 10,
    SCENERY_DATA_MEMBER_MATERIAL = 11,
} SceneryDataMember;

typedef enum WallDataMember {
    WALL_DATA_MEMBER_PID = 0,
    WALL_DATA_MEMBER_NAME = 1,
    WALL_DATA_MEMBER_DESCRIPTION = 2,
    WALL_DATA_MEMBER_FID = 3,
    WALL_DATA_MEMBER_LIGHT_DISTANCE = 4,
    WALL_DATA_MEMBER_LIGHT_INTENSITY = 5,
    WALL_DATA_MEMBER_FLAGS = 6,
    WALL_DATA_MEMBER_EXTENDED_FLAGS = 7,
    WALL_DATA_MEMBER_SID = 8,
    WALL_DATA_MEMBER_MATERIAL = 9,
} WallDataMember;

typedef enum MiscDataMember {
    MISC_DATA_MEMBER_PID = 0,
    MISC_DATA_MEMBER_NAME = 1,
    MISC_DATA_MEMBER_DESCRIPTION = 2,
    MISC_DATA_MEMBER_FID = 3,
    MISC_DATA_MEMBER_LIGHT_DISTANCE = 4,
    MISC_DATA_MEMBER_LIGHT_INTENSITY = 5,
    MISC_DATA_MEMBER_FLAGS = 6,
    MISC_DATA_MEMBER_EXTENDED_FLAGS = 7,
} MiscDataMember;

typedef enum ProtoDataMemberType {
    PROTO_DATA_MEMBER_TYPE_INT = 1,
    PROTO_DATA_MEMBER_TYPE_STRING = 2,
} ProtoDataMemberType;

typedef union ProtoDataMemberValue {
    int integerValue;
    char* stringValue;
} ProtoDataMemberValue;

typedef enum PrototypeMessage {
    PROTOTYPE_MESSAGE_NAME,
    PROTOTYPE_MESSAGE_DESCRIPTION,
} PrototypeMesage;

extern char cd_path_base[];
extern char proto_path_base[];

extern char* mp_perk_code_strs[1 + PERK_COUNT];
extern char* mp_critter_stats_list[2 + STAT_COUNT];
extern MessageList proto_msg_files[6];
extern char* race_type_strs[RACE_TYPE_COUNT];
extern char* scenery_pro_type[SCENERY_TYPE_COUNT];
extern MessageList proto_main_msg_file;
extern char* item_pro_material[MATERIAL_TYPE_COUNT];
extern char* proto_none_str;
extern char* body_type_strs[BODY_TYPE_COUNT];
extern char* item_pro_type[ITEM_TYPE_COUNT];
extern char* damage_code_strs[DAMAGE_TYPE_COUNT];
extern char* cal_type_strs[CALIBER_TYPE_COUNT];
extern char** perk_code_strs;
extern char** critter_stats_list;

void proto_make_path(char* path, int pid);
int proto_list_str(int pid, char* proto_path);
size_t proto_size(int type);
bool proto_action_can_use(int pid);
bool proto_action_can_use_on(int pid);
bool proto_action_can_look_at(int pid);
bool proto_action_can_talk_to(int pid);
int proto_action_can_pickup(int pid);
char* proto_name(int pid);
char* proto_description(int pid);
int proto_critter_init(Proto* a1, int a2);
void clear_pupdate_data(Object* obj);
int proto_read_protoUpdateData(Object* obj, DB_FILE* stream);
int proto_write_protoUpdateData(Object* obj, DB_FILE* stream);
int proto_update_gen(Object* obj);
int proto_update_init(Object* obj);
int proto_dude_update_gender();
int proto_dude_init(const char* path);
int proto_data_member(int pid, int member, ProtoDataMemberValue* value);
int proto_init();
void proto_reset();
void proto_exit();
int proto_header_load();
int proto_save_pid(int pid);
int proto_load_pid(int pid, Proto** out_proto);
int proto_find_free_subnode(int type, Proto** out_ptr);
void proto_remove_all();
int proto_ptr(int pid, Proto** out_proto);
int proto_undo_new_id(int type);
int proto_max_id(int a1);
int ResetPlayer();

} // namespace fallout

#endif /* FALLOUT_GAME_PROTO_H_ */
