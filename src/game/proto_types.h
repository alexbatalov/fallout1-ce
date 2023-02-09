#ifndef FALLOUT_GAME_PROTO_TYPES_H_
#define FALLOUT_GAME_PROTO_TYPES_H_

namespace fallout {

// Number of prototypes in prototype extent.
#define PROTO_LIST_EXTENT_SIZE 16

// Max number of prototypes of one type to be stored in prototype cache lists.
// Once this value is reached the top most proto extent is removed from the
// cache list.
//
// See:
// - [sub_4A2108]
// - [sub_4A2040]
#define PROTO_LIST_MAX_ENTRIES 512

#define WEAPON_TWO_HAND 0x00000200

enum {
    GENDER_MALE,
    GENDER_FEMALE,
    GENDER_COUNT,
};

enum {
    ITEM_TYPE_ARMOR,
    ITEM_TYPE_CONTAINER,
    ITEM_TYPE_DRUG,
    ITEM_TYPE_WEAPON,
    ITEM_TYPE_AMMO,
    ITEM_TYPE_MISC,
    ITEM_TYPE_KEY,
    ITEM_TYPE_COUNT,
};

enum {
    SCENERY_TYPE_DOOR,
    SCENERY_TYPE_STAIRS,
    SCENERY_TYPE_ELEVATOR,
    SCENERY_TYPE_LADDER_UP,
    SCENERY_TYPE_LADDER_DOWN,
    SCENERY_TYPE_GENERIC,
    SCENERY_TYPE_COUNT,
};

enum {
    MATERIAL_TYPE_GLASS,
    MATERIAL_TYPE_METAL,
    MATERIAL_TYPE_PLASTIC,
    MATERIAL_TYPE_WOOD,
    MATERIAL_TYPE_DIRT,
    MATERIAL_TYPE_STONE,
    MATERIAL_TYPE_CEMENT,
    MATERIAL_TYPE_LEATHER,
    MATERIAL_TYPE_COUNT,
};

enum {
    DAMAGE_TYPE_NORMAL,
    DAMAGE_TYPE_LASER,
    DAMAGE_TYPE_FIRE,
    DAMAGE_TYPE_PLASMA,
    DAMAGE_TYPE_ELECTRICAL,
    DAMAGE_TYPE_EMP,
    DAMAGE_TYPE_EXPLOSION,
    DAMAGE_TYPE_COUNT,
};

enum {
    CALIBER_TYPE_NONE,
    CALIBER_TYPE_ROCKET,
    CALIBER_TYPE_FLAMETHROWER_FUEL,
    CALIBER_TYPE_C_ENERGY_CELL,
    CALIBER_TYPE_D_ENERGY_CELL,
    CALIBER_TYPE_223,
    CALIBER_TYPE_5_MM,
    CALIBER_TYPE_40_CAL,
    CALIBER_TYPE_10_MM,
    CALIBER_TYPE_44_CAL,
    CALIBER_TYPE_14_MM,
    CALIBER_TYPE_12_GAUGE,
    CALIBER_TYPE_9_MM,
    CALIBER_TYPE_BB,
    CALIBER_TYPE_COUNT,
};

enum {
    RACE_TYPE_CAUCASIAN,
    RACE_TYPE_AFRICAN,
    RACE_TYPE_COUNT,
};

enum {
    BODY_TYPE_BIPED,
    BODY_TYPE_QUADRUPED,
    BODY_TYPE_ROBOTIC,
    BODY_TYPE_COUNT,
};

enum {
    KILL_TYPE_MAN,
    KILL_TYPE_WOMAN,
    KILL_TYPE_CHILD,
    KILL_TYPE_SUPER_MUTANT,
    KILL_TYPE_GHOUL,
    KILL_TYPE_BRAHMIN,
    KILL_TYPE_RADSCORPION,
    KILL_TYPE_RAT,
    KILL_TYPE_FLOATER,
    KILL_TYPE_CENTAUR,
    KILL_TYPE_ROBOT,
    KILL_TYPE_DOG,
    KILL_TYPE_MANTIS,
    KILL_TYPE_DEATH_CLAW,
    KILL_TYPE_PLANT,
    KILL_TYPE_COUNT,
};

enum {
    PROTO_ID_POWER_ARMOR = 3,
    PROTO_ID_SMALL_ENERGY_CELL = 38,
    PROTO_ID_MICRO_FUSION_CELL = 39,
    PROTO_ID_STIMPACK = 40,
    PROTO_ID_MONEY = 41,
    PROTO_ID_FIRST_AID_KIT = 47,
    PROTO_ID_RADAWAY = 48,
    PROTO_ID_DYNAMITE_I = 51,
    PROTO_ID_GEIGER_COUNTER_I = 52,
    PROTO_ID_MENTATS = 53,
    PROTO_ID_STEALTH_BOY_I = 54,
    PROTO_ID_MOTION_SENSOR = 59,
    PROTO_ID_BIG_BOOK_OF_SCIENCE = 73,
    PROTO_ID_DEANS_ELECTRONICS = 76,
    PROTO_ID_FLARE = 79,
    PROTO_ID_FIRST_AID_BOOK = 80,
    PROTO_ID_PLASTIC_EXPLOSIVES_I = 85,
    PROTO_ID_SCOUT_HANDBOOK = 86,
    PROTO_ID_BUFF_OUT = 87,
    PROTO_ID_DOCTORS_BAG = 91,
    PROTO_ID_GUNS_AND_BULLETS = 102,
    PROTO_ID_NUKA_COLA = 106,
    PROTO_ID_PSYCHO = 110,
    PROTO_ID_BEER = 124,
    PROTO_ID_BOOZE = 125,
    PROTO_ID_SUPER_STIMPACK = 144,
    PROTO_ID_MOLOTOV_COCKTAIL = 159,
    PROTO_ID_LIT_FLARE = 205,
    PROTO_ID_DYNAMITE_II = 206, // armed
    PROTO_ID_GEIGER_COUNTER_II = 207,
    PROTO_ID_PLASTIC_EXPLOSIVES_II = 209, // armed
    PROTO_ID_STEALTH_BOY_II = 210,
    PROTO_ID_HARDENED_POWER_ARMOR = 232,
};

#define PROTO_ID_0x1000098 0x1000098
#define PROTO_ID_0x10001E0 0x10001E0
#define PROTO_ID_0x2000031 0x2000031
#define PROTO_ID_0x2000158 0x2000158
#define PROTO_ID_CAR 0x20003F1
#define PROTO_ID_0x200050D 0x200050D
#define PROTO_ID_0x2000099 0x2000099
#define PROTO_ID_0x20001A5 0x20001A5
#define PROTO_ID_0x20001D6 0x20001D6
#define PROTO_ID_0x20001EB 0x20001EB
#define FID_0x20001F5 0x20001F5
// first exit grid
#define PROTO_ID_0x5000010 0x5000010
// last exit grid
#define PROTO_ID_0x5000017 0x5000017

typedef enum ItemProtoFlags {
    ItemProtoFlags_0x08 = 0x08,
    ItemProtoFlags_0x10 = 0x10,
    ItemProtoFlags_0x1000 = 0x1000,
    ItemProtoFlags_0x8000 = 0x8000,
    ItemProtoFlags_0x20000000 = 0x20000000,
    ItemProtoFlags_0x80000000 = 0x80000000,
} ItemProtoFlags;

typedef enum ItemProtoExtendedFlags {
    ItemProtoExtendedFlags_BigGun = 0x0100,
    ItemProtoExtendedFlags_IsTwoHanded = 0x0200,
    ItemProtoExtendedFlags_0x0800 = 0x0800,
    ItemProtoExtendedFlags_0x1000 = 0x1000,
    ItemProtoExtendedFlags_0x2000 = 0x2000,
    ItemProtoExtendedFlags_0x8000 = 0x8000,

    // This flag is used on weapons to indicate that's an natural (integral)
    // part of it's owner, for example Claw, or Robot's Rocket Launcher. Items
    // with this flag on do count toward total weight and cannot be dropped.
    ItemProtoExtendedFlags_NaturalWeapon = 0x08000000,
} ItemProtoExtendedFlags;

typedef struct {
    int armorClass; // d.ac
    int damageResistance[7]; // d.dam_resist
    int damageThreshold[7]; // d.dam_thresh
    int perk; // d.perk
    int maleFid; // d.male_fid
    int femaleFid; // d.female_fid
} ProtoItemArmorData;

typedef struct {
    int maxSize; // d.max_size
    int openFlags; // d.open_flags
} ProtoItemContainerData;

typedef struct {
    int stat[3]; // d.stat
    int amount[3]; // d.amount
    int duration1; // d.duration1
    int amount1[3]; // d.amount1
    int duration2; // d.duration2
    int amount2[3]; // d.amount2
    int addictionChance; // d.addiction_chance
    int withdrawalEffect; // d.withdrawal_effect
    int withdrawalOnset; // d.withdrawal_onset
} ProtoItemDrugData;

typedef struct {
    int animationCode; // d.animation_code
    int minDamage; // d.min_damage
    int maxDamage; // d.max_damage
    int damageType; // d.dt
    int maxRange1; // d.max_range1
    int maxRange2; // d.max_range2
    int projectilePid; // d.proj_pid
    int minStrength; // d.min_st
    int actionPointCost1; // d.mp_cost1
    int actionPointCost2; // d.mp_cost2
    int criticalFailureType; // d.crit_fail_table
    int perk; // d.perk
    int rounds; // d.rounds
    int caliber; // d.caliber
    int ammoTypePid; // d.ammo_type_pid
    int ammoCapacity; // d.max_ammo
    unsigned char soundCode; // d.sound_id
} ProtoItemWeaponData;

typedef struct {
    int caliber; // d.caliber
    int quantity; // d.quantity
    int armorClassModifier; // d.ac_adjust
    int damageResistanceModifier; // d.dr_adjust
    int damageMultiplier; // d.dam_mult
    int damageDivisor; // d.dam_div
} ProtoItemAmmoData;

typedef struct {
    int powerTypePid; // d.power_type_pid
    int powerType; // d.power_type
    int charges; // d.charges
} ProtoItemMiscData;

typedef struct {
    int keyCode; // d.key_code
} ProtoItemKeyData;

typedef struct ItemProtoData {
    union {
        struct {
            int field_0;
            int field_4;
            int field_8; // max charges
            int field_C;
            int field_10;
            int field_14;
            int field_18;
        } unknown;
        ProtoItemArmorData armor;
        ProtoItemContainerData container;
        ProtoItemDrugData drug;
        ProtoItemWeaponData weapon;
        ProtoItemAmmoData ammo;
        ProtoItemMiscData misc;
        ProtoItemKeyData key;
    };
} ItemProtoData;

typedef struct ItemProto {
    int pid; // pid
    int messageId; // message_num
    int fid; // fid
    int lightDistance; // light_distance
    int lightIntensity; // light_intensity
    int flags; // flags
    int extendedFlags; // flags_ext
    int sid; // sid
    int type; // type
    ItemProtoData data; // d
    int material; // material
    int size; // size
    int weight; // weight
    int cost; // cost
    int inventoryFid; // inv_fid
    unsigned char field_80;
} ItemProto;

typedef struct CritterProtoData {
    int flags; // d.flags
    int baseStats[35]; // d.stat_base
    int bonusStats[35]; // d.stat_bonus
    int skills[18]; // d.stat_points
    int bodyType; // d.body
    int experience;
    int killType;
} CritterProtoData;

typedef struct CritterProto {
    int pid; // pid
    int messageId; // message_num
    int fid; // fid
    int lightDistance; // light_distance
    int lightIntensity; // light_intensity
    int flags; // flags
    int extendedFlags; // flags_ext
    int sid; // sid
    CritterProtoData data; // d
    int headFid; // head_fid
    int aiPacket; // ai_packet
    int team; // team_num
} CritterProto;

typedef struct {
    int openFlags; // d.open_flags
    int keyCode; // d.key_code
} SceneryProtoDoorData;

typedef struct {
    int field_0; // d.lower_tile
    int field_4; // d.upper_tile
} SceneryProtoStairsData;

typedef struct {
    int type;
    int level;
} SceneryProtoElevatorData;

typedef struct {
    int field_0;
} SceneryProtoLadderData;

typedef struct {
    int field_0;
} SceneryProtoGenericData;

typedef struct SceneryProtoData {
    union {
        SceneryProtoDoorData door;
        SceneryProtoStairsData stairs;
        SceneryProtoElevatorData elevator;
        SceneryProtoLadderData ladder;
        SceneryProtoGenericData generic;
    };
} SceneryProtoData;

typedef struct SceneryProto {
    int pid; // id
    int messageId; // message_num
    int fid; // fid
    int lightDistance; // light_distance
    int lightIntensity; // light_intensity
    int flags; // flags
    int extendedFlags; // flags_ext
    int sid; // sid
    int type; // type
    SceneryProtoData data;
    int material;
    int field_30; //
    unsigned char field_34;
} SceneryProto;

typedef struct WallProto {
    int pid; // id
    int messageId; // message_num
    int fid; // fid
    int lightDistance; // light_distance
    int lightIntensity; // light_intensity
    int flags; // flags
    int extendedFlags; // flags_ext
    int sid; // sid
    int material; // material
} WallProto;

typedef struct TileProto {
    int pid; // id
    int messageId; // message_num
    int fid; // fid
    int flags; // flags
    int extendedFlags; // flags_ext
    int sid; // sid
    int material; // material
} TileProto;

typedef struct MiscProto {
    int pid; // id
    int messageId; // message_num
    int fid; // fid
    int lightDistance; // light_distance
    int lightIntensity; // light_intensity
    int flags; // flags
    int extendedFlags; // flags_ext
} MiscProto;

typedef union Proto {
    struct {
        int pid; // pid
        int messageId; // message_num
        int fid; // fid

        // TODO: Move to NonTile props?
        int lightDistance;
        int lightIntensity;
        int flags;
        int extendedFlags;
        int sid;
    };
    ItemProto item;
    CritterProto critter;
    SceneryProto scenery;
    WallProto wall;
    TileProto tile;
    MiscProto misc;
} Proto;

typedef struct ProtoListExtent {
    Proto* proto[PROTO_LIST_EXTENT_SIZE];
    // Number of protos in the extent
    int length;
    struct ProtoListExtent* next;
} ProtoListExtent;

typedef struct ProtoList {
    ProtoListExtent* head;
    ProtoListExtent* tail;
    // Number of extents in the list.
    int length;
    // Number of lines in proto/{type}/{type}.lst.
    int max_entries_num;
} ProtoList;

} // namespace fallout

#endif /* FALLOUT_GAME_PROTO_TYPES_H_ */
