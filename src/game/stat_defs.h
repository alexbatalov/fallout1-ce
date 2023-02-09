#ifndef FALLOUT_GAME_STAT_DEFS_H_
#define FALLOUT_GAME_STAT_DEFS_H_

namespace fallout {

// The minimum value of SPECIAL stat.
#define PRIMARY_STAT_MIN (1)

// The maximum value of SPECIAL stat.
#define PRIMARY_STAT_MAX (10)

// The number of values of SPECIAL stat.
//
// Every stat value has it's own human readable description. This value is used
// as number of these descriptions.
#define PRIMARY_STAT_RANGE ((PRIMARY_STAT_MAX) - (PRIMARY_STAT_MIN) + 1)

// The maximum number of PC level.
#define PC_LEVEL_MAX 21

// Available stats.
typedef enum Stat {
    STAT_STRENGTH,
    STAT_PERCEPTION,
    STAT_ENDURANCE,
    STAT_CHARISMA,
    STAT_INTELLIGENCE,
    STAT_AGILITY,
    STAT_LUCK,
    STAT_MAXIMUM_HIT_POINTS,
    STAT_MAXIMUM_ACTION_POINTS,
    STAT_ARMOR_CLASS,
    STAT_UNARMED_DAMAGE,
    STAT_MELEE_DAMAGE,
    STAT_CARRY_WEIGHT,
    STAT_SEQUENCE,
    STAT_HEALING_RATE,
    STAT_CRITICAL_CHANCE,
    STAT_BETTER_CRITICALS,
    STAT_DAMAGE_THRESHOLD,
    STAT_DAMAGE_THRESHOLD_LASER,
    STAT_DAMAGE_THRESHOLD_FIRE,
    STAT_DAMAGE_THRESHOLD_PLASMA,
    STAT_DAMAGE_THRESHOLD_ELECTRICAL,
    STAT_DAMAGE_THRESHOLD_EMP,
    STAT_DAMAGE_THRESHOLD_EXPLOSION,
    STAT_DAMAGE_RESISTANCE,
    STAT_DAMAGE_RESISTANCE_LASER,
    STAT_DAMAGE_RESISTANCE_FIRE,
    STAT_DAMAGE_RESISTANCE_PLASMA,
    STAT_DAMAGE_RESISTANCE_ELECTRICAL,
    STAT_DAMAGE_RESISTANCE_EMP,
    STAT_DAMAGE_RESISTANCE_EXPLOSION,
    STAT_RADIATION_RESISTANCE,
    STAT_POISON_RESISTANCE,
    STAT_AGE,
    STAT_GENDER,
    STAT_CURRENT_HIT_POINTS,
    STAT_CURRENT_POISON_LEVEL,
    STAT_CURRENT_RADIATION_LEVEL,
    STAT_COUNT,

    // Number of primary stats.
    PRIMARY_STAT_COUNT = 7,

    // Number of SPECIAL stats (primary + secondary).
    SPECIAL_STAT_COUNT = 33,

    // Number of saveable stats (i.e. excluding CURRENT pseudostats).
    SAVEABLE_STAT_COUNT = 35,
} Stat;

#define STAT_INVALID -1

// Special stats that are only relevant to player character.
typedef enum PcStat {
    PC_STAT_UNSPENT_SKILL_POINTS,
    PC_STAT_LEVEL,
    PC_STAT_EXPERIENCE,
    PC_STAT_REPUTATION,
    PC_STAT_KARMA,
    PC_STAT_COUNT,
} PcStat;

} // namespace fallout

#endif /* FALLOUT_GAME_STAT_DEFS_H_ */
