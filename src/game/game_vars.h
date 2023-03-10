#ifndef FALLOUT_GAME_GAME_VARS_H_
#define FALLOUT_GAME_GAME_VARS_H_

namespace fallout {

typedef enum GameGlobalVar {
    GVAR_CHILD_KILLER_SHADY,
    GVAR_CHILD_KILLER_HUB,
    GVAR_NUM_RADSCORPIONS,
    GVAR_CHILD_KILLER_ADYTUM,
    GVAR_DOGS_KILLED,
    GVAR_DOG_EMPATHY,
    GVAR_QUEST_PEOPLE,
    GVAR_FOLLOWERS_INVADED,
    GVAR_MAP_ENTRANCE_NUMBER,
    GVAR_DAYS_TO_VAULT13_DISCOVERY,
    GVAR_VAULT_WATER,
    GVAR_VAULT_INVADED,
    GVAR_SHADY_SANDS_INVADED,
    GVAR_NECROPOLIS_INVADED,
    GVAR_HUB_INVADED,
    GVAR_JUNKTOWN_INVADED,
    GVAR_BROTHERHOOD_INVADED,
    GVAR_VATS_BLOWN,
    GVAR_MASTER_BLOWN,
    GVAR_NECROPOLIS_VISITED,
    GVAR_NECROPOLIS_KNOWN,
    GVAR_MOTEL_OF_DOOM_VISITED,
    GVAR_HALL_OF_THE_DEAD_VISITED,
    GVAR_WATERSHED_VISITED,
    GVAR_NECROPOLIS_VAULT_VISITED,
    GVAR_TANDI_X,
    GVAR_TANDI_STATUS,
    GVAR_TANDI_DEAD,
    GVAR_RADSCORPIONS_KILLED,
    GVAR_NECROP_MUTANTS_KILLED,
    GVAR_NECROP_WATER_CHIP_TAKEN,
    GVAR_NECROP_WATER_PUMP_FIXED,
    GVAR_LOAD_MAP_INDEX,
    GVAR_ANIMALS_KILLED,
    GVAR_HUMANS_KILLED,
    GVAR_SUPER_MUTANTS_KILLED,
    GVAR_HIRED_BY_KILLIAN,
    GVAR_KILLIAN_DEAD,
    GVAR_GIZMO_DEAD,
    GVAR_HIRED_BY_GIZMO,
    GVAR_IN_JAIL,
    GVAR_BUG_PLANTED,
    GVAR_GOT_CONFESSION,
    GVAR_RADSCORPION_SEED,
    GVAR_DEATHCLAW_SEED,
    GVAR_BROTHERHOOD_SEED,
    GVAR_HUB_SEED,
    GVAR_PLAYER_LOCATION,
    GVAR_LOXLEY_X,
    GVAR_MAXSON_X,
    GVAR_CATHEDRAL,
    GVAR_VATS_STATUS,
    GVAR_CHILDREN_STATUS,
    GVAR_MASTER_DEAD,
    GVAR_LIEUTENANTS_DEAD,
    GVAR_COUNTDOWN_TO_DESTRUCTION,
    GVAR_BOMB_DISARMED,
    GVAR_ENTERING_VATS_HOW,
    GVAR_WATER_SHED_STATUS,
    GVAR_PLAYER_CAPTURED,
    GVAR_SIGNAL_REWARD,
    GVAR_SHADOW_PASSWORD,
    GVAR_FOLLOWER_STEALTH_HELP,
    GVAR_FOLLOWER_MACHO_HELP,
    GVAR_KNIGHT_WARNING,
    GVAR_WORLD_TERRAIN,
    GVAR_NECROP_WATER,
    GVAR_VAULT_13,
    GVAR_SHADY_SANDS,
    GVAR_RAIDERS,
    GVAR_BURIED_VAULT,
    GVAR_JUNKTOWN,
    GVAR_NECROPOLIS,
    GVAR_THE_HUB,
    GVAR_BROTHERHOOD_OF_STEEL,
    GVAR_ANGELS_BONEYARD,
    GVAR_THE_GLOW,
    GVAR_CHILDREN_OF_CATHEDRAL,
    GVAR_THE_VATS,
    GVAR_MASTERS_LAIR,
    GVAR_BROTHERHOOD_12,
    GVAR_BROTHERHOOD_34,
    GVAR_BROTHERHOOD_ENTRANCE,
    GVAR_CAVES,
    GVAR_CHILDREN_1,
    GVAR_DESERT_1,
    GVAR_DESERT_2,
    GVAR_DESERT_3,
    GVAR_HALL_OF_THE_DEAD,
    GVAR_HOTEL_OF_DOOM,
    GVAR_JUNKTOWN_CASINO,
    GVAR_JUNKTOWN_ENTRANCE,
    GVAR_JUNKTOWN_KILLIAN,
    GVAR_SHADY_SANDS_EAST,
    GVAR_SHADY_SANDS_WEST,
    GVAR_VAULT_13_MAP,
    GVAR_BURIED_VAULT_MAP,
    GVAR_VAULT_ENTRANCE,
    GVAR_NECROP_VAULT,
    GVAR_WATERSHED,
    GVAR_KILL_DEATHCLAW,
    GVAR_FIND_WATER_CHIP,
    GVAR_KILL_RADSCORPIONS,
    GVAR_RESCUE_TANDI,
    GVAR_CAPTURE_GIZMO,
    GVAR_KILL_KILLIAN,
    GVAR_MISSING_CARAVAN,
    GVAR_STEAL_NECKLACE,
    GVAR_BECOME_AN_INITIATE,
    GVAR_FIND_LOST_INITIATE,
    GVAR_WARNED_BROTHERHOOD,
    GVAR_KILL_MERCHANT,
    GVAR_KILL_JAIN,
    GVAR_KILL_SUPER_MUTANTS,
    GVAR_GARL_DEAD,
    GVAR_TOTAL_RAIDERS,
    GVAR_MISTAKEN_ID,
    GVAR_WITH_CARAVAN,
    GVAR_IAN_STATUS,
    GVAR_TOLYA_STATUS,
    GVAR_PETROX_STATUS,
    GVAR_HERNANDEZ_STATUS,
    GVAR_KENJI_STATUS,
    GVAR_HUNTER_STATUS,
    GVAR_SETH_STATUS,
    GVAR_TRENT_STATUS,
    GVAR_JASON_STATUS,
    GVAR_ROMEO_JULIET,
    GVAR_GANG_WAR,
    GVAR_DESTROY_FOLLOWERS,
    GVAR_LOST_BROTHER,
    GVAR_BLADES_HELP,
    GVAR_TRAIN_FOLLOWERS,
    GVAR_FIND_AGENT,
    GVAR_TANGLER_DEAD,
    GVAR_BECOME_BLADE,
    GVAR_BLADES_LEFT,
    GVAR_RIPPERS_LEFT,
    GVAR_FIX_FARM,
    GVAR_START_POWER,
    GVAR_WEAPONS_ARMED,
    GVAR_FOUND_DISK,
    GVAR_WEAPON_LOCKER,
    GVAR_SAVE_SINTHIA,
    GVAR_SAUL_QUEST,
    GVAR_TRISH_QUEST,
    GVAR_VATS_ALERT,
    GVAR_VATS_COUNTDOWN,
    GVAR_FOLLOWERS_INVADED_DATE,
    GVAR_NECROPOLIS_INVADED_DATE,
    GVAR_THE_HUB_INVADED_DATE,
    GVAR_BROTHERHOOD_INVADED_DATE,
    GVAR_JUNKTOWN_INVADED_DATE,
    GVAR_SHADY_SANDS_INVADED_DATE,
    GVAR_VAULT_13_INVADED_DATE,
    GVAR_PLAYER_REPUATION,
    GVAR_BERSERKER_REPUTATION,
    GVAR_CHAMPION_REPUTATION,
    GVAR_CHILDKILLER_REPUATION,
    GVAR_GOOD_MONSTER,
    GVAR_BAD_MONSTER,
    GVAR_ARTIFACT_DISK,
    GVAR_FEV_DISK,
    GVAR_SECURITY_DISK,
    GVAR_ALPHA_DISK,
    GVAR_DELTA_DISK,
    GVAR_VREE_DISK,
    GVAR_HONOR_DISK,
    GVAR_RENT_TIME,
    GVAR_SAUL_STATUS,
    GVAR_GIZMO_STATUS,
    GVAR_GANGS,
    GVAR_GANG_BEGONE,
    GVAR_GANG_1,
    GVAR_GANG_2,
    GVAR_FOOLS_SCOUT,
    GVAR_FSCOUT_1,
    GVAR_CRYPTS_SCOUT,
    GVAR_CSCOUT_1,
    GVAR_POWER,
    GVAR_POWER_GENERATOR,
    GVAR_GENERATOR_1,
    GVAR_GENERATOR_2,
    GVAR_GENERATOR_3,
    GVAR_PEASANTS,
    GVAR_DOG_PHIL,
    GVAR_DOG_1,
    GVAR_DOG_2,
    GVAR_WATER_THIEF,
    GVAR_NUKA_COLA_ADDICT,
    GVAR_BUFF_OUT_ADDICT,
    GVAR_MENTATS_ADDICT,
    GVAR_PSYCHO_ADDICT,
    GVAR_RADAWAY_ADDICT,
    GVAR_ALCOHOL_ADDICT,
    GVAR_CATHEDRAL_ENEMY,
    GVAR_MORPHEUS_KNOWN,
    GVAR_KNOW_NIGHTKIN,
    GVAR_PC_WANTED,
    GVAR_CRIMSON_CARAVANS_STATUS,
    GVAR_WATER_MERCHANTS_STATUS,
    GVAR_FARGO_TRADERS_STATUS,
    GVAR_UNDERGROUND_STATUS,
    GVAR_DECKER_STATUS,
    GVAR_CC_JOB,
    GVAR_WATER_JOB,
    GVAR_FARGO_JOB,
    GVAR_Loxley_known,
    GVAR_MARK_DEATHCLAW,
    GVAR_MUTANT_DISK,
    GVAR_BROTHER_HISTORY,
    GVAR_SOPHIA_DISK,
    GVAR_MAXSON_DISK,
    GVAR_TANDI_ESCAPE,
    GVAR_RAID_LOOTING,
    GVAR_CVAN_DRIVER,
    GVAR_CVAN_GUARD,
    GVAR_TANDI_HEREBEFORE,
    GVAR_TALKED_ABOUT_TANDI,
    GVAR_DECKER_KNOWN,
    GVAR_WANTED_FOR_MURDER,
    GVAR_GREENE_DEAD,
    GVAR_WANTED_THEFT,
    GVAR_BROTHERHOOD_INVASION,
    GVAR_GLOW_POWER,
    GVAR_HUB_FILLER_28,
    GVAR_HUB_FILLER_29,
    GVAR_HUB_FILLER_30,
    GVAR_NUM_RADIO,
    GVAR_RADIO_MISTAKE,
    GVAR_CHANNEL_SLOT_DOWN,
    GVAR_MASTER_FILLER_4,
    GVAR_MASTER_FILLER_5,
    GVAR_MASTER_FILLER_6,
    GVAR_MASTER_FILLER_7,
    GVAR_MASTER_FILLER_8,
    GVAR_MASTER_FILLER_9,
    GVAR_MASTER_FILLER_10,
    GVAR_CALM_REBELS,
    GVAR_CURE_JARVIS,
    GVAR_MAKE_ANTIDOTE,
    GVAR_MORPHEUS_DELIVERS_PLAYER,
    GVAR_DESTROY_VATS,
    GVAR_DESTROY_MASTER,
    GVAR_KATJA_STATUS,
    GVAR_ENEMY_VAULT_13,
    GVAR_ENEMY_SHADY_SANDS,
    GVAR_ENEMY_JUNKTOWN,
    GVAR_ENEMY_HUB,
    GVAR_ENEMY_NECROPOLIS,
    GVAR_ENEMY_BROTHERHOOD,
    GVAR_ENEMY_ADYTUM,
    GVAR_ENEMY_RIPPERS,
    GVAR_ENEMY_BLADES,
    GVAR_ENEMY_RAIDERS,
    GVAR_ENEMY_CATHEDRAL,
    GVAR_ENEMY_FOLLOWERS,
    GVAR_GENERIC_FILLER_20,
    GVAR_WATER_CHIP_1,
    GVAR_WATER_CHIP_2,
    GVAR_WATER_CHIP_3,
    GVAR_WATER_CHIP_4,
    GVAR_WATER_CHIP_5,
    GVAR_WATER_CHIP_6,
    GVAR_WATER_CHIP_7,
    GVAR_WATER_CHIP_8,
    GVAR_WATER_CHIP_9,
    GVAR_WATER_CHIP_10,
    GVAR_WATER_CHIP_11,
    GVAR_WATER_CHIP_12,
    GVAR_WATER_CHIP_13,
    GVAR_WATER_CHIP_14,
    GVAR_WATER_CHIP_15,
    GVAR_DESTROY_VATS_1,
    GVAR_DESTROY_VATS_2,
    GVAR_DESTROY_VATS_3,
    GVAR_DESTROY_VATS_4,
    GVAR_DESTROY_VATS_5,
    GVAR_DESTROY_VATS_6,
    GVAR_DESTROY_VATS_7,
    GVAR_DESTROY_VATS_8,
    GVAR_DESTROY_VATS_9,
    GVAR_DESTROY_VATS_10,
    GVAR_DESTROY_VATS_11,
    GVAR_DESTROY_VATS_12,
    GVAR_DESTROY_VATS_13,
    GVAR_DESTROY_VATS_14,
    GVAR_DESTROY_VATS_15,
    GVAR_WATER_THIEF_1,
    GVAR_WATER_THIEF_2,
    GVAR_WATER_THIEF_3,
    GVAR_WATER_THIEF_4,
    GVAR_WATER_THIEF_5,
    GVAR_WATER_THIEF_6,
    GVAR_WATER_THIEF_7,
    GVAR_WATER_THIEF_8,
    GVAR_WATER_THIEF_9,
    GVAR_WATER_THIEF_10,
    GVAR_CALM_REBELS_1,
    GVAR_CALM_REBELS_2,
    GVAR_CALM_REBELS_3,
    GVAR_CALM_REBELS_4,
    GVAR_CALM_REBELS_5,
    GVAR_CALM_REBELS_6,
    GVAR_CALM_REBELS_7,
    GVAR_DESTROY_MASTER_1,
    GVAR_DESTROY_MASTER_2,
    GVAR_DESTROY_MASTER_3,
    GVAR_DESTROY_MASTER_4,
    GVAR_DESTROY_MASTER_5,
    GVAR_DESTROY_MASTER_6,
    GVAR_DESTROY_MASTER_7,
    GVAR_DESTROY_MASTER_8,
    GVAR_DESTROY_MASTER_9,
    GVAR_DESTROY_MASTER_10,
    GVAR_DESTROY_MASTER_11,
    GVAR_DESTROY_MASTER_12,
    GVAR_DESTROY_MASTER_13,
    GVAR_DESTROY_MASTER_14,
    GVAR_DESTROY_MASTER_15,
    GVAR_STOP_SCORPIONS_1,
    GVAR_STOP_SCORPIONS_2,
    GVAR_STOP_SCORPIONS_3,
    GVAR_STOP_SCORPIONS_4,
    GVAR_STOP_SCORPIONS_5,
    GVAR_STOP_SCORPIONS_6,
    GVAR_STOP_SCORPIONS_7,
    GVAR_STOP_SCORPIONS_8,
    GVAR_STOP_SCORPIONS_9,
    GVAR_STOP_SCORPIONS_10,
    GVAR_SAVE_TANDI_1,
    GVAR_SAVE_TANDI_2,
    GVAR_SAVE_TANDI_3,
    GVAR_SAVE_TANDI_4,
    GVAR_SAVE_TANDI_5,
    GVAR_SAVE_TANDI_6,
    GVAR_SAVE_TANDI_7,
    GVAR_SAVE_TANDI_8,
    GVAR_SAVE_TANDI_9,
    GVAR_SAVE_TANDI_10,
    GVAR_CURE_JARVIS_1,
    GVAR_CURE_JARVIS_2,
    GVAR_CURE_JARVIS_3,
    GVAR_CURE_JARVIS_4,
    GVAR_CURE_JARVIS_5,
    GVAR_CURE_JARVIS_6,
    GVAR_CURE_JARVIS_7,
    GVAR_CURE_JARVIS_8,
    GVAR_CURE_JARVIS_9,
    GVAR_CURE_JARVIS_10,
    GVAR_MAKE_ANTIDOTE_1,
    GVAR_MAKE_ANTIDOTE_2,
    GVAR_MAKE_ANTIDOTE_3,
    GVAR_MAKE_ANTIDOTE_4,
    GVAR_MAKE_ANTIDOTE_5,
    GVAR_MAKE_ANTIDOTE_6,
    GVAR_MAKE_ANTIDOTE_7,
    GVAR_MAKE_ANTIDOTE_8,
    GVAR_MAKE_ANTIDOTE_9,
    GVAR_MAKE_ANTIDOTE_10,
    GVAR_KILL_MUTANTS_1,
    GVAR_KILL_MUTANTS_2,
    GVAR_KILL_MUTANTS_3,
    GVAR_KILL_MUTANTS_4,
    GVAR_KILL_MUTANTS_5,
    GVAR_KILL_MUTANTS_6,
    GVAR_KILL_MUTANTS_7,
    GVAR_KILL_MUTANTS_8,
    GVAR_KILL_MUTANTS_9,
    GVAR_KILL_MUTANTS_10,
    GVAR_FIX_NECROP_PUMP_1,
    GVAR_FIX_NECROP_PUMP_2,
    GVAR_FIX_NECROP_PUMP_3,
    GVAR_FIX_NECROP_PUMP_4,
    GVAR_FIX_NECROP_PUMP_5,
    GVAR_FIX_NECROP_PUMP_6,
    GVAR_FIX_NECROP_PUMP_7,
    GVAR_FIX_NECROP_PUMP_8,
    GVAR_FIX_NECROP_PUMP_9,
    GVAR_FIX_NECROP_PUMP_10,
    GVAR_KILL_KILLIAN_1,
    GVAR_KILL_KILLIAN_2,
    GVAR_KILL_KILLIAN_3,
    GVAR_KILL_KILLIAN_4,
    GVAR_KILL_KILLIAN_5,
    GVAR_KILL_KILLIAN_6,
    GVAR_KILL_KILLIAN_7,
    GVAR_KILL_KILLIAN_8,
    GVAR_KILL_KILLIAN_9,
    GVAR_KILL_KILLIAN_10,
    GVAR_STOP_GIZMO_1,
    GVAR_STOP_GIZMO_2,
    GVAR_STOP_GIZMO_3,
    GVAR_STOP_GIZMO_4,
    GVAR_STOP_GIZMO_5,
    GVAR_STOP_GIZMO_6,
    GVAR_STOP_GIZMO_7,
    GVAR_STOP_GIZMO_8,
    GVAR_STOP_GIZMO_9,
    GVAR_STOP_GIZMO_10,
    GVAR_SAVE_SINTHIA_1,
    GVAR_SAVE_SINTHIA_2,
    GVAR_SAVE_SINTHIA_3,
    GVAR_SAVE_SINTHIA_4,
    GVAR_SAVE_SINTHIA_5,
    GVAR_SAVE_SINTHIA_6,
    GVAR_SAVE_SINTHIA_7,
    GVAR_SAVE_SINTHIA_8,
    GVAR_SAVE_SINTHIA_9,
    GVAR_SAVE_SINTHIA_10,
    GVAR_TRISH_QUEST_1,
    GVAR_TRISH_QUEST_2,
    GVAR_TRISH_QUEST_3,
    GVAR_TRISH_QUEST_4,
    GVAR_TRISH_QUEST_5,
    GVAR_TRISH_QUEST_6,
    GVAR_TRISH_QUEST_7,
    GVAR_TRISH_QUEST_8,
    GVAR_TRISH_QUEST_9,
    GVAR_TRISH_QUEST_10,
    GVAR_SAUL_QUEST_1,
    GVAR_SAUL_QUEST_2,
    GVAR_SAUL_QUEST_3,
    GVAR_SAUL_QUEST_4,
    GVAR_SAUL_QUEST_5,
    GVAR_SAUL_QUEST_6,
    GVAR_SAUL_QUEST_7,
    GVAR_SAUL_QUEST_8,
    GVAR_SAUL_QUEST_9,
    GVAR_SAUL_QUEST_10,
    GVAR_STEAL_NECKLACE_1,
    GVAR_STEAL_NECKLACE_2,
    GVAR_STEAL_NECKLACE_3,
    GVAR_STEAL_NECKLACE_4,
    GVAR_STEAL_NECKLACE_5,
    GVAR_STEAL_NECKLACE_6,
    GVAR_STEAL_NECKLACE_7,
    GVAR_STEAL_NECKLACE_8,
    GVAR_STEAL_NECKLACE_9,
    GVAR_STEAL_NECKLACE_10,
    GVAR_KILL_JAIN_1,
    GVAR_KILL_JAIN_2,
    GVAR_KILL_JAIN_3,
    GVAR_KILL_JAIN_4,
    GVAR_KILL_JAIN_5,
    GVAR_KILL_JAIN_6,
    GVAR_KILL_JAIN_7,
    GVAR_KILL_JAIN_8,
    GVAR_KILL_JAIN_9,
    GVAR_KILL_JAIN_10,
    GVAR_KILL_MERCHANT_1,
    GVAR_KILL_MERCHANT_2,
    GVAR_KILL_MERCHANT_3,
    GVAR_KILL_MERCHANT_4,
    GVAR_KILL_MERCHANT_5,
    GVAR_KILL_MERCHANT_6,
    GVAR_KILL_MERCHANT_7,
    GVAR_KILL_MERCHANT_8,
    GVAR_KILL_MERCHANT_9,
    GVAR_KILL_MERCHANT_10,
    GVAR_DEATHCLAW_SEED_1,
    GVAR_DEATHCLAW_SEED_2,
    GVAR_DEATHCLAW_SEED_3,
    GVAR_DEATHCLAW_SEED_4,
    GVAR_DEATHCLAW_SEED_5,
    GVAR_DEATHCLAW_SEED_6,
    GVAR_DEATHCLAW_SEED_7,
    GVAR_DEATHCLAW_SEED_8,
    GVAR_DEATHCLAW_SEED_9,
    GVAR_FRY_DEAD,
    GVAR_MISSING_CARAVAN_1,
    GVAR_MISSING_CARAVAN_2,
    GVAR_MISSING_CARAVAN_3,
    GVAR_MISSING_CARAVAN_4,
    GVAR_MISSING_CARAVAN_5,
    GVAR_MISSING_CARAVAN_6,
    GVAR_MISSING_CARAVAN_7,
    GVAR_MISSING_CARAVAN_8,
    GVAR_MISSING_CARAVAN_9,
    GVAR_MISSING_CARAVAN_10,
    GVAR_BECOME_INITIATE_1,
    GVAR_BECOME_INITIATE_2,
    GVAR_BECOME_INITIATE_3,
    GVAR_BECOME_INITIATE_4,
    GVAR_BECOME_INITIATE_5,
    GVAR_BECOME_INITIATE_6,
    GVAR_BECOME_INITIATE_7,
    GVAR_BECOME_INITIATE_8,
    GVAR_BECOME_INITIATE_9,
    GVAR_BECOME_INITIATE_10,
    GVAR_FIND_INITIATE_1,
    GVAR_FIND_INITIATE_2,
    GVAR_FIND_INITIATE_3,
    GVAR_FIND_INITIATE_4,
    GVAR_FIND_INITIATE_5,
    GVAR_FIND_INITIATE_6,
    GVAR_FIND_INITIATE_7,
    GVAR_FIND_INITIATE_8,
    GVAR_FIND_INITIATE_9,
    GVAR_FIND_INITIATE_10,
    GVAR_ROMEO_JULIET_1,
    GVAR_STOP_GANGS_1,
    GVAR_STOP_GANGS_2,
    GVAR_STOP_GANGS_3,
    GVAR_STOP_GANGS_4,
    GVAR_STOP_GANGS_5,
    GVAR_STOP_GANGS_6,
    GVAR_STOP_GANGS_7,
    GVAR_STOP_GANGS_8,
    GVAR_STOP_GANGS_9,
    GVAR_CATCH_SPY_1,
    GVAR_CATCH_SPY_2,
    GVAR_CATCH_SPY_3,
    GVAR_CATCH_SPY_4,
    GVAR_CATCH_SPY_5,
    GVAR_CATCH_SPY_6,
    GVAR_CATCH_SPY_7,
    GVAR_CATCH_SPY_8,
    GVAR_CATCH_SPY_9,
    GVAR_CATCH_SPY_10,
    GVAR_CATCH_SPY_11,
    GVAR_CATCH_SPY_12,
    GVAR_CATCH_SPY_13,
    GVAR_CATCH_SPY_14,
    GVAR_CATCH_SPY_15,
    GVAR_CATCH_SPY_16,
    GVAR_CATCH_SPY_17,
    GVAR_CATCH_SPY_18,
    GVAR_CATCH_SPY_19,
    GVAR_BECOME_BLADE_1,
    GVAR_BECOME_BLADE_2,
    GVAR_BECOME_BLADE_3,
    GVAR_BECOME_BLADE_4,
    GVAR_DELIVER_PACKAGE_1,
    GVAR_DELIVER_PACKAGE_2,
    GVAR_DELIVER_PACKAGE_3,
    GVAR_DELIVER_PACKAGE_4,
    GVAR_DELIVER_PACKAGE_5,
    GVAR_FIX_FARM_1,
    GVAR_FIX_FARM_2,
    GVAR_FIX_FARM_3,
    GVAR_FIX_FARM_4,
    GVAR_LOST_BROTHER_1,
    GVAR_LOST_BROTHER_2,
    GVAR_LOST_BROTHER_3,
    GVAR_LOST_BROTHER_4,
    GVAR_LOST_BROTHER_5,
    GVAR_POWER_GLOW_1,
    GVAR_DISARM_TRAPS_1,
    GVAR_DISARM_TRAPS_2,
    GVAR_DISARM_TRAPS_3,
    GVAR_MAX_MUTANTS,
    GVAR_TIME_CHIP_GONE,
    GVAR_SET_DEAD,
    GVAR_MUTANTS_GONE,
    GVAR_BUST_SKULZ,
    GVAR_SHERRY_TURNS,
    GVAR_TRISH_STATUS,
    GVAR_MARK_V13_1,
    GVAR_MARK_V13_2,
    GVAR_MARK_V13_3,
    GVAR_MARK_V13_4,
    GVAR_MARK_V15_1,
    GVAR_MARK_V15_2,
    GVAR_MARK_V15_3,
    GVAR_MARK_V15_4,
    GVAR_MARK_SHADY_1,
    GVAR_MARK_SHADY_2,
    GVAR_MARK_SHADY_3,
    GVAR_MARK_JUNK_1,
    GVAR_MARK_JUNK_2,
    GVAR_MARK_JUNK_3,
    GVAR_MARK_RAIDERS_1,
    GVAR_MARK_NECROP_1,
    GVAR_MARK_NECROP_2,
    GVAR_MARK_NECROP_3,
    GVAR_MARK_HUB_1,
    GVAR_MARK_HUB_2,
    GVAR_MARK_HUB_3,
    GVAR_MARK_HUB_4,
    GVAR_MARK_HUB_5,
    GVAR_MARK_HUB_6,
    GVAR_MARK_BROTHER_1,
    GVAR_MARK_BROTHER_2,
    GVAR_MARK_BROTHER_3,
    GVAR_MARK_BROTHER_4,
    GVAR_MARK_BROTHER_5,
    GVAR_MARK_BASE_1,
    GVAR_MARK_BASE_2,
    GVAR_MARK_BASE_3,
    GVAR_MARK_BASE_4,
    GVAR_MARK_BASE_5,
    GVAR_MARK_GLOW_1,
    GVAR_MARK_GLOW_2,
    GVAR_MARK_LA_1,
    GVAR_MARK_LA_2,
    GVAR_MARK_LA_3,
    GVAR_MARK_LA_4,
    GVAR_MARK_LA_5,
    GVAR_MARK_CHILD_1,
    GVAR_MARK_CHILD_2,
    GVAR_STRANGER_STATUS,
    GVAR_GAME_DIFFICULTY,
    GVAR_RUNNING_BURNING_GUY,
    GVAR_ARADESH_STATUS,
    GVAR_RHOMBUS_STATUS,
    GVAR_KIND_TO_HAROLD,
    GVAR_GARRET_STATUS,
    GVAR_RADIO_COMPUTER_OFF,
    GVAR_FORCE_FIELDS_OFF,
    GVAR_FIELD_COMPUTER_MODIFIED,
    GVAR_GARLS_FRIEND,
    GVAR_ZIMMERMAN,
    GVAR_BLADES,
    GVAR_GUN_RUNNER,
    GVAR_CHEMISTRY_BOOK,
    GVAR_ENEMY_REGULATOR,
    GVAR_ENEMY_BLADE,
} GameGlobalVar;

} // namespace fallout

#endif /* FALLOUT_GAME_GAME_VARS_H_ */
