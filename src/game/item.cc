#include "game/item.h"

#include <stdio.h>
#include <string.h>

#include "game/anim.h"
#include "game/automap.h"
#include "game/combat.h"
#include "game/critter.h"
#include "game/display.h"
#include "game/game.h"
#include "game/intface.h"
#include "game/inventry.h"
#include "game/light.h"
#include "game/map.h"
#include "game/message.h"
#include "game/object.h"
#include "game/perk.h"
#include "game/protinst.h"
#include "game/proto.h"
#include "game/queue.h"
#include "game/roll.h"
#include "game/skill.h"
#include "game/stat.h"
#include "game/tile.h"
#include "game/trait.h"
#include "platform_compat.h"
#include "plib/gnw/debug.h"
#include "plib/gnw/memory.h"

namespace fallout {

static void item_compact(int inventoryItemIndex, Inventory* inventory);
static int item_move_func(Object* a1, Object* a2, Object* a3, int quantity, bool a5);
static bool item_identical(Object* a1, Object* a2);
static int item_m_stealth_effect_on(Object* object);
static int item_m_stealth_effect_off(Object* critter, Object* item);
static int insert_drug_effect(Object* critter_obj, Object* item_obj, int a3, int* stats, int* mods);
static void perform_drug_effect(Object* critter_obj, int* stats, int* mods, bool is_immediate);
static int insert_withdrawal(Object* obj, int a2, int a3, int a4, int a5);
static int item_wd_clear_all(Object* a1, void* data);
static void perform_withdrawal_start(Object* obj, int perk, int a3);
static void perform_withdrawal_end(Object* obj, int a2);
static int pid_to_gvar(int drugPid);

// Maps weapon extended flags to skill.
//
// 0x505738
static int attack_skill[9] = {
    -1,
    SKILL_UNARMED,
    SKILL_UNARMED,
    SKILL_MELEE_WEAPONS,
    SKILL_MELEE_WEAPONS,
    SKILL_THROWING,
    SKILL_SMALL_GUNS,
    SKILL_SMALL_GUNS,
    SKILL_SMALL_GUNS,
};

// A map of item's extendedFlags to animation.
//
// 0x50575C
static int attack_anim[9] = {
    ANIM_STAND,
    ANIM_THROW_PUNCH,
    ANIM_KICK_LEG,
    ANIM_SWING_ANIM,
    ANIM_THRUST_ANIM,
    ANIM_THROW_ANIM,
    ANIM_FIRE_SINGLE,
    ANIM_FIRE_BURST,
    ANIM_FIRE_CONTINUOUS,
};

// Maps weapon extended flags to weapon class
//
// 0x505780
static int attack_subtype[9] = {
    ATTACK_TYPE_NONE, // 0 // None
    ATTACK_TYPE_UNARMED, // 1 // Punch // Brass Knuckles, Power First
    ATTACK_TYPE_UNARMED, // 2 // Kick?
    ATTACK_TYPE_MELEE, // 3 // Swing //  Sledgehammer (prim), Club, Knife (prim), Spear (prim), Crowbar
    ATTACK_TYPE_MELEE, // 4 // Thrust // Sledgehammer (sec), Knife (sec), Spear (sec)
    ATTACK_TYPE_THROW, // 5 // Throw // Rock,
    ATTACK_TYPE_RANGED, // 6 // Single // 10mm SMG (prim), Rocket Launcher, Hunting Rifle, Plasma Rifle, Laser Pistol
    ATTACK_TYPE_RANGED, // 7 // Burst // 10mm SMG (sec), Minigun
    ATTACK_TYPE_RANGED, // 8 // Continous // Only: Flamer, Improved Flamer, Flame Breath
};

// 0x5057A4
static int drug_gvar[7] = {
    GVAR_NUKA_COLA_ADDICT,
    GVAR_BUFF_OUT_ADDICT,
    GVAR_MENTATS_ADDICT,
    GVAR_PSYCHO_ADDICT,
    GVAR_RADAWAY_ADDICT,
    GVAR_ALCOHOL_ADDICT,
    GVAR_ALCOHOL_ADDICT,
};

// 0x5057C0
static int drug_pid[7] = {
    PROTO_ID_NUKA_COLA,
    PROTO_ID_BUFF_OUT,
    PROTO_ID_MENTATS,
    PROTO_ID_PSYCHO,
    PROTO_ID_RADAWAY,
    PROTO_ID_BEER,
    PROTO_ID_BOOZE,
};

// item.msg
//
// 0x59CF08
static MessageList item_message_file;

// 0x59CF10
static int wd_onset;

// 0x59CF14
static Object* wd_obj;

// 0x59CF18
static int wd_gvar;

// 0x469C10
int item_init()
{
    char path[COMPAT_MAX_PATH];

    if (!message_init(&item_message_file)) {
        return -1;
    }

    snprintf(path, sizeof(path), "%s%s", msg_path, "item.msg");

    if (!message_load(&item_message_file, path)) {
        return -1;
    }

    return 0;
}

// 0x469C74
int item_reset()
{
    return 0;
}

// 0x469C78
int item_exit()
{
    message_exit(&item_message_file);
    return 0;
}

// 0x469C84
int item_load(DB_FILE* stream)
{
    return 0;
}

// 0x469C84
int item_save(DB_FILE* stream)
{
    return 0;
}

// 0x469C88
int item_add_mult(Object* owner, Object* itemToAdd, int quantity)
{
    if (quantity < 1) {
        return -1;
    }

    int parentType = FID_TYPE(owner->fid);
    if (parentType == OBJ_TYPE_ITEM) {
        int itemType = item_get_type(owner);
        if (itemType == ITEM_TYPE_CONTAINER) {
            // NOTE: Uninline.
            int sizeToAdd = item_size(itemToAdd);
            sizeToAdd *= quantity;

            int currentSize = item_c_curr_size(owner);
            int maxSize = item_c_max_size(owner);
            if (currentSize + sizeToAdd >= maxSize) {
                return -6;
            }

            Object* containerOwner = obj_top_environment(owner);
            if (containerOwner != NULL) {
                if (FID_TYPE(containerOwner->fid) == OBJ_TYPE_CRITTER) {
                    int weightToAdd = item_weight(itemToAdd);
                    weightToAdd *= quantity;

                    int currentWeight = item_total_weight(containerOwner);
                    int maxWeight = stat_level(containerOwner, STAT_CARRY_WEIGHT);
                    if (currentWeight + weightToAdd > maxWeight) {
                        return -6;
                    }
                }
            }
        } else if (itemType == ITEM_TYPE_MISC) {
            // NOTE: Uninline.
            int powerTypePid = item_m_cell_pid(owner);
            if (powerTypePid != itemToAdd->pid) {
                return -1;
            }
        } else {
            return -1;
        }
    } else if (parentType == OBJ_TYPE_CRITTER) {
        if (critter_body_type(owner) != BODY_TYPE_BIPED) {
            return -5;
        }

        int weightToAdd = item_weight(itemToAdd);
        weightToAdd *= quantity;

        int currentWeight = item_total_weight(owner);
        int maxWeight = stat_level(owner, STAT_CARRY_WEIGHT);
        if (currentWeight + weightToAdd > maxWeight) {
            return -6;
        }
    }

    return item_add_force(owner, itemToAdd, quantity);
}

// 0x469DE4
int item_add_force(Object* owner, Object* itemToAdd, int quantity)
{
    if (quantity < 1) {
        return -1;
    }

    Inventory* inventory = &(owner->data.inventory);

    int index;
    for (index = 0; index < inventory->length; index++) {
        if (item_identical(inventory->items[index].item, itemToAdd) != 0) {
            break;
        }
    }

    if (index == inventory->length) {
        if (inventory->length == inventory->capacity || inventory->items == NULL) {
            InventoryItem* inventoryItems = (InventoryItem*)mem_realloc(inventory->items, sizeof(InventoryItem) * (inventory->capacity + 10));
            if (inventoryItems == NULL) {
                return -1;
            }

            inventory->items = inventoryItems;
            inventory->capacity += 10;
        }

        inventory->items[inventory->length].item = itemToAdd;
        inventory->items[inventory->length].quantity = quantity;

        if (itemToAdd->pid == PROTO_ID_STEALTH_BOY_II) {
            if ((itemToAdd->flags & OBJECT_IN_ANY_HAND) != 0) {
                // NOTE: Uninline.
                item_m_stealth_effect_on(owner);
            }
        }

        inventory->length++;
        itemToAdd->owner = owner;

        return 0;
    }

    if (itemToAdd == inventory->items[index].item) {
        debug_printf("Warning! Attempt to add same item twice in item_add()\n");
        return 0;
    }

    if (item_get_type(itemToAdd) == ITEM_TYPE_AMMO) {
        // NOTE: Uninline.
        int ammoQuantityToAdd = item_w_curr_ammo(itemToAdd);

        int ammoQuantity = item_w_curr_ammo(inventory->items[index].item);

        // NOTE: Uninline.
        int capacity = item_w_max_ammo(itemToAdd);

        ammoQuantity += ammoQuantityToAdd;
        if (ammoQuantity > capacity) {
            item_w_set_curr_ammo(itemToAdd, ammoQuantity - capacity);
            inventory->items[index].quantity++;
        } else {
            item_w_set_curr_ammo(itemToAdd, ammoQuantity);
        }

        inventory->items[index].quantity += quantity - 1;
    } else {
        inventory->items[index].quantity += quantity;
    }

    obj_erase_object(inventory->items[index].item, NULL);
    inventory->items[index].item = itemToAdd;
    itemToAdd->owner = owner;

    return 0;
}

// 0x469FB8
int item_remove_mult(Object* owner, Object* itemToRemove, int quantity)
{
    Inventory* inventory = &(owner->data.inventory);
    Object* item1 = inven_left_hand(owner);
    Object* item2 = inven_right_hand(owner);

    int index = 0;
    for (; index < inventory->length; index++) {
        InventoryItem* inventoryItem = &(inventory->items[index]);
        if (inventoryItem->item == itemToRemove) {
            break;
        }

        if (item_get_type(inventoryItem->item) == ITEM_TYPE_CONTAINER) {
            if (item_remove_mult(inventoryItem->item, itemToRemove, quantity) == 0) {
                return 0;
            }
        }
    }

    if (index == inventory->length) {
        return -1;
    }

    InventoryItem* inventoryItem = &(inventory->items[index]);
    if (inventoryItem->quantity < quantity) {
        return -1;
    }

    if (inventoryItem->quantity == quantity) {
        // NOTE: Uninline.
        item_compact(index, inventory);
    } else {
        // TODO: Not sure about this line.
        if (obj_copy(&(inventoryItem->item), itemToRemove) == -1) {
            return -1;
        }

        obj_disconnect(inventoryItem->item, NULL);

        inventoryItem->quantity -= quantity;

        if (item_get_type(itemToRemove) == ITEM_TYPE_AMMO) {
            int capacity = item_w_max_ammo(itemToRemove);
            item_w_set_curr_ammo(inventoryItem->item, capacity);
        }
    }

    if (itemToRemove->pid == PROTO_ID_STEALTH_BOY_I || itemToRemove->pid == PROTO_ID_STEALTH_BOY_II) {
        if (itemToRemove == item1 || itemToRemove == item2) {
            Object* owner = obj_top_environment(itemToRemove);
            if (owner != NULL) {
                item_m_stealth_effect_off(owner, itemToRemove);
            }
        }
    }

    itemToRemove->owner = NULL;
    itemToRemove->flags &= ~OBJECT_EQUIPPED;

    return 0;
}

// 0x46A118
static void item_compact(int inventoryItemIndex, Inventory* inventory)
{
    for (int index = inventoryItemIndex + 1; index < inventory->length; index++) {
        InventoryItem* prev = &(inventory->items[index - 1]);
        InventoryItem* curr = &(inventory->items[index]);
        memcpy(prev, curr, sizeof(*prev));
    }
    inventory->length--;
}

// 0x46A148
static int item_move_func(Object* a1, Object* a2, Object* a3, int quantity, bool a5)
{
    if (item_remove_mult(a1, a3, quantity) == -1) {
        return -1;
    }

    int rc;
    if (a5) {
        rc = item_add_force(a2, a3, quantity);
    } else {
        rc = item_add_mult(a2, a3, quantity);
    }

    if (rc != 0) {
        if (item_add_force(a1, a3, quantity) != 0) {
            Object* owner = obj_top_environment(a1);
            if (owner == NULL) {
                owner = a1;
            }

            if (owner->tile != -1) {
                Rect updatedRect;
                obj_connect(a3, owner->tile, owner->elevation, &updatedRect);
                tile_refresh_rect(&updatedRect, map_elevation);
            }
        }
        return -1;
    }

    a3->owner = a2;

    return 0;
}

// 0x46A1DC
int item_move(Object* a1, Object* a2, Object* a3, int quantity)
{
    return item_move_func(a1, a2, a3, quantity, false);
}

// 0x46A1E4
int item_move_force(Object* a1, Object* a2, Object* a3, int quantity)
{
    return item_move_func(a1, a2, a3, quantity, true);
}

// 0x46A1EC
void item_move_all(Object* a1, Object* a2)
{
    Inventory* inventory = &(a1->data.inventory);
    while (inventory->length > 0) {
        InventoryItem* inventoryItem = &(inventory->items[0]);
        item_move_func(a1, a2, inventoryItem->item, inventoryItem->quantity, true);
    }
}

// 0x46A220
int item_drop_all(Object* critter, int tile)
{
    bool hasEquippedItems = false;

    int frmId = critter->fid & 0xFFF;

    Inventory* inventory = &(critter->data.inventory);
    for (int index = 0; index < inventory->length; index++) {
        InventoryItem* inventoryItem = &(inventory->items[index]);
        Object* item = inventoryItem->item;
        if (item->pid == PROTO_ID_MONEY) {
            if (item_remove_mult(critter, item, inventoryItem->quantity) != 0) {
                return -1;
            }

            if (obj_connect(item, tile, critter->elevation, NULL) != 0) {
                if (item_add_force(critter, item, 1) != 0) {
                    obj_destroy(item);
                }
                return -1;
            }

            item->data.item.misc.charges = inventoryItem->quantity;
        } else {
            if ((item->flags & OBJECT_EQUIPPED) != 0) {
                hasEquippedItems = true;

                if ((item->flags & OBJECT_WORN) != 0) {
                    Proto* proto;
                    if (proto_ptr(critter->pid, &proto) == -1) {
                        return -1;
                    }

                    frmId = proto->fid & 0xFFF;
                    adjust_ac(critter, item, NULL);
                }
            }

            for (int index = 0; index < inventoryItem->quantity; index++) {
                if (item_remove_mult(critter, item, 1) != 0) {
                    return -1;
                }

                if (obj_connect(item, tile, critter->elevation, NULL) != 0) {
                    if (item_add_force(critter, item, 1) != 0) {
                        obj_destroy(item);
                    }
                    return -1;
                }
            }
        }
    }

    if (hasEquippedItems) {
        Rect updatedRect;
        int fid = art_id(OBJ_TYPE_CRITTER, frmId, FID_ANIM_TYPE(critter->fid), 0, (critter->fid & 0x70000000) >> 28);
        obj_change_fid(critter, fid, &updatedRect);
        if (FID_ANIM_TYPE(critter->fid) == ANIM_STAND) {
            tile_refresh_rect(&updatedRect, map_elevation);
        }
    }

    return -1;
}

// 0x46A34C
static bool item_identical(Object* a1, Object* a2)
{
    if (a1->pid != a2->pid) {
        return false;
    }

    if (a1->sid != a2->sid) {
        return false;
    }

    if ((a1->flags & (OBJECT_EQUIPPED | OBJECT_USED)) != 0) {
        return false;
    }

    if ((a2->flags & (OBJECT_EQUIPPED | OBJECT_USED)) != 0) {
        return false;
    }

    Proto* proto;
    proto_ptr(a1->pid, &proto);
    if (proto->item.type == ITEM_TYPE_CONTAINER) {
        return false;
    }

    Inventory* inventory1 = &(a1->data.inventory);
    Inventory* inventory2 = &(a2->data.inventory);
    if (inventory1->length != 0 || inventory2->length != 0) {
        return false;
    }

    int v1;
    if (proto->item.type == ITEM_TYPE_AMMO || a1->pid == PROTO_ID_MONEY) {
        v1 = a2->data.item.ammo.quantity;
        a2->data.item.ammo.quantity = a1->data.item.ammo.quantity;
    }

    // NOTE: Probably inlined memcmp, but I'm not sure why it only checks 32
    // bytes.
    int i;
    for (i = 0; i < 8; i++) {
        if (a1->field_2C_array[i] != a2->field_2C_array[i]) {
            break;
        }
    }

    if (proto->item.type == ITEM_TYPE_AMMO || a1->pid == PROTO_ID_MONEY) {
        a2->data.item.ammo.quantity = v1;
    }

    return i == 8;
}

// 0x46A440
char* item_name(Object* obj)
{
    // NOTE: Compatibility.
    //
    // 0x4F92BC
    static char _name[] = "<item>";

    // 0x5057DC
    static char* name = _name;

    name = proto_name(obj->pid);
    return name;
}

// 0x46A450
char* item_description(Object* obj)
{
    return proto_description(obj->pid);
}

// 0x46A458
int item_get_type(Object* item)
{
    Proto* proto;

    proto_ptr(item->pid, &proto);
    return proto->item.type;
}

// 0x46A474
int item_material(Object* item)
{
    Proto* proto;

    proto_ptr(item->pid, &proto);
    return proto->item.material;
}

// 0x46A490
int item_size(Object* item)
{
    Proto* proto;

    proto_ptr(item->pid, &proto);
    return proto->item.size;
}

// 0x46A4AC
int item_weight(Object* item)
{
    Proto* item_proto;
    Proto* ammo_proto;
    int type;
    int weight;
    int curr_ammo;
    int ammo_pid;

    proto_ptr(item->pid, &item_proto);
    type = item_proto->item.type;
    weight = item_proto->item.weight;

    switch (type) {
    case ITEM_TYPE_CONTAINER:
        weight += item_total_weight(item);
        break;
    case ITEM_TYPE_WEAPON:
        curr_ammo = item_w_curr_ammo(item);
        if (curr_ammo > 0) {
            ammo_pid = item_w_ammo_pid(item);
            if (ammo_pid != -1) {
                if (proto_ptr(ammo_pid, &ammo_proto) != -1) {
                    weight += ammo_proto->item.weight * ((curr_ammo - 1) / ammo_proto->item.data.ammo.quantity + 1);
                }
            }
        }
        break;
    }

    return weight;
}

// Returns cost of item.
//
// When [item] is container the returned cost includes cost of container
// itself plus cost of contained items.
//
// When [item] is a weapon the returned value includes cost of weapon
// itself plus cost of remaining ammo (see below).
//
// When [item] is an ammo it's cost is calculated from ratio of fullness.
//
// 0x46A544
int item_cost(Object* obj)
{
    // TODO: This function needs review. A lot of functionality is inlined.
    // Find these functions and use them.
    if (obj == NULL) {
        return 0;
    }

    Proto* proto;
    proto_ptr(obj->pid, &proto);

    int cost = proto->item.cost;

    switch (proto->item.type) {
    case ITEM_TYPE_CONTAINER:
        cost += item_total_cost(obj);
        break;
    case ITEM_TYPE_WEAPON:
        if (1) {
            // NOTE: Uninline.
            int ammoQuantity = item_w_curr_ammo(obj);
            if (ammoQuantity > 0) {
                // NOTE: Uninline.
                int ammoTypePid = item_w_ammo_pid(obj);
                if (ammoTypePid != -1) {
                    Proto* ammoProto;
                    proto_ptr(ammoTypePid, &ammoProto);

                    cost += ammoQuantity * ammoProto->item.cost / ammoProto->item.data.ammo.quantity;
                }
            }
        }
        break;
    case ITEM_TYPE_AMMO:
        if (1) {
            // NOTE: Uninline.
            int ammoQuantity = item_w_curr_ammo(obj);
            cost *= ammoQuantity;
            // NOTE: Uninline.
            int ammoCapacity = item_w_max_ammo(obj);
            cost /= ammoCapacity;
        }
        break;
    }

    return cost;
}

// 0x46A634
int item_total_cost(Object* obj)
{
    if (obj == NULL) {
        return 0;
    }

    int cost = 0;

    Inventory* inventory = &(obj->data.inventory);
    for (int index = 0; index < inventory->length; index++) {
        InventoryItem* inventoryItem = &(inventory->items[index]);
        if (item_get_type(inventoryItem->item) == ITEM_TYPE_AMMO) {
            Proto* proto;
            proto_ptr(inventoryItem->item->pid, &proto);

            // Ammo stack in inventory is a bit special. It is counted in clips,
            // `inventoryItem->quantity` is the number of clips. The ammo object
            // itself tracks remaining number of ammo in only one instance of
            // the clip implying all other clips in the stack are full.
            //
            // In order to correctly calculate cost of the ammo stack, add cost
            // of all full clips...
            cost += proto->item.cost * (inventoryItem->quantity - 1);

            // ...and add cost of the current clip, which is proportional to
            // it's capacity.
            cost += item_cost(inventoryItem->item);
        } else {
            cost += item_cost(inventoryItem->item) * inventoryItem->quantity;
        }
    }

    if (FID_TYPE(obj->fid) == OBJ_TYPE_CRITTER) {
        Object* item2 = inven_right_hand(obj);
        if (item2 != NULL && (item2->flags & OBJECT_IN_RIGHT_HAND) == 0) {
            cost += item_cost(item2);
        }

        Object* item1 = inven_left_hand(obj);
        if (item1 != NULL && (item1->flags & OBJECT_IN_LEFT_HAND) == 0) {
            cost += item_cost(item1);
        }

        Object* armor = inven_worn(obj);
        if (armor != NULL && (armor->flags & OBJECT_WORN) == 0) {
            cost += item_cost(armor);
        }
    }

    return cost;
}

// 0x46A728
int item_total_weight(Object* obj)
{
    if (obj == NULL) {
        return 0;
    }

    int weight = 0;

    Inventory* inventory = &(obj->data.inventory);
    for (int index = 0; index < inventory->length; index++) {
        InventoryItem* inventoryItem = &(inventory->items[index]);
        Object* item = inventoryItem->item;
        weight += item_weight(item) * inventoryItem->quantity;
    }

    if (FID_TYPE(obj->fid) == OBJ_TYPE_CRITTER) {
        Object* item2 = inven_right_hand(obj);
        if (item2 != NULL) {
            if ((item2->flags & OBJECT_IN_RIGHT_HAND) == 0) {
                weight += item_weight(item2);
            }
        }

        Object* item1 = inven_left_hand(obj);
        if (item1 != NULL) {
            if ((item1->flags & OBJECT_IN_LEFT_HAND) == 0) {
                weight += item_weight(item1);
            }
        }

        Object* armor = inven_worn(obj);
        if (armor != NULL) {
            if ((armor->flags & OBJECT_WORN) == 0) {
                weight += item_weight(armor);
            }
        }
    }

    return weight;
}

// 0x46A7C4
bool item_grey(Object* weapon)
{
    if (weapon == NULL) {
        return false;
    }

    if (item_get_type(weapon) != ITEM_TYPE_WEAPON) {
        return false;
    }

    int flags = obj_dude->data.critter.combat.results;
    if ((flags & DAM_CRIP_ARM_LEFT) != 0 && (flags & DAM_CRIP_ARM_RIGHT) != 0) {
        return true;
    }

    // NOTE: Uninline.
    bool isTwoHanded = item_w_is_2handed(weapon);
    if (isTwoHanded) {
        if ((flags & DAM_CRIP_ARM_LEFT) != 0 || (flags & DAM_CRIP_ARM_RIGHT) != 0) {
            return true;
        }
    }

    return false;
}

// 0x46A838
int item_inv_fid(Object* item)
{
    Proto* proto;

    proto_ptr(item->pid, &proto);
    return proto->item.inventoryFid;
}

// 0x46A874
Object* item_hit_with(Object* critter, int hit_mode)
{
    switch (hit_mode) {
    case HIT_MODE_LEFT_WEAPON_PRIMARY:
    case HIT_MODE_LEFT_WEAPON_SECONDARY:
    case HIT_MODE_LEFT_WEAPON_RELOAD:
        return inven_left_hand(critter);
    case HIT_MODE_RIGHT_WEAPON_PRIMARY:
    case HIT_MODE_RIGHT_WEAPON_SECONDARY:
    case HIT_MODE_RIGHT_WEAPON_RELOAD:
        return inven_right_hand(critter);
    }

    return NULL;
}

// 0x46A8B8
int item_mp_cost(Object* critter, int hit_mode, bool aiming)
{
    Object* item;

    item = item_hit_with(critter, hit_mode);
    if (item != NULL && item_get_type(item) != ITEM_TYPE_WEAPON) {
        return 2;
    }

    return item_w_mp_cost(critter, hit_mode, aiming);
}

// 0x46A904
int item_count(Object* obj, Object* a2)
{
    int quantity = 0;

    Inventory* inventory = &(obj->data.inventory);
    for (int index = 0; index < inventory->length; index++) {
        InventoryItem* inventoryItem = &(inventory->items[index]);
        Object* item = inventoryItem->item;
        if (item == a2) {
            quantity = inventoryItem->quantity;

            // SFALL: Fix incorrect value being returned if there is a container
            // item in the inventory.
            break;
        } else {
            if (item_get_type(item) == ITEM_TYPE_CONTAINER) {
                quantity = item_count(item, a2);
                if (quantity > 0) {
                    break;
                }
            }
        }
    }

    return quantity;
}

// 0x46A96C
int item_queued(Object* obj)
{
    if (obj == NULL) {
        return false;
    }

    if ((obj->flags & OBJECT_USED) != 0) {
        return true;
    }

    Inventory* inventory = &(obj->data.inventory);
    for (int index = 0; index < inventory->length; index++) {
        InventoryItem* inventoryItem = &(inventory->items[index]);
        if ((inventoryItem->item->flags & OBJECT_USED) != 0) {
            return true;
        }

        if (item_get_type(inventoryItem->item) == ITEM_TYPE_CONTAINER) {
            if (item_queued(inventoryItem->item)) {
                return true;
            }
        }
    }

    return false;
}

// 0x46A9F0
Object* item_replace(Object* a1, Object* a2, int a3)
{
    if (a1 == NULL) {
        return NULL;
    }

    if (a2 == NULL) {
        return NULL;
    }

    Inventory* inventory = &(a1->data.inventory);
    for (int index = 0; index < inventory->length; index++) {
        InventoryItem* inventoryItem = &(inventory->items[index]);
        if (item_identical(inventoryItem->item, a2)) {
            Object* item = inventoryItem->item;
            if (item_remove_mult(a1, item, 1) == 0) {
                item->flags |= a3;
                if (item_add_force(a1, item, 1) == 0) {
                    return item;
                }

                item->flags &= ~a3;
                if (item_add_force(a1, item, 1) != 0) {
                    obj_destroy(item);
                }
            }
        }

        if (item_get_type(inventoryItem->item) == ITEM_TYPE_CONTAINER) {
            Object* obj = item_replace(inventoryItem->item, a2, a3);
            if (obj != NULL) {
                return obj;
            }
        }
    }

    return NULL;
}

// 0x46AAE4
int item_w_subtype(Object* weapon, int hitMode)
{
    if (weapon == NULL) {
        return ATTACK_TYPE_UNARMED;
    }

    Proto* proto;
    proto_ptr(weapon->pid, &proto);

    int index;
    if (hitMode == HIT_MODE_LEFT_WEAPON_PRIMARY || hitMode == HIT_MODE_RIGHT_WEAPON_PRIMARY) {
        index = proto->item.extendedFlags & 0xF;
    } else {
        index = (proto->item.extendedFlags & 0xF0) >> 4;
    }

    return attack_subtype[index];
}

// 0x46AB30
int item_w_skill(Object* weapon, int hitMode)
{
    if (weapon == NULL) {
        return SKILL_UNARMED;
    }

    Proto* proto;
    proto_ptr(weapon->pid, &proto);

    int index;
    if (hitMode == HIT_MODE_LEFT_WEAPON_PRIMARY || hitMode == HIT_MODE_RIGHT_WEAPON_PRIMARY) {
        index = proto->item.extendedFlags & 0xF;
    } else {
        index = (proto->item.extendedFlags & 0xF0) >> 4;
    }

    int skill = attack_skill[index];

    if (skill == SKILL_SMALL_GUNS) {
        int damageType = item_w_damage_type(weapon);
        if (damageType == DAMAGE_TYPE_LASER || damageType == DAMAGE_TYPE_PLASMA || damageType == DAMAGE_TYPE_ELECTRICAL) {
            skill = SKILL_ENERGY_WEAPONS;
        } else {
            if ((proto->item.extendedFlags & ItemProtoExtendedFlags_BigGun) != 0) {
                skill = SKILL_BIG_GUNS;
            }
        }
    }

    return skill;
}

// Returns skill value when critter is about to perform `hitMode`.
//
// 0x46ABE4
int item_w_skill_level(Object* critter, int hit_mode)
{
    Object* weapon;
    int skill;

    // NOTE: Uninline.
    weapon = item_hit_with(critter, hit_mode);
    if (weapon != NULL) {
        skill = item_w_skill(weapon, hit_mode);
    } else {
        skill = SKILL_UNARMED;
    }

    return skill_level(critter, skill);
}

// 0x46AC28
int item_w_damage_min_max(Object* weapon, int* min_damage, int* max_damage)
{
    Proto* proto;

    if (weapon == NULL) {
        return -1;
    }

    proto_ptr(weapon->pid, &proto);

    if (min_damage != NULL) {
        *min_damage = proto->item.data.weapon.minDamage;
    }

    if (max_damage != NULL) {
        *max_damage = proto->item.data.weapon.maxDamage;
    }

    return 0;
}

// 0x46AC88
int item_w_damage(Object* critter, int hit_mode)
{
    int min_damage = 0;
    int max_damage = 0;
    int bonus_damage = 0;
    int subtype;

    if (critter == NULL) {
        return 0;
    }

    // NOTE: Uninline.
    Object* weapon = item_hit_with(critter, hit_mode);

    if (weapon != NULL) {
        // NOTE: Uninline.
        item_w_damage_min_max(weapon, &min_damage, &max_damage);

        subtype = item_w_subtype(weapon, hit_mode);
        if (subtype == ATTACK_TYPE_MELEE || subtype == ATTACK_TYPE_UNARMED) {
            bonus_damage = stat_level(critter, STAT_MELEE_DAMAGE);
        }
    } else {
        min_damage = 1;
        max_damage = stat_level(critter, STAT_MELEE_DAMAGE) + 2;
    }

    return roll_random(min_damage, bonus_damage + max_damage);
}

// 0x46AD4C
int item_w_damage_type(Object* weapon)
{
    Proto* proto;

    if (weapon != NULL) {
        proto_ptr(weapon->pid, &proto);

        return proto->item.data.weapon.damageType;
    }

    return 0;
}

// 0x46AD6C
int item_w_is_2handed(Object* weapon)
{
    Proto* proto;

    if (weapon == NULL) {
        return 0;
    }

    proto_ptr(weapon->pid, &proto);

    return (proto->item.extendedFlags & WEAPON_TWO_HAND) != 0;
}

// 0x46ADB0
int item_w_anim(Object* critter, int hit_mode)
{
    Object* weapon;

    // NOTE: Uninline.
    weapon = item_hit_with(critter, hit_mode);
    return item_w_anim_weap(weapon, hit_mode);
}

// 0x46ADDC
int item_w_anim_weap(Object* weapon, int hit_mode)
{
    Proto* proto;
    int index;

    if (hit_mode == HIT_MODE_KICK) {
        return ANIM_KICK_LEG;
    }

    if (weapon == NULL) {
        return ANIM_THROW_PUNCH;
    }

    proto_ptr(weapon->pid, &proto);

    if (hit_mode == HIT_MODE_LEFT_WEAPON_PRIMARY || hit_mode == HIT_MODE_RIGHT_WEAPON_PRIMARY) {
        index = proto->item.extendedFlags & 0xF;
    } else {
        index = (proto->item.extendedFlags & 0xF0) >> 4;
    }

    return attack_anim[index];
}

// 0x46AE38
int item_w_max_ammo(Object* ammoOrWeapon)
{
    Proto* proto;

    if (ammoOrWeapon == NULL) {
        return 0;
    }

    proto_ptr(ammoOrWeapon->pid, &proto);

    if (proto->item.type == ITEM_TYPE_AMMO) {
        return proto->item.data.ammo.quantity;
    } else {
        return proto->item.data.weapon.ammoCapacity;
    }
}

// 0x46AE64
int item_w_curr_ammo(Object* ammoOrWeapon)
{
    Proto* proto;

    if (ammoOrWeapon == NULL) {
        return 0;
    }

    proto_ptr(ammoOrWeapon->pid, &proto);

    // NOTE: Looks like the condition jumps were erased during compilation only
    // because ammo's quantity and weapon's ammo quantity coincidently stored
    // in the same offset relative to [Object].
    if (proto->item.type == ITEM_TYPE_AMMO) {
        return ammoOrWeapon->data.item.ammo.quantity;
    } else {
        return ammoOrWeapon->data.item.weapon.ammoQuantity;
    }
}

// 0x46AE8C
int item_w_caliber(Object* ammoOrWeapon)
{
    Proto* proto;

    if (ammoOrWeapon == NULL) {
        return 0;
    }

    proto_ptr(ammoOrWeapon->pid, &proto);

    if (proto->item.type != ITEM_TYPE_AMMO) {
        if (proto_ptr(ammoOrWeapon->data.item.weapon.ammoTypePid, &proto) == -1) {
            return 0;
        }
    }

    return proto->item.data.ammo.caliber;
}

// 0x46AED8
void item_w_set_curr_ammo(Object* ammoOrWeapon, int quantity)
{
    if (ammoOrWeapon == NULL) {
        return;
    }

    // NOTE: Uninline.
    int capacity = item_w_max_ammo(ammoOrWeapon);
    if (quantity > capacity) {
        quantity = capacity;
    }

    Proto* proto;
    proto_ptr(ammoOrWeapon->pid, &proto);

    if (proto->item.type == ITEM_TYPE_AMMO) {
        ammoOrWeapon->data.item.ammo.quantity = quantity;
    } else {
        ammoOrWeapon->data.item.weapon.ammoQuantity = quantity;
    }
}

// 0x46AF2C
int item_w_try_reload(Object* critter, Object* weapon)
{
    // NOTE: Uninline.
    int quantity = item_w_curr_ammo(weapon);
    int capacity = item_w_max_ammo(weapon);
    if (quantity == capacity) {
        return -1;
    }

    int inventoryItemIndex = -1;
    while (1) {
        Object* ammo = inven_find_type(critter, ITEM_TYPE_AMMO, &inventoryItemIndex);
        if (ammo == NULL) {
            break;
        }

        if (item_w_can_reload(weapon, ammo)) {
            int rc = item_w_reload(weapon, ammo);
            if (rc == 0) {
                obj_destroy(ammo);
            }

            if (rc == -1) {
                return -1;
            }

            return 0;
        }
    }

    return -1;
}

// Checks if weapon can be reloaded with the specified ammo.
//
// 0x46AFB8
bool item_w_can_reload(Object* weapon, Object* ammo)
{
    if (ammo == NULL) {
        return false;
    }

    Proto* weaponProto;
    proto_ptr(weapon->pid, &weaponProto);

    Proto* ammoProto;
    proto_ptr(ammo->pid, &ammoProto);

    if (weaponProto->item.type != ITEM_TYPE_WEAPON) {
        return false;
    }

    if (ammoProto->item.type != ITEM_TYPE_AMMO) {
        return false;
    }

    // Check ammo matches weapon caliber.
    if (weaponProto->item.data.weapon.caliber != ammoProto->item.data.ammo.caliber) {
        return false;
    }

    // If weapon is not empty, we should only reload it with the same ammo.
    if (item_w_curr_ammo(weapon) != 0) {
        if (weapon->data.item.weapon.ammoTypePid != ammo->pid) {
            return false;
        }
    }

    return true;
}

// 0x46B020
int item_w_reload(Object* weapon, Object* ammo)
{
    if (!item_w_can_reload(weapon, ammo)) {
        return -1;
    }

    // NOTE: Uninline.
    int ammoQuantity = item_w_curr_ammo(weapon);

    // NOTE: Uninline.
    int ammoCapacity = item_w_max_ammo(weapon);

    // NOTE: Uninline.
    int v10 = item_w_curr_ammo(ammo);

    int v11 = v10;
    if (ammoQuantity < ammoCapacity) {
        int v12;
        if (ammoQuantity + v10 > ammoCapacity) {
            v11 = v10 - (ammoCapacity - ammoQuantity);
            v12 = ammoCapacity;
        } else {
            v11 = 0;
            v12 = ammoQuantity + v10;
        }

        weapon->data.item.weapon.ammoTypePid = ammo->pid;

        item_w_set_curr_ammo(ammo, v11);
        item_w_set_curr_ammo(weapon, v12);
    }

    return v11;
}

// 0x46B104
int item_w_range(Object* critter, int hit_mode)
{
    Object* weapon;
    Proto* proto;
    int range;
    int max_range;

    // NOTE: Uninline.
    weapon = item_hit_with(critter, hit_mode);

    if (weapon == NULL || hit_mode == HIT_MODE_PUNCH || hit_mode == HIT_MODE_KICK) {
        return 1;
    }

    proto_ptr(weapon->pid, &proto);

    if (hit_mode == HIT_MODE_LEFT_WEAPON_PRIMARY || hit_mode == HIT_MODE_RIGHT_WEAPON_PRIMARY) {
        range = proto->item.data.weapon.maxRange1;
    } else {
        range = proto->item.data.weapon.maxRange2;
    }

    if (item_w_subtype(weapon, hit_mode) == ATTACK_TYPE_THROW) {
        if (critter == obj_dude) {
            max_range = 3 * (stat_level(critter, STAT_STRENGTH) + 2 * perk_level(PERK_HEAVE_HO));
        } else {
            max_range = 3 * stat_level(critter, STAT_STRENGTH);
        }

        if (range > max_range) {
            range = max_range;
        }
    }

    return range;
}

// Returns action points required for hit mode.
//
// 0x46B1E8
int item_w_mp_cost(Object* critter, int hit_mode, bool aiming)
{
    Object* weapon;
    int action_points;
    int weapon_subtype;

    if (hit_mode == HIT_MODE_LEFT_WEAPON_RELOAD || hit_mode == HIT_MODE_RIGHT_WEAPON_RELOAD) {
        return 2;
    }

    // NOTE: Uninline.
    weapon = item_hit_with(critter, hit_mode);
    if (weapon == NULL || hit_mode == HIT_MODE_PUNCH || hit_mode == HIT_MODE_KICK) {
        action_points = 3;
    } else {
        if (hit_mode == HIT_MODE_LEFT_WEAPON_PRIMARY || hit_mode == HIT_MODE_RIGHT_WEAPON_PRIMARY) {
            // NOTE: Uninline.
            action_points = item_w_primary_mp_cost(weapon);
        } else {
            // NOTE: Uninline.
            action_points = item_w_secondary_mp_cost(weapon);
        }

        if (critter == obj_dude) {
            if (trait_level(TRAIT_FAST_SHOT)) {
                action_points -= 1;
            }
        }
    }

    if (critter == obj_dude) {
        weapon_subtype = item_w_subtype(weapon, hit_mode);

        if (perk_level(PERK_BONUS_HTH_ATTACKS)) {
            if (weapon_subtype == ATTACK_TYPE_MELEE || weapon_subtype == ATTACK_TYPE_UNARMED) {
                action_points -= 1;
            }
        }

        if (perk_level(PERK_BONUS_RATE_OF_FIRE)) {
            if (weapon_subtype == ATTACK_TYPE_RANGED) {
                action_points -= 1;
            }
        }
    }

    if (aiming) {
        action_points += 1;
    }

    if (action_points < 1) {
        action_points = 1;
    }

    return action_points;
}

// 0x46B2FC
int item_w_min_st(Object* weapon)
{
    if (weapon == NULL) {
        return -1;
    }

    Proto* proto;
    proto_ptr(weapon->pid, &proto);

    return proto->item.data.weapon.minStrength;
}

// 0x46B324
int item_w_crit_fail(Object* weapon)
{
    if (weapon == NULL) {
        return -1;
    }

    Proto* proto;
    proto_ptr(weapon->pid, &proto);

    return proto->item.data.weapon.criticalFailureType;
}

// 0x46B34C
int item_w_perk(Object* weapon)
{
    if (weapon == NULL) {
        return -1;
    }

    Proto* proto;
    proto_ptr(weapon->pid, &proto);

    return proto->item.data.weapon.perk;
}

// 0x46B374
int item_w_rounds(Object* weapon)
{
    if (weapon == NULL) {
        return -1;
    }

    Proto* proto;
    proto_ptr(weapon->pid, &proto);

    return proto->item.data.weapon.rounds;
}

// 0x46B39C
int item_w_anim_code(Object* weapon)
{
    if (weapon == NULL) {
        return -1;
    }

    Proto* proto;
    proto_ptr(weapon->pid, &proto);

    return proto->item.data.weapon.animationCode;
}

// 0x46B3C4
int item_w_proj_pid(Object* weapon)
{
    if (weapon == NULL) {
        return -1;
    }

    Proto* proto;
    proto_ptr(weapon->pid, &proto);

    return proto->item.data.weapon.projectilePid;
}

// 0x46B3EC
int item_w_ammo_pid(Object* weapon)
{
    if (weapon == NULL) {
        return -1;
    }

    if (item_get_type(weapon) != ITEM_TYPE_WEAPON) {
        return -1;
    }

    return weapon->data.item.weapon.ammoTypePid;
}

// 0x46B40C
char item_w_sound_id(Object* weapon)
{
    if (weapon == NULL) {
        return '\0';
    }

    Proto* proto;
    proto_ptr(weapon->pid, &proto);

    return proto->item.data.weapon.soundCode & 0xFF;
}

// 0x46B450
int item_w_called_shot(Object* critter, int hit_mode)
{
    int anim;
    Object* weapon;
    int damage_type;

    if (critter == obj_dude) {
        if (trait_level(TRAIT_FAST_SHOT)) {
            return 0;
        }
    }

    // NOTE: Uninline.
    anim = item_w_anim(critter, hit_mode);
    if (anim == ANIM_FIRE_BURST || anim == ANIM_FIRE_CONTINUOUS) {
        return 0;
    }

    // NOTE: Uninline.
    weapon = item_hit_with(critter, hit_mode);
    damage_type = item_w_damage_type(weapon);

    if (damage_type == DAMAGE_TYPE_EXPLOSION
        || damage_type == DAMAGE_TYPE_FIRE
        || damage_type == DAMAGE_TYPE_EMP
        || (anim == ANIM_THROW_ANIM && damage_type == DAMAGE_TYPE_PLASMA)) {
        return 0;
    }

    return 1;
}

// 0x46B500
int item_w_can_unload(Object* weapon)
{
    if (weapon == NULL) {
        return false;
    }

    if (item_get_type(weapon) != ITEM_TYPE_WEAPON) {
        return false;
    }

    // NOTE: Uninline.
    if (item_w_max_ammo(weapon) <= 0) {
        return false;
    }

    // NOTE: Uninline.
    if (item_w_curr_ammo(weapon) <= 0) {
        return false;
    }

    if (item_w_ammo_pid(weapon) == -1) {
        return false;
    }

    return true;
}

// 0x46B56C
Object* item_w_unload(Object* weapon)
{
    if (!item_w_can_unload(weapon)) {
        return NULL;
    }

    // NOTE: Uninline.
    int ammoTypePid = item_w_ammo_pid(weapon);
    if (ammoTypePid == -1) {
        return NULL;
    }

    Object* ammo;
    if (obj_pid_new(&ammo, ammoTypePid) != 0) {
        return NULL;
    }

    obj_disconnect(ammo, NULL);

    // NOTE: Uninline.
    int ammoQuantity = item_w_curr_ammo(weapon);

    // NOTE: Uninline.
    int ammoCapacity = item_w_max_ammo(ammo);

    int remainingQuantity;
    if (ammoQuantity <= ammoCapacity) {
        item_w_set_curr_ammo(ammo, ammoQuantity);
        remainingQuantity = 0;
    } else {
        item_w_set_curr_ammo(ammo, ammoCapacity);
        remainingQuantity = ammoQuantity - ammoCapacity;
    }
    item_w_set_curr_ammo(weapon, remainingQuantity);

    return ammo;
}

// 0x46B650
int item_w_primary_mp_cost(Object* weapon)
{
    Proto* proto;

    if (weapon == NULL) {
        return -1;
    }

    proto_ptr(weapon->pid, &proto);
    return proto->item.data.weapon.actionPointCost1;
}

// 0x46B678
int item_w_secondary_mp_cost(Object* weapon)
{
    Proto* proto;

    if (weapon == NULL) {
        return -1;
    }

    proto_ptr(weapon->pid, &proto);
    return proto->item.data.weapon.actionPointCost2;
}

// 0x46B6A0
int item_ar_ac(Object* armor)
{
    Proto* proto;

    if (armor == NULL) {
        return 0;
    }

    proto_ptr(armor->pid, &proto);
    return proto->item.data.armor.armorClass;
}

// 0x46B6C0
int item_ar_dr(Object* armor, int damageType)
{
    Proto* proto;

    if (armor == NULL) {
        return 0;
    }

    proto_ptr(armor->pid, &proto);
    return proto->item.data.armor.damageResistance[damageType];
}

// 0x46B6E0
int item_ar_dt(Object* armor, int damageType)
{
    Proto* proto;

    if (armor == NULL) {
        return 0;
    }

    proto_ptr(armor->pid, &proto);
    return proto->item.data.armor.damageThreshold[damageType];
}

// 0x46B700
int item_ar_perk(Object* armor)
{
    Proto* proto;

    if (armor == NULL) {
        return -1;
    }

    proto_ptr(armor->pid, &proto);
    return proto->item.data.armor.perk;
}

// 0x46B728
int item_ar_male_fid(Object* armor)
{
    Proto* proto;

    if (armor == NULL) {
        return -1;
    }

    proto_ptr(armor->pid, &proto);
    return proto->item.data.armor.maleFid;
}

// 0x46B750
int item_ar_female_fid(Object* armor)
{
    Proto* proto;

    if (armor == NULL) {
        return -1;
    }

    proto_ptr(armor->pid, &proto);
    return proto->item.data.armor.femaleFid;
}

// 0x46B778
int item_m_max_charges(Object* misc_item)
{
    Proto* proto;

    if (misc_item == NULL) {
        return 0;
    }

    proto_ptr(misc_item->pid, &proto);
    return proto->item.data.misc.charges;
}

// 0x46B798
int item_m_curr_charges(Object* misc_item)
{
    if (misc_item == NULL) {
        return 0;
    }

    return misc_item->data.item.misc.charges;
}

// 0x46B7A0
int item_m_set_charges(Object* miscItem, int charges)
{
    // NOTE: Uninline.
    int maxCharges = item_m_max_charges(miscItem);

    if (charges > maxCharges) {
        charges = maxCharges;
    }

    miscItem->data.item.misc.charges = charges;

    return 0;
}

// 0x46B7DC
int item_m_cell(Object* miscItem)
{
    if (miscItem == NULL) {
        return 0;
    }

    Proto* proto;
    proto_ptr(miscItem->pid, &proto);

    return proto->item.data.misc.powerType;
}

// 0x46B7FC
int item_m_cell_pid(Object* miscItem)
{
    if (miscItem == NULL) {
        return -1;
    }

    Proto* proto;
    proto_ptr(miscItem->pid, &proto);

    return proto->item.data.misc.powerTypePid;
}

// 0x46B824
bool item_m_uses_charges(Object* miscItem)
{
    if (miscItem == NULL) {
        return false;
    }

    Proto* proto;
    proto_ptr(miscItem->pid, &proto);

    return proto->item.data.misc.charges != 0;
}

// 0x46B84C
int item_m_use_charged_item(Object* critter, Object* miscItem)
{
    int pid = miscItem->pid;
    if (pid == PROTO_ID_STEALTH_BOY_I
        || pid == PROTO_ID_GEIGER_COUNTER_I
        || pid == PROTO_ID_STEALTH_BOY_II
        || pid == PROTO_ID_GEIGER_COUNTER_II) {
        // NOTE: Uninline.
        bool isOn = item_m_on(miscItem);

        if (isOn) {
            item_m_turn_off(miscItem);
        } else {
            item_m_turn_on(miscItem);
        }
    } else if (pid == PROTO_ID_MOTION_SENSOR) {
        // NOTE: Uninline.
        if (item_m_dec_charges(miscItem) == 0) {
            automap(true, true);
        } else {
            MessageListItem messageListItem;
            // %s has no charges left.
            messageListItem.num = 5;
            if (message_search(&item_message_file, &messageListItem)) {
                char text[80];
                const char* itemName = object_name(miscItem);
                snprintf(text, sizeof(text), messageListItem.text, itemName);
                display_print(text);
            }
        }
    }

    return 0;
}

// 0x46B94C
int item_m_dec_charges(Object* item)
{
    // NOTE: Uninline.
    int charges = item_m_curr_charges(item);
    if (charges <= 0) {
        return -1;
    }

    // NOTE: Uninline.
    item_m_set_charges(item, charges - 1);

    return 0;
}

// 0x46B998
int item_m_trickle(Object* item, void* data)
{
    // NOTE: Uninline.
    if (item_m_dec_charges(item) == 0) {
        int delay;
        if (item->pid == PROTO_ID_STEALTH_BOY_I || item->pid == PROTO_ID_STEALTH_BOY_II) {
            delay = 600;
        } else {
            delay = 3000;
        }

        queue_add(delay, item, NULL, EVENT_TYPE_ITEM_TRICKLE);
    } else {
        Object* critter = obj_top_environment(item);
        if (critter == obj_dude) {
            MessageListItem messageListItem;
            // %s has no charges left.
            messageListItem.num = 5;
            if (message_search(&item_message_file, &messageListItem)) {
                char text[80];
                const char* itemName = object_name(item);
                snprintf(text, sizeof(text), messageListItem.text, itemName);
                display_print(text);
            }
        }
        item_m_turn_off(item);
    }

    return 0;
}

// 0x46BA50
bool item_m_on(Object* obj)
{
    if (obj == NULL) {
        return false;
    }

    if (!item_m_uses_charges(obj)) {
        return false;
    }

    return queue_find(obj, EVENT_TYPE_ITEM_TRICKLE);
}

// Turns on geiger counter or stealth boy.
//
// 0x46BA78
int item_m_turn_on(Object* item)
{
    MessageListItem messageListItem;
    char text[80];

    Object* critter = obj_top_environment(item);
    if (critter == NULL) {
        // This item can only be used from the interface bar.
        messageListItem.num = 9;
        if (message_search(&item_message_file, &messageListItem)) {
            display_print(messageListItem.text);
        }

        return -1;
    }

    // NOTE: Uninline.
    if (item_m_dec_charges(item) != 0) {
        if (critter == obj_dude) {
            messageListItem.num = 5;
            if (message_search(&item_message_file, &messageListItem)) {
                char* name = object_name(item);
                snprintf(text, sizeof(text), messageListItem.text, name);
                display_print(text);
            }
        }

        return -1;
    }

    if (item->pid == PROTO_ID_STEALTH_BOY_I || item->pid == PROTO_ID_STEALTH_BOY_II) {
        queue_add(600, item, 0, EVENT_TYPE_ITEM_TRICKLE);
        item->pid = PROTO_ID_STEALTH_BOY_II;

        if (critter != NULL) {
            // NOTE: Uninline.
            item_m_stealth_effect_on(critter);
        }
    } else {
        queue_add(3000, item, 0, EVENT_TYPE_ITEM_TRICKLE);
        item->pid = PROTO_ID_GEIGER_COUNTER_II;
    }

    if (critter == obj_dude) {
        // %s is on.
        messageListItem.num = 6;
        if (message_search(&item_message_file, &messageListItem)) {
            char* name = object_name(item);
            snprintf(text, sizeof(text), messageListItem.text, name);
            display_print(text);
        }

        if (item->pid == PROTO_ID_GEIGER_COUNTER_II) {
            // You pass the Geiger counter over you body. The rem counter reads: %d
            messageListItem.num = 8;
            if (message_search(&item_message_file, &messageListItem)) {
                int radiation = critter_get_rads(critter);
                snprintf(text, sizeof(text), messageListItem.text, radiation);
                display_print(text);
            }
        }
    }

    return 0;
}

// Turns off geiger counter or stealth boy.
//
// 0x46BC40
int item_m_turn_off(Object* item)
{
    Object* owner = obj_top_environment(item);

    queue_remove_this(item, EVENT_TYPE_ITEM_TRICKLE);

    if (owner != NULL && item->pid == PROTO_ID_STEALTH_BOY_II) {
        item_m_stealth_effect_off(owner, item);
    }

    if (item->pid == PROTO_ID_STEALTH_BOY_I || item->pid == PROTO_ID_STEALTH_BOY_II) {
        item->pid = PROTO_ID_STEALTH_BOY_I;
    } else {
        item->pid = PROTO_ID_GEIGER_COUNTER_I;
    }

    if (owner == obj_dude) {
        intface_update_items(false);
    }

    if (owner == obj_dude) {
        // %s is off.
        MessageListItem messageListItem;
        messageListItem.num = 7;
        if (message_search(&item_message_file, &messageListItem)) {
            const char* name = object_name(item);
            char text[80];
            snprintf(text, sizeof(text), messageListItem.text, name);
            display_print(text);
        }
    }

    return 0;
}

// 0x46BCF4
int item_m_turn_off_from_queue(Object* obj, void* data)
{
    item_m_turn_off(obj);
    return 1;
}

// 0x46BD00
static int item_m_stealth_effect_on(Object* object)
{
    if ((object->flags & OBJECT_TRANS_GLASS) != 0) {
        return -1;
    }

    object->flags |= OBJECT_TRANS_GLASS;

    Rect rect;
    obj_bound(object, &rect);
    tile_refresh_rect(&rect, object->elevation);

    return 0;
}

// 0x46BD38
static int item_m_stealth_effect_off(Object* critter, Object* item)
{
    Object* item1 = inven_left_hand(critter);
    if (item1 != NULL && item1 != item && item1->pid == PROTO_ID_STEALTH_BOY_II) {
        return -1;
    }

    Object* item2 = inven_right_hand(critter);
    if (item2 != NULL && item2 != item && item2->pid == PROTO_ID_STEALTH_BOY_II) {
        return -1;
    }

    if ((critter->flags & OBJECT_TRANS_GLASS) == 0) {
        return -1;
    }

    critter->flags &= ~OBJECT_TRANS_GLASS;

    Rect rect;
    obj_bound(critter, &rect);
    tile_refresh_rect(&rect, critter->elevation);

    return 0;
}

// 0x46BDA0
int item_c_max_size(Object* container)
{
    Proto* proto;

    if (container == NULL) {
        return 0;
    }

    proto_ptr(container->pid, &proto);
    return proto->item.data.container.maxSize;
}

// 0x46BDC0
int item_c_curr_size(Object* container)
{
    if (container == NULL) {
        return 0;
    }

    int totalSize = 0;

    Inventory* inventory = &(container->data.inventory);
    for (int index = 0; index < inventory->length; index++) {
        InventoryItem* inventoryItem = &(inventory->items[index]);

        int size = item_size(inventoryItem->item);
        totalSize += inventory->items[index].quantity * size;
    }

    return totalSize;
}

// 0x46BE20
static int insert_drug_effect(Object* critter, Object* item, int a3, int* stats, int* mods)
{
    int index;
    for (index = 0; index < 3; index++) {
        if (mods[index] != 0) {
            break;
        }
    }

    if (index == 3) {
        return -1;
    }

    DrugEffectEvent* drugEffectEvent = (DrugEffectEvent*)mem_malloc(sizeof(*drugEffectEvent));
    if (drugEffectEvent == NULL) {
        return -1;
    }

    drugEffectEvent->drugPid = item->pid;

    for (index = 0; index < 3; index++) {
        drugEffectEvent->stats[index] = stats[index];
        drugEffectEvent->modifiers[index] = mods[index];
    }

    int delay = 600 * a3;
    if (critter == obj_dude) {
        if (trait_level(TRAIT_CHEM_RESISTANT)) {
            delay /= 2;
        }
    }

    if (queue_add(delay, critter, drugEffectEvent, EVENT_TYPE_DRUG) == -1) {
        mem_free(drugEffectEvent);
        return -1;
    }

    return 0;
}

// 0x46BEEC
static void perform_drug_effect(Object* critter, int* stats, int* mods, bool isImmediate)
{
    int v10;
    int v11;
    int v12;
    MessageListItem messageListItem;
    const char* name;
    const char* text;
    char v24[92]; // TODO: Size is probably wrong.
    char str[92]; // TODO: Size is probably wrong.

    bool statsChanged = false;

    int v5 = 0;
    bool v32 = false;
    if (stats[0] == -2) {
        v5 = 1;
        v32 = true;
    }

    for (int index = v5; index < 3; index++) {
        int stat = stats[index];
        if (stat == -1) {
            continue;
        }

        if (stat == STAT_CURRENT_HIT_POINTS) {
            critter->data.critter.combat.maneuver &= ~CRITTER_MANUEVER_FLEEING;
        }

        v10 = stat_get_bonus(critter, stat);

        int before;
        if (critter == obj_dude) {
            before = stat_level(obj_dude, stat);
        }

        if (v32) {
            v11 = roll_random(mods[index - 1], mods[index]) + v10;
            v32 = false;
        } else {
            v11 = mods[index] + v10;
        }

        if (stat == STAT_CURRENT_HIT_POINTS) {
            v12 = stat_get_base(critter, STAT_CURRENT_HIT_POINTS);
            if (v11 + v12 <= 0 && critter != obj_dude) {
                name = critter_name(critter);
                // %s succumbs to the adverse effects of chems.
                text = getmsg(&item_message_file, &messageListItem, 600);
                snprintf(v24, sizeof(v24), text, name);
            }
        }

        stat_set_bonus(critter, stat, v11);

        if (critter == obj_dude) {
            if (stat == STAT_CURRENT_HIT_POINTS) {
                intface_update_hit_points(true);
            }

            int after = stat_level(critter, stat);
            if (after != before) {
                // 1 - You gained %d %s.
                // 2 - You lost %d %s.
                messageListItem.num = after < before ? 2 : 1;
                if (message_search(&item_message_file, &messageListItem)) {
                    char* statName = stat_name(stat);
                    snprintf(str, sizeof(str), messageListItem.text, after < before ? before - after : after - before, statName);
                    display_print(str);
                    statsChanged = true;
                }
            }
        }
    }

    if (stat_level(critter, STAT_CURRENT_HIT_POINTS) > 0) {
        if (critter == obj_dude && !statsChanged && isImmediate) {
            // Nothing happens.
            messageListItem.num = 10;
            if (message_search(&item_message_file, &messageListItem)) {
                display_print(messageListItem.text);
            }
        }
    } else {
        if (critter == obj_dude) {
            // You suffer a fatal heart attack from chem overdose.
            messageListItem.num = 4;
            if (message_search(&item_message_file, &messageListItem)) {
                strcpy(v24, messageListItem.text);
                // TODO: Why message is ignored?
            }
        } else {
            name = critter_name(critter);
            // %s succumbs to the adverse effects of chems.
            text = getmsg(&item_message_file, &messageListItem, 600);
            snprintf(v24, sizeof(v24), text, name);
            // TODO: Why message is ignored?
        }
    }
}

// 0x46C09C
int item_d_take_drug(Object* critter, Object* item)
{
    Proto* proto;
    int addiction_chance;

    if (critter_is_dead(critter)) {
        return -1;
    }

    if (critter_body_type(critter) == BODY_TYPE_ROBOTIC) {
        return -1;
    }

    proto_ptr(item->pid, &proto);

    wd_obj = critter;
    wd_gvar = pid_to_gvar(item->pid);
    wd_onset = proto->item.data.drug.withdrawalOnset;

    queue_clear_type(EVENT_TYPE_WITHDRAWAL, item_wd_clear_all);
    perform_drug_effect(critter, proto->item.data.drug.stat, proto->item.data.drug.amount, true);
    insert_drug_effect(critter, item, proto->item.data.drug.duration1, proto->item.data.drug.stat, proto->item.data.drug.amount1);
    insert_drug_effect(critter, item, proto->item.data.drug.duration2, proto->item.data.drug.stat, proto->item.data.drug.amount2);

    if (!item_d_check_addict(item->pid)) {
        addiction_chance = proto->item.data.drug.addictionChance;
        if (critter == obj_dude) {
            if (trait_level(TRAIT_CHEM_RELIANT)) {
                addiction_chance *= 2;
            }

            if (trait_level(TRAIT_CHEM_RESISTANT)) {
                addiction_chance /= 2;
            }

            if (perk_level(PERK_FLOWER_CHILD)) {
                addiction_chance /= 2;
            }
        }

        if (roll_random(1, 100) <= addiction_chance) {
            insert_withdrawal(critter, 1, proto->item.data.drug.withdrawalOnset, proto->item.data.drug.withdrawalEffect, item->pid);

            if (critter == obj_dude) {
                // NOTE: Uninline.
                item_d_set_addict(item->pid);
            }
        }
    }

    return 1;
}

// 0x46C22C
int item_d_clear(Object* obj, void* data)
{
    if (isPartyMember(obj)) {
        return 0;
    }

    item_d_process(obj, data);

    return 1;
}

// 0x46C258
int item_d_process(Object* obj, void* data)
{
    DrugEffectEvent* drug_effect_event;

    drug_effect_event = (DrugEffectEvent*)data;
    perform_drug_effect(obj, drug_effect_event->stats, drug_effect_event->modifiers, false);

    if (obj != obj_dude) {
        return 0;
    }

    if ((obj->data.critter.combat.results & DAM_DEAD) == 0) {
        return 0;
    }

    return 1;
}

// 0x46C288
int item_d_load(DB_FILE* stream, void** data)
{
    DrugEffectEvent* drug_effect_event;

    drug_effect_event = (DrugEffectEvent*)mem_malloc(sizeof(*drug_effect_event));
    if (drug_effect_event == NULL) {
        return -1;
    }

    if (db_freadIntCount(stream, drug_effect_event->stats, 3) == -1) {
        mem_free(drug_effect_event);
        return -1;
    }

    if (db_freadIntCount(stream, drug_effect_event->modifiers, 3) == -1) {
        mem_free(drug_effect_event);
        return -1;
    }

    *data = drug_effect_event;
    return 0;
}

// 0x46C30C
int item_d_save(DB_FILE* stream, void* data)
{
    DrugEffectEvent* drugEffectEvent;

    drugEffectEvent = (DrugEffectEvent*)data;
    if (db_fwriteIntCount(stream, drugEffectEvent->stats, 3) == -1) return -1;
    if (db_fwriteIntCount(stream, drugEffectEvent->modifiers, 3) == -1) return -1;

    return 0;
}

// 0x46C348
static int insert_withdrawal(Object* obj, int a2, int duration, int perk, int pid)
{
    WithdrawalEvent* withdrawalEvent = (WithdrawalEvent*)mem_malloc(sizeof(*withdrawalEvent));
    if (withdrawalEvent == NULL) {
        return -1;
    }

    withdrawalEvent->field_0 = a2;
    withdrawalEvent->pid = pid;
    withdrawalEvent->perk = perk;

    if (queue_add(600 * duration, obj, withdrawalEvent, EVENT_TYPE_WITHDRAWAL) == -1) {
        mem_free(withdrawalEvent);
        return -1;
    }

    return 0;
}

// 0x46C3B4
int item_wd_clear(Object* obj, void* data)
{
    WithdrawalEvent* withdrawalEvent = (WithdrawalEvent*)data;

    if (isPartyMember(obj)) {
        return 0;
    }

    if (!withdrawalEvent->field_0) {
        perform_withdrawal_end(obj, withdrawalEvent->perk);
    }

    return 1;
}

// 0x46C40C
static int item_wd_clear_all(Object* obj, void* data)
{
    WithdrawalEvent* withdrawalEvent = (WithdrawalEvent*)data;

    if (obj != wd_obj) {
        return 0;
    }

    if (pid_to_gvar(withdrawalEvent->pid) != wd_gvar) {
        return 0;
    }

    if (!withdrawalEvent->field_0) {
        perform_withdrawal_end(wd_obj, withdrawalEvent->perk);
    }

    insert_withdrawal(obj, 1, wd_onset, withdrawalEvent->perk, withdrawalEvent->pid);

    wd_obj = NULL;

    return 1;
}

// 0x46C4A0
int item_wd_process(Object* obj, void* data)
{
    WithdrawalEvent* withdrawalEvent = (WithdrawalEvent*)data;

    if (withdrawalEvent->field_0) {
        perform_withdrawal_start(obj, withdrawalEvent->perk, withdrawalEvent->pid);
    } else {
        perform_withdrawal_end(obj, withdrawalEvent->perk);

        if (obj == obj_dude) {
            // NOTE: Uninline.
            item_d_unset_addict(withdrawalEvent->pid);
        }
    }

    if (obj == obj_dude) {
        return 1;
    }

    return 0;
}

// 0x46C54C
int item_wd_load(DB_FILE* stream, void** dataPtr)
{
    WithdrawalEvent* withdrawalEvent = (WithdrawalEvent*)mem_malloc(sizeof(*withdrawalEvent));
    if (withdrawalEvent == NULL) {
        return -1;
    }

    if (db_freadInt(stream, &(withdrawalEvent->field_0)) == -1) goto err;
    if (db_freadInt(stream, &(withdrawalEvent->pid)) == -1) goto err;
    if (db_freadInt(stream, &(withdrawalEvent->perk)) == -1) goto err;

    *dataPtr = withdrawalEvent;
    return 0;

err:

    mem_free(withdrawalEvent);
    return -1;
}

// 0x46C5CC
int item_wd_save(DB_FILE* stream, void* data)
{
    WithdrawalEvent* withdrawalEvent = (WithdrawalEvent*)data;

    if (db_fwriteInt(stream, withdrawalEvent->field_0) == -1) return -1;
    if (db_fwriteInt(stream, withdrawalEvent->pid) == -1) return -1;
    if (db_fwriteInt(stream, withdrawalEvent->perk) == -1) return -1;

    return 0;
}

// 0x46C60C
static void perform_withdrawal_start(Object* obj, int perk, int pid)
{
    int duration;

    perk_add_effect(obj, perk);

    if (obj == obj_dude) {
        display_print(perk_description(perk));
    }

    duration = 10080;
    if (obj == obj_dude) {
        if (trait_level(TRAIT_CHEM_RELIANT)) {
            duration /= 2;
        }

        if (perk_level(PERK_FLOWER_CHILD)) {
            duration /= 2;
        }
    }

    insert_withdrawal(obj, 0, duration, perk, pid);
}

// 0x46C678
static void perform_withdrawal_end(Object* obj, int perk)
{
    MessageListItem messageListItem;

    perk_remove_effect(obj, perk);

    if (obj == obj_dude) {
        messageListItem.num = 3;
        if (message_search(&item_message_file, &messageListItem)) {
            display_print(messageListItem.text);
        }
    }
}

// 0x46C6B4
static int pid_to_gvar(int pid)
{
    int index;

    for (index = 0; index < ADDICTION_COUNT; index++) {
        if (drug_pid[index] == pid) {
            return drug_gvar[index];
        }
    }

    return -1;
}

// 0x46C6E8
void item_d_set_addict(int pid)
{
    int gvar;

    // NOTE: Uninline.
    gvar = pid_to_gvar(pid);
    if (gvar != -1) {
        game_global_vars[gvar] = 1;
    }

    pc_flag_on(PC_FLAG_ADDICTED);
}

// 0x46C73C
void item_d_unset_addict(int pid)
{
    int gvar;

    // NOTE: Uninline.
    gvar = pid_to_gvar(pid);
    if (gvar != -1) {
        game_global_vars[gvar] = 0;
    }

    if (!item_d_check_addict(-1)) {
        pc_flag_off(PC_FLAG_ADDICTED);
    }
}

// Returns `true` if dude has addiction to item with given pid or any addition
// if [pid] is -1.
//
// 0x46C79C
bool item_d_check_addict(int pid)
{
    int index;

    for (index = 0; index < ADDICTION_COUNT; index++) {
        if (drug_pid[index] == pid) {
            return game_global_vars[drug_pid[index]] != 0;
        }

        if (pid == -1) {
            if (game_global_vars[drug_pid[index]] != 0) {
                return true;
            }
        }
    }

    return false;
}

// 0x46C804
int item_caps_total(Object* obj)
{
    int amount = 0;

    Inventory* inventory = &(obj->data.inventory);
    for (int i = 0; i < inventory->length; i++) {
        InventoryItem* inventoryItem = &(inventory->items[i]);
        Object* item = inventoryItem->item;

        if (item->pid == PROTO_ID_MONEY) {
            amount += inventoryItem->quantity;
        } else {
            if (item_get_type(item) == ITEM_TYPE_CONTAINER) {
                // recursively collect amount of caps in container
                amount += item_caps_total(item);
            }
        }
    }

    return amount;
}

// 0x46C868
int item_caps_adjust(Object* obj, int amount)
{
    int caps = item_caps_total(obj);
    if (amount < 0 && caps < -amount) {
        return -1;
    }

    if (amount <= 0 || caps != 0) {
        Inventory* inventory = &(obj->data.inventory);

        for (int index = 0; index < inventory->length && amount != 0; index++) {
            InventoryItem* inventoryItem = &(inventory->items[index]);
            Object* item = inventoryItem->item;
            if (item->pid == PROTO_ID_MONEY) {
                if (amount <= 0 && -amount >= inventoryItem->quantity) {
                    obj_erase_object(item, NULL);

                    amount += inventoryItem->quantity;

                    // NOTE: Uninline.
                    item_compact(index, inventory);

                    index = -1;
                } else {
                    inventoryItem->quantity += amount;
                    amount = 0;
                }
            }
        }

        for (int index = 0; index < inventory->length && amount != 0; index++) {
            InventoryItem* inventoryItem = &(inventory->items[index]);
            Object* item = inventoryItem->item;
            if (item_get_type(item) == ITEM_TYPE_CONTAINER) {
                int capsInContainer = item_caps_total(item);
                if (amount <= 0 || capsInContainer <= 0) {
                    if (amount < 0) {
                        if (capsInContainer < -amount) {
                            if (item_caps_adjust(item, capsInContainer) == 0) {
                                amount += capsInContainer;
                            }
                        } else {
                            if (item_caps_adjust(item, amount) == 0) {
                                amount = 0;
                            }
                        }
                    }
                } else {
                    if (item_caps_adjust(item, amount) == 0) {
                        amount = 0;
                    }
                }
            }
        }

        return 0;
    }

    Object* item;
    if (obj_pid_new(&item, PROTO_ID_MONEY) == 0) {
        obj_disconnect(item, NULL);
        if (item_add_force(obj, item, amount) != 0) {
            obj_erase_object(item, NULL);
            return -1;
        }
    }

    return 0;
}

// 0x46CA48
int item_caps_get_amount(Object* item)
{
    if (item->pid != PROTO_ID_MONEY) {
        return -1;
    }

    return item->data.item.misc.charges;
}

// 0x46CA58
int item_caps_set_amount(Object* item, int amount)
{
    if (item->pid != PROTO_ID_MONEY) {
        return -1;
    }

    item->data.item.misc.charges = amount;

    return 0;
}

} // namespace fallout
