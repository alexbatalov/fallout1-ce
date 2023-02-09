#ifndef FALLOUT_GAME_PROTINST_H_
#define FALLOUT_GAME_PROTINST_H_

#include "game/object_types.h"

namespace fallout {

int obj_sid(Object* object, int* sidPtr);
int obj_new_sid(Object* object, int* sidPtr);
int obj_new_sid_inst(Object* obj, int a2, int a3);
int obj_look_at(Object* a1, Object* a2);
int obj_look_at_func(Object* a1, Object* a2, void (*a3)(char* string));
int obj_examine(Object* a1, Object* a2);
int obj_examine_func(Object* critter, Object* target, void (*fn)(char* string));
int obj_pickup(Object* critter, Object* item);
int obj_remove_from_inven(Object* critter, Object* item);
int obj_drop(Object* a1, Object* a2);
int obj_destroy(Object* obj);
int obj_use_radio(Object* item_obj);
int protinst_use_item(Object* a1, Object* a2);
int obj_use_item(Object* a1, Object* a2);
int protinst_use_item_on(Object* a1, Object* a2, Object* item);
int obj_use_item_on(Object* a1, Object* a2, Object* a3);
int check_scenery_ap_cost(Object* obj, Object* a2);
int obj_use(Object* a1, Object* a2);
int obj_use_door(Object* a1, Object* a2, int a3);
int obj_use_container(Object* critter, Object* item);
int obj_use_skill_on(Object* a1, Object* a2, int skill);
bool obj_is_a_portal(Object* obj);
bool obj_is_lockable(Object* obj);
bool obj_is_locked(Object* obj);
int obj_lock(Object* obj);
int obj_unlock(Object* obj);
bool obj_is_openable(Object* obj);
int obj_is_open(Object* obj);
int obj_toggle_open(Object* obj);
int obj_open(Object* obj);
int obj_close(Object* obj);
bool obj_lock_is_jammed(Object* obj);
int obj_jam_lock(Object* obj);
int obj_unjam_lock(Object* obj);
int obj_unjam_all_locks();
int obj_attempt_placement(Object* obj, int tile, int elevation, int a4);

} // namespace fallout

#endif /* FALLOUT_GAME_PROTINST_H_ */
