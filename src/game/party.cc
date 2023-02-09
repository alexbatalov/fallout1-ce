#include "game/party.h"

#include <stdio.h>
#include <string.h>

#include "game/anim.h"
#include "game/combatai.h"
#include "game/config.h"
#include "game/critter.h"
#include "game/display.h"
#include "game/game.h"
#include "game/gdialog.h"
#include "game/item.h"
#include "game/loadsave.h"
#include "game/map.h"
#include "game/message.h"
#include "game/object.h"
#include "game/protinst.h"
#include "game/proto.h"
#include "game/queue.h"
#include "game/roll.h"
#include "game/scripts.h"
#include "game/skill.h"
#include "game/stat.h"
#include "game/textobj.h"
#include "game/tile.h"
#include "plib/color/color.h"
#include "plib/gnw/debug.h"
#include "plib/gnw/gnw.h"
#include "plib/gnw/memory.h"

namespace fallout {

// TODO: Not sure if the same struct is used in `itemSaveListHead` and
// `partyMemberList`.
typedef struct PartyMember {
    Object* object;
    Script* script;
    int* vars;
    struct PartyMember* next;
} PartyMember;

static Object* partyMemberFindID(int id);
static int partyMemberNewObjID();
static int partyMemberNewObjIDRecurseFind(Object* object, int objectId);
static int partyMemberPrepItemSave(Object* object);
static int partyMemberItemSaveAll(Object* object);
static int partyMemberItemSave(Object* object);
static int partyMemberItemRecover(PartyMember* partyMember);
static int partyMemberItemRecoverAll();
static int partyMemberClearItemList();
static int partyFixMultipleMembers();

// 0x50630C
PartyMember* itemSaveListHead = NULL;

// 0x662824
static PartyMember partyMemberList[20];

// Number of critters added to party.
//
// 0x506310
static int partyMemberCount = 0;

// 0x506314
static int partyMemberItemCount = 20000;

// 0x506318
static int partyStatePrepped = 0;

// 0x485250
int partyMemberAdd(Object* object)
{
    int index;
    PartyMember* partyMember;
    Script* script;

    if (partyMemberCount >= 20) {
        return -1;
    }

    for (index = 0; index < partyMemberCount; index++) {
        partyMember = &(partyMemberList[index]);
        if (partyMember->object == object || partyMember->object->pid == object->pid) {
            return 0;
        }
    }

    if (partyStatePrepped) {
        debug_printf("\npartyMemberAdd DENIED: %s\n", critter_name(object));
        return -1;
    }

    partyMember = &(partyMemberList[partyMemberCount]);
    partyMember->object = object;
    partyMember->script = NULL;
    partyMember->vars = NULL;

    object->id = (object->pid & 0xFFFFFF) + 18000;
    object->flags |= (OBJECT_NO_REMOVE | OBJECT_NO_SAVE);

    partyMemberCount++;

    if (scr_ptr(object->sid, &script) != -1) {
        script->scr_flags |= (SCRIPT_FLAG_0x08 | SCRIPT_FLAG_0x10);
        script->scr_oid = object->id;

        object->sid = ((object->pid & 0xFFFFFF) + 18000) | (SCRIPT_TYPE_CRITTER << 24);
        script->scr_id = object->sid;
    }

    combatai_switch_team(object, 0);

    return 0;
}

// 0x485358
int partyMemberRemove(Object* object)
{
    int index;
    Script* script;

    if (partyMemberCount == 0) {
        return -1;
    }

    if (object == NULL) {
        return -1;
    }

    for (index = 1; index < partyMemberCount; index++) {
        PartyMember* partyMember = &(partyMemberList[index]);
        if (partyMember->object == object) {
            break;
        }
    }

    if (index == partyMemberCount) {
        return -1;
    }

    if (partyStatePrepped) {
        debug_printf("\npartyMemberRemove DENIED: %s\n", critter_name(object));
        return -1;
    }

    if (index < partyMemberCount - 1) {
        partyMemberList[index].object = partyMemberList[partyMemberCount - 1].object;
    }

    object->flags &= ~(OBJECT_NO_REMOVE | OBJECT_NO_SAVE);

    partyMemberCount--;

    if (scr_ptr(object->sid, &script) != -1) {
        script->scr_flags &= ~(SCRIPT_FLAG_0x08 | SCRIPT_FLAG_0x10);
    }

    return 0;
}

// 0x48542C
int partyMemberPrepSave()
{
    int index;
    PartyMember* partyMember;
    Script* script;

    partyStatePrepped = 1;

    for (index = 0; index < partyMemberCount; index++) {
        partyMember = &(partyMemberList[index]);

        if (index > 0) {
            partyMember->object->flags &= ~(OBJECT_NO_REMOVE | OBJECT_NO_SAVE);
        }

        if (scr_ptr(partyMember->object->sid, &script) != -1) {
            script->scr_flags &= ~(SCRIPT_FLAG_0x08 | SCRIPT_FLAG_0x10);
        }
    }

    return 0;
}

// 0x49466C
int partyMemberUnPrepSave()
{
    int index;
    PartyMember* partyMember;
    Script* script;

    for (index = 0; index < partyMemberCount; index++) {
        partyMember = &(partyMemberList[index]);

        if (index > 0) {
            partyMember->object->flags |= (OBJECT_NO_REMOVE | OBJECT_NO_SAVE);
        }

        if (scr_ptr(partyMember->object->sid, &script) != -1) {
            script->scr_flags |= (SCRIPT_FLAG_0x08 | SCRIPT_FLAG_0x10);
        }
    }

    partyStatePrepped = 0;

    return 0;
}

// 0x4854EC
int partyMemberSave(DB_FILE* stream)
{
    int index;
    PartyMember* partyMember;

    if (db_fwriteInt(stream, partyMemberCount) == -1) return -1;
    if (db_fwriteInt(stream, partyMemberItemCount) == -1) return -1;

    for (index = 1; index < partyMemberCount; index++) {
        partyMember = &(partyMemberList[index]);
        if (db_fwriteInt(stream, partyMember->object->id) == -1) return -1;
    }

    return 0;
}

// 0x485554
static Object* partyMemberFindID(int id)
{
    Object* object;

    object = obj_find_first();
    while (object != NULL) {
        if (object->id == id) {
            break;
        }
        object = obj_find_next();
    }

    return object;
}

// 0x485570
int partyMemberPrepLoad()
{
    int index;
    PartyMember* partyMember;
    Script* script;

    if (partyStatePrepped) {
        return -1;
    }

    partyStatePrepped = 1;

    for (index = 0; index < partyMemberCount; index++) {
        partyMember = &(partyMemberList[index]);

        if (scr_ptr(partyMember->object->sid, &script) != -1) {
            partyMember->script = (Script*)mem_malloc(sizeof(*script));
            if (partyMember->script == NULL) {
                GNWSystemError("\n  Error!: partyMemberPrepLoad: Out of memory!");
                exit(1);
            }

            memcpy(partyMember->script, script, sizeof(*script));

            if (script->scr_num_local_vars != 0 && script->scr_local_var_offset != -1) {
                partyMember->vars = (int*)mem_malloc(sizeof(*partyMember->vars) * script->scr_num_local_vars);
                if (partyMember->vars == NULL) {
                    GNWSystemError("\n  Error!: partyMemberPrepLoad: Out of memory!");
                    exit(1);
                }

                memcpy(partyMember->vars, map_local_vars + script->scr_local_var_offset, sizeof(int) * script->scr_num_local_vars);
            }

            // NOTE: Uninline.
            if (partyMemberItemSaveAll(partyMember->object) == -1) {
                GNWSystemError("\n  Error!: partyMemberPrepLoad: partyMemberItemSaveAll failed!");
                exit(1);
            }

            script->scr_flags &= ~(SCRIPT_FLAG_0x08 | SCRIPT_FLAG_0x10);

            scr_remove(script->scr_id);
        } else {
            debug_printf("\n  Error!: partyMemberPrepLoad: Can't find script!");
        }
    }

    return 0;
}

// 0x4856C8
int partyMemberRecoverLoad()
{
    int index;
    PartyMember* partyMember;
    Script* script;
    int sid = -1;

    if (partyStatePrepped != 1) {
        debug_printf("\npartyMemberRecoverLoad DENIED");
        return -1;
    }

    debug_printf("\n");

    for (index = 0; index < partyMemberCount; index++) {
        partyMember = &(partyMemberList[index]);
        if (partyMember->script != NULL) {
            if (scr_new(&sid, SCRIPT_TYPE_CRITTER) == -1) {
                GNWSystemError("\n  Error!: partyMemberRecoverLoad: Can't create script!");
                exit(1);
            }

            if (scr_ptr(sid, &script) == -1) {
                GNWSystemError("\n  Error!: partyMemberRecoverLoad: Can't find script!");
                exit(1);
            }

            memcpy(script, partyMember->script, sizeof(*script));

            partyMember->object->sid = ((partyMember->object->pid & 0xFFFFFF) + 18000) | (SCRIPT_TYPE_CRITTER << 24);
            script->scr_id = partyMember->object->sid;

            script->program = NULL;
            script->scr_flags &= ~(SCRIPT_FLAG_0x01 | SCRIPT_FLAG_0x04);

            mem_free(partyMember->script);
            partyMember->script = NULL;

            script->scr_flags |= (SCRIPT_FLAG_0x08 | SCRIPT_FLAG_0x10);

            if (partyMember->vars != NULL) {
                script->scr_local_var_offset = map_malloc_local_var(script->scr_num_local_vars);
                memcpy(map_local_vars + script->scr_local_var_offset, partyMember->vars, sizeof(int) * script->scr_num_local_vars);
            }

            debug_printf("[Party Member %d]: %s\n", index, critter_name(partyMember->object));
        }
    }

    // NOTE: Uninline.
    if (partyMemberItemRecoverAll() == -1) {
        GNWSystemError("\n  Error!: partyMemberRecoverLoad: Can't recover item scripts!");
        exit(1);
    }

    partyStatePrepped = 0;

    if (!isLoadingGame()) {
        partyFixMultipleMembers();
    }

    return 0;
}

// 0x485888
int partyMemberLoad(DB_FILE* stream)
{
    int objectIds[20];
    int index;
    Object* object;

    if (db_freadInt(stream, &partyMemberCount) == -1) return -1;
    if (db_freadInt(stream, &partyMemberItemCount) == -1) return -1;

    partyMemberList[0].object = obj_dude;

    if (partyMemberCount != 0) {
        for (index = 1; index < partyMemberCount; index++) {
            if (db_freadInt(stream, &(objectIds[index])) == -1) return -1;
        }

        for (index = 1; index < partyMemberCount; index++) {
            object = partyMemberFindID(objectIds[index]);

            if (object == NULL) {
                debug_printf("\n  Error: partyMemberLoad: Can't match ID!");
                return -1;
            }

            partyMemberList[index].object = object;
        }

        if (partyMemberUnPrepSave() == -1) {
            return -1;
        }
    }

    partyFixMultipleMembers();

    return 0;
}

// 0x485978
void partyMemberClear()
{
    int index;

    if (partyStatePrepped) {
        partyMemberUnPrepSave();
    }

    for (index = partyMemberCount; index > 1; index--) {
        partyMemberRemove(partyMemberList[1].object);
    }

    partyMemberCount = 1;

    scr_remove_all();
    partyMemberClearItemList();

    partyStatePrepped = 0;
}

// 0x4859C8
int partyMemberSyncPosition()
{
    int index;
    PartyMember* partyMember;

    for (index = 1; index < partyMemberCount; index++) {
        partyMember = &(partyMemberList[index]);
        if ((partyMember->object->flags & OBJECT_HIDDEN) == 0) {
            obj_attempt_placement(partyMember->object, obj_dude->tile, obj_dude->elevation, 2);
        }
    }

    return 0;
}

// Heals party members according to their healing rate.
//
// 0x485A18
int partyMemberRestingHeal(int a1)
{
    int v1;
    int index;
    PartyMember* partyMember;
    int healingRate;

    v1 = a1 / 3;
    if (v1 == 0) {
        return 0;
    }

    for (index = 0; index < partyMemberCount; index++) {
        partyMember = &(partyMemberList[index]);
        if (PID_TYPE(partyMember->object->pid) == OBJ_TYPE_CRITTER) {
            healingRate = stat_level(partyMember->object, STAT_HEALING_RATE);
            critter_adjust_hits(partyMember->object, v1 * healingRate);
        }
    }

    return 1;
}

// 0x485A78
Object* partyMemberFindObjFromPid(int pid)
{
    int index;
    Object* object;

    for (index = 0; index < partyMemberCount; index++) {
        object = partyMemberList[index].object;
        if (object->pid == pid) {
            return object;
        }
    }

    return NULL;
}

// Returns `true` if specified object is a party member.
//
// 0x485AAC
bool isPartyMember(Object* object)
{
    int index;

    if (object->id < 18000) {
        return false;
    }

    for (index = 0; index < partyMemberCount; index++) {
        if (partyMemberList[index].object == object) {
            return true;
        }
    }

    return false;
}

// Returns number of active critters in the party.
//
// 0x485AE8
int getPartyMemberCount()
{
    int index;
    Object* object;
    int count = partyMemberCount;

    for (index = 1; index < partyMemberCount; index++) {
        object = partyMemberList[index].object;
        if ((object->flags & OBJECT_HIDDEN) != 0) {
            count--;
        }
    }

    return count;
}

// 0x485B1C
static int partyMemberNewObjID()
{
    // 0x50631C
    static int curID = 20000;

    Object* object;

    do {
        curID++;

        object = obj_find_first();
        while (object != NULL) {
            if (object->id == curID) {
                break;
            }

            Inventory* inventory = &(object->data.inventory);

            int index;
            for (index = 0; index < inventory->length; index++) {
                InventoryItem* inventoryItem = &(inventory->items[index]);
                Object* item = inventoryItem->item;
                if (item->id == curID) {
                    break;
                }

                if (partyMemberNewObjIDRecurseFind(item, curID)) {
                    break;
                }
            }

            if (index < inventory->length) {
                break;
            }

            object = obj_find_next();
        }
    } while (object != NULL);

    curID++;

    return curID;
}

// 0x485BA0
static int partyMemberNewObjIDRecurseFind(Object* obj, int objectId)
{
    Inventory* inventory = &(obj->data.inventory);
    for (int index = 0; index < inventory->length; index++) {
        InventoryItem* inventoryItem = &(inventory->items[index]);
        if (inventoryItem->item->id == objectId) {
            return 1;
        }

        if (partyMemberNewObjIDRecurseFind(inventoryItem->item, objectId)) {
            return 1;
        }
    }

    return 0;
}

// 0x485BEC
int partyMemberPrepItemSaveAll()
{
    int partyMemberIndex;
    Object* object;
    Inventory* inventory;
    int inventoryItemIndex;

    for (partyMemberIndex = 0; partyMemberIndex < partyMemberCount; partyMemberIndex++) {
        object = partyMemberList[partyMemberIndex].object;

        inventory = &(object->data.inventory);
        for (inventoryItemIndex = 0; inventoryItemIndex < inventory->length; inventoryItemIndex++) {
            partyMemberPrepItemSave(inventory->items[inventoryItemIndex].item);
        }
    }

    return 0;
}

// 0x485C40
static int partyMemberPrepItemSave(Object* object)
{
    if (object->sid != -1) {
        Script* script;
        if (scr_ptr(object->sid, &script) == -1) {
            GNWSystemError("\n  Error!: partyMemberPrepItemSaveAll: Can't find script!");
            exit(1);
        }

        script->scr_flags |= (SCRIPT_FLAG_0x08 | SCRIPT_FLAG_0x10);
    }

    Inventory* inventory = &(object->data.inventory);
    for (int index = 0; index < inventory->length; index++) {
        InventoryItem* inventoryItem = &(inventory->items[index]);
        partyMemberPrepItemSave(inventoryItem->item);
    }

    return 0;
}

// 0x485CAC
static int partyMemberItemSaveAll(Object* object)
{
    Inventory* inventory;
    InventoryItem* inventoryItem;
    int index;

    inventory = &(object->data.inventory);
    for (index = 0; index < inventory->length; index++) {
        inventoryItem = &(inventory->items[index]);
        partyMemberItemSave(inventoryItem->item);
    }

    return 0;
}

// 0x485CDC
static int partyMemberItemSave(Object* object)
{
    Script* script;
    PartyMember* node;
    PartyMember* temp;
    Inventory* inventory;
    int index;

    if (object->sid != -1) {
        if (scr_ptr(object->sid, &script) == -1) {
            GNWSystemError("\n  Error!: partyMemberItemSave: Can't find script!");
            exit(1);
        }

        if (object->id < 20000) {
            script->scr_oid = partyMemberNewObjID();
            object->id = script->scr_oid;
        }

        node = (PartyMember*)mem_malloc(sizeof(*node));
        if (node == NULL) {
            GNWSystemError("\n  Error!: partyMemberItemSave: Out of memory!");
            exit(1);
        }

        node->object = object;

        node->script = (Script*)mem_malloc(sizeof(*script));
        if (node->script == NULL) {
            GNWSystemError("\n  Error!: partyMemberItemSave: Out of memory!");
            exit(1);
        }

        memcpy(node->script, script, sizeof(*script));

        if (script->scr_num_local_vars != 0 && script->scr_local_var_offset != -1) {
            node->vars = (int*)mem_malloc(sizeof(*node->vars) * script->scr_num_local_vars);
            if (node->vars == NULL) {
                GNWSystemError("\n  Error!: partyMemberItemSave: Out of memory!");
                exit(1);
            }

            memcpy(node->vars, map_local_vars + script->scr_local_var_offset, sizeof(int) * script->scr_num_local_vars);
        } else {
            node->vars = NULL;
        }

        temp = itemSaveListHead;
        itemSaveListHead = node;
        node->next = temp;
    }

    inventory = &(object->data.inventory);
    for (index = 0; index < inventory->length; index++) {
        partyMemberItemSave(inventory->items[index].item);
    }

    return 0;
}

// 0x485E30
static int partyMemberItemRecover(PartyMember* partyMember)
{
    int sid = -1;
    Script* script;

    if (scr_new(&sid, SCRIPT_TYPE_ITEM) == -1) {
        GNWSystemError("\n  Error!: partyMemberItemRecover: Can't create script!");
        exit(1);
    }

    if (scr_ptr(sid, &script) == -1) {
        GNWSystemError("\n  Error!: partyMemberItemRecover: Can't find script!");
        exit(1);
    }

    memcpy(script, partyMember->script, sizeof(*script));

    partyMember->object->sid = partyMemberItemCount | (SCRIPT_TYPE_ITEM << 24);
    script->scr_id = partyMemberItemCount | (SCRIPT_TYPE_ITEM << 24);

    script->program = NULL;
    script->scr_flags &= ~(SCRIPT_FLAG_0x01 | SCRIPT_FLAG_0x04 | SCRIPT_FLAG_0x08 | SCRIPT_FLAG_0x10);

    partyMemberItemCount++;

    mem_free(partyMember->script);
    partyMember->script = NULL;

    if (partyMember->vars != NULL) {
        script->scr_local_var_offset = map_malloc_local_var(script->scr_num_local_vars);
        memcpy(map_local_vars + script->scr_local_var_offset, partyMember->vars, sizeof(int) * script->scr_num_local_vars);
    }

    return 0;
}

// 0x485F38
static int partyMemberItemRecoverAll()
{
    PartyMember* partyMember;

    while (itemSaveListHead != NULL) {
        partyMember = itemSaveListHead;
        itemSaveListHead = itemSaveListHead->next;
        partyMemberItemRecover(partyMember);
        mem_free(partyMember);
    }

    return 0;
}

// 0x485F6C
static int partyMemberClearItemList()
{
    PartyMember* node;

    while (itemSaveListHead != NULL) {
        node = itemSaveListHead;
        itemSaveListHead = itemSaveListHead->next;

        if (node->script != NULL) {
            mem_free(node->script);
        }

        if (node->vars != NULL) {
            mem_free(node->vars);
        }

        mem_free(node);
    }

    partyMemberItemCount = 20000;

    return 0;
}

// 0x485FC8
static int partyFixMultipleMembers()
{
    Object* object;
    int critterCount;
    bool v1;
    bool v2;
    Object* candidate;
    int index;
    Script* script;

    debug_printf("\n\n\n[Party Members]:");

    critterCount = 0;

    // TODO: This loop is wrong. Looks like it can restart itself from the
    // beginning. Probably was implemented with two nested loops.
    object = obj_find_first();
    while (object != NULL) {
        v1 = false;

        if (PID_TYPE(object->pid) == OBJ_TYPE_CRITTER) {
            critterCount++;
        }

        switch (object->pid) {
        case 0x100004C:
        case 0x100007A:
        case 0x10000D2:
        case 0x100003F:
        case 0x100012E:
            v1 = true;
            break;
        }

        if (v1) {
            debug_printf("\n   PM: %s", critter_name(object));

            v2 = false;
            if (object->sid != -1) {
                candidate = partyMemberFindObjFromPid(object->pid);
                if (candidate != NULL && candidate != object) {
                    if (candidate->sid != object->sid) {
                        object->sid = -1;
                    }
                    v2 = true;
                }
            } else {
                v2 = true;
            }

            if (v2) {
                candidate = partyMemberFindObjFromPid(object->pid);
                if (candidate != object) {
                    debug_printf("\nDestroying evil critter doppleganger!");

                    if (object->sid != -1) {
                        scr_remove(object->sid);
                        object->sid = -1;
                    } else {
                        if (queue_remove_this(object, EVENT_TYPE_SCRIPT) == -1) {
                            debug_printf("\nERROR Removing Timed Events on FIX remove!!\n");
                        }
                    }

                    obj_erase_object(object, NULL);
                } else {
                    debug_printf("\nError: Attempting to destroy evil critter doppleganger FAILED!");
                }
            }
        }

        object = obj_find_next();
    }

    for (index = 0; index < partyMemberCount; index++) {
        object = partyMemberList[index].object;

        if (scr_ptr(object->sid, &script) != -1) {
            script->owner = object;
        } else {
            debug_printf("\nError: Failed to fix party member critter scripts!");
        }
    }

    debug_printf("\nTotal Critter Count: %d\n\n", critterCount);

    return 0;
}

} // namespace fallout
