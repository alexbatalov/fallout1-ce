#ifndef FALLOUT_GAME_ANIMATION_H_
#define FALLOUT_GAME_ANIMATION_H_

#include "game/object_types.h"

namespace fallout {

typedef enum AnimationRequestOptions {
    ANIMATION_REQUEST_UNRESERVED = 0x01,
    ANIMATION_REQUEST_RESERVED = 0x02,
    ANIMATION_REQUEST_NO_STAND = 0x04,
    ANIMATION_REQUEST_0x100 = 0x100,
    ANIMATION_REQUEST_INSIGNIFICANT = 0x200,
} AnimationRequestOptions;

// Basic animations: 0-19
// Knockdown and death: 20-35
// Change positions: 36-37
// Weapon: 38-47
// Single-frame death animations (the last frame of knockdown and death animations): 48-63
typedef enum AnimationType {
    ANIM_STAND = 0,
    ANIM_WALK = 1,
    ANIM_JUMP_BEGIN = 2,
    ANIM_JUMP_END = 3,
    ANIM_CLIMB_LADDER = 4,
    ANIM_FALLING = 5,
    ANIM_UP_STAIRS_RIGHT = 6,
    ANIM_UP_STAIRS_LEFT = 7,
    ANIM_DOWN_STAIRS_RIGHT = 8,
    ANIM_DOWN_STAIRS_LEFT = 9,
    ANIM_MAGIC_HANDS_GROUND = 10,
    ANIM_MAGIC_HANDS_MIDDLE = 11,
    ANIM_MAGIC_HANDS_UP = 12,
    ANIM_DODGE_ANIM = 13,
    ANIM_HIT_FROM_FRONT = 14,
    ANIM_HIT_FROM_BACK = 15,
    ANIM_THROW_PUNCH = 16,
    ANIM_KICK_LEG = 17,
    ANIM_THROW_ANIM = 18,
    ANIM_RUNNING = 19,
    ANIM_FALL_BACK = 20,
    ANIM_FALL_FRONT = 21,
    ANIM_BAD_LANDING = 22,
    ANIM_BIG_HOLE = 23,
    ANIM_CHARRED_BODY = 24,
    ANIM_CHUNKS_OF_FLESH = 25,
    ANIM_DANCING_AUTOFIRE = 26,
    ANIM_ELECTRIFY = 27,
    ANIM_SLICED_IN_HALF = 28,
    ANIM_BURNED_TO_NOTHING = 29,
    ANIM_ELECTRIFIED_TO_NOTHING = 30,
    ANIM_EXPLODED_TO_NOTHING = 31,
    ANIM_MELTED_TO_NOTHING = 32,
    ANIM_FIRE_DANCE = 33,
    ANIM_FALL_BACK_BLOOD = 34,
    ANIM_FALL_FRONT_BLOOD = 35,
    ANIM_PRONE_TO_STANDING = 36,
    ANIM_BACK_TO_STANDING = 37,
    ANIM_TAKE_OUT = 38,
    ANIM_PUT_AWAY = 39,
    ANIM_PARRY_ANIM = 40,
    ANIM_THRUST_ANIM = 41,
    ANIM_SWING_ANIM = 42,
    ANIM_POINT = 43,
    ANIM_UNPOINT = 44,
    ANIM_FIRE_SINGLE = 45,
    ANIM_FIRE_BURST = 46,
    ANIM_FIRE_CONTINUOUS = 47,
    ANIM_FALL_BACK_SF = 48,
    ANIM_FALL_FRONT_SF = 49,
    ANIM_BAD_LANDING_SF = 50,
    ANIM_BIG_HOLE_SF = 51,
    ANIM_CHARRED_BODY_SF = 52,
    ANIM_CHUNKS_OF_FLESH_SF = 53,
    ANIM_DANCING_AUTOFIRE_SF = 54,
    ANIM_ELECTRIFY_SF = 55,
    ANIM_SLICED_IN_HALF_SF = 56,
    ANIM_BURNED_TO_NOTHING_SF = 57,
    ANIM_ELECTRIFIED_TO_NOTHING_SF = 58,
    ANIM_EXPLODED_TO_NOTHING_SF = 59,
    ANIM_MELTED_TO_NOTHING_SF = 60,
    ANIM_FIRE_DANCE_SF = 61,
    ANIM_FALL_BACK_BLOOD_SF = 62,
    ANIM_FALL_FRONT_BLOOD_SF = 63,
    ANIM_CALLED_SHOT_PIC = 64,
    ANIM_COUNT = 65,
    FIRST_KNOCKDOWN_AND_DEATH_ANIM = ANIM_FALL_BACK,
    LAST_KNOCKDOWN_AND_DEATH_ANIM = ANIM_FALL_FRONT_BLOOD,
    FIRST_SF_DEATH_ANIM = ANIM_FALL_BACK_SF,
    LAST_SF_DEATH_ANIM = ANIM_FALL_FRONT_BLOOD_SF,
} AnimationType;

#define FID_ANIM_TYPE(value) ((value)&0xFF0000) >> 16

// Signature of animation callback accepting 2 parameters.
typedef int AnimationCallback(void*, void*);

// Signature of animation callback accepting 3 parameters.
typedef int AnimationCallback3(void*, void*, void*);

typedef Object* PathBuilderCallback(Object* object, int tile, int elevation);

typedef struct StraightPathNode {
    int tile;
    int elevation;
    int x;
    int y;
} StraightPathNode;

void anim_init();
void anim_reset();
void anim_exit();
int register_begin(int a1);
int register_priority(int a1);
int register_clear(Object* a1);
int register_end();
int check_registry(Object* obj);
int anim_busy(Object* a1);
int register_object_move_to_object(Object* owner, Object* destination, int actionPoints, int delay);
int register_object_run_to_object(Object* owner, Object* destination, int actionPoints, int delay);
int register_object_move_to_tile(Object* owner, int tile, int elevation, int actionPoints, int delay);
int register_object_run_to_tile(Object* owner, int tile, int elevation, int actionPoints, int delay);
int register_object_move_straight_to_tile(Object* object, int tile, int elevation, int anim, int delay);
int register_object_animate_and_move_straight(Object* owner, int tile, int elev, int anim, int delay);
int register_object_move_on_stairs(Object* owner, Object* stairs, int delay);
int register_object_check_falling(Object* owner, int delay);
int register_object_animate(Object* owner, int anim, int delay);
int register_object_animate_reverse(Object* owner, int anim, int delay);
int register_object_animate_and_hide(Object* owner, int anim, int delay);
int register_object_turn_towards(Object* owner, int tile);
int register_object_inc_rotation(Object* owner);
int register_object_dec_rotation(Object* owner);
int register_object_erase(Object* object);
int register_object_must_erase(Object* object);
int register_object_call(void* a1, void* a2, AnimationCallback* proc, int delay);
int register_object_call3(void* a1, void* a2, void* a3, AnimationCallback3* proc, int delay);
int register_object_must_call(void* a1, void* a2, AnimationCallback* proc, int delay);
int register_object_fset(Object* object, int flag, int delay);
int register_object_funset(Object* object, int flag, int delay);
int register_object_flatten(Object* object, int delay);
int register_object_change_fid(Object* owner, int fid, int delay);
int register_object_take_out(Object* owner, int weaponAnimationCode, int delay);
int register_object_light(Object* owner, int lightDistance, int delay);
int register_object_outline(Object* object, bool outline, int delay);
int register_object_play_sfx(Object* owner, const char* soundEffectName, int delay);
int register_object_animate_forever(Object* owner, int anim, int delay);
int register_ping(int a1, int a2);
int make_path(Object* object, int from, int to, unsigned char* a4, int a5);
int make_path_func(Object* object, int from, int to, unsigned char* rotations, int a5, PathBuilderCallback* callback);
int idist(int a1, int a2, int a3, int a4);
int EST(int tile1, int tile2);
int make_straight_path(Object* a1, int from, int to, StraightPathNode* pathNodes, Object** a5, int a6);
int make_straight_path_func(Object* a1, int from, int to, StraightPathNode* pathNodes, Object** a5, int a6, PathBuilderCallback* callback);
int anim_move_on_stairs(Object* obj, int tile, int elevation, int anim, int animationSequenceIndex);
int check_for_falling(Object* obj, int anim, int a3);
void object_animate();
int check_move(int* a1);
int dude_move(int a1);
int dude_run(int a1);
void dude_fidget();
void dude_stand(Object* obj, int rotation, int fid);
void dude_standup(Object* a1);
int anim_hide(Object* object, int animationSequenceIndex);
int anim_change_fid(Object* obj, int animationSequenceIndex, int fid);
void anim_stop();
unsigned int compute_tpf(Object* object, int fid);

} // namespace fallout

#endif /* FALLOUT_GAME_ANIMATION_H_ */
