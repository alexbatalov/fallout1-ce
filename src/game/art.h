#ifndef FALLOUT_GAME_ART_H_
#define FALLOUT_GAME_ART_H_

#include "game/cache.h"
#include "game/heap.h"
#include "game/object_types.h"
#include "game/proto_types.h"

namespace fallout {

typedef enum Head {
    HEAD_INVALID,
    HEAD_MARCUS,
    HEAD_MYRON,
    HEAD_ELDER,
    HEAD_LYNETTE,
    HEAD_HAROLD,
    HEAD_TANDI,
    HEAD_COM_OFFICER,
    HEAD_SULIK,
    HEAD_PRESIDENT,
    HEAD_HAKUNIN,
    HEAD_BOSS,
    HEAD_DYING_HAKUNIN,
    HEAD_COUNT,
} Head;

typedef enum HeadAnimation {
    HEAD_ANIMATION_VERY_GOOD_REACTION = 0,
    FIDGET_GOOD = 1,
    HEAD_ANIMATION_GOOD_TO_NEUTRAL = 2,
    HEAD_ANIMATION_NEUTRAL_TO_GOOD = 3,
    FIDGET_NEUTRAL = 4,
    HEAD_ANIMATION_NEUTRAL_TO_BAD = 5,
    HEAD_ANIMATION_BAD_TO_NEUTRAL = 6,
    FIDGET_BAD = 7,
    HEAD_ANIMATION_VERY_BAD_REACTION = 8,
    HEAD_ANIMATION_GOOD_PHONEMES = 9,
    HEAD_ANIMATION_NEUTRAL_PHONEMES = 10,
    HEAD_ANIMATION_BAD_PHONEMES = 11,
} HeadAnimation;

typedef enum Background {
    BACKGROUND_0,
    BACKGROUND_1,
    BACKGROUND_2,
    BACKGROUND_HUB,
    BACKGROUND_NECROPOLIS,
    BACKGROUND_BROTHERHOOD,
    BACKGROUND_MILITARY_BASE,
    BACKGROUND_JUNK_TOWN,
    BACKGROUND_CATHEDRAL,
    BACKGROUND_SHADY_SANDS,
    BACKGROUND_VAULT,
    BACKGROUND_MASTER,
    BACKGROUND_FOLLOWER,
    BACKGROUND_RAIDERS,
    BACKGROUND_CAVE,
    BACKGROUND_ENCLAVE,
    BACKGROUND_WASTELAND,
    BACKGROUND_BOSS,
    BACKGROUND_PRESIDENT,
    BACKGROUND_TENT,
    BACKGROUND_ADOBE,
    BACKGROUND_COUNT,
} Background;

typedef struct Art {
    int field_0;
    short framesPerSecond;
    short actionFrame;
    short frameCount;
    short xOffsets[6];
    short yOffsets[6];
    int dataOffsets[6];
    int padding[6];
    int dataSize;
} Art;

typedef struct ArtFrame {
    short width;
    short height;
    int size;
    short x;
    short y;
} ArtFrame;

typedef struct HeadDescription {
    int goodFidgetCount;
    int neutralFidgetCount;
    int badFidgetCount;
} HeadDescription;

typedef enum WeaponAnimation {
    WEAPON_ANIMATION_NONE,
    WEAPON_ANIMATION_KNIFE, // d
    WEAPON_ANIMATION_CLUB, // e
    WEAPON_ANIMATION_HAMMER, // f
    WEAPON_ANIMATION_SPEAR, // g
    WEAPON_ANIMATION_PISTOL, // h
    WEAPON_ANIMATION_SMG, // i
    WEAPON_ANIMATION_SHOTGUN, // j
    WEAPON_ANIMATION_LASER_RIFLE, // k
    WEAPON_ANIMATION_MINIGUN, // l
    WEAPON_ANIMATION_LAUNCHER, // m
    WEAPON_ANIMATION_COUNT,
} WeaponAnimation;

extern int art_vault_guy_num;
extern int art_vault_person_nums[GENDER_COUNT];
extern int art_mapper_blank_tile;

extern Cache art_cache;
extern HeadDescription* head_info;

int art_init();
void art_reset();
void art_exit();
char* art_dir(int objectType);
int art_get_disable(int objectType);
void art_toggle_disable(int objectType);
int art_total(int objectType);
int art_head_fidgets(int headFid);
void scale_art(int fid, unsigned char* dest, int width, int height, int pitch);
Art* art_ptr_lock(int fid, CacheEntry** cache_entry);
unsigned char* art_ptr_lock_data(int fid, int frame, int direction, CacheEntry** out_cache_entry);
unsigned char* art_lock(int fid, CacheEntry** out_cache_entry, int* widthPtr, int* heightPtr);
int art_ptr_unlock(CacheEntry* cache_entry);
int art_discard(int fid);
int art_flush();
int art_get_base_name(int objectType, int a2, char* a3);
int art_get_code(int a1, int a2, char* a3, char* a4);
char* art_get_name(int a1);
int art_read_lst(const char* path, char** artListPtr, int* artListSizePtr);
int art_frame_fps(Art* art);
int art_frame_action_frame(Art* art);
int art_frame_max_frame(Art* art);
int art_frame_width(Art* art, int frame, int direction);
int art_frame_length(Art* art, int frame, int direction);
int art_frame_width_length(Art* art, int frame, int direction, int* out_width, int* out_height);
int art_frame_hot(Art* art, int frame, int direction, int* a4, int* a5);
int art_frame_offset(Art* art, int rotation, int* out_offset_x, int* out_offset_y);
unsigned char* art_frame_data(Art* art, int frame, int direction);
ArtFrame* frame_ptr(Art* art, int frame, int direction);
bool art_exists(int fid);
bool art_fid_valid(int fid);
int art_alias_num(int a1);
int art_alias_fid(int fid);
int art_data_size(int a1, int* out_size);
int art_data_load(int a1, int* a2, unsigned char* data);
void art_data_free(void* ptr);
int art_id(int objectType, int frmId, int animType, int a4, int rotation);
Art* load_frame(const char* path);
int load_frame_into(const char* path, unsigned char* data);
int save_frame(const char* path, unsigned char* data);

} // namespace fallout

#endif /* FALLOUT_GAME_ART_H_ */
