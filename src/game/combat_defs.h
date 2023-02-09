#ifndef FALLOUT_GAME_COMBAT_DEFS_H_
#define FALLOUT_GAME_COMBAT_DEFS_H_

#include "game/object_types.h"

namespace fallout {

#define EXPLOSION_TARGET_COUNT (6)

#define CRTICIAL_EFFECT_COUNT (6)

#define WEAPON_CRITICAL_FAILURE_TYPE_COUNT (7)
#define WEAPON_CRITICAL_FAILURE_EFFECT_COUNT (5)

typedef enum CombatState {
    COMBAT_STATE_0x01 = 0x01,
    COMBAT_STATE_0x02 = 0x02,
    COMBAT_STATE_0x08 = 0x08,
} CombatState;

typedef enum HitMode {
    HIT_MODE_LEFT_WEAPON_PRIMARY = 0,
    HIT_MODE_LEFT_WEAPON_SECONDARY = 1,
    HIT_MODE_RIGHT_WEAPON_PRIMARY = 2,
    HIT_MODE_RIGHT_WEAPON_SECONDARY = 3,
    HIT_MODE_PUNCH = 4,
    HIT_MODE_KICK = 5,
    HIT_MODE_LEFT_WEAPON_RELOAD = 6,
    HIT_MODE_RIGHT_WEAPON_RELOAD = 7,

    // Punch Level 2
    HIT_MODE_STRONG_PUNCH = 8,

    // Punch Level 3
    HIT_MODE_HAMMER_PUNCH = 9,

    // Punch Level 4 aka 'Lightning Punch'
    HIT_MODE_HAYMAKER = 10,

    // Punch Level 5 aka 'Chop Punch'
    HIT_MODE_JAB = 11,

    // Punch Level 6 aka 'Dragon Punch'
    HIT_MODE_PALM_STRIKE = 12,

    // Punch Level 7 aka 'Force Punch'
    HIT_MODE_PIERCING_STRIKE = 13,

    // Kick Level 2
    HIT_MODE_STRONG_KICK = 14,

    // Kick Level 3
    HIT_MODE_SNAP_KICK = 15,

    // Kick Level 4 aka 'Roundhouse Kick'
    HIT_MODE_POWER_KICK = 16,

    // Kick Level 5
    HIT_MODE_HIP_KICK = 17,

    // Kick Level 6 aka 'Jump Kick'
    HIT_MODE_HOOK_KICK = 18,

    // Kick Level 7 aka 'Death Blossom Kick'
    HIT_MODE_PIERCING_KICK = 19,
    HIT_MODE_COUNT,
    FIRST_ADVANCED_PUNCH_HIT_MODE = HIT_MODE_STRONG_PUNCH,
    LAST_ADVANCED_PUNCH_HIT_MODE = HIT_MODE_PIERCING_STRIKE,
    FIRST_ADVANCED_KICK_HIT_MODE = HIT_MODE_STRONG_KICK,
    LAST_ADVANCED_KICK_HIT_MODE = HIT_MODE_PIERCING_KICK,
    FIRST_ADVANCED_UNARMED_HIT_MODE = FIRST_ADVANCED_PUNCH_HIT_MODE,
    LAST_ADVANCED_UNARMED_HIT_MODE = LAST_ADVANCED_KICK_HIT_MODE,
} HitMode;

typedef enum HitLocation {
    HIT_LOCATION_HEAD,
    HIT_LOCATION_LEFT_ARM,
    HIT_LOCATION_RIGHT_ARM,
    HIT_LOCATION_TORSO,
    HIT_LOCATION_RIGHT_LEG,
    HIT_LOCATION_LEFT_LEG,
    HIT_LOCATION_EYES,
    HIT_LOCATION_GROIN,
    HIT_LOCATION_UNCALLED,
    HIT_LOCATION_COUNT,
    HIT_LOCATION_SPECIFIC_COUNT = HIT_LOCATION_COUNT - 1,
} HitLocation;

typedef struct STRUCT_664980 {
    Object* attacker;
    Object* defender;
    int actionPointsBonus;
    int accuracyBonus;
    int damageBonus;
    int minDamage;
    int maxDamage;
    int field_1C; // probably bool, indicating field_20 and field_24 used
    int field_20; // flags on attacker
    int field_24; // flags on defender
} STRUCT_664980;

typedef struct Attack {
    Object* attacker;
    int hitMode;
    Object* weapon;
    int attackHitLocation;
    int attackerDamage;
    int attackerFlags;
    int ammoQuantity;
    int criticalMessageId;
    Object* defender;
    int tile;
    int defenderHitLocation;
    int defenderDamage;
    int defenderFlags;
    int defenderKnockback;
    Object* oops;
    int extrasLength;
    Object* extras[EXPLOSION_TARGET_COUNT];
    int extrasHitLocation[EXPLOSION_TARGET_COUNT];
    int extrasDamage[EXPLOSION_TARGET_COUNT];
    int extrasFlags[EXPLOSION_TARGET_COUNT];
    int extrasKnockback[EXPLOSION_TARGET_COUNT];
} Attack;

// Provides metadata about critical hit effect.
typedef struct CriticalHitDescription {
    int damageMultiplier;

    // Damage flags that will be applied to defender.
    int flags;

    // Stat to check to upgrade this critical hit to massive critical hit or
    // -1 if there is no massive critical hit.
    int massiveCriticalStat;

    // Bonus/penalty to massive critical stat.
    int massiveCriticalStatModifier;

    // Additional damage flags if this critical hit become massive critical.
    int massiveCriticalFlags;

    int messageId;
    int massiveCriticalMessageId;
} CriticalHitDescription;

typedef enum CombatBadShot {
    COMBAT_BAD_SHOT_OK = 0,
    COMBAT_BAD_SHOT_NO_AMMO = 1,
    COMBAT_BAD_SHOT_OUT_OF_RANGE = 2,
    COMBAT_BAD_SHOT_NOT_ENOUGH_AP = 3,
    COMBAT_BAD_SHOT_ALREADY_DEAD = 4,
    COMBAT_BAD_SHOT_AIM_BLOCKED = 5,
    COMBAT_BAD_SHOT_ARM_CRIPPLED = 6,
    COMBAT_BAD_SHOT_BOTH_ARMS_CRIPPLED = 7,
} CombatBadShot;

} // namespace fallout

#endif /* FALLOUT_GAME_COMBAT_DEFS_H_ */
