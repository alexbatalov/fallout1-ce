#ifndef FALLOUT_GAME_OBJECT_H_
#define FALLOUT_GAME_OBJECT_H_

#include "game/inventry.h"
#include "game/map_defs.h"
#include "game/object_types.h"
#include "plib/db/db.h"
#include "plib/gnw/rect.h"

namespace fallout {

typedef struct ObjectWithFlags {
    int flags;
    Object* object;
} ObjectWithFlags;

extern unsigned char* wallBlendTable;
extern unsigned char* glassBlendTable;
extern unsigned char* steamBlendTable;
extern unsigned char* energyBlendTable;
extern unsigned char* redBlendTable;

extern unsigned char glassGrayTable[256];
extern unsigned char commonGrayTable[256];
extern Object* obj_egg;
extern Object* obj_dude;

int obj_init(unsigned char* buf, int width, int height, int pitch);
void obj_reset();
void obj_exit();
int obj_load(DB_FILE* stream);
int obj_save(DB_FILE* stream);
void obj_render_pre_roof(Rect* rect, int elevation);
void obj_render_post_roof(Rect* rect, int elevation);
int obj_new(Object** objectPtr, int fid, int pid);
int obj_pid_new(Object** objectPtr, int pid);
int obj_copy(Object** a1, Object* a2);
int obj_connect(Object* obj, int tile_index, int elev, Rect* rect);
int obj_disconnect(Object* obj, Rect* rect);
int obj_offset(Object* obj, int x, int y, Rect* rect);
int obj_move(Object* a1, int a2, int a3, int elevation, Rect* a5);
int obj_move_to_tile(Object* obj, int tile, int elevation, Rect* rect);
int obj_reset_roof();
int obj_change_fid(Object* obj, int fid, Rect* rect);
int obj_set_frame(Object* obj, int frame, Rect* rect);
int obj_inc_frame(Object* obj, Rect* rect);
int obj_dec_frame(Object* obj, Rect* rect);
int obj_set_rotation(Object* obj, int direction, Rect* rect);
int obj_inc_rotation(Object* obj, Rect* rect);
int obj_dec_rotation(Object* obj, Rect* rect);
void obj_rebuild_all_light();
int obj_set_light(Object* obj, int lightDistance, int lightIntensity, Rect* rect);
int obj_get_visible_light(Object* obj);
int obj_turn_on_light(Object* obj, Rect* rect);
int obj_turn_off_light(Object* obj, Rect* rect);
int obj_turn_on(Object* obj, Rect* rect);
int obj_turn_off(Object* obj, Rect* rect);
int obj_turn_on_outline(Object* obj, Rect* rect);
int obj_turn_off_outline(Object* obj, Rect* rect);
int obj_toggle_flat(Object* obj, Rect* rect);
int obj_erase_object(Object* a1, Rect* a2);
int obj_inven_free(Inventory* inventory);
bool obj_action_can_talk_to(Object* obj);
Object* obj_top_environment(Object* obj);
void obj_remove_all();
Object* obj_find_first();
Object* obj_find_next();
Object* obj_find_first_at(int elevation);
Object* obj_find_next_at();
void obj_bound(Object* obj, Rect* rect);
bool obj_occupied(int tile_num, int elev);
Object* obj_blocking_at(Object* a1, int tile_num, int elev);
int obj_scroll_blocking_at(int tile_num, int elev);
Object* obj_sight_blocking_at(Object* a1, int tile_num, int elev);
int obj_dist(Object* object1, Object* object2);
int obj_create_list(int tile, int elevation, int objectType, Object*** objectsPtr);
void obj_delete_list(Object** objects);
void translucent_trans_buf_to_buf(unsigned char* src, int srcWidth, int srcHeight, int srcPitch, unsigned char* dest, int destX, int destY, int destPitch, unsigned char* a9, unsigned char* a10);
void dark_trans_buf_to_buf(unsigned char* src, int srcWidth, int srcHeight, int srcPitch, unsigned char* dest, int destX, int destY, int destPitch, int light);
void dark_translucent_trans_buf_to_buf(unsigned char* src, int srcWidth, int srcHeight, int srcPitch, unsigned char* dest, int destX, int destY, int destPitch, int light, unsigned char* a10, unsigned char* a11);
void intensity_mask_buf_to_buf(unsigned char* src, int srcWidth, int srcHeight, int srcPitch, unsigned char* dest, int destPitch, unsigned char* mask, int maskPitch, int light);
int obj_outline_object(Object* obj, int a2, Rect* rect);
int obj_remove_outline(Object* obj, Rect* rect);
int obj_intersects_with(Object* object, int x, int y);
int obj_create_intersect_list(int x, int y, int elevation, int objectType, ObjectWithFlags** entriesPtr);
void obj_delete_intersect_list(ObjectWithFlags** a1);
void obj_set_seen(int tile);
void obj_process_seen();
char* object_name(Object* obj);
char* object_description(Object* obj);
void obj_preload_art_cache(int flags);
int obj_save_obj(DB_FILE* stream, Object* object);
int obj_load_obj(DB_FILE* stream, Object** objectPtr, int elevation, Object* owner);
int obj_save_dude(DB_FILE* stream);
int obj_load_dude(DB_FILE* stream);
void obj_fix_violence_settings(int* fid);

} // namespace fallout

#endif /* FALLOUT_GAME_OBJECT_H_ */
