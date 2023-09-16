#include "game/anim.h"

#include <stdio.h>
#include <string.h>

#include "game/art.h"
#include "game/combat.h"
#include "game/combat_defs.h"
#include "game/combatai.h"
#include "game/critter.h"
#include "game/display.h"
#include "game/game.h"
#include "game/gconfig.h"
#include "game/gmouse.h"
#include "game/gsound.h"
#include "game/intface.h"
#include "game/item.h"
#include "game/map.h"
#include "game/object.h"
#include "game/perk.h"
#include "game/protinst.h"
#include "game/proto.h"
#include "game/roll.h"
#include "game/scripts.h"
#include "game/stat.h"
#include "game/textobj.h"
#include "game/tile.h"
#include "game/trait.h"
#include "plib/color/color.h"
#include "plib/gnw/debug.h"
#include "plib/gnw/input.h"
#include "plib/gnw/rect.h"
#include "plib/gnw/svga.h"
#include "plib/gnw/vcr.h"

namespace fallout {

#define ANIMATION_SEQUENCE_LIST_CAPACITY 21
#define ANIMATION_DESCRIPTION_LIST_CAPACITY 40
#define ANIMATION_SAD_LIST_CAPACITY 16

#define ANIMATION_SEQUENCE_FORCED 0x01

typedef enum AnimationKind {
    ANIM_KIND_MOVE_TO_OBJECT = 0,
    ANIM_KIND_MOVE_TO_TILE = 1,
    ANIM_KIND_MOVE_TO_TILE_STRAIGHT = 2,
    ANIM_KIND_MOVE_TO_TILE_STRAIGHT_AND_WAIT_FOR_COMPLETE = 3,
    ANIM_KIND_ANIMATE = 4,
    ANIM_KIND_ANIMATE_REVERSED = 5,
    ANIM_KIND_ANIMATE_AND_HIDE = 6,
    ANIM_KIND_ROTATE_TO_TILE = 7,
    ANIM_KIND_ROTATE_CLOCKWISE = 8,
    ANIM_KIND_ROTATE_COUNTER_CLOCKWISE = 9,
    ANIM_KIND_HIDE = 10,
    ANIM_KIND_CALLBACK = 11,
    ANIM_KIND_CALLBACK3 = 12,
    ANIM_KIND_SET_FLAG = 14,
    ANIM_KIND_UNSET_FLAG = 15,
    ANIM_KIND_TOGGLE_FLAT = 16,
    ANIM_KIND_SET_FID = 17,
    ANIM_KIND_TAKE_OUT_WEAPON = 18,
    ANIM_KIND_SET_LIGHT_DISTANCE = 19,
    ANIM_KIND_MOVE_ON_STAIRS = 20,
    ANIM_KIND_CHECK_FALLING = 23,
    ANIM_KIND_TOGGLE_OUTLINE = 24,
    ANIM_KIND_ANIMATE_FOREVER = 25,
    ANIM_KIND_26 = 26,
    ANIM_KIND_27 = 27,
    ANIM_KIND_NOOP = 28,
} AnimationKind;

typedef enum AnimationSequenceFlags {
    // Specifies that the animation sequence has high priority, it cannot be
    // cleared.
    ANIM_SEQ_PRIORITIZED = 0x01,

    // Specifies that the animation sequence started combat animation mode and
    // therefore should balance it with appropriate finish call.
    ANIM_SEQ_COMBAT_ANIM_STARTED = 0x02,

    // Specifies that the animation sequence is reserved (TODO: explain what it
    // actually means).
    ANIM_SEQ_RESERVED = 0x04,

    // Specifies that the animation sequence is in the process of adding actions
    // to it (that is in the middle of begin/end calls).
    ANIM_SEQ_ACCUMULATING = 0x08,

    // TODO: Add description.
    ANIM_SEQ_0x10 = 0x10,

    // TODO: Add description.
    ANIM_SEQ_0x20 = 0x20,

    // Specifies that the animation sequence is negligible and will be end
    // immediately when a new animation sequence is requested for the same
    // object.
    ANIM_SEQ_INSIGNIFICANT = 0x40,

    // Specifies that the animation sequence should not return to ANIM_STAND
    // when it's completed.
    ANIM_SEQ_NO_STAND = 0x80,
} AnimationSequenceFlags;

typedef enum AnimationSadFlags {
    // Specifies that the animation should play from end to start.
    ANIM_SAD_REVERSE = 0x01,

    // Specifies that the animation should use straight movement mode (as
    // opposed to normal movement mode).
    ANIM_SAD_STRAIGHT = 0x02,

    // Specifies that no frame change should occur during animation.
    ANIM_SAD_NO_ANIM = 0x04,

    // Specifies that the animation should be played fully from start to finish.
    //
    // NOTE: This flag is only used together with straight movement mode to
    // implement knockdown. Without this flag when the knockdown distance is
    // short, say 1 or 2 tiles, knockdown animation might not be completed by
    // the time critter reached it's destination. With this flag set animation
    // will be played to it's final frame.
    ANIM_SAD_WAIT_FOR_COMPLETION = 0x10,

    // Unknown, set once, never read.
    ANIM_SAD_0x20 = 0x20,

    // Specifies that the owner of the animation should be hidden when animation
    // is completed.
    ANIM_SAD_HIDE_ON_END = 0x40,

    // Specifies that the animation should never end.
    ANIM_SAD_FOREVER = 0x80,
} AnimationSadFlags;

typedef struct AnimationDescription {
    int kind;
    union {
        Object* owner;

        // - ANIM_KIND_CALLBACK
        // - ANIM_KIND_CALLBACK3
        void* param2;
    };

    union {
        // - ANIM_KIND_MOVE_TO_OBJECT
        Object* destination;

        // - ANIM_KIND_CALLBACK
        void* param1;
    };
    union {
        // - ANIM_KIND_MOVE_TO_TILE
        // - ANIM_KIND_ANIMATE_AND_MOVE_TO_TILE_STRAIGHT
        // - ANIM_KIND_MOVE_TO_TILE_STRAIGHT
        struct {
            int tile;
            int elevation;
        };

        // ANIM_KIND_SET_FID
        int fid;

        // ANIM_KIND_TAKE_OUT_WEAPON
        int weaponAnimationCode;

        // ANIM_KIND_SET_LIGHT_DISTANCE
        int lightDistance;

        // ANIM_KIND_TOGGLE_OUTLINE
        bool outline;
    };
    int anim;
    int delay;

    // ANIM_KIND_CALLBACK
    AnimationCallback* callback;

    // ANIM_KIND_CALLBACK3
    AnimationCallback3* callback3;

    union {
        // - ANIM_KIND_SET_FLAG
        // - ANIM_KIND_UNSET_FLAG
        unsigned int objectFlag;

        // - ANIM_KIND_HIDE
        // - ANIM_KIND_CALLBACK
        unsigned int extendedFlags;
    };

    union {
        // - ANIM_KIND_MOVE_TO_TILE
        // - ANIM_KIND_MOVE_TO_OBJECT
        int actionPoints;

        // ANIM_KIND_26
        int animationSequenceIndex;

        // ANIM_KIND_CALLBACK3
        void* param3;
    };
    CacheEntry* artCacheKey;
} AnimationDescription;

typedef struct AnimationSequence {
    int field_0;
    // Index of current animation in [animations] array or -1 if animations in
    // this sequence is not playing.
    int animationIndex;
    // Number of scheduled animations in [animations] array.
    int length;
    unsigned int flags;
    AnimationDescription animations[ANIMATION_DESCRIPTION_LIST_CAPACITY];
} AnimationSequence;

typedef struct PathNode {
    int tile;
    int from;
    // actual type is likely char
    int rotation;
    int field_C;
    int field_10;
} PathNode;

// TODO: I don't know what `sad` means, but it's definitely better than
// `STRUCT_530014`. Find a better name.
typedef struct AnimationSad {
    unsigned int flags;
    Object* obj;
    int fid; // fid
    int anim;

    // Timestamp (in game ticks) when animation last occurred.
    unsigned int animationTimestamp;

    // Number of ticks per frame (taking art's fps and overall animation speed
    // settings into account).
    unsigned int ticksPerFrame;

    int animationSequenceIndex;
    int field_1C; // length of field_28
    int field_20; // current index in field_28
    int field_24;
    union {
        unsigned char rotations[3200];
        StraightPathNode field_28[200];
    };
} AnimationSad;

static int anim_free_slot(int a1);
static int anim_preload(Object* object, int fid, CacheEntry** cacheEntryPtr);
static void anim_cleanup();
static int anim_set_check(int a1);
static int anim_set_continue(int a1, int a2);
static int anim_set_end(int a1);
static bool anim_can_use_door(Object* critter, Object* door);
static int anim_move_to_object(Object* from, Object* to, int a3, int anim, int animationSequenceIndex);
static int make_stair_path(Object* object, int from, int fromElevation, int to, int toElevation, StraightPathNode* a6, Object** obstaclePtr);
static int anim_move_to_tile(Object* obj, int tile_num, int elev, int a4, int anim, int animationSequenceIndex);
static int anim_move(Object* obj, int tile, int elev, int a3, int anim, int a5, int animationSequenceIndex);
static int anim_move_straight_to_tile(Object* obj, int tile, int elevation, int anim, int animationSequenceIndex, int flags);
static void object_move(int index);
static void object_straight_move(int index);
static int anim_animate(Object* obj, int anim, int animationSequenceIndex, int flags);
static void object_anim_compact();
static int anim_turn_towards(Object* obj, int delta, int animationSequenceIndex);
static int check_gravity(int tile, int elevation);

// 0x4FEA98
static int curr_sad = 0;

// 0x4FEA9C
static int curr_anim_set = -1;

// 0x4FEAA0
static bool anim_in_init = false;

// 0x4FEAA4
static bool anim_in_anim_stop = false;

// 0x4FEAA8
static bool anim_in_bk = false;

// 0x540014
static AnimationSad sad[ANIMATION_SAD_LIST_CAPACITY];

// 0x5566D4
static PathNode dad[2000];

// 0x560314
static AnimationSequence anim_set[ANIMATION_SEQUENCE_LIST_CAPACITY];

// 0x56A1E4
static unsigned char seen[5000];

// 0x54CA94
static PathNode child[2000];

// 0x56B56C
static int curr_anim_counter;

// 0x4134B0
void anim_init()
{
    anim_in_init = true;
    anim_reset();
    anim_in_init = false;
}

// 0x4134D0
void anim_reset()
{
    int index;

    if (!anim_in_init) {
        // NOTE: Uninline.
        anim_stop();
    }

    curr_sad = 0;
    curr_anim_set = -1;

    for (index = 0; index < ANIMATION_SEQUENCE_LIST_CAPACITY; index++) {
        anim_set[index].field_0 = -1000;
        anim_set[index].flags = 0;
    }
}

// 0x413548
void anim_exit()
{
    // NOTE: Uninline.
    anim_stop();
}

// 0x413584
int register_begin(int requestOptions)
{
    if (curr_anim_set != -1) {
        return -1;
    }

    if (anim_in_anim_stop) {
        return -1;
    }

    int v1 = anim_free_slot(requestOptions);
    if (v1 == -1) {
        return -1;
    }

    AnimationSequence* animationSequence = &(anim_set[v1]);
    animationSequence->flags |= ANIM_SEQ_ACCUMULATING;

    if ((requestOptions & ANIMATION_REQUEST_RESERVED) != 0) {
        animationSequence->flags |= ANIM_SEQ_RESERVED;
    }

    if ((requestOptions & ANIMATION_REQUEST_INSIGNIFICANT) != 0) {
        animationSequence->flags |= ANIM_SEQ_INSIGNIFICANT;
    }

    if ((requestOptions & ANIMATION_REQUEST_NO_STAND) != 0) {
        animationSequence->flags |= ANIM_SEQ_NO_STAND;
    }

    curr_anim_set = v1;

    curr_anim_counter = 0;

    return 0;
}

// 0x413630
static int anim_free_slot(int requestOptions)
{
    int v1 = -1;
    int v2 = 0;
    for (int index = 0; index < ANIMATION_SEQUENCE_LIST_CAPACITY; index++) {
        AnimationSequence* animationSequence = &(anim_set[index]);
        if (animationSequence->field_0 != -1000 || (animationSequence->flags & ANIM_SEQ_ACCUMULATING) != 0 || (animationSequence->flags & ANIM_SEQ_0x20) != 0) {
            if (!(animationSequence->flags & ANIM_SEQ_RESERVED)) {
                v2++;
            }
        } else if (v1 == -1 && ((requestOptions & ANIMATION_REQUEST_0x100) == 0 || (animationSequence->flags & ANIM_SEQ_0x10) == 0)) {
            v1 = index;
        }
    }

    if (v1 == -1) {
        if ((requestOptions & ANIMATION_REQUEST_RESERVED) != 0) {
            debug_printf("Unable to begin reserved animation!\n");
        }

        return -1;
    } else if ((requestOptions & ANIMATION_REQUEST_RESERVED) != 0 || v2 < 13) {
        return v1;
    }

    return -1;
}

// 0x4136D0
int register_priority(int a1)
{
    if (curr_anim_set == -1) {
        return -1;
    }

    if (a1 == 0) {
        return -1;
    }

    anim_set[curr_anim_set].flags |= ANIM_SEQ_PRIORITIZED;

    return 0;
}

// 0x413708
int register_clear(Object* a1)
{
    for (int animationSequenceIndex = 0; animationSequenceIndex < ANIMATION_SEQUENCE_LIST_CAPACITY; animationSequenceIndex++) {
        AnimationSequence* animationSequence = &(anim_set[animationSequenceIndex]);
        if (animationSequence->field_0 == -1000) {
            continue;
        }

        int animationDescriptionIndex;
        for (animationDescriptionIndex = 0; animationDescriptionIndex < animationSequence->length; animationDescriptionIndex++) {
            AnimationDescription* animationDescription = &(animationSequence->animations[animationDescriptionIndex]);
            if (a1 != animationDescription->owner || animationDescription->kind == 11) {
                continue;
            }

            break;
        }

        if (animationDescriptionIndex == animationSequence->length) {
            continue;
        }

        if ((animationSequence->flags & ANIM_SEQ_PRIORITIZED) != 0) {
            return -2;
        }

        anim_set_end(animationSequenceIndex);

        return 0;
    }

    return -1;
}

// 0x413788
int register_end()
{
    if (curr_anim_set == -1) {
        return -1;
    }

    AnimationSequence* animationSequence = &(anim_set[curr_anim_set]);
    animationSequence->field_0 = 0;
    animationSequence->length = curr_anim_counter;
    animationSequence->animationIndex = -1;
    animationSequence->flags &= ~ANIM_SEQ_ACCUMULATING;
    animationSequence->animations[0].delay = 0;
    if (isInCombat()) {
        combat_anim_begin();
        animationSequence->flags |= ANIM_SEQ_COMBAT_ANIM_STARTED;
    }

    int v1 = curr_anim_set;
    curr_anim_set = -1;

    if (!(animationSequence->flags & ANIM_SEQ_0x10)) {
        anim_set_continue(v1, 1);
    }

    return 0;
}

// 0x41384C
static int anim_preload(Object* object, int fid, CacheEntry** cacheEntryPtr)
{
    *cacheEntryPtr = NULL;

    if (art_ptr_lock(fid, cacheEntryPtr) != NULL) {
        art_ptr_unlock(*cacheEntryPtr);
        *cacheEntryPtr = NULL;
        return 0;
    }

    return -1;
}

// 0x413878
static void anim_cleanup()
{
    if (curr_anim_set == -1) {
        return;
    }

    for (int index = 0; index < ANIMATION_SEQUENCE_LIST_CAPACITY; index++) {
        anim_set[index].flags &= ~(ANIM_SEQ_ACCUMULATING | ANIM_SEQ_0x10);
    }

    AnimationSequence* animationSequence = &(anim_set[curr_anim_set]);
    for (int index = 0; index < curr_anim_counter; index++) {
        AnimationDescription* animationDescription = &(animationSequence->animations[index]);
        if (animationDescription->artCacheKey != NULL) {
            art_ptr_unlock(animationDescription->artCacheKey);
        }

        if (animationDescription->kind == ANIM_KIND_CALLBACK && animationDescription->callback == (AnimationCallback*)gsnd_anim_sound) {
            gsound_delete_sfx((Sound*)animationDescription->param1);
        }
    }

    curr_anim_set = -1;
}

// 0x41390C
int check_registry(Object* obj)
{
    if (curr_anim_set == -1) {
        return -1;
    }

    if (curr_anim_counter >= ANIMATION_DESCRIPTION_LIST_CAPACITY) {
        return -1;
    }

    if (obj == NULL) {
        return 0;
    }

    for (int animationSequenceIndex = 0; animationSequenceIndex < ANIMATION_SEQUENCE_LIST_CAPACITY; animationSequenceIndex++) {
        AnimationSequence* animationSequence = &(anim_set[animationSequenceIndex]);

        if (animationSequenceIndex != curr_anim_set && animationSequence->field_0 != -1000) {
            for (int animationDescriptionIndex = 0; animationDescriptionIndex < animationSequence->length; animationDescriptionIndex++) {
                AnimationDescription* animationDescription = &(animationSequence->animations[animationDescriptionIndex]);
                if (obj == animationDescription->owner && animationDescription->kind != 11) {
                    if ((animationSequence->flags & ANIM_SEQ_INSIGNIFICANT) == 0) {
                        return -1;
                    }

                    anim_set_end(animationSequenceIndex);
                }
            }
        }
    }

    return 0;
}

// Returns -1 if object is playing some animation.
//
// 0x4139A8
int anim_busy(Object* a1)
{
    if (curr_anim_counter >= ANIMATION_DESCRIPTION_LIST_CAPACITY || a1 == NULL) {
        return 0;
    }

    for (int animationSequenceIndex = 0; animationSequenceIndex < ANIMATION_SEQUENCE_LIST_CAPACITY; animationSequenceIndex++) {
        AnimationSequence* animationSequence = &(anim_set[animationSequenceIndex]);
        if (animationSequenceIndex != curr_anim_set && animationSequence->field_0 != -1000) {
            for (int animationDescriptionIndex = 0; animationDescriptionIndex < animationSequence->length; animationDescriptionIndex++) {
                AnimationDescription* animationDescription = &(animationSequence->animations[animationDescriptionIndex]);
                if (a1 != animationDescription->owner) {
                    continue;
                }

                if (animationDescription->kind == ANIM_KIND_CALLBACK) {
                    continue;
                }

                if (animationSequence->length == 1 && animationDescription->anim == ANIM_STAND) {
                    continue;
                }

                return -1;
            }
        }
    }

    return 0;
}

// 0x413A3C
int register_object_move_to_object(Object* owner, Object* destination, int actionPoints, int delay)
{
    if (check_registry(owner) == -1 || actionPoints == 0) {
        anim_cleanup();
        return -1;
    }

    if (owner->tile == destination->tile && owner->elevation == destination->elevation) {
        return 0;
    }

    AnimationDescription* animationDescription = &(anim_set[curr_anim_set].animations[curr_anim_counter]);
    animationDescription->kind = ANIM_KIND_MOVE_TO_OBJECT;
    animationDescription->anim = ANIM_WALK;
    animationDescription->owner = owner;
    animationDescription->destination = destination;
    animationDescription->actionPoints = actionPoints;
    animationDescription->delay = delay;

    int fid = art_id(FID_TYPE(owner->fid), owner->fid & 0xFFF, animationDescription->anim, (owner->fid & 0xF000) >> 12, owner->rotation + 1);

    // NOTE: Uninline.
    if (anim_preload(owner, fid, &(animationDescription->artCacheKey)) == -1) {
        anim_cleanup();
        return -1;
    }

    curr_anim_counter++;

    return register_object_turn_towards(owner, destination->tile);
}

// 0x413B48
int register_object_run_to_object(Object* owner, Object* destination, int actionPoints, int delay)
{
    if (check_registry(owner) == -1 || actionPoints == 0) {
        anim_cleanup();
        return -1;
    }

    if (owner->tile == destination->tile && owner->elevation == destination->elevation) {
        return 0;
    }

    AnimationDescription* animationDescription = &(anim_set[curr_anim_set].animations[curr_anim_counter]);
    animationDescription->kind = ANIM_KIND_MOVE_TO_OBJECT;
    animationDescription->owner = owner;
    animationDescription->destination = destination;

    if ((FID_TYPE(owner->fid) == OBJ_TYPE_CRITTER && (owner->data.critter.combat.results & DAM_CRIP_LEG_ANY) != 0)
        || (owner == obj_dude && is_pc_flag(PC_FLAG_SNEAKING) && !perk_level(PERK_SILENT_RUNNING))
        || (!art_exists(art_id(FID_TYPE(owner->fid), owner->fid & 0xFFF, ANIM_RUNNING, 0, owner->rotation + 1)))) {
        animationDescription->anim = ANIM_WALK;
    } else {
        animationDescription->anim = ANIM_RUNNING;
    }

    animationDescription->actionPoints = actionPoints;
    animationDescription->delay = delay;

    int fid = art_id(FID_TYPE(owner->fid), owner->fid & 0xFFF, animationDescription->anim, (owner->fid & 0xF000) >> 12, owner->rotation + 1);

    // NOTE: Uninline.
    if (anim_preload(owner, fid, &(animationDescription->artCacheKey)) == -1) {
        anim_cleanup();
        return -1;
    }

    curr_anim_counter++;
    return register_object_turn_towards(owner, destination->tile);
}

// 0x413CDC
int register_object_move_to_tile(Object* owner, int tile, int elevation, int actionPoints, int delay)
{
    if (check_registry(owner) == -1 || actionPoints == 0) {
        anim_cleanup();
        return -1;
    }

    if (tile == owner->tile && elevation == owner->elevation) {
        return 0;
    }

    AnimationDescription* animationDescription = &(anim_set[curr_anim_set].animations[curr_anim_counter]);
    animationDescription->kind = ANIM_KIND_MOVE_TO_TILE;
    animationDescription->anim = ANIM_WALK;
    animationDescription->owner = owner;
    animationDescription->tile = tile;
    animationDescription->elevation = elevation;
    animationDescription->actionPoints = actionPoints;
    animationDescription->delay = delay;

    int fid = art_id(FID_TYPE(owner->fid), owner->fid & 0xFFF, animationDescription->anim, (owner->fid & 0xF000) >> 12, owner->rotation + 1);

    // NOTE: Uninline.
    if (anim_preload(owner, fid, &(animationDescription->artCacheKey)) == -1) {
        anim_cleanup();
        return -1;
    }

    curr_anim_counter++;

    return 0;
}

// 0x413DE8
int register_object_run_to_tile(Object* owner, int tile, int elevation, int actionPoints, int delay)
{
    if (check_registry(owner) == -1 || actionPoints == 0) {
        anim_cleanup();
        return -1;
    }

    if (tile == owner->tile && elevation == owner->elevation) {
        return 0;
    }

    AnimationDescription* animationDescription = &(anim_set[curr_anim_set].animations[curr_anim_counter]);
    animationDescription->kind = ANIM_KIND_MOVE_TO_TILE;
    animationDescription->owner = owner;
    animationDescription->tile = tile;
    animationDescription->elevation = elevation;

    if ((FID_TYPE(owner->fid) == OBJ_TYPE_CRITTER && (owner->data.critter.combat.results & DAM_CRIP_LEG_ANY) != 0)
        || (owner == obj_dude && is_pc_flag(PC_FLAG_SNEAKING) && !perk_level(PERK_SILENT_RUNNING))
        || (!art_exists(art_id(FID_TYPE(owner->fid), owner->fid & 0xFFF, ANIM_RUNNING, 0, owner->rotation + 1)))) {
        animationDescription->anim = ANIM_WALK;
    } else {
        animationDescription->anim = ANIM_RUNNING;
    }

    animationDescription->actionPoints = actionPoints;
    animationDescription->delay = delay;

    int fid = art_id(FID_TYPE(owner->fid), owner->fid & 0xFFF, animationDescription->anim, (owner->fid & 0xF000) >> 12, owner->rotation + 1);

    // NOTE: Uninline.
    if (anim_preload(owner, fid, &(animationDescription->artCacheKey)) == -1) {
        anim_cleanup();
        return -1;
    }

    curr_anim_counter++;

    return 0;
}

// 0x413F60
int register_object_move_straight_to_tile(Object* object, int tile, int elevation, int anim, int delay)
{
    if (check_registry(object) == -1) {
        anim_cleanup();
        return -1;
    }

    if (tile == object->tile && elevation == object->elevation) {
        return 0;
    }

    AnimationSequence* animationSequence = &(anim_set[curr_anim_set]);

    AnimationDescription* animationDescription = &(animationSequence->animations[curr_anim_counter]);
    animationDescription->kind = ANIM_KIND_MOVE_TO_TILE_STRAIGHT;
    animationDescription->owner = object;
    animationDescription->tile = tile;
    animationDescription->elevation = elevation;
    animationDescription->anim = anim;
    animationDescription->delay = delay;

    int fid = art_id(FID_TYPE(object->fid), object->fid & 0xFFF, animationDescription->anim, (object->fid & 0xF000) >> 12, object->rotation + 1);

    // NOTE: Uninline.
    if (anim_preload(object, fid, &(animationDescription->artCacheKey)) == -1) {
        anim_cleanup();
        return -1;
    }

    curr_anim_counter++;

    return 0;
}

// 0x414060
int register_object_animate_and_move_straight(Object* owner, int tile, int elevation, int anim, int delay)
{
    if (check_registry(owner) == -1) {
        anim_cleanup();
        return -1;
    }

    if (tile == owner->tile && elevation == owner->elevation) {
        return 0;
    }

    AnimationSequence* animationSequence = &(anim_set[curr_anim_set]);
    AnimationDescription* animationDescription = &(animationSequence->animations[curr_anim_counter]);
    animationDescription->kind = ANIM_KIND_MOVE_TO_TILE_STRAIGHT_AND_WAIT_FOR_COMPLETE;
    animationDescription->owner = owner;
    animationDescription->tile = tile;
    animationDescription->elevation = elevation;
    animationDescription->anim = anim;
    animationDescription->delay = delay;

    int fid = art_id(FID_TYPE(owner->fid), owner->fid & 0xFFF, animationDescription->anim, (owner->fid & 0xF000) >> 12, owner->rotation + 1);

    // NOTE: Uninline.
    if (anim_preload(owner, fid, &(animationDescription->artCacheKey)) == -1) {
        anim_cleanup();
        return -1;
    }

    curr_anim_counter++;

    return 0;
}

// 0x414160
int register_object_move_on_stairs(Object* owner, Object* stairs, int delay)
{
    int anim;
    int destTile;
    int destElevation;
    AnimationSequence* animationSequence;
    AnimationDescription* animationDescription;
    int fid;

    if (check_registry(owner) == -1) {
        anim_cleanup();
        return -1;
    }

    if (owner->elevation == stairs->elevation) {
        anim = ANIM_UP_STAIRS_LEFT;
        destTile = stairs->tile + 4;
        destElevation = stairs->elevation + 1;
    } else {
        anim = ANIM_DOWN_STAIRS_RIGHT;
        destTile = stairs->tile + 200;
        destElevation = stairs->elevation;
    }

    if (destTile == owner->tile && destElevation == owner->elevation) {
        return 0;
    }

    animationSequence = &(anim_set[curr_anim_set]);
    animationDescription = &(animationSequence->animations[curr_anim_counter]);
    animationDescription->kind = ANIM_KIND_MOVE_ON_STAIRS;
    animationDescription->owner = owner;
    animationDescription->tile = destTile;
    animationDescription->elevation = destElevation;
    animationDescription->anim = anim;
    animationDescription->delay = delay;

    fid = art_id(FID_TYPE(owner->fid), owner->fid & 0xFFF, animationDescription->anim, (owner->fid & 0xF000) >> 12, owner->rotation + 1);

    // NOTE: Uninline
    if (anim_preload(owner, fid, &(animationDescription->artCacheKey)) == -1) {
        anim_cleanup();
        return -1;
    }

    curr_anim_counter++;

    return 0;
}

// 0x414298
int register_object_check_falling(Object* owner, int delay)
{
    AnimationSequence* animationSequence;
    AnimationDescription* animationDescription;
    int fid;

    if (check_registry(owner) == -1) {
        anim_cleanup();
        return -1;
    }

    animationSequence = &(anim_set[curr_anim_set]);
    animationDescription = &(animationSequence->animations[curr_anim_counter]);
    animationDescription->kind = ANIM_KIND_CHECK_FALLING;
    animationDescription->anim = ANIM_FALLING;
    animationDescription->owner = owner;
    animationDescription->delay = delay;

    fid = art_id(FID_TYPE(owner->fid), owner->fid & 0xFFF, animationDescription->anim, (owner->fid & 0xF000) >> 12, owner->rotation + 1);

    // NOTE: Uninline
    if (anim_preload(owner, fid, &(animationDescription->artCacheKey)) == -1) {
        anim_cleanup();
        return -1;
    }

    curr_anim_counter++;

    return 0;
}

// 0x414380
int register_object_animate(Object* owner, int anim, int delay)
{
    if (check_registry(owner) == -1) {
        anim_cleanup();
        return -1;
    }

    AnimationSequence* animationSequence = &(anim_set[curr_anim_set]);
    AnimationDescription* animationDescription = &(animationSequence->animations[curr_anim_counter]);
    animationDescription->kind = ANIM_KIND_ANIMATE;
    animationDescription->owner = owner;
    animationDescription->anim = anim;
    animationDescription->delay = delay;

    int fid = art_id(FID_TYPE(owner->fid), owner->fid & 0xFFF, animationDescription->anim, (owner->fid & 0xF000) >> 12, owner->rotation + 1);

    // NOTE: Uninline.
    if (anim_preload(owner, fid, &(animationDescription->artCacheKey)) == -1) {
        anim_cleanup();
        return -1;
    }

    curr_anim_counter++;

    return 0;
}

// 0x414460
int register_object_animate_reverse(Object* owner, int anim, int delay)
{
    if (check_registry(owner) == -1) {
        anim_cleanup();
        return -1;
    }

    AnimationSequence* animationSequence = &(anim_set[curr_anim_set]);
    AnimationDescription* animationDescription = &(animationSequence->animations[curr_anim_counter]);
    animationDescription->kind = ANIM_KIND_ANIMATE_REVERSED;
    animationDescription->owner = owner;
    animationDescription->anim = anim;
    animationDescription->delay = delay;
    animationDescription->artCacheKey = NULL;

    int fid = art_id(FID_TYPE(owner->fid), owner->fid & 0xFFF, animationDescription->anim, (owner->fid & 0xF000) >> 12, owner->rotation + 1);

    // NOTE: Uninline.
    if (anim_preload(owner, fid, &(animationDescription->artCacheKey)) == -1) {
        anim_cleanup();
        return -1;
    }

    curr_anim_counter++;

    return 0;
}

// 0x414544
int register_object_animate_and_hide(Object* owner, int anim, int delay)
{
    if (check_registry(owner) == -1) {
        anim_cleanup();
        return -1;
    }

    AnimationSequence* animationSequence = &(anim_set[curr_anim_set]);
    AnimationDescription* animationDescription = &(animationSequence->animations[curr_anim_counter]);
    animationDescription->kind = ANIM_KIND_ANIMATE_AND_HIDE;
    animationDescription->owner = owner;
    animationDescription->anim = anim;
    animationDescription->delay = delay;
    animationDescription->artCacheKey = NULL;

    int fid = art_id(FID_TYPE(owner->fid), owner->fid & 0xFFF, anim, (owner->fid & 0xF000) >> 12, owner->rotation + 1);

    // NOTE: Uninline.
    if (anim_preload(owner, fid, &(animationDescription->artCacheKey)) == -1) {
        anim_cleanup();
        return -1;
    }

    curr_anim_counter++;

    return 0;
}

// 0x414628
int register_object_turn_towards(Object* owner, int tile)
{
    if (check_registry(owner) == -1) {
        anim_cleanup();
        return -1;
    }

    AnimationSequence* animationSequence = &(anim_set[curr_anim_set]);
    AnimationDescription* animationDescription = &(animationSequence->animations[curr_anim_counter]);
    animationDescription->kind = ANIM_KIND_ROTATE_TO_TILE;
    animationDescription->delay = -1;
    animationDescription->artCacheKey = NULL;
    animationDescription->owner = owner;
    animationDescription->tile = tile;

    curr_anim_counter++;

    return 0;
}

// 0x4146AC
int register_object_inc_rotation(Object* owner)
{
    if (check_registry(owner) == -1) {
        anim_cleanup();
        return -1;
    }

    AnimationSequence* animationSequence = &(anim_set[curr_anim_set]);
    AnimationDescription* animationDescription = &(animationSequence->animations[curr_anim_counter]);
    animationDescription->kind = ANIM_KIND_ROTATE_CLOCKWISE;
    animationDescription->delay = -1;
    animationDescription->artCacheKey = NULL;
    animationDescription->owner = owner;

    curr_anim_counter++;

    return 0;
}

// 0x41472C
int register_object_dec_rotation(Object* owner)
{
    if (check_registry(owner) == -1) {
        anim_cleanup();
        return -1;
    }

    AnimationSequence* animationSequence = &(anim_set[curr_anim_set]);
    AnimationDescription* animationDescription = &(animationSequence->animations[curr_anim_counter]);
    animationDescription->kind = ANIM_KIND_ROTATE_COUNTER_CLOCKWISE;
    animationDescription->delay = -1;
    animationDescription->artCacheKey = NULL;
    animationDescription->owner = owner;

    curr_anim_counter++;

    return 0;
}

// 0x4147AC
int register_object_erase(Object* object)
{
    if (check_registry(object) == -1) {
        anim_cleanup();
        return -1;
    }

    AnimationSequence* animationSequence = &(anim_set[curr_anim_set]);
    AnimationDescription* animationDescription = &(animationSequence->animations[curr_anim_counter]);
    animationDescription->kind = ANIM_KIND_HIDE;
    animationDescription->delay = -1;
    animationDescription->artCacheKey = NULL;
    animationDescription->extendedFlags = 0;
    animationDescription->owner = object;
    curr_anim_counter++;

    return 0;
}

// 0x414834
int register_object_must_erase(Object* object)
{
    if (check_registry(object) == -1) {
        anim_cleanup();
        return -1;
    }

    AnimationSequence* animationSequence = &(anim_set[curr_anim_set]);
    AnimationDescription* animationDescription = &(animationSequence->animations[curr_anim_counter]);
    animationDescription->kind = ANIM_KIND_HIDE;
    animationDescription->delay = -1;
    animationDescription->artCacheKey = NULL;
    animationDescription->extendedFlags = ANIMATION_SEQUENCE_FORCED;
    animationDescription->owner = object;
    curr_anim_counter++;

    return 0;
}

// 0x4148BC
int register_object_call(void* a1, void* a2, AnimationCallback* proc, int delay)
{
    if (check_registry(NULL) == -1 || proc == NULL) {
        anim_cleanup();
        return -1;
    }

    AnimationSequence* animationSequence = &(anim_set[curr_anim_set]);
    AnimationDescription* animationDescription = &(animationSequence->animations[curr_anim_counter]);
    animationDescription->kind = ANIM_KIND_CALLBACK;
    animationDescription->extendedFlags = 0;
    animationDescription->artCacheKey = NULL;
    animationDescription->param2 = a2;
    animationDescription->param1 = a1;
    animationDescription->callback = proc;
    animationDescription->delay = delay;

    curr_anim_counter++;

    return 0;
}

// Same as `register_object_call` but accepting 3 parameters.
//
// 0x414950
int register_object_call3(void* a1, void* a2, void* a3, AnimationCallback3* proc, int delay)
{
    if (check_registry(NULL) == -1 || proc == NULL) {
        anim_cleanup();
        return -1;
    }

    AnimationSequence* animationSequence = &(anim_set[curr_anim_set]);
    AnimationDescription* animationDescription = &(animationSequence->animations[curr_anim_counter]);
    animationDescription->kind = ANIM_KIND_CALLBACK3;
    animationDescription->extendedFlags = 0;
    animationDescription->artCacheKey = NULL;
    animationDescription->param2 = a2;
    animationDescription->param1 = a1;
    animationDescription->callback3 = proc;
    animationDescription->param3 = a3;
    animationDescription->delay = delay;

    curr_anim_counter++;

    return 0;
}

// 0x4149E4
int register_object_must_call(void* a1, void* a2, AnimationCallback* proc, int delay)
{
    if (check_registry(NULL) == -1 || proc == NULL) {
        anim_cleanup();
        return -1;
    }

    AnimationSequence* animationSequence = &(anim_set[curr_anim_set]);
    AnimationDescription* animationDescription = &(animationSequence->animations[curr_anim_counter]);
    animationDescription->kind = ANIM_KIND_CALLBACK;
    animationDescription->extendedFlags = ANIMATION_SEQUENCE_FORCED;
    animationDescription->artCacheKey = NULL;
    animationDescription->param2 = a2;
    animationDescription->param1 = a1;
    animationDescription->callback = proc;
    animationDescription->delay = delay;

    curr_anim_counter++;

    return 0;
}

// The [flag] parameter should be one of OBJECT_* flags. The way it's handled
// down the road implies it should not be a group of flags (joined with bitwise
// OR), but a one particular flag.
//
// 0x414A78
int register_object_fset(Object* object, int flag, int delay)
{
    if (check_registry(object) == -1) {
        anim_cleanup();
        return -1;
    }

    AnimationSequence* animationSequence = &(anim_set[curr_anim_set]);
    AnimationDescription* animationDescription = &(animationSequence->animations[curr_anim_counter]);
    animationDescription->kind = ANIM_KIND_SET_FLAG;
    animationDescription->artCacheKey = NULL;
    animationDescription->owner = object;
    animationDescription->objectFlag = flag;
    animationDescription->delay = delay;

    curr_anim_counter++;

    return 0;
}

// The [flag] parameter should be one of OBJECT_* flags. The way it's handled
// down the road implies it should not be a group of flags (joined with bitwise
// OR), but a one particular flag.
//
// 0x414AF8
int register_object_funset(Object* object, int flag, int delay)
{
    if (check_registry(object) == -1) {
        anim_cleanup();
        return -1;
    }

    AnimationSequence* animationSequence = &(anim_set[curr_anim_set]);
    AnimationDescription* animationDescription = &(animationSequence->animations[curr_anim_counter]);
    animationDescription->kind = ANIM_KIND_UNSET_FLAG;
    animationDescription->artCacheKey = NULL;
    animationDescription->owner = object;
    animationDescription->objectFlag = flag;
    animationDescription->delay = delay;

    curr_anim_counter++;

    return 0;
}

// 0x414B78
int register_object_flatten(Object* object, int delay)
{
    AnimationSequence* animationSequence;
    AnimationDescription* animationDescription;

    if (check_registry(object) == -1) {
        anim_cleanup();
        return -1;
    }

    animationSequence = &(anim_set[curr_anim_set]);
    animationDescription = &(animationSequence->animations[curr_anim_counter]);
    animationDescription->kind = ANIM_KIND_TOGGLE_FLAT;
    animationDescription->artCacheKey = NULL;
    animationDescription->owner = object;
    animationDescription->delay = delay;

    curr_anim_counter++;

    return 0;
}

// 0x414BF4
int register_object_change_fid(Object* owner, int fid, int delay)
{
    if (check_registry(owner) == -1) {
        anim_cleanup();
        return -1;
    }

    AnimationSequence* animationSequence = &(anim_set[curr_anim_set]);
    AnimationDescription* animationDescription = &(animationSequence->animations[curr_anim_counter]);
    animationDescription->kind = ANIM_KIND_SET_FID;
    animationDescription->owner = owner;
    animationDescription->fid = fid;
    animationDescription->delay = delay;

    // NOTE: Uninline.
    if (anim_preload(owner, fid, &(animationDescription->artCacheKey)) == -1) {
        anim_cleanup();
        return -1;
    }

    curr_anim_counter++;

    return 0;
}

// 0x414CAC
int register_object_take_out(Object* owner, int weaponAnimationCode, int delay)
{
    const char* sfx = gsnd_build_character_sfx_name(owner, ANIM_TAKE_OUT, weaponAnimationCode);
    if (register_object_play_sfx(owner, sfx, delay) == -1) {
        return -1;
    }

    if (check_registry(owner) == -1) {
        anim_cleanup();
        return -1;
    }

    AnimationSequence* animationSequence = &(anim_set[curr_anim_set]);
    AnimationDescription* animationDescription = &(animationSequence->animations[curr_anim_counter]);
    animationDescription->kind = ANIM_KIND_TAKE_OUT_WEAPON;
    animationDescription->anim = ANIM_TAKE_OUT;
    animationDescription->delay = 0;
    animationDescription->owner = owner;
    animationDescription->weaponAnimationCode = weaponAnimationCode;

    int fid = art_id(FID_TYPE(owner->fid), owner->fid & 0xFFF, ANIM_TAKE_OUT, weaponAnimationCode, owner->rotation + 1);

    // NOTE: Uninline.
    if (anim_preload(owner, fid, &(animationDescription->artCacheKey)) == -1) {
        anim_cleanup();
        return -1;
    }

    curr_anim_counter++;

    return 0;
}

// 0x414DB4
int register_object_light(Object* owner, int lightDistance, int delay)
{
    if (check_registry(owner) == -1) {
        anim_cleanup();
        return -1;
    }

    AnimationSequence* animationSequence = &(anim_set[curr_anim_set]);
    AnimationDescription* animationDescription = &(animationSequence->animations[curr_anim_counter]);
    animationDescription->kind = ANIM_KIND_SET_LIGHT_DISTANCE;
    animationDescription->artCacheKey = NULL;
    animationDescription->owner = owner;
    animationDescription->lightDistance = lightDistance;
    animationDescription->delay = delay;

    curr_anim_counter++;

    return 0;
}

// 0x414E34
int register_object_outline(Object* object, bool outline, int delay)
{
    if (check_registry(object) == -1) {
        anim_cleanup();
        return -1;
    }

    AnimationSequence* animationSequence = &(anim_set[curr_anim_set]);
    AnimationDescription* animationDescription = &(animationSequence->animations[curr_anim_counter]);
    animationDescription->kind = ANIM_KIND_TOGGLE_OUTLINE;
    animationDescription->artCacheKey = NULL;
    animationDescription->owner = object;
    animationDescription->outline = outline;
    animationDescription->delay = delay;

    curr_anim_counter++;

    return 0;
}

// 0x414EB4
int register_object_play_sfx(Object* owner, const char* soundEffectName, int delay)
{
    if (check_registry(owner) == -1) {
        anim_cleanup();
        return -1;
    }

    AnimationSequence* animationSequence = &(anim_set[curr_anim_set]);
    AnimationDescription* animationDescription = &(animationSequence->animations[curr_anim_counter]);
    animationDescription->kind = ANIM_KIND_CALLBACK;
    animationDescription->owner = owner;
    if (soundEffectName != NULL) {
        int volume = gsound_compute_relative_volume(owner);
        animationDescription->param1 = gsound_load_sound_volume(soundEffectName, owner, volume);
        if (animationDescription->param1 != NULL) {
            animationDescription->callback = (AnimationCallback*)gsnd_anim_sound;
        } else {
            animationDescription->kind = ANIM_KIND_NOOP;
        }
    } else {
        animationDescription->kind = ANIM_KIND_NOOP;
    }

    animationDescription->artCacheKey = NULL;
    animationDescription->delay = delay;

    curr_anim_counter++;

    return 0;
}

// 0x414F68
int register_object_animate_forever(Object* owner, int anim, int delay)
{
    if (check_registry(owner) == -1) {
        anim_cleanup();
        return -1;
    }

    AnimationSequence* animationSequence = &(anim_set[curr_anim_set]);
    AnimationDescription* animationDescription = &(animationSequence->animations[curr_anim_counter]);
    animationDescription->kind = ANIM_KIND_ANIMATE_FOREVER;
    animationDescription->owner = owner;
    animationDescription->anim = anim;
    animationDescription->delay = delay;

    int fid = art_id(FID_TYPE(owner->fid), owner->fid & 0xFFF, anim, (owner->fid & 0xF000) >> 12, owner->rotation + 1);

    // NOTE: Uninline.
    if (anim_preload(owner, fid, &(animationDescription->artCacheKey)) == -1) {
        anim_cleanup();
        return -1;
    }

    curr_anim_counter++;

    return 0;
}

// 0x41504C
int register_ping(int a1, int delay)
{
    if (check_registry(NULL) == -1) {
        anim_cleanup();
        return -1;
    }

    int animationSequenceIndex = anim_free_slot(a1 | ANIMATION_REQUEST_0x100);
    if (animationSequenceIndex == -1) {
        return -1;
    }

    anim_set[animationSequenceIndex].flags = ANIM_SEQ_0x10;

    AnimationSequence* animationSequence = &(anim_set[curr_anim_set]);
    AnimationDescription* animationDescription = &(animationSequence->animations[curr_anim_counter]);
    animationDescription->owner = NULL;
    animationDescription->kind = ANIM_KIND_26;
    animationDescription->artCacheKey = NULL;
    animationDescription->animationSequenceIndex = animationSequenceIndex;
    animationDescription->delay = delay;

    curr_anim_counter++;

    return 0;
}

// 0x415170
static int anim_set_check(int animationSequenceIndex)
{
    if (animationSequenceIndex == -1) {
        return -1;
    }

    AnimationSequence* animationSequence = &(anim_set[animationSequenceIndex]);
    if (animationSequence->field_0 == -1000) {
        return -1;
    }

    while (1) {
        if (animationSequence->field_0 >= animationSequence->length) {
            return 0;
        }

        if (animationSequence->field_0 > animationSequence->animationIndex) {
            AnimationDescription* animationDescription = &(animationSequence->animations[animationSequence->field_0]);
            if (animationDescription->delay < 0) {
                return 0;
            }

            if (animationDescription->delay > 0) {
                animationDescription->delay--;
                return 0;
            }
        }

        AnimationDescription* animationDescription = &(animationSequence->animations[animationSequence->field_0++]);

        int rc;
        Rect rect;
        switch (animationDescription->kind) {
        case ANIM_KIND_MOVE_TO_OBJECT:
            rc = anim_move_to_object(animationDescription->owner, animationDescription->destination, animationDescription->actionPoints, animationDescription->anim, animationSequenceIndex);
            break;
        case ANIM_KIND_MOVE_TO_TILE:
            rc = anim_move_to_tile(animationDescription->owner, animationDescription->tile, animationDescription->elevation, animationDescription->actionPoints, animationDescription->anim, animationSequenceIndex);
            break;
        case ANIM_KIND_MOVE_TO_TILE_STRAIGHT:
            rc = anim_move_straight_to_tile(animationDescription->owner, animationDescription->tile, animationDescription->elevation, animationDescription->anim, animationSequenceIndex, 0);
            break;
        case ANIM_KIND_MOVE_TO_TILE_STRAIGHT_AND_WAIT_FOR_COMPLETE:
            rc = anim_move_straight_to_tile(animationDescription->owner, animationDescription->tile, animationDescription->elevation, animationDescription->anim, animationSequenceIndex, ANIM_SAD_WAIT_FOR_COMPLETION);
            break;
        case ANIM_KIND_ANIMATE:
            rc = anim_animate(animationDescription->owner, animationDescription->anim, animationSequenceIndex, 0);
            break;
        case ANIM_KIND_ANIMATE_REVERSED:
            rc = anim_animate(animationDescription->owner, animationDescription->anim, animationSequenceIndex, ANIM_SAD_REVERSE);
            break;
        case ANIM_KIND_ANIMATE_AND_HIDE:
            rc = anim_animate(animationDescription->owner, animationDescription->anim, animationSequenceIndex, ANIM_SAD_HIDE_ON_END);
            if (rc == -1) {
                // NOTE: Uninline.
                rc = anim_hide(animationDescription->owner, animationSequenceIndex);
            }
            break;
        case ANIM_KIND_ANIMATE_FOREVER:
            rc = anim_animate(animationDescription->owner, animationDescription->anim, animationSequenceIndex, ANIM_SAD_FOREVER);
            break;
        case ANIM_KIND_ROTATE_TO_TILE:
            if (!critter_is_prone(animationDescription->owner)) {
                int rotation = tile_dir(animationDescription->owner->tile, animationDescription->tile);
                dude_stand(animationDescription->owner, rotation, -1);
            }
            anim_set_continue(animationSequenceIndex, 0);
            rc = 0;
            break;
        case ANIM_KIND_ROTATE_CLOCKWISE:
            rc = anim_turn_towards(animationDescription->owner, 1, animationSequenceIndex);
            break;
        case ANIM_KIND_ROTATE_COUNTER_CLOCKWISE:
            rc = anim_turn_towards(animationDescription->owner, -1, animationSequenceIndex);
            break;
        case ANIM_KIND_HIDE:
            // NOTE: Uninline.
            rc = anim_hide(animationDescription->owner, animationSequenceIndex);
            break;
        case ANIM_KIND_CALLBACK:
            rc = animationDescription->callback(animationDescription->param1, animationDescription->param2);
            if (rc == 0) {
                rc = anim_set_continue(animationSequenceIndex, 0);
            }
            break;
        case ANIM_KIND_CALLBACK3:
            rc = animationDescription->callback3(animationDescription->param1, animationDescription->param2, animationDescription->param3);
            if (rc == 0) {
                rc = anim_set_continue(animationSequenceIndex, 0);
            }
            break;
        case ANIM_KIND_SET_FLAG:
            if (animationDescription->objectFlag == OBJECT_LIGHTING) {
                if (obj_turn_on_light(animationDescription->owner, &rect) == 0) {
                    tile_refresh_rect(&rect, animationDescription->owner->elevation);
                }
            } else if (animationDescription->objectFlag == OBJECT_HIDDEN) {
                if (obj_turn_off(animationDescription->owner, &rect) == 0) {
                    tile_refresh_rect(&rect, animationDescription->owner->elevation);
                }
            } else {
                animationDescription->owner->flags |= animationDescription->objectFlag;
            }

            rc = anim_set_continue(animationSequenceIndex, 0);
            break;
        case ANIM_KIND_UNSET_FLAG:
            if (animationDescription->objectFlag == OBJECT_LIGHTING) {
                if (obj_turn_off_light(animationDescription->owner, &rect) == 0) {
                    tile_refresh_rect(&rect, animationDescription->owner->elevation);
                }
            } else if (animationDescription->objectFlag == OBJECT_HIDDEN) {
                if (obj_turn_on(animationDescription->owner, &rect) == 0) {
                    tile_refresh_rect(&rect, animationDescription->owner->elevation);
                }
            } else {
                animationDescription->owner->flags &= ~animationDescription->objectFlag;
            }

            rc = anim_set_continue(animationSequenceIndex, 0);
            break;
        case ANIM_KIND_TOGGLE_FLAT:
            if (obj_toggle_flat(animationDescription->owner, &rect) == 0) {
                tile_refresh_rect(&rect, animationDescription->owner->elevation);
            }
            rc = anim_set_continue(animationSequenceIndex, 0);
            break;
        case ANIM_KIND_SET_FID:
            rc = anim_change_fid(animationDescription->owner, animationSequenceIndex, animationDescription->fid);
            break;
        case ANIM_KIND_TAKE_OUT_WEAPON:
            rc = anim_animate(animationDescription->owner, ANIM_TAKE_OUT, animationSequenceIndex, animationDescription->tile);
            break;
        case ANIM_KIND_SET_LIGHT_DISTANCE:
            obj_set_light(animationDescription->owner, animationDescription->lightDistance, animationDescription->owner->lightIntensity, &rect);
            tile_refresh_rect(&rect, animationDescription->owner->elevation);
            rc = anim_set_continue(animationSequenceIndex, 0);
            break;
        case ANIM_KIND_MOVE_ON_STAIRS:
            rc = anim_move_on_stairs(animationDescription->owner, animationDescription->tile, animationDescription->elevation, animationDescription->anim, animationSequenceIndex);
            break;
        case ANIM_KIND_CHECK_FALLING:
            rc = check_for_falling(animationDescription->owner, animationDescription->anim, animationSequenceIndex);
            break;
        case ANIM_KIND_TOGGLE_OUTLINE:
            if (animationDescription->outline) {
                if (obj_turn_on_outline(animationDescription->owner, &rect) == 0) {
                    tile_refresh_rect(&rect, animationDescription->owner->elevation);
                }
            } else {
                if (obj_turn_off_outline(animationDescription->owner, &rect) == 0) {
                    tile_refresh_rect(&rect, animationDescription->owner->elevation);
                }
            }
            rc = anim_set_continue(animationSequenceIndex, 0);
            break;
        case ANIM_KIND_26:
            anim_set[animationDescription->animationSequenceIndex].flags &= ~ANIM_SEQ_0x10;
            rc = anim_set_continue(animationDescription->animationSequenceIndex, 1);
            if (rc != -1) {
                rc = anim_set_continue(animationSequenceIndex, 0);
            }
            break;
        case ANIM_KIND_NOOP:
            rc = anim_set_continue(animationSequenceIndex, 0);
            break;
        default:
            rc = -1;
            break;
        }

        if (rc == -1) {
            anim_set_end(animationSequenceIndex);
        }

        if (animationSequence->field_0 == -1000) {
            return -1;
        }
    }
}

// 0x415620
static int anim_set_continue(int animationSequenceIndex, int a2)
{
    if (animationSequenceIndex == -1) {
        return -1;
    }

    AnimationSequence* animationSequence = &(anim_set[animationSequenceIndex]);
    if (animationSequence->field_0 == -1000) {
        return -1;
    }

    animationSequence->animationIndex++;
    if (animationSequence->animationIndex == animationSequence->length) {
        return anim_set_end(animationSequenceIndex);
    } else {
        if (a2) {
            return anim_set_check(animationSequenceIndex);
        }
    }

    return 0;
}

// 0x41568C
static int anim_set_end(int animationSequenceIndex)
{
    AnimationSequence* animationSequence;
    AnimationDescription* animationDescription;
    int i;

    if (animationSequenceIndex == -1) {
        return -1;
    }

    animationSequence = &(anim_set[animationSequenceIndex]);
    if (animationSequence->field_0 == -1000) {
        return -1;
    }

    for (i = 0; i < curr_sad; i++) {
        AnimationSad* sad_entry = &(sad[i]);
        if (sad_entry->animationSequenceIndex == animationSequenceIndex) {
            sad_entry->field_20 = -1000;
        }
    }

    for (i = 0; i < animationSequence->length; i++) {
        animationDescription = &(animationSequence->animations[i]);
        if (animationDescription->kind == ANIM_KIND_HIDE && ((i < animationSequence->animationIndex) || (animationDescription->extendedFlags & ANIMATION_SEQUENCE_FORCED))) {
            Rect rect;
            int elevation = animationDescription->owner->elevation;
            obj_erase_object(animationDescription->owner, &rect);
            tile_refresh_rect(&rect, elevation);
        }
    }

    for (i = 0; i < animationSequence->length; i++) {
        animationDescription = &(animationSequence->animations[i]);
        if (animationDescription->artCacheKey) {
            art_ptr_unlock(animationDescription->artCacheKey);
        }

        if (animationDescription->kind != 11 && animationDescription->kind != 12) {
            // TODO: Check.
            if (animationDescription->kind != ANIM_KIND_26) {
                Object* owner = animationDescription->owner;
                if (FID_TYPE(owner->fid) == OBJ_TYPE_CRITTER) {
                    int j = 0;
                    for (; j < i; j++) {
                        AnimationDescription* ad = &(animationSequence->animations[j]);
                        if (owner == ad->owner) {
                            if (ad->kind != ANIM_KIND_CALLBACK && ad->kind != ANIM_KIND_CALLBACK3) {
                                break;
                            }
                        }
                    }

                    if (i == j) {
                        int k = 0;
                        for (; k < animationSequence->animationIndex; k++) {
                            AnimationDescription* ad = &(animationSequence->animations[k]);
                            if (ad->kind == ANIM_KIND_HIDE && ad->owner == owner) {
                                break;
                            }
                        }

                        if (k == animationSequence->animationIndex) {
                            for (int m = 0; m < curr_sad; m++) {
                                if (sad[m].obj == owner) {
                                    sad[m].field_20 = -1000;
                                    break;
                                }
                            }

                            if ((animationSequence->flags & ANIM_SEQ_NO_STAND) == 0 && !critter_is_prone(owner)) {
                                dude_stand(owner, owner->rotation, -1);
                            }
                        }
                    }
                }
            }
        } else if (i >= animationSequence->field_0) {
            if (animationDescription->extendedFlags & ANIMATION_SEQUENCE_FORCED) {
                animationDescription->callback(animationDescription->param1, animationDescription->param2);
            } else {
                if (animationDescription->kind == ANIM_KIND_CALLBACK && animationDescription->callback == (AnimationCallback*)gsnd_anim_sound) {
                    gsound_delete_sfx((Sound*)animationDescription->param1);
                }
            }
        }
    }

    animationSequence->animationIndex = -1;
    animationSequence->field_0 = -1000;
    if ((animationSequence->flags & ANIM_SEQ_COMBAT_ANIM_STARTED) != 0) {
        combat_anim_finished();
    }

    if (anim_in_bk) {
        animationSequence->flags = ANIM_SEQ_0x20;
    } else {
        animationSequence->flags = 0;
    }

    return 0;
}

// 0x415940
static bool anim_can_use_door(Object* critter, Object* door)
{
    int body_type;
    Proto* door_proto;

    if (critter == obj_dude) {
        return false;
    }

    if (FID_TYPE(critter->fid) != OBJ_TYPE_CRITTER) {
        return false;
    }

    body_type = critter_body_type(critter);
    if (body_type != BODY_TYPE_BIPED && body_type != BODY_TYPE_ROBOTIC) {
        return false;
    }

    if (FID_TYPE(door->fid) != OBJ_TYPE_SCENERY) {
        return false;
    }

    if (proto_ptr(door->pid, &door_proto) == -1) {
        return false;
    }

    if (door_proto->scenery.type != SCENERY_TYPE_DOOR) {
        return false;
    }

    if (obj_is_locked(door)) {
        return false;
    }

    return true;
}

// 0x4159D4
int make_path(Object* object, int from, int to, unsigned char* rotations, int a5)
{
    return make_path_func(object, from, to, rotations, a5, obj_blocking_at);
}

// 0x4159E8
int make_path_func(Object* object, int from, int to, unsigned char* rotations, int a5, PathBuilderCallback* callback)
{
    if (a5) {
        if (callback(object, to, object->elevation) != NULL) {
            return 0;
        }
    }

    bool isNotInCombat = !isInCombat();

    memset(seen, 0, sizeof(seen));

    seen[from / 8] |= 1 << (from & 7);

    child[0].tile = from;
    child[0].from = -1;
    child[0].rotation = 0;
    child[0].field_C = EST(from, to);
    child[0].field_10 = 0;

    for (int index = 1; index < 2000; index += 1) {
        child[index].tile = -1;
    }

    int toScreenX;
    int toScreenY;
    tile_coord(to, &toScreenX, &toScreenY, object->elevation);

    int closedPathNodeListLength = 0;
    int openPathNodeListLength = 1;
    PathNode temp;

    while (1) {
        int v63 = -1;

        PathNode* prev = NULL;
        int v12 = 0;
        for (int index = 0; v12 < openPathNodeListLength; index += 1) {
            PathNode* curr = &(child[index]);
            if (curr->tile != -1) {
                v12++;
                if (v63 == -1 || (curr->field_C + curr->field_10) < (prev->field_C + prev->field_10)) {
                    prev = curr;
                    v63 = index;
                }
            }
        }

        PathNode* curr = &(child[v63]);

        memcpy(&temp, curr, sizeof(temp));

        openPathNodeListLength -= 1;

        curr->tile = -1;

        if (temp.tile == to) {
            if (openPathNodeListLength == 0) {
                openPathNodeListLength = 1;
            }
            break;
        }

        PathNode* curr1 = &(dad[closedPathNodeListLength]);
        memcpy(curr1, &temp, sizeof(temp));

        closedPathNodeListLength += 1;

        if (closedPathNodeListLength == 2000) {
            return 0;
        }

        for (int rotation = 0; rotation < ROTATION_COUNT; rotation++) {
            int tile = tile_num_in_direction(temp.tile, rotation, 1);
            int bit = 1 << (tile & 7);
            if ((seen[tile / 8] & bit) != 0) {
                continue;
            }

            if (tile != to) {
                Object* v24 = callback(object, tile, object->elevation);
                if (v24 != NULL) {
                    if (!anim_can_use_door(object, v24)) {
                        continue;
                    }
                }
            }

            int v25 = 0;
            for (; v25 < 2000; v25++) {
                if (child[v25].tile == -1) {
                    break;
                }
            }

            openPathNodeListLength += 1;

            if (openPathNodeListLength == 2000) {
                return 0;
            }

            seen[tile / 8] |= bit;

            PathNode* v27 = &(child[v25]);
            v27->tile = tile;
            v27->from = temp.tile;
            v27->rotation = rotation;

            int newX;
            int newY;
            tile_coord(tile, &newX, &newY, object->elevation);

            v27->field_C = idist(newX, newY, toScreenX, toScreenY);
            v27->field_10 = temp.field_10 + 50;

            if (isNotInCombat && temp.rotation != rotation) {
                v27->field_10 += 10;
            }
        }

        if (openPathNodeListLength == 0) {
            break;
        }
    }

    if (openPathNodeListLength != 0) {
        unsigned char* v39 = rotations;
        int index = 0;
        for (; index < 800; index++) {
            if (temp.tile == from) {
                break;
            }

            if (v39 != NULL) {
                *v39 = temp.rotation & 0xFF;
                v39 += 1;
            }

            int j = 0;
            while (dad[j].tile != temp.from) {
                j++;
            }

            PathNode* v36 = &(dad[j]);
            memcpy(&temp, v36, sizeof(temp));
        }

        if (rotations != NULL) {
            // Looks like array resevering, probably because A* finishes it's path from end to start,
            // this probably reverses it start-to-end.
            unsigned char* beginning = rotations;
            unsigned char* ending = rotations + index - 1;
            int middle = index / 2;
            for (int index = 0; index < middle; index++) {
                unsigned char rotation = *ending;
                *ending = *beginning;
                *beginning = rotation;

                ending -= 1;
                beginning += 1;
            }
        }

        return index;
    }

    return 0;
}

// 0x415D9C
int idist(int x1, int y1, int x2, int y2)
{
    int dx = x2 - x1;
    if (dx < 0) {
        dx = -dx;
    }

    int dy = y2 - y1;
    if (dy < 0) {
        dy = -dy;
    }

    int dm = (dx <= dy) ? dx : dy;

    return dx + dy - (dm / 2);
}

// 0x415DC0
int EST(int tile1, int tile2)
{
    int x1;
    int y1;
    tile_coord(tile1, &x1, &y1, map_elevation);

    int x2;
    int y2;
    tile_coord(tile2, &x2, &y2, map_elevation);

    return idist(x1, y1, x2, y2);
}

// 0x415E0C
int make_straight_path(Object* a1, int from, int to, StraightPathNode* pathNodes, Object** a5, int a6)
{
    return make_straight_path_func(a1, from, to, pathNodes, a5, a6, obj_blocking_at);
}

// TODO: Rather complex, but understandable, needs testing.
//
// 0x415E28
int make_straight_path_func(Object* a1, int from, int to, StraightPathNode* pathNodes, Object** a5, int a6, PathBuilderCallback* callback)
{
    if (a5 != NULL) {
        Object* v11 = callback(a1, from, a1->elevation);
        if (v11 != NULL) {
            if (v11 != *a5 && (a6 != 32 || (v11->flags & OBJECT_SHOOT_THRU) == 0)) {
                *a5 = v11;
                return 0;
            }
        }
    }

    int fromX;
    int fromY;
    tile_coord(from, &fromX, &fromY, a1->elevation);
    fromX += 16;
    fromY += 8;

    int toX;
    int toY;
    tile_coord(to, &toX, &toY, a1->elevation);
    toX += 16;
    toY += 8;

    int stepX;
    int deltaX = toX - fromX;
    if (deltaX > 0)
        stepX = 1;
    else if (deltaX < 0)
        stepX = -1;
    else
        stepX = 0;

    int stepY;
    int deltaY = toY - fromY;
    if (deltaY > 0)
        stepY = 1;
    else if (deltaY < 0)
        stepY = -1;
    else
        stepY = 0;

    int v48 = 2 * abs(toX - fromX);
    int v47 = 2 * abs(toY - fromY);

    int tileX = fromX;
    int tileY = fromY;

    int pathNodeIndex = 0;
    int prevTile = from;
    int v22 = 0;
    int tile;

    if (v48 <= v47) {
        int middle = v48 - v47 / 2;
        while (true) {
            tile = tile_num(tileX, tileY, a1->elevation);

            v22 += 1;
            if (v22 == a6) {
                if (pathNodeIndex >= 200) {
                    return 0;
                }

                if (pathNodes != NULL) {
                    StraightPathNode* pathNode = &(pathNodes[pathNodeIndex]);
                    pathNode->tile = tile;
                    pathNode->elevation = a1->elevation;

                    tile_coord(tile, &fromX, &fromY, a1->elevation);
                    pathNode->x = tileX - fromX - 16;
                    pathNode->y = tileY - fromY - 8;
                }

                v22 = 0;
                pathNodeIndex++;
            }

            if (tileY == toY) {
                if (a5 != NULL) {
                    *a5 = NULL;
                }
                break;
            }

            if (middle >= 0) {
                tileX += stepX;
                middle -= v47;
            }

            tileY += stepY;
            middle += v48;

            if (tile != prevTile) {
                if (a5 != NULL) {
                    Object* obj = callback(a1, tile, a1->elevation);
                    if (obj != NULL) {
                        if (obj != *a5 && (a6 != 32 || (obj->flags & OBJECT_SHOOT_THRU) == 0)) {
                            *a5 = obj;
                            break;
                        }
                    }
                }
                prevTile = tile;
            }
        }
    } else {
        int middle = v47 - v48 / 2;
        while (true) {
            tile = tile_num(tileX, tileY, a1->elevation);

            v22 += 1;
            if (v22 == a6) {
                if (pathNodeIndex >= 200) {
                    return 0;
                }

                if (pathNodes != NULL) {
                    StraightPathNode* pathNode = &(pathNodes[pathNodeIndex]);
                    pathNode->tile = tile;
                    pathNode->elevation = a1->elevation;

                    tile_coord(tile, &fromX, &fromY, a1->elevation);
                    pathNode->x = tileX - fromX - 16;
                    pathNode->y = tileY - fromY - 8;
                }

                v22 = 0;
                pathNodeIndex++;
            }

            if (tileX == toX) {
                if (a5 != NULL) {
                    *a5 = NULL;
                }
                break;
            }

            if (middle >= 0) {
                tileY += stepY;
                middle -= v48;
            }

            tileX += stepX;
            middle += v47;

            if (tile != prevTile) {
                if (a5 != NULL) {
                    Object* obj = callback(a1, tile, a1->elevation);
                    if (obj != NULL) {
                        if (obj != *a5 && (a6 != 32 || (obj->flags & OBJECT_SHOOT_THRU) == 0)) {
                            *a5 = obj;
                            break;
                        }
                    }
                }
                prevTile = tile;
            }
        }
    }

    if (v22 != 0) {
        if (pathNodeIndex >= 200) {
            return 0;
        }

        if (pathNodes != NULL) {
            StraightPathNode* pathNode = &(pathNodes[pathNodeIndex]);
            pathNode->tile = tile;
            pathNode->elevation = a1->elevation;

            tile_coord(tile, &fromX, &fromY, a1->elevation);
            pathNode->x = tileX - fromX - 16;
            pathNode->y = tileY - fromY - 8;
        }

        pathNodeIndex += 1;
    } else {
        if (pathNodeIndex > 0 && pathNodes != NULL) {
            pathNodes[pathNodeIndex - 1].elevation = a1->elevation;
        }
    }

    return pathNodeIndex;
}

// 0x416258
static int anim_move_to_object(Object* from, Object* to, int a3, int anim, int animationSequenceIndex)
{
    bool hidden = (to->flags & OBJECT_HIDDEN);
    to->flags |= OBJECT_HIDDEN;

    int moveSadIndex = anim_move(from, to->tile, to->elevation, -1, anim, 0, animationSequenceIndex);

    if (!hidden) {
        to->flags &= ~OBJECT_HIDDEN;
    }

    if (moveSadIndex == -1) {
        return -1;
    }

    AnimationSad* sad_entry = &(sad[moveSadIndex]);
    // NOTE: Original code is somewhat different. Due to some kind of
    // optimization this value is either 1 or 2, which is later used in
    // subsequent calculations and rotations array lookup.
    bool isMultihex = (from->flags & OBJECT_MULTIHEX);
    sad_entry->field_1C -= (isMultihex ? 2 : 1);
    if (sad_entry->field_1C <= 0) {
        sad_entry->field_20 = -1000;
        anim_set_continue(animationSequenceIndex, 0);
    }

    sad_entry->field_24 = tile_num_in_direction(to->tile, sad_entry->rotations[isMultihex ? sad_entry->field_1C + 1 : sad_entry->field_1C], 1);

    if (isMultihex) {
        sad_entry->field_24 = tile_num_in_direction(sad_entry->field_24, sad_entry->rotations[sad_entry->field_1C], 1);
    }

    if (a3 != -1 && a3 < sad_entry->field_1C) {
        sad_entry->field_1C = a3;
    }

    return 0;
}

// 0x4163B4
static int make_stair_path(Object* object, int from, int fromElevation, int to, int toElevation, StraightPathNode* a6, Object** obstaclePtr)
{
    int elevation = fromElevation;
    if (elevation > toElevation) {
        elevation = toElevation;
    }

    int fromX;
    int fromY;
    tile_coord(from, &fromX, &fromY, fromElevation);
    fromX += 16;
    fromY += 8;

    int toX;
    int toY;
    tile_coord(to, &toX, &toY, toElevation);
    toX += 16;
    toY += 8;

    if (obstaclePtr != NULL) {
        *obstaclePtr = NULL;
    }

    int ddx = 2 * abs(toX - fromX);

    int stepX;
    int deltaX = toX - fromX;
    if (deltaX > 0) {
        stepX = 1;
    } else if (deltaX < 0) {
        stepX = -1;
    } else {
        stepX = 0;
    }

    int ddy = 2 * abs(toY - fromY);

    int stepY;
    int deltaY = toY - fromY;
    if (deltaY > 0) {
        stepY = 1;
    } else if (deltaY < 0) {
        stepY = -1;
    } else {
        stepY = 0;
    }

    int tileX = fromX;
    int tileY = fromY;

    int pathNodeIndex = 0;
    int prevTile = from;
    int iteration = 0;
    int tile;

    if (ddx > ddy) {
        int middle = ddy - ddx / 2;
        while (true) {
            tile = tile_num(tileX, tileY, elevation);

            iteration += 1;
            if (iteration == 16) {
                if (pathNodeIndex >= 200) {
                    return 0;
                }

                if (a6 != NULL) {
                    StraightPathNode* pathNode = &(a6[pathNodeIndex]);
                    pathNode->tile = tile;
                    pathNode->elevation = elevation;

                    tile_coord(tile, &fromX, &fromY, elevation);
                    pathNode->x = tileX - fromX - 16;
                    pathNode->y = tileY - fromY - 8;
                }

                iteration = 0;
                pathNodeIndex++;
            }

            if (tileX == toX) {
                break;
            }

            if (middle >= 0) {
                tileY += stepY;
                middle -= ddx;
            }

            tileX += stepX;
            middle += ddy;

            if (tile != prevTile) {
                if (obstaclePtr != NULL) {
                    *obstaclePtr = obj_blocking_at(object, tile, object->elevation);
                    if (*obstaclePtr != NULL) {
                        break;
                    }
                }
                prevTile = tile;
            }
        }
    } else {
        int middle = ddx - ddy / 2;
        while (true) {
            tile = tile_num(tileX, tileY, elevation);

            iteration += 1;
            if (iteration == 16) {
                if (pathNodeIndex >= 200) {
                    return 0;
                }

                if (a6 != NULL) {
                    StraightPathNode* pathNode = &(a6[pathNodeIndex]);
                    pathNode->tile = tile;
                    pathNode->elevation = elevation;

                    tile_coord(tile, &fromX, &fromY, elevation);
                    pathNode->x = tileX - fromX - 16;
                    pathNode->y = tileY - fromY - 8;
                }

                iteration = 0;
                pathNodeIndex++;
            }

            if (tileY == toY) {
                break;
            }

            if (middle >= 0) {
                tileX += stepX;
                middle -= ddy;
            }

            tileY += stepY;
            middle += ddx;

            if (tile != prevTile) {
                if (obstaclePtr != NULL) {
                    *obstaclePtr = obj_blocking_at(object, tile, object->elevation);
                    if (*obstaclePtr != NULL) {
                        break;
                    }
                }
                prevTile = tile;
            }
        }
    }

    if (iteration != 0) {
        if (pathNodeIndex >= 200) {
            return 0;
        }

        if (a6 != NULL) {
            StraightPathNode* pathNode = &(a6[pathNodeIndex]);
            pathNode->tile = tile;
            pathNode->elevation = elevation;

            tile_coord(tile, &fromX, &fromY, elevation);
            pathNode->x = tileX - fromX - 16;
            pathNode->y = tileY - fromY - 8;
        }

        pathNodeIndex++;
    } else {
        if (pathNodeIndex > 0) {
            if (a6 != NULL) {
                a6[pathNodeIndex - 1].elevation = toElevation;
            }
        }
    }

    return pathNodeIndex;
}

// 0x416754
static int anim_move_to_tile(Object* obj, int tile, int elev, int a4, int anim, int animationSequenceIndex)
{
    int v1;

    v1 = anim_move(obj, tile, elev, -1, anim, 0, animationSequenceIndex);
    if (v1 == -1) {
        return -1;
    }

    if (obj_blocking_at(obj, tile, elev)) {
        AnimationSad* sad_entry = &(sad[v1]);
        sad_entry->field_1C--;
        if (sad_entry->field_1C <= 0) {
            sad_entry->field_20 = -1000;
            anim_set_continue(animationSequenceIndex, 0);
        }

        sad_entry->field_24 = tile_num_in_direction(tile, sad_entry->rotations[sad_entry->field_1C], 1);
        if (a4 != -1 && a4 < sad_entry->field_1C) {
            sad_entry->field_1C = a4;
        }
    }

    return 0;
}

// 0x416778
static int anim_move(Object* obj, int tile, int elev, int a3, int anim, int a5, int animationSequenceIndex)
{
    if (curr_sad == ANIMATION_SAD_LIST_CAPACITY) {
        return -1;
    }

    AnimationSad* sad_entry = &(sad[curr_sad]);
    sad_entry->obj = obj;

    if (a5) {
        sad_entry->flags = ANIM_SAD_0x20;
    } else {
        sad_entry->flags = 0;
    }

    sad_entry->field_20 = -2000;
    sad_entry->fid = art_id(FID_TYPE(obj->fid), obj->fid & 0xFFF, anim, (obj->fid & 0xF000) >> 12, obj->rotation + 1);
    sad_entry->animationTimestamp = 0;
    sad_entry->ticksPerFrame = compute_tpf(obj, sad_entry->fid);
    sad_entry->field_24 = tile;
    sad_entry->animationSequenceIndex = animationSequenceIndex;
    sad_entry->anim = anim;

    sad_entry->field_1C = make_path(obj, obj->tile, tile, sad_entry->rotations, a5);
    if (sad_entry->field_1C == 0) {
        sad_entry->field_20 = -1000;
        return -1;
    }

    if (a3 != -1 && sad_entry->field_1C > a3) {
        sad_entry->field_1C = a3;
    }

    return curr_sad++;
}

// 0x4168D0
static int anim_move_straight_to_tile(Object* obj, int tile, int elevation, int anim, int animationSequenceIndex, int flags)
{
    if (curr_sad == ANIMATION_SAD_LIST_CAPACITY) {
        return -1;
    }

    AnimationSad* sad_entry = &(sad[curr_sad]);
    sad_entry->obj = obj;
    sad_entry->flags = flags | ANIM_SAD_STRAIGHT;
    if (anim == -1) {
        sad_entry->fid = obj->fid;
        sad_entry->flags |= ANIM_SAD_NO_ANIM;
    } else {
        sad_entry->fid = art_id(FID_TYPE(obj->fid), obj->fid & 0xFFF, anim, (obj->fid & 0xF000) >> 12, obj->rotation + 1);
    }
    sad_entry->field_20 = -2000;
    sad_entry->animationTimestamp = 0;
    sad_entry->ticksPerFrame = compute_tpf(obj, sad_entry->fid);
    sad_entry->animationSequenceIndex = animationSequenceIndex;

    int v15;
    if (FID_TYPE(obj->fid) == OBJ_TYPE_CRITTER) {
        if (FID_ANIM_TYPE(obj->fid) == ANIM_JUMP_BEGIN)
            v15 = 16;
        else
            v15 = 4;
    } else {
        v15 = 32;
    }

    sad_entry->field_1C = make_straight_path(obj, obj->tile, tile, sad_entry->field_28, NULL, v15);
    if (sad_entry->field_1C == 0) {
        sad_entry->field_20 = -1000;
        return -1;
    }

    curr_sad++;

    return 0;
}

// 0x416AA8
int anim_move_on_stairs(Object* obj, int tile, int elevation, int anim, int animationSequenceIndex)
{
    if (curr_sad == ANIMATION_SAD_LIST_CAPACITY) {
        return -1;
    }

    AnimationSad* sad_entry = &(sad[curr_sad]);
    sad_entry->flags = ANIM_SAD_STRAIGHT;
    sad_entry->obj = obj;
    if (anim == -1) {
        sad_entry->fid = obj->fid;
        sad_entry->flags |= ANIM_SAD_NO_ANIM;
    } else {
        sad_entry->fid = art_id(FID_TYPE(obj->fid), obj->fid & 0xFFF, anim, (obj->fid & 0xF000) >> 12, obj->rotation + 1);
    }
    sad_entry->field_20 = -2000;
    sad_entry->animationTimestamp = 0;
    sad_entry->ticksPerFrame = compute_tpf(obj, sad_entry->fid);
    sad_entry->animationSequenceIndex = animationSequenceIndex;
    sad_entry->field_1C = make_stair_path(obj, obj->tile, obj->elevation, tile, elevation, sad_entry->field_28, NULL);
    if (sad_entry->field_1C == 0) {
        sad_entry->field_20 = -1000;
        return -1;
    }

    curr_sad++;

    return 0;
}

// 0x416BC4
int check_for_falling(Object* obj, int anim, int a3)
{
    if (curr_sad == ANIMATION_SAD_LIST_CAPACITY) {
        return -1;
    }

    if (check_gravity(obj->tile, obj->elevation) == obj->elevation) {
        return -1;
    }

    AnimationSad* sad_entry = &(sad[curr_sad]);
    sad_entry->flags = ANIM_SAD_STRAIGHT;
    sad_entry->obj = obj;
    if (anim == -1) {
        sad_entry->fid = obj->fid;
        sad_entry->flags |= ANIM_SAD_NO_ANIM;
    } else {
        sad_entry->fid = art_id(FID_TYPE(obj->fid), obj->fid & 0xFFF, anim, (obj->fid & 0xF000) >> 12, obj->rotation + 1);
    }
    sad_entry->field_20 = -2000;
    sad_entry->animationTimestamp = 0;
    sad_entry->ticksPerFrame = compute_tpf(obj, sad_entry->fid);
    sad_entry->animationSequenceIndex = a3;
    sad_entry->field_1C = make_straight_path_func(obj, obj->tile, obj->tile, sad_entry->field_28, 0, 16, obj_blocking_at);
    if (sad_entry->field_1C == 0) {
        sad_entry->field_20 = -1000;
        return -1;
    }

    curr_sad++;

    return 0;
}

// 0x416CDC
static void object_move(int index)
{
    AnimationSad* sad_entry = &(sad[index]);
    Object* object = sad_entry->obj;

    Rect dirty;
    Rect temp;

    if (sad_entry->field_20 == -2000) {
        obj_move_to_tile(object, object->tile, object->elevation, &dirty);

        obj_set_frame(object, 0, &temp);
        rect_min_bound(&dirty, &temp, &dirty);

        obj_set_rotation(object, sad_entry->rotations[0], &temp);
        rect_min_bound(&dirty, &temp, &dirty);

        int fid = art_id(FID_TYPE(object->fid), object->fid & 0xFFF, sad_entry->anim, (object->fid & 0xF000) >> 12, object->rotation + 1);
        obj_change_fid(object, fid, &temp);
        rect_min_bound(&dirty, &temp, &dirty);

        sad_entry->field_20 = 0;
    } else {
        obj_inc_frame(object, &dirty);
    }

    int frameX;
    int frameY;

    CacheEntry* cacheHandle;
    Art* art = art_ptr_lock(object->fid, &cacheHandle);
    if (art != NULL) {
        art_frame_hot(art, object->frame, object->rotation, &frameX, &frameY);
        art_ptr_unlock(cacheHandle);
    } else {
        frameX = 0;
        frameY = 0;
    }

    obj_offset(object, frameX, frameY, &temp);
    rect_min_bound(&dirty, &temp, &dirty);

    int rotation = sad_entry->rotations[sad_entry->field_20];
    int y = off_tile[1][rotation];
    int x = off_tile[0][rotation];
    if ((x > 0 && x <= object->x) || (x < 0 && x >= object->x) || (y > 0 && y <= object->y) || (y < 0 && y >= object->y)) {
        x = object->x - x;
        y = object->y - y;

        int v10 = tile_num_in_direction(object->tile, rotation, 1);
        Object* v12 = obj_blocking_at(object, v10, object->elevation);
        if (v12 != NULL) {
            if (!anim_can_use_door(object, v12)) {
                sad_entry->field_1C = make_path(object, object->tile, sad_entry->field_24, sad_entry->rotations, 1);
                if (sad_entry->field_1C != 0) {
                    obj_move_to_tile(object, object->tile, object->elevation, &temp);
                    rect_min_bound(&dirty, &temp, &dirty);

                    obj_set_frame(object, 0, &temp);
                    rect_min_bound(&dirty, &temp, &dirty);

                    obj_set_rotation(object, sad_entry->rotations[0], &temp);
                    rect_min_bound(&dirty, &temp, &dirty);

                    sad_entry->field_20 = 0;
                } else {
                    sad_entry->field_20 = -1000;
                }
                v10 = -1;
            } else {
                obj_use_door(object, v12, 0);
            }
        }

        if (v10 != -1) {
            obj_move_to_tile(object, v10, object->elevation, &temp);
            rect_min_bound(&dirty, &temp, &dirty);

            int v17 = 0;
            if (isInCombat() && FID_TYPE(object->fid) == OBJ_TYPE_CRITTER) {
                int v18 = critter_compute_ap_from_distance(object, 1);
                if (combat_free_move < v18) {
                    int ap = object->data.critter.combat.ap;
                    int v20 = v18 - combat_free_move;
                    combat_free_move = 0;
                    if (v20 > ap) {
                        object->data.critter.combat.ap = 0;
                    } else {
                        object->data.critter.combat.ap = ap - v20;
                    }
                } else {
                    combat_free_move -= v18;
                }

                if (object == obj_dude) {
                    intface_update_move_points(obj_dude->data.critter.combat.ap, combat_free_move);
                }

                v17 = (object->data.critter.combat.ap + combat_free_move) <= 0;
            }

            sad_entry->field_20 += 1;

            if (sad_entry->field_20 == sad_entry->field_1C || v17) {
                sad_entry->field_20 = -1000;
            } else {
                obj_set_rotation(object, sad_entry->rotations[sad_entry->field_20], &temp);
                rect_min_bound(&dirty, &temp, &dirty);

                obj_offset(object, x, y, &temp);
                rect_min_bound(&dirty, &temp, &dirty);
            }
        }
    }

    tile_refresh_rect(&dirty, object->elevation);
    if (sad_entry->field_20 == -1000) {
        anim_set_continue(sad_entry->animationSequenceIndex, 1);
    }
}

// 0x417128
static void object_straight_move(int index)
{
    AnimationSad* sad_entry = &(sad[index]);
    Object* object = sad_entry->obj;

    Rect dirtyRect;
    Rect temp;

    if (sad_entry->field_20 == -2000) {
        obj_change_fid(object, sad_entry->fid, &dirtyRect);
        sad_entry->field_20 = 0;
    } else {
        obj_bound(object, &dirtyRect);
    }

    CacheEntry* cacheHandle;
    Art* art = art_ptr_lock(object->fid, &cacheHandle);
    if (art != NULL) {
        int lastFrame = art_frame_max_frame(art) - 1;
        art_ptr_unlock(cacheHandle);

        if ((sad_entry->flags & ANIM_SAD_NO_ANIM) == 0) {
            if ((sad_entry->flags & ANIM_SAD_WAIT_FOR_COMPLETION) == 0 || object->frame < lastFrame) {
                obj_inc_frame(object, &temp);
                rect_min_bound(&dirtyRect, &temp, &dirtyRect);
            }
        }

        if (sad_entry->field_20 < sad_entry->field_1C) {
            StraightPathNode* v12 = &(sad_entry->field_28[sad_entry->field_20]);

            obj_move_to_tile(object, v12->tile, v12->elevation, &temp);
            rect_min_bound(&dirtyRect, &temp, &dirtyRect);

            obj_offset(object, v12->x, v12->y, &temp);
            rect_min_bound(&dirtyRect, &temp, &dirtyRect);

            sad_entry->field_20++;
        }

        if (sad_entry->field_20 == sad_entry->field_1C) {
            if ((sad_entry->flags & ANIM_SAD_WAIT_FOR_COMPLETION) == 0 || object->frame == lastFrame) {
                sad_entry->field_20 = -1000;
            }
        }

        tile_refresh_rect(&dirtyRect, sad_entry->obj->elevation);

        if (sad_entry->field_20 == -1000) {
            anim_set_continue(sad_entry->animationSequenceIndex, 1);
        }
    }
}

// 0x417320
static int anim_animate(Object* obj, int anim, int animationSequenceIndex, int flags)
{
    if (curr_sad == ANIMATION_SAD_LIST_CAPACITY) {
        return -1;
    }

    AnimationSad* sad_entry = &(sad[curr_sad]);

    int fid;
    if (anim == ANIM_TAKE_OUT) {
        sad_entry->flags = 0;
        fid = art_id(FID_TYPE(obj->fid), obj->fid & 0xFFF, ANIM_TAKE_OUT, flags, obj->rotation + 1);
    } else {
        sad_entry->flags = flags;
        fid = art_id(FID_TYPE(obj->fid), obj->fid & 0xFFF, anim, (obj->fid & 0xF000) >> 12, obj->rotation + 1);
    }

    if (!art_exists(fid)) {
        return -1;
    }

    sad_entry->obj = obj;
    sad_entry->fid = fid;
    sad_entry->animationSequenceIndex = animationSequenceIndex;
    sad_entry->animationTimestamp = 0;
    sad_entry->ticksPerFrame = compute_tpf(obj, sad_entry->fid);
    sad_entry->field_20 = 0;
    sad_entry->field_1C = 0;

    curr_sad++;

    return 0;
}

// 0x417498
void object_animate()
{
    if (curr_sad == 0) {
        return;
    }

    anim_in_bk = 1;

    for (int index = 0; index < curr_sad; index++) {
        AnimationSad* sad_entry = &(sad[index]);
        if (sad_entry->field_20 == -1000) {
            continue;
        }

        Object* object = sad_entry->obj;

        unsigned int time = get_time();
        if (elapsed_tocks(time, sad_entry->animationTimestamp) < sad_entry->ticksPerFrame) {
            continue;
        }

        sad_entry->animationTimestamp = time;

        if (anim_set_check(sad_entry->animationSequenceIndex) == -1) {
            continue;
        }

        if (sad_entry->field_1C > 0) {
            if ((sad_entry->flags & ANIM_SAD_STRAIGHT) != 0) {
                object_straight_move(index);
            } else {
                int savedTile = object->tile;
                object_move(index);
                if (savedTile != object->tile) {
                    scr_chk_spatials_in(object, object->tile, object->elevation);
                }
            }
            continue;
        }

        if (sad_entry->field_20 == 0) {
            for (int index = 0; index < curr_sad; index++) {
                AnimationSad* otherSad = &(sad[index]);
                if (object == otherSad->obj && otherSad->field_20 == -2000) {
                    otherSad->field_20 = -1000;
                    anim_set_continue(otherSad->animationSequenceIndex, 1);
                }
            }
            sad_entry->field_20 = -2000;
        }

        Rect dirtyRect;
        Rect tempRect;

        obj_bound(object, &dirtyRect);

        if (object->fid == sad_entry->fid) {
            if ((sad_entry->flags & ANIM_SAD_REVERSE) == 0) {
                CacheEntry* cacheHandle;
                Art* art = art_ptr_lock(object->fid, &cacheHandle);
                if (art != NULL) {
                    if ((sad_entry->flags & ANIM_SAD_FOREVER) == 0 && object->frame == art_frame_max_frame(art) - 1) {
                        sad_entry->field_20 = -1000;
                        art_ptr_unlock(cacheHandle);

                        if ((sad_entry->flags & ANIM_SAD_HIDE_ON_END) != 0) {
                            // NOTE: Uninline.
                            anim_hide(object, -1);
                        }

                        anim_set_continue(sad_entry->animationSequenceIndex, 1);
                        continue;
                    } else {
                        obj_inc_frame(object, &tempRect);
                        rect_min_bound(&dirtyRect, &tempRect, &dirtyRect);

                        int frameX;
                        int frameY;
                        art_frame_hot(art, object->frame, object->rotation, &frameX, &frameY);

                        obj_offset(object, frameX, frameY, &tempRect);
                        rect_min_bound(&dirtyRect, &tempRect, &dirtyRect);

                        art_ptr_unlock(cacheHandle);
                    }
                }

                tile_refresh_rect(&dirtyRect, map_elevation);

                continue;
            }

            if ((sad_entry->flags & ANIM_SAD_FOREVER) != 0 || object->frame != 0) {
                int x;
                int y;

                CacheEntry* cacheHandle;
                Art* art = art_ptr_lock(object->fid, &cacheHandle);
                if (art != NULL) {
                    art_frame_hot(art, object->frame, object->rotation, &x, &y);
                    art_ptr_unlock(cacheHandle);
                }

                obj_dec_frame(object, &tempRect);
                rect_min_bound(&dirtyRect, &tempRect, &dirtyRect);

                obj_offset(object, -x, -y, &tempRect);
                rect_min_bound(&dirtyRect, &tempRect, &dirtyRect);

                tile_refresh_rect(&dirtyRect, map_elevation);
                continue;
            }

            sad_entry->field_20 = -1000;
            anim_set_continue(sad_entry->animationSequenceIndex, 1);
        } else {
            int x;
            int y;

            CacheEntry* cacheHandle;
            Art* art = art_ptr_lock(object->fid, &cacheHandle);
            if (art != NULL) {
                art_frame_offset(art, object->rotation, &x, &y);
                art_ptr_unlock(cacheHandle);
            } else {
                x = 0;
                y = 0;
            }

            Rect v29;
            obj_change_fid(object, sad_entry->fid, &v29);
            rect_min_bound(&dirtyRect, &v29, &dirtyRect);

            art = art_ptr_lock(object->fid, &cacheHandle);
            if (art != NULL) {
                int frame;
                if ((sad_entry->flags & ANIM_SAD_REVERSE) != 0) {
                    frame = art_frame_max_frame(art) - 1;
                } else {
                    frame = 0;
                }

                obj_set_frame(object, frame, &v29);
                rect_min_bound(&dirtyRect, &v29, &dirtyRect);

                int frameX;
                int frameY;
                art_frame_hot(art, object->frame, object->rotation, &frameX, &frameY);

                Rect v19;
                obj_offset(object, x + frameX, y + frameY, &v19);
                rect_min_bound(&dirtyRect, &v19, &dirtyRect);

                art_ptr_unlock(cacheHandle);
            } else {
                obj_set_frame(object, 0, &v29);
                rect_min_bound(&dirtyRect, &v29, &dirtyRect);
            }

            tile_refresh_rect(&dirtyRect, map_elevation);
        }
    }

    anim_in_bk = 0;

    object_anim_compact();
}

// 0x417880
static void object_anim_compact()
{
    for (int index = 0; index < ANIMATION_SEQUENCE_LIST_CAPACITY; index++) {
        AnimationSequence* animationSequence = &(anim_set[index]);
        if ((animationSequence->flags & ANIM_SEQ_0x20) != 0) {
            animationSequence->flags = 0;
        }
    }

    int index = 0;
    for (; index < curr_sad; index++) {
        if (sad[index].field_20 == -1000) {
            int v2 = index + 1;
            for (; v2 < curr_sad; v2++) {
                if (sad[v2].field_20 != -1000) {
                    break;
                }
            }

            if (v2 == curr_sad) {
                break;
            }

            if (index != v2) {
                memcpy(&(sad[index]), &(sad[v2]), sizeof(AnimationSad));
                sad[v2].field_20 = -1000;
                sad[v2].flags = 0;
            }
        }
    }
    curr_sad = index;
}

// 0x417964
int check_move(int* a1)
{
    int x;
    int y;
    mouse_get_position(&x, &y);

    int tile = tile_num(x, y, map_elevation);
    if (tile == -1) {
        return -1;
    }

    if (isInCombat()) {
        if (*a1 != -1) {
            if (keys[SDL_SCANCODE_RCTRL] || keys[SDL_SCANCODE_LCTRL]) {
                int hitMode;
                bool aiming;
                intface_get_attack(&hitMode, &aiming);

                int v6 = item_mp_cost(obj_dude, hitMode, aiming);
                *a1 = *a1 - v6;
                if (*a1 <= 0) {
                    return -1;
                }
            }
        }
    } else {
        bool interruptWalk;
        configGetBool(&game_config, GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_INTERRUPT_WALK_KEY, &interruptWalk);
        if (interruptWalk) {
            register_clear(obj_dude);
        }
    }

    return tile;
}

// 0x417A1C
int dude_move(int action_points)
{
    static int lastDestination = -2;

    int dest;

    dest = check_move(&action_points);
    if (dest == -1) {
        return -1;
    }

    if (lastDestination == dest) {
        return dude_run(action_points);
    }

    lastDestination = dest;

    register_begin(ANIMATION_REQUEST_RESERVED);
    register_object_move_to_tile(obj_dude, dest, obj_dude->elevation, action_points, 0);
    return register_end();
}

// 0x417A5C
int dude_run(int action_points)
{
    int dest;

    dest = check_move(&action_points);
    if (dest == -1) {
        return -1;
    }

    if (!perk_level(PERK_SILENT_RUNNING)) {
        pc_flag_off(PC_FLAG_SNEAKING);
    }

    register_begin(ANIMATION_REQUEST_RESERVED);
    register_object_run_to_tile(obj_dude, dest, obj_dude->elevation, action_points, 0);
    return register_end();
}

// 0x417AB0
void dude_fidget()
{
    // 0x4FEAAC
    static unsigned int last_time = 0;

    // 0x4FEAB0
    static unsigned int next_time = 0;

    // 0x56B570
    static Object* fidget_ptr[100];

    if (game_user_wants_to_quit != 0) {
        return;
    }

    if (isInCombat()) {
        return;
    }

    if (vcr_status() != VCR_STATE_TURNED_OFF) {
        return;
    }

    if ((obj_dude->flags & OBJECT_HIDDEN) != 0) {
        return;
    }

    unsigned int now = get_bk_time();
    if (elapsed_tocks(now, last_time) <= next_time) {
        return;
    }

    last_time = now;

    int count = 0;
    Object* object = obj_find_first_at(obj_dude->elevation);
    while (object != NULL) {
        if (count >= 100) {
            break;
        }

        if ((object->flags & OBJECT_HIDDEN) == 0 && FID_TYPE(object->fid) == OBJ_TYPE_CRITTER && FID_ANIM_TYPE(object->fid) == ANIM_STAND && !critter_is_dead(object)) {
            Rect rect;
            obj_bound(object, &rect);

            Rect intersection;
            if (rect_inside_bound(&rect, &scr_size, &intersection) == 0) {
                fidget_ptr[count++] = object;
            }
        }

        object = obj_find_next_at();
    }

    int v13;
    if (count != 0) {
        int r = roll_random(0, count - 1);
        Object* object = fidget_ptr[r];

        register_begin(ANIMATION_REQUEST_UNRESERVED | ANIMATION_REQUEST_INSIGNIFICANT);

        bool v8 = false;
        if (object == obj_dude) {
            v8 = true;
        } else {
            char v15[16];
            v15[0] = '\0';
            art_get_base_name(1, object->fid & 0xFFF, v15);
            if (v15[0] == 'm' || v15[0] == 'M') {
                if (obj_dist(object, obj_dude) < stat_level(obj_dude, STAT_PERCEPTION) * 2) {
                    v8 = true;
                }
            }
        }

        if (v8) {
            const char* sfx = gsnd_build_character_sfx_name(object, ANIM_STAND, CHARACTER_SOUND_EFFECT_UNUSED);
            register_object_play_sfx(object, sfx, 0);
        }

        register_object_animate(object, ANIM_STAND, 0);
        register_end();

        v13 = 20 / count;
    } else {
        v13 = 7;
    }

    if (v13 < 1) {
        v13 = 1;
    } else if (v13 > 7) {
        v13 = 7;
    }

    next_time = roll_random(0, 3000) + 1000 * v13;
}

// 0x417CB0
void dude_stand(Object* obj, int rotation, int fid)
{
    Rect rect;

    obj_set_rotation(obj, rotation, &rect);

    int x = 0;
    int y = 0;

    int weaponAnimationCode = (obj->fid & 0xF000) >> 12;
    if (weaponAnimationCode != 0) {
        if (fid == -1) {
            int takeOutFid = art_id(FID_TYPE(obj->fid), obj->fid & 0xFFF, ANIM_TAKE_OUT, weaponAnimationCode, obj->rotation + 1);
            CacheEntry* takeOutFrmHandle;
            Art* takeOutFrm = art_ptr_lock(takeOutFid, &takeOutFrmHandle);
            if (takeOutFrm != NULL) {
                int frameCount = art_frame_max_frame(takeOutFrm);
                for (int frame = 0; frame < frameCount; frame++) {
                    int offsetX;
                    int offsetY;
                    art_frame_hot(takeOutFrm, frame, obj->rotation, &offsetX, &offsetY);
                    x += offsetX;
                    y += offsetY;
                }
                art_ptr_unlock(takeOutFrmHandle);

                CacheEntry* standFrmHandle;
                int standFid = art_id(FID_TYPE(obj->fid), obj->fid & 0xFFF, ANIM_STAND, 0, obj->rotation + 1);
                Art* standFrm = art_ptr_lock(standFid, &standFrmHandle);
                if (standFrm != NULL) {
                    int offsetX;
                    int offsetY;
                    if (art_frame_offset(standFrm, obj->rotation, &offsetX, &offsetY) == 0) {
                        x += offsetX;
                        y += offsetY;
                    }
                    art_ptr_unlock(standFrmHandle);
                }
            }
        }
    }

    if (fid == -1) {
        int anim;
        if (FID_ANIM_TYPE(obj->fid) == ANIM_FIRE_DANCE) {
            anim = ANIM_FIRE_DANCE;
        } else {
            anim = ANIM_STAND;
        }
        fid = art_id(FID_TYPE(obj->fid), (obj->fid & 0xFFF), anim, (obj->fid & 0xF000) >> 12, obj->rotation + 1);
    }

    Rect temp;
    obj_change_fid(obj, fid, &temp);
    rect_min_bound(&rect, &temp, &rect);

    obj_move_to_tile(obj, obj->tile, obj->elevation, &temp);
    rect_min_bound(&rect, &temp, &rect);

    obj_set_frame(obj, 0, &temp);
    rect_min_bound(&rect, &temp, &rect);

    obj_offset(obj, x, y, &temp);
    rect_min_bound(&rect, &temp, &rect);

    tile_refresh_rect(&rect, obj->elevation);
}

// 0x417EAC
void dude_standup(Object* a1)
{
    register_begin(ANIMATION_REQUEST_RESERVED);

    int anim;
    if (FID_ANIM_TYPE(a1->fid) == ANIM_FALL_BACK) {
        anim = ANIM_BACK_TO_STANDING;
    } else {
        anim = ANIM_PRONE_TO_STANDING;
    }

    register_object_animate(a1, anim, 0);
    register_end();
    a1->data.critter.combat.results &= ~DAM_KNOCKED_DOWN;
}

// 0x417EF0
static int anim_turn_towards(Object* obj, int delta, int animationSequenceIndex)
{
    if (!critter_is_prone(obj)) {
        int rotation = obj->rotation + delta;
        if (rotation >= ROTATION_COUNT) {
            rotation = ROTATION_NE;
        } else if (rotation < 0) {
            rotation = ROTATION_NW;
        }

        dude_stand(obj, rotation, -1);
    }

    anim_set_continue(animationSequenceIndex, 0);

    return 0;
}

// 0x417F64
int anim_hide(Object* object, int animationSequenceIndex)
{
    Rect rect;

    if (obj_turn_off(object, &rect) == 0) {
        tile_refresh_rect(&rect, object->elevation);
    }

    if (animationSequenceIndex != -1) {
        anim_set_continue(animationSequenceIndex, 0);
    }

    return 0;
}

// 0x417F98
int anim_change_fid(Object* obj, int animationSequenceIndex, int fid)
{
    Rect rect;
    Rect v7;

    if (FID_ANIM_TYPE(fid)) {
        obj_change_fid(obj, fid, &rect);
        obj_set_frame(obj, 0, &v7);
        rect_min_bound(&rect, &v7, &rect);
        tile_refresh_rect(&rect, obj->elevation);
    } else {
        dude_stand(obj, obj->rotation, fid);
    }

    anim_set_continue(animationSequenceIndex, 0);

    return 0;
}

// 0x418004
void anim_stop()
{
    int index;

    anim_in_anim_stop = true;
    curr_anim_set = -1;

    for (index = 0; index < ANIMATION_SEQUENCE_LIST_CAPACITY; index++) {
        anim_set_end(index);
    }

    anim_in_anim_stop = false;
    curr_sad = 0;
}

// 0x418040
static int check_gravity(int tile, int elevation)
{
    for (; elevation > 0; elevation--) {
        int x;
        int y;
        tile_coord(tile, &x, &y, elevation);

        int squareTile = square_num(x + 2, y + 8, elevation);
        int fid = art_id(OBJ_TYPE_TILE, square[elevation]->field_0[squareTile] & 0xFFF, 0, 0, 0);
        if (fid != art_id(OBJ_TYPE_TILE, 1, 0, 0, 0)) {
            break;
        }
    }
    return elevation;
}

// 0x4180CC
unsigned int compute_tpf(Object* object, int fid)
{
    int fps;

    CacheEntry* handle;
    Art* frm = art_ptr_lock(fid, &handle);
    if (frm != NULL) {
        fps = art_frame_fps(frm);
        art_ptr_unlock(handle);
    } else {
        fps = 10;
    }

    if (isInCombat()) {
        if (FID_ANIM_TYPE(fid) == ANIM_WALK) {
            int playerSpeedup = 0;
            config_get_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_PLAYER_SPEEDUP_KEY, &playerSpeedup);

            if (object != obj_dude || playerSpeedup == 1) {
                int combatSpeed = 0;
                config_get_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_COMBAT_SPEED_KEY, &combatSpeed);
                fps += combatSpeed;
            }
        }
    }

    return 1000 / fps;
}

} // namespace fallout
