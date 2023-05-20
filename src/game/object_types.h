#ifndef FALLOUT_GAME_OBJECT_TYPES_H_
#define FALLOUT_GAME_OBJECT_TYPES_H_

namespace fallout {

// Rotation
typedef enum Rotation {
    ROTATION_NE, // 0
    ROTATION_E, // 1
    ROTATION_SE, // 2
    ROTATION_SW, // 3
    ROTATION_W, // 4
    ROTATION_NW, // 5
    ROTATION_COUNT,
} Rotation;

enum {
    OBJ_TYPE_ITEM,
    OBJ_TYPE_CRITTER,
    OBJ_TYPE_SCENERY,
    OBJ_TYPE_WALL,
    OBJ_TYPE_TILE,
    OBJ_TYPE_MISC,
    OBJ_TYPE_INTERFACE,
    OBJ_TYPE_INVENTORY,
    OBJ_TYPE_HEAD,
    OBJ_TYPE_BACKGROUND,
    OBJ_TYPE_SKILLDEX,
    OBJ_TYPE_COUNT,
};

#define FID_TYPE(value) ((value)&0xF000000) >> 24
#define PID_TYPE(value) (value) >> 24
#define SID_TYPE(value) (value) >> 24

typedef enum OutlineType {
    OUTLINE_TYPE_HOSTILE = 1,
    OUTLINE_TYPE_2 = 2,
    OUTLINE_TYPE_4 = 4,
    OUTLINE_TYPE_FRIENDLY = 8,
    OUTLINE_TYPE_ITEM = 16,
} OutlineType;

typedef enum ObjectFlags {
    OBJECT_HIDDEN = 0x01,

    // Specifies that the object should not be saved to the savegame file.
    //
    // This flag is used in these situations:
    //  - To prevent saving of system objects like dude (which has separate
    // saving routine), egg, mouse cursors, etc.
    //  - To prevent saving of temporary objects (projectiles, explosion
    // effects, etc.).
    //  - To prevent saving of objects which cannot be removed for some reason,
    // like objects trying to delete themselves from scripting engine (used
    // together with `OBJECT_HIDDEN` to prevent affecting game world).
    OBJECT_NO_SAVE = 0x04,
    OBJECT_FLAT = 0x08,
    OBJECT_NO_BLOCK = 0x10,
    OBJECT_LIGHTING = 0x20,

    // Specifies that the object should not be removed (freed) from the game
    // world for whatever reason.
    //
    // This flag is used to prevent freeing of system objects like dude, egg,
    // mouse cursors, etc.
    OBJECT_NO_REMOVE = 0x400,
    OBJECT_MULTIHEX = 0x800,
    OBJECT_NO_HIGHLIGHT = 0x1000,
    OBJECT_USED = 0x2000,
    OBJECT_TRANS_RED = 0x4000,
    OBJECT_TRANS_NONE = 0x8000,
    OBJECT_TRANS_WALL = 0x10000,
    OBJECT_TRANS_GLASS = 0x20000,
    OBJECT_TRANS_STEAM = 0x40000,
    OBJECT_TRANS_ENERGY = 0x80000,
    OBJECT_IN_LEFT_HAND = 0x1000000,
    OBJECT_IN_RIGHT_HAND = 0x2000000,
    OBJECT_WORN = 0x4000000,
    OBJECT_WALL_TRANS_END = 0x10000000,
    OBJECT_LIGHT_THRU = 0x20000000,
    OBJECT_SEEN = 0x40000000,
    OBJECT_SHOOT_THRU = 0x80000000,

    OBJECT_IN_ANY_HAND = OBJECT_IN_LEFT_HAND | OBJECT_IN_RIGHT_HAND,
    OBJECT_EQUIPPED = OBJECT_IN_ANY_HAND | OBJECT_WORN,
    OBJECT_FLAG_0xFC000 = OBJECT_TRANS_ENERGY | OBJECT_TRANS_STEAM | OBJECT_TRANS_GLASS | OBJECT_TRANS_WALL | OBJECT_TRANS_NONE | OBJECT_TRANS_RED,
    OBJECT_OPEN_DOOR = OBJECT_SHOOT_THRU | OBJECT_LIGHT_THRU | OBJECT_NO_BLOCK,
} ObjectFlags;

typedef enum CritterFlags {
    CRITTER_BARTER = 0x02,
    CRITTER_NO_STEAL = 0x20,
    CRITTER_NO_DROP = 0x40,
    CRITTER_NO_LIMBS = 0x80,
    CRITTER_NO_AGE = 0x100,
    CRITTER_NO_HEAL = 0x200,
    CRITTER_INVULNERABLE = 0x400,
    CRITTER_FLAT = 0x800,
    CRITTER_SPECIAL_DEATH = 0x1000,
    CRITTER_LONG_LIMBS = 0x2000,
    CRITTER_NO_KNOCKBACK = 0x4000,
} CritterFlags;

#define OUTLINE_TYPE_MASK 0xFFFFFF
#define OUTLINE_PALETTED 0x40000000
#define OUTLINE_DISABLED 0x80000000

// These two values are the same but stored in different fields.
#define CONTAINER_FLAG_JAMMED 0x04000000
#define DOOR_FLAG_JAMMGED 0x04000000

#define CONTAINER_FLAG_LOCKED 0x02000000
#define DOOR_FLAG_LOCKED 0x02000000

typedef enum CritterManeuver {
    CRITTER_MANEUVER_NONE = 0,
    CRITTER_MANEUVER_ENGAGING = 0x01,
    CRITTER_MANEUVER_DISENGAGING = 0x02,
    CRITTER_MANUEVER_FLEEING = 0x04,
} CritterManeuver;

typedef enum Dam {
    DAM_KNOCKED_OUT = 0x01,
    DAM_KNOCKED_DOWN = 0x02,
    DAM_CRIP_LEG_LEFT = 0x04,
    DAM_CRIP_LEG_RIGHT = 0x08,
    DAM_CRIP_ARM_LEFT = 0x10,
    DAM_CRIP_ARM_RIGHT = 0x20,
    DAM_BLIND = 0x40,
    DAM_DEAD = 0x80,
    DAM_HIT = 0x100,
    DAM_CRITICAL = 0x200,
    DAM_ON_FIRE = 0x400,
    DAM_BYPASS = 0x800,
    DAM_EXPLODE = 0x1000,
    DAM_DESTROY = 0x2000,
    DAM_DROP = 0x4000,
    DAM_LOSE_TURN = 0x8000,
    DAM_HIT_SELF = 0x10000,
    DAM_LOSE_AMMO = 0x20000,
    DAM_DUD = 0x40000,
    DAM_HURT_SELF = 0x80000,
    DAM_RANDOM_HIT = 0x100000,
    DAM_CRIP_RANDOM = 0x200000,
    DAM_BACKWASH = 0x400000,
    DAM_PERFORM_REVERSE = 0x800000,
    DAM_CRIP_LEG_ANY = DAM_CRIP_LEG_LEFT | DAM_CRIP_LEG_RIGHT,
    DAM_CRIP_ARM_ANY = DAM_CRIP_ARM_LEFT | DAM_CRIP_ARM_RIGHT,
    DAM_CRIP = DAM_CRIP_LEG_ANY | DAM_CRIP_ARM_ANY | DAM_BLIND,
} Dam;

#define OBJ_LOCKED 0x02000000
#define OBJ_JAMMED 0x04000000

typedef struct Object Object;

typedef struct InventoryItem {
    Object* item;
    int quantity;
} InventoryItem;

// Represents inventory of the object.
typedef struct Inventory {
    int length;
    int capacity;
    InventoryItem* items;
} Inventory;

typedef struct WeaponObjectData {
    int ammoQuantity; // obj_pudg.pudweapon.cur_ammo_quantity
    int ammoTypePid; // obj_pudg.pudweapon.cur_ammo_type_pid
} WeaponObjectData;

typedef struct AmmoItemData {
    int quantity; // obj_pudg.pudammo.cur_ammo_quantity
} AmmoItemData;

typedef struct MiscItemData {
    int charges; // obj_pudg.pudmisc_item.curr_charges
} MiscItemData;

typedef struct KeyItemData {
    int keyCode; // obj_pudg.pudkey_item.cur_key_code
} KeyItemData;

typedef union ItemObjectData {
    WeaponObjectData weapon;
    AmmoItemData ammo;
    MiscItemData misc;
    KeyItemData key;
} ItemObjectData;

typedef struct CritterCombatData {
    int maneuver; // obj_pud.combat_data.maneuver
    int ap; // obj_pud.combat_data.curr_mp
    int results; // obj_pud.combat_data.results
    int damageLastTurn; // obj_pud.combat_data.damage_last_turn
    int aiPacket; // obj_pud.combat_data.ai_packet
    int team; // obj_pud.combat_data.team_num
    union {
        Object* whoHitMe; // obj_pud.combat_data.who_hit_me
        int whoHitMeCid;
    };
} CritterCombatData;

typedef struct CritterObjectData {
    int field_0; // obj_pud.reaction_to_pc
    CritterCombatData combat; // obj_pud.combat_data
    int hp; // obj_pud.curr_hp
    int radiation; // obj_pud.curr_rad
    int poison; // obj_pud.curr_poison
} CritterObjectData;

typedef struct DoorSceneryData {
    int openFlags; // obj_pudg.pudportal.cur_open_flags
} DoorSceneryData;

typedef struct StairsSceneryData {
    int destinationMap; // obj_pudg.pudstairs.destMap
    int destinationBuiltTile; // obj_pudg.pudstairs.destBuiltTile
} StairsSceneryData;

typedef struct ElevatorSceneryData {
    int type;
    int level;
} ElevatorSceneryData;

typedef struct LadderSceneryData {
    int destinationBuiltTile;
} LadderSceneryData;

typedef union SceneryObjectData {
    DoorSceneryData door;
    StairsSceneryData stairs;
    ElevatorSceneryData elevator;
    LadderSceneryData ladder;
} SceneryObjectData;

typedef struct MiscObjectData {
    int map;
    int tile;
    int elevation;
    int rotation;
} MiscObjectData;

typedef struct ObjectData {
    Inventory inventory;
    union {
        CritterObjectData critter;
        struct {
            int flags;
            union {
                ItemObjectData item;
                SceneryObjectData scenery;
                MiscObjectData misc;
            };
        };
    };
} ObjectData;

typedef struct Object {
    int id; // obj_id
    int tile; // obj_tile_num
    int x; // obj_x
    int y; // obj_y
    int sx; // obj_sx
    int sy; // obj_sy
    int frame; // obj_cur_frm
    int rotation; // obj_cur_rot
    int fid; // obj_fid
    int flags; // obj_flags
    int elevation; // obj_elev
    union {
        int field_2C_array[14];
        ObjectData data;
    };
    int pid; // obj_pid
    int cid; // obj_cid
    int lightDistance; // obj_light_distance
    int lightIntensity; // obj_light_intensity
    int outline; // obj_outline
    int sid; // obj_sid
    Object* owner;
    int field_80;
} Object;

typedef struct ObjectListNode {
    Object* obj;
    struct ObjectListNode* next;
} ObjectListNode;

#define BUILT_TILE_TILE_MASK 0x3FFFFFF
#define BUILT_TILE_ELEVATION_MASK 0xE0000000
#define BUILT_TILE_ELEVATION_SHIFT 29
#define BUILT_TILE_ROTATION_MASK 0x1C000000
#define BUILT_TILE_ROTATION_SHIFT 26

static inline int builtTileGetTile(int builtTile)
{
    return builtTile & BUILT_TILE_TILE_MASK;
}

static inline int builtTileGetElevation(int builtTile)
{
    return (builtTile & BUILT_TILE_ELEVATION_MASK) >> BUILT_TILE_ELEVATION_SHIFT;
}

static inline int builtTileGetRotation(int builtTile)
{
    return (builtTile & BUILT_TILE_ROTATION_MASK) >> BUILT_TILE_ROTATION_SHIFT;
}

static inline int builtTileCreate(int tile, int elevation)
{
    return tile | ((elevation << BUILT_TILE_ELEVATION_SHIFT) & BUILT_TILE_ELEVATION_MASK);
}

} // namespace fallout

#endif /* FALLOUT_GAME_OBJECT_TYPES_H_ */
