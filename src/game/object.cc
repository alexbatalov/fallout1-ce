#include "game/object.h"

#include <assert.h>
#include <string.h>

#include <algorithm>

#include "game/anim.h"
#include "game/art.h"
#include "game/combat.h"
#include "game/critter.h"
#include "game/game.h"
#include "game/gconfig.h"
#include "game/gmouse.h"
#include "game/item.h"
#include "game/light.h"
#include "game/map.h"
#include "game/party.h"
#include "game/protinst.h"
#include "game/proto.h"
#include "game/scripts.h"
#include "game/textobj.h"
#include "game/tile.h"
#include "game/worldmap.h"
#include "plib/color/color.h"
#include "plib/gnw/debug.h"
#include "plib/gnw/grbuf.h"
#include "plib/gnw/input.h"
#include "plib/gnw/memory.h"
#include "plib/gnw/svga.h"

namespace fallout {

static int obj_read_obj(Object* obj, DB_FILE* stream);
static int obj_load_func(DB_FILE* stream);
static void obj_fix_combat_cid_for_dude();
static void object_fix_weapon_ammo(Object* obj);
static int obj_write_obj(Object* obj, DB_FILE* stream);
static int obj_object_table_init();
static int obj_offset_table_init();
static void obj_offset_table_exit();
static int obj_order_table_init();
static int obj_order_comp_func_even(const void* a1, const void* a2);
static int obj_order_comp_func_odd(const void* a1, const void* a2);
static void obj_order_table_exit();
static int obj_render_table_init();
static void obj_render_table_exit();
static void obj_light_table_init();
static void obj_blend_table_init();
static void obj_blend_table_exit();
static void obj_misc_table_init();
static int obj_create_object(Object** objectPtr);
static void obj_destroy_object(Object** objectPtr);
static int obj_create_object_node(ObjectListNode** nodePtr);
static void obj_destroy_object_node(ObjectListNode** nodePtr);
static int obj_node_ptr(Object* obj, ObjectListNode** out_node, ObjectListNode** out_prev_node);
static void obj_insert(ObjectListNode* ptr);
static int obj_remove(ObjectListNode* a1, ObjectListNode* a2);
static int obj_connect_to_tile(ObjectListNode* node, int tile_index, int elev, Rect* rect);
static int obj_adjust_light(Object* obj, int a2, Rect* rect);
static void obj_render_outline(Object* object, Rect* rect);
static void obj_render_object(Object* object, Rect* rect, int light);
static int obj_preload_sort(const void* a1, const void* a2);

// 0x505B70
static bool objInitialized = false;

// 0x505B74
static int updateHexWidth = 0;

// 0x505B78
static int updateHexHeight = 0;

// 0x505B7C
static int updateHexArea = 0;

// 0x505B80
static int* orderTable[2] = {
    NULL,
    NULL,
};

// 0x505B88
static int* offsetTable[2] = {
    NULL,
    NULL,
};

// 0x505B90
static int* offsetDivTable = NULL;

// 0x505B94
static int* offsetModTable = NULL;

// 0x505B98
static ObjectListNode** renderTable = NULL;

// 0x505B9C
static int outlineCount = 0;

// Contains objects that are not bounded to tiles.
//
// 0x505BA0
static ObjectListNode* floatingObjects = NULL;

// 0x505BA4
static int centerToUpperLeft = 0;

// 0x505BA8
static int find_elev = 0;

// 0x505BAC
static int find_tile = 0;

// 0x505BB0
static ObjectListNode* find_ptr = NULL;

// 0x505BB4
static int* preload_list = NULL;

// 0x505BB8
static int preload_list_index = 0;

// 0x505BBC
static int translucence = 0;

// 0x505BC0
static int highlight_fid = -1;

// 0x505BC4
static Rect light_rect[9] = {
    { 0, 0, 96, 42 },
    { 0, 0, 160, 74 },
    { 0, 0, 224, 106 },
    { 0, 0, 288, 138 },
    { 0, 0, 352, 170 },
    { 0, 0, 416, 202 },
    { 0, 0, 480, 234 },
    { 0, 0, 544, 266 },
    { 0, 0, 608, 298 },
};

// 0x505C54
static int light_distance[36] = {
    1,
    2,
    3,
    4,
    5,
    6,
    7,
    8,
    2,
    3,
    4,
    5,
    6,
    7,
    8,
    3,
    4,
    5,
    6,
    7,
    8,
    4,
    5,
    6,
    7,
    8,
    5,
    6,
    7,
    8,
    6,
    7,
    8,
    7,
    8,
    8,
};

// 0x505CE4
static int fix_violence_level = -1;

// 0x505CE8
static int obj_last_roof_x = -1;

// 0x505CEC
static int obj_last_roof_y = -1;

// 0x505CF0
static int obj_last_elev = -1;

// 0x505CF4
static bool obj_last_is_empty = true;

// 0x505CF8
unsigned char* wallBlendTable = NULL;

// 0x505CFC
unsigned char* glassBlendTable = NULL;

// 0x505D00
unsigned char* steamBlendTable = NULL;

// 0x505D04
unsigned char* energyBlendTable = NULL;

// 0x505D08
unsigned char* redBlendTable = NULL;

// 0x637730
static int light_blocked[6][36];

// 0x637A90
static int light_offsets[2][6][36];

// 0x638150
static Object* outlinedObjects[100];

// 0x6382E0
static Rect buf_rect;

// Contains objects that are bounded to tiles.
//
// 0x6382F0
static ObjectListNode* objectTable[HEX_GRID_SIZE];

// 0x65F3F0
static Rect updateAreaPixelBounds;

// 0x65F400
unsigned char glassGrayTable[256];

// 0x65F500
unsigned char commonGrayTable[256];

// Translucent "egg" effect around player.
//
// 0x65F600
Object* obj_egg;

// 0x65F604
static int buf_size;

// 0x65F608
static unsigned char* back_buf;

// 0x65F60C
static int buf_length;

// 0x65F610
static int buf_full;

// 0x65F614
static int buf_width;

// 0x65F618
Object* obj_dude;

// 0x65F61C
static char obj_seen_check[5001];

// 0x6609A5
static char obj_seen[5001];

// 0x47A590
int obj_init(unsigned char* buf, int width, int height, int pitch)
{
    int dudeFid;
    int eggFid;

    memset(obj_seen, 0, 5001);
    updateAreaPixelBounds.lrx = width + 320;
    updateAreaPixelBounds.ulx = -320;
    updateAreaPixelBounds.lry = height + 240;
    updateAreaPixelBounds.uly = -240;

    updateHexWidth = (updateAreaPixelBounds.lrx + 320 + 1) / 32 + 1;
    updateHexHeight = (updateAreaPixelBounds.lry + 240 + 1) / 12 + 1;
    updateHexArea = updateHexWidth * updateHexHeight;

    if (obj_object_table_init() == -1) {
        return -1;
    }

    if (obj_offset_table_init() == -1) {
        return -1;
    }

    if (obj_order_table_init() == -1) {
        obj_offset_table_exit();
        return -1;
    }

    if (obj_render_table_init() == -1) {
        obj_order_table_exit();
        obj_offset_table_exit();
        return -1;
    }

    if (light_init() == -1) {
        obj_order_table_exit();
        obj_offset_table_exit();
        return -1;
    }

    if (text_object_init(buf, width, height) == -1) {
        obj_order_table_exit();
        obj_offset_table_exit();
        return -1;
    }

    obj_light_table_init();
    obj_blend_table_init();
    obj_misc_table_init();

    buf_width = width;
    buf_length = height;
    back_buf = buf;

    buf_rect.ulx = 0;
    buf_rect.uly = 0;
    buf_rect.lrx = width - 1;
    buf_rect.lry = height - 1;

    buf_size = height * width;
    buf_full = pitch;

    dudeFid = art_id(OBJ_TYPE_CRITTER, art_vault_guy_num, 0, 0, 0);
    obj_new(&obj_dude, dudeFid, 0x1000000);

    obj_dude->flags |= OBJECT_NO_REMOVE;
    obj_dude->flags |= OBJECT_NO_SAVE;
    obj_dude->flags |= OBJECT_HIDDEN;
    obj_dude->flags |= OBJECT_LIGHT_THRU;
    obj_set_light(obj_dude, 4, 0x10000, NULL);

    if (partyMemberAdd(obj_dude) == -1) {
        debug_printf("\n  Error: Can't add Player into party!");
        exit(1);
    }

    eggFid = art_id(OBJ_TYPE_INTERFACE, 2, 0, 0, 0);
    obj_new(&obj_egg, eggFid, -1);
    obj_egg->flags |= OBJECT_NO_REMOVE;
    obj_egg->flags |= OBJECT_NO_SAVE;
    obj_egg->flags |= OBJECT_HIDDEN;
    obj_egg->flags |= OBJECT_LIGHT_THRU;

    objInitialized = true;

    return 0;
}

// 0x47A810
void obj_reset()
{
    if (objInitialized) {
        text_object_reset();
        obj_remove_all();
        memset(obj_seen, 0, 5001);
        light_reset();
    }
}

// 0x47A840
void obj_exit()
{
    if (objInitialized) {
        obj_dude->flags &= ~OBJECT_NO_REMOVE;
        obj_egg->flags &= ~OBJECT_NO_REMOVE;

        obj_remove_all();
        text_object_exit();

        // NOTE: Uninline.
        obj_blend_table_exit();

        light_exit();

        // NOTE: Uninline.
        obj_render_table_exit();

        // NOTE: Uninline.
        obj_order_table_exit();

        obj_offset_table_exit();
    }
}

// 0x47A904
static int obj_read_obj(Object* obj, DB_FILE* stream)
{
    int field_74;

    if (db_freadInt(stream, &(obj->id)) == -1) return -1;
    if (db_freadInt(stream, &(obj->tile)) == -1) return -1;
    if (db_freadInt(stream, &(obj->x)) == -1) return -1;
    if (db_freadInt(stream, &(obj->y)) == -1) return -1;
    if (db_freadInt(stream, &(obj->sx)) == -1) return -1;
    if (db_freadInt(stream, &(obj->sy)) == -1) return -1;
    if (db_freadInt(stream, &(obj->frame)) == -1) return -1;
    if (db_freadInt(stream, &(obj->rotation)) == -1) return -1;
    if (db_freadInt(stream, &(obj->fid)) == -1) return -1;
    if (db_freadInt(stream, &(obj->flags)) == -1) return -1;
    if (db_freadInt(stream, &(obj->elevation)) == -1) return -1;
    if (db_freadInt(stream, &(obj->pid)) == -1) return -1;
    if (db_freadInt(stream, &(obj->cid)) == -1) return -1;
    if (db_freadInt(stream, &(obj->lightDistance)) == -1) return -1;
    if (db_freadInt(stream, &(obj->lightIntensity)) == -1) return -1;
    if (db_freadInt(stream, &field_74) == -1) return -1;
    if (db_freadInt(stream, &(obj->sid)) == -1) return -1;
    if (db_freadInt(stream, &(obj->field_80)) == -1) return -1;

    obj->outline = 0;
    obj->owner = NULL;

    if (proto_read_protoUpdateData(obj, stream) != 0) {
        return -1;
    }

    if (obj->pid < 0x5000010 || obj->pid > 0x5000017) {
        if (PID_TYPE(obj->pid) == 0 && !(map_data.flags & 0x01)) {
            object_fix_weapon_ammo(obj);
        }
    } else {
        if (obj->data.misc.map <= 0) {
            if ((obj->fid & 0xFFF) < 33) {
                obj->fid = art_id(OBJ_TYPE_MISC, (obj->fid & 0xFFF) + 16, FID_ANIM_TYPE(obj->fid), 0, 0);
            }
        }
    }

    return 0;
}

// 0x47AAF4
int obj_load(DB_FILE* stream)
{
    int rc = obj_load_func(stream);

    fix_violence_level = -1;

    return rc;
}

// 0x47AB08
static int obj_load_func(DB_FILE* stream)
{
    if (stream == NULL) {
        return -1;
    }

    bool fixMapInventory;
    if (!configGetBool(&game_config, GAME_CONFIG_MAPPER_KEY, GAME_CONFIG_FIX_MAP_INVENTORY_KEY, &fixMapInventory)) {
        fixMapInventory = false;
    }

    if (!config_get_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_VIOLENCE_LEVEL_KEY, &fix_violence_level)) {
        fix_violence_level = VIOLENCE_LEVEL_MAXIMUM_BLOOD;
    }

    int objectCount;
    if (db_freadInt(stream, &objectCount) == -1) {
        return -1;
    }

    if (preload_list != NULL) {
        mem_free(preload_list);
    }

    if (objectCount != 0) {
        preload_list = (int*)mem_malloc(sizeof(*preload_list) * objectCount);
        if (preload_list == NULL) {
            return -1;
        }
        preload_list_index = 0;
    }

    for (int elevation = 0; elevation < ELEVATION_COUNT; elevation++) {
        int objectCountAtElevation;
        if (db_freadInt(stream, &objectCountAtElevation) == -1) {
            return -1;
        }

        for (int objectIndex = 0; objectIndex < objectCountAtElevation; objectIndex++) {
            ObjectListNode* objectListNode;

            // NOTE: Uninline.
            if (obj_create_object_node(&objectListNode) == -1) {
                return -1;
            }

            if (obj_create_object(&(objectListNode->obj)) == -1) {
                // NOTE: Uninline.
                obj_destroy_object_node(&objectListNode);
                return -1;
            }

            if (obj_read_obj(objectListNode->obj, stream) != 0) {
                // NOTE: Uninline.
                obj_destroy_object(&(objectListNode->obj));

                // NOTE: Uninline.
                obj_destroy_object_node(&objectListNode);

                return -1;
            }

            objectListNode->obj->outline = 0;
            preload_list[preload_list_index++] = objectListNode->obj->fid;

            if (objectListNode->obj->sid != -1) {
                Script* script;
                if (scr_ptr(objectListNode->obj->sid, &script) == -1) {
                    objectListNode->obj->sid = -1;
                    debug_printf("\nError connecting object to script!");
                } else {
                    script->owner = objectListNode->obj;
                    objectListNode->obj->field_80 = script->scr_script_idx;
                }
            }

            obj_fix_violence_settings(&(objectListNode->obj->fid));
            objectListNode->obj->elevation = elevation;

            obj_insert(objectListNode);

            if ((objectListNode->obj->flags & OBJECT_NO_REMOVE) && PID_TYPE(objectListNode->obj->pid) == OBJ_TYPE_CRITTER && objectListNode->obj->pid != 18000) {
                objectListNode->obj->flags &= ~OBJECT_NO_REMOVE;
            }

            Inventory* inventory = &(objectListNode->obj->data.inventory);
            if (inventory->length != 0) {
                inventory->items = (InventoryItem*)mem_malloc(sizeof(InventoryItem) * inventory->capacity);
                if (inventory->items == NULL) {
                    return -1;
                }

                for (int inventoryItemIndex = 0; inventoryItemIndex < inventory->length; inventoryItemIndex++) {
                    InventoryItem* inventoryItem = &(inventory->items[inventoryItemIndex]);
                    if (db_freadInt(stream, &(inventoryItem->quantity)) != 0) {
                        debug_printf("Error loading inventory\n");
                        return -1;
                    }

                    if (fixMapInventory) {
                        inventoryItem->item = (Object*)mem_malloc(sizeof(Object));
                        if (inventoryItem->item == NULL) {
                            debug_printf("Error loading inventory\n");
                            return -1;
                        }

                        if (obj_read_obj(inventoryItem->item, stream) != 0) {
                            debug_printf("Error loading inventory\n");
                            return -1;
                        }
                    } else {
                        if (obj_load_obj(stream, &(inventoryItem->item), elevation, objectListNode->obj) == -1) {
                            return -1;
                        }
                    }
                }
            } else {
                inventory->capacity = 0;
                inventory->items = NULL;
            }
        }
    }

    obj_rebuild_all_light();

    return 0;
}

// 0x47AE9C
static void obj_fix_combat_cid_for_dude()
{
    Object** critterList;
    int critterListLength = obj_create_list(-1, map_elevation, OBJ_TYPE_CRITTER, &critterList);

    if (obj_dude->data.critter.combat.whoHitMeCid == -1) {
        obj_dude->data.critter.combat.whoHitMe = NULL;
    } else {
        int index = find_cid(0, obj_dude->data.critter.combat.whoHitMeCid, critterList, critterListLength);
        if (index != critterListLength) {
            obj_dude->data.critter.combat.whoHitMe = critterList[index];
        } else {
            obj_dude->data.critter.combat.whoHitMe = NULL;
        }
    }

    if (critterListLength != 0) {
        // NOTE: Uninline.
        obj_delete_list(critterList);
    }
}

// Fixes ammo pid and number of charges.
//
// 0x47AF1C
static void object_fix_weapon_ammo(Object* obj)
{
    if (PID_TYPE(obj->pid) != OBJ_TYPE_ITEM) {
        return;
    }

    Proto* proto;
    if (proto_ptr(obj->pid, &proto) == -1) {
        debug_printf("\nError: obj_load: proto_ptr failed on pid");
        exit(1);
    }

    int charges;
    if (item_get_type(obj) == ITEM_TYPE_WEAPON) {
        int ammoTypePid = obj->data.item.weapon.ammoTypePid;
        if (ammoTypePid == 0xCCCCCCCC || ammoTypePid == -1) {
            obj->data.item.weapon.ammoTypePid = proto->item.data.weapon.ammoTypePid;
        }

        charges = obj->data.item.weapon.ammoQuantity;
        if (charges == 0xCCCCCCCC || charges == -1 || charges != proto->item.data.weapon.ammoCapacity) {
            obj->data.item.weapon.ammoQuantity = proto->item.data.weapon.ammoCapacity;
        }
    } else {
        if (PID_TYPE(obj->pid) == OBJ_TYPE_MISC) {
            // FIXME: looks like this code in unreachable
            charges = obj->data.item.misc.charges;
            if (charges == 0xCCCCCCCC) {
                charges = proto->item.data.misc.charges;
                obj->data.item.misc.charges = charges;
                if (charges == 0xCCCCCCCC) {
                    debug_printf("\nError: Misc Item Prototype %s: charges incorrect!", proto_name(obj->pid));
                    obj->data.item.misc.charges = 0;
                }
            } else {
                if (charges != proto->item.data.misc.charges) {
                    obj->data.item.misc.charges = proto->item.data.misc.charges;
                }
            }
        }
    }
}

// 0x47B000
static int obj_write_obj(Object* obj, DB_FILE* stream)
{
    if (db_fwriteInt(stream, obj->id) == -1) return -1;
    if (db_fwriteInt(stream, obj->tile) == -1) return -1;
    if (db_fwriteInt(stream, obj->x) == -1) return -1;
    if (db_fwriteInt(stream, obj->y) == -1) return -1;
    if (db_fwriteInt(stream, obj->sx) == -1) return -1;
    if (db_fwriteInt(stream, obj->sy) == -1) return -1;
    if (db_fwriteInt(stream, obj->frame) == -1) return -1;
    if (db_fwriteInt(stream, obj->rotation) == -1) return -1;
    if (db_fwriteInt(stream, obj->fid) == -1) return -1;
    if (db_fwriteInt(stream, obj->flags) == -1) return -1;
    if (db_fwriteInt(stream, obj->elevation) == -1) return -1;
    if (db_fwriteInt(stream, obj->pid) == -1) return -1;
    if (db_fwriteInt(stream, obj->cid) == -1) return -1;
    if (db_fwriteInt(stream, obj->lightDistance) == -1) return -1;
    if (db_fwriteInt(stream, obj->lightIntensity) == -1) return -1;
    if (db_fwriteInt(stream, obj->outline) == -1) return -1;
    if (db_fwriteInt(stream, obj->sid) == -1) return -1;
    if (db_fwriteInt(stream, obj->field_80) == -1) return -1;
    if (proto_write_protoUpdateData(obj, stream) == -1) return -1;

    return 0;
}

// 0x47B15C
int obj_save(DB_FILE* stream)
{
    if (stream == NULL) {
        return -1;
    }

    obj_process_seen();

    int objectCount = 0;

    long objectCountPos = db_ftell(stream);
    if (db_fwriteInt(stream, objectCount) == -1) {
        return -1;
    }

    for (int elevation = 0; elevation < ELEVATION_COUNT; elevation++) {
        int objectCountAtElevation = 0;

        long objectCountAtElevationPos = db_ftell(stream);
        if (db_fwriteInt(stream, objectCountAtElevation) == -1) {
            return -1;
        }

        for (int tile = 0; tile < HEX_GRID_SIZE; tile++) {
            for (ObjectListNode* objectListNode = objectTable[tile]; objectListNode != NULL; objectListNode = objectListNode->next) {
                Object* object = objectListNode->obj;
                if (object->elevation != elevation) {
                    continue;
                }

                if ((object->flags & OBJECT_NO_SAVE) != 0) {
                    continue;
                }

                CritterCombatData* combatData = NULL;
                Object* whoHitMe = NULL;
                if (PID_TYPE(object->pid) == OBJ_TYPE_CRITTER) {
                    combatData = &(object->data.critter.combat);
                    whoHitMe = combatData->whoHitMe;
                    if (whoHitMe != 0) {
                        if (combatData->whoHitMeCid != -1) {
                            combatData->whoHitMeCid = whoHitMe->cid;
                        }
                    } else {
                        combatData->whoHitMeCid = -1;
                    }
                }

                if (obj_write_obj(object, stream) == -1) {
                    return -1;
                }

                if (PID_TYPE(object->pid) == OBJ_TYPE_CRITTER) {
                    combatData->whoHitMe = whoHitMe;
                }

                Inventory* inventory = &(object->data.inventory);
                for (int index = 0; index < inventory->length; index++) {
                    InventoryItem* inventoryItem = &(inventory->items[index]);

                    if (db_fwriteInt(stream, inventoryItem->quantity) == -1) {
                        return -1;
                    }

                    if (obj_save_obj(stream, inventoryItem->item) == -1) {
                        return -1;
                    }
                }

                objectCountAtElevation++;
            }
        }

        long pos = db_ftell(stream);
        db_fseek(stream, objectCountAtElevationPos, SEEK_SET);
        db_fwriteInt(stream, objectCountAtElevation);
        db_fseek(stream, pos, SEEK_SET);

        objectCount += objectCountAtElevation;
    }

    long pos = db_ftell(stream);
    db_fseek(stream, objectCountPos, SEEK_SET);
    db_fwriteInt(stream, objectCount);
    db_fseek(stream, pos, SEEK_SET);

    return 0;
}

// 0x47B350
void obj_render_pre_roof(Rect* rect, int elevation)
{
    if (!objInitialized) {
        return;
    }

    Rect updatedRect;
    if (rect_inside_bound(rect, &buf_rect, &updatedRect) != 0) {
        return;
    }

    // CE: Constrain rect to tile bounds so that we don't draw outside.
    if (tile_inside_bound(&updatedRect) != 0) {
        // Mouse hex cursor is a special case - should be shown as outline when
        // out of bounds (see `obj_render_outline`).
        outlineCount = 0;
        if ((obj_mouse_flat->flags & OBJECT_HIDDEN) == 0
            && (obj_mouse_flat->outline & OUTLINE_TYPE_MASK) != 0
            && (obj_mouse_flat->outline & OUTLINE_DISABLED) == 0) {
            outlinedObjects[outlineCount++] = obj_mouse_flat;
        }
        return;
    }

    int ambientIntensity = light_get_ambient();
    int minX = updatedRect.ulx - 320;
    int minY = updatedRect.uly - 240;
    int maxX = updatedRect.lrx + 320;
    int maxY = updatedRect.lry + 240;
    int upperLeftTile = tile_num(minX, minY, elevation, true);
    int updateAreaHexWidth = (maxX - minX + 1) / 32;
    int updateAreaHexHeight = (maxY - minY + 1) / 12;

    int parity = tile_center_tile & 1;
    int* orders = orderTable[parity];
    int* offsets = offsetTable[parity];

    outlineCount = 0;

    int renderCount = 0;
    for (int i = 0; i < updateHexArea; i++) {
        int offsetIndex = *orders++;
        if (updateAreaHexHeight > offsetDivTable[offsetIndex] && updateAreaHexWidth > offsetModTable[offsetIndex]) {
            int tile = upperLeftTile + offsetTable[parity][offsetIndex];
            ObjectListNode* objectListNode = hexGridTileIsValid(tile)
                ? objectTable[tile]
                : NULL;

            int lightIntensity;
            if (objectListNode != NULL) {
                lightIntensity = std::max(ambientIntensity, light_get_tile(elevation, objectListNode->obj->tile));
            }

            while (objectListNode != NULL) {
                if (elevation < objectListNode->obj->elevation) {
                    break;
                }

                if (elevation == objectListNode->obj->elevation) {
                    if ((objectListNode->obj->flags & OBJECT_FLAT) == 0) {
                        break;
                    }

                    if ((objectListNode->obj->flags & OBJECT_HIDDEN) == 0) {
                        obj_render_object(objectListNode->obj, &updatedRect, lightIntensity);

                        if ((objectListNode->obj->outline & OUTLINE_TYPE_MASK) != 0) {
                            if ((objectListNode->obj->outline & OUTLINE_DISABLED) == 0 && outlineCount < 100) {
                                outlinedObjects[outlineCount++] = objectListNode->obj;
                            }
                        }
                    }
                }

                objectListNode = objectListNode->next;
            }

            if (objectListNode != NULL) {
                renderTable[renderCount++] = objectListNode;
            }
        }
    }

    for (int i = 0; i < renderCount; i++) {
        int lightIntensity;

        ObjectListNode* objectListNode = renderTable[i];
        if (objectListNode != NULL) {
            lightIntensity = std::max(ambientIntensity, light_get_tile(elevation, objectListNode->obj->tile));
        }

        while (objectListNode != NULL) {
            Object* object = objectListNode->obj;
            if (elevation < object->elevation) {
                break;
            }

            if (elevation == objectListNode->obj->elevation) {
                if ((objectListNode->obj->flags & OBJECT_HIDDEN) == 0) {
                    obj_render_object(object, &updatedRect, lightIntensity);

                    if ((objectListNode->obj->outline & OUTLINE_TYPE_MASK) != 0) {
                        if ((objectListNode->obj->outline & OUTLINE_DISABLED) == 0 && outlineCount < 100) {
                            outlinedObjects[outlineCount++] = objectListNode->obj;
                        }
                    }
                }
            }

            objectListNode = objectListNode->next;
        }
    }
}

// 0x47B5EC
void obj_render_post_roof(Rect* rect, int elevation)
{
    if (!objInitialized) {
        return;
    }

    Rect updatedRect;
    if (rect_inside_bound(rect, &buf_rect, &updatedRect) != 0) {
        return;
    }

    // CE: Constrain rect to tile bounds so that we don't draw outside.
    Rect constrainedRect = updatedRect;
    if (tile_inside_bound(&constrainedRect) != 0) {
        constrainedRect.ulx = 0;
        constrainedRect.uly = 0;
        constrainedRect.lrx = 0;
        constrainedRect.lry = 0;
    }

    for (int index = 0; index < outlineCount; index++) {
        // Mouse hex cursor is a special case - should be shown without
        // constraining otherwise its hidden.
        if (outlinedObjects[index] == obj_mouse_flat) {
            obj_render_outline(outlinedObjects[index], &updatedRect);
        } else {
            obj_render_outline(outlinedObjects[index], &constrainedRect);
        }
    }

    text_object_render(&updatedRect);

    ObjectListNode* objectListNode = floatingObjects;
    while (objectListNode != NULL) {
        Object* object = objectListNode->obj;
        if ((object->flags & OBJECT_HIDDEN) == 0) {
            obj_render_object(object, &updatedRect, 0x10000);
        }
        objectListNode = objectListNode->next;
    }
}

// 0x47B878
int obj_new(Object** objectPtr, int fid, int pid)
{
    ObjectListNode* objectListNode;

    // NOTE: Uninline;
    if (obj_create_object_node(&objectListNode) == -1) {
        return -1;
    }

    if (obj_create_object(&(objectListNode->obj)) == -1) {
        // Uninline.
        obj_destroy_object_node(&objectListNode);
        return -1;
    }

    objectListNode->obj->fid = fid;
    obj_insert(objectListNode);

    if (objectPtr) {
        *objectPtr = objectListNode->obj;
    }

    objectListNode->obj->pid = pid;
    objectListNode->obj->id = new_obj_id();

    if (pid == -1 || PID_TYPE(pid) == OBJ_TYPE_TILE) {
        Inventory* inventory = &(objectListNode->obj->data.inventory);
        inventory->length = 0;
        inventory->items = NULL;
        return 0;
    }

    proto_update_init(objectListNode->obj);

    Proto* proto = NULL;
    if (proto_ptr(pid, &proto) == -1) {
        return 0;
    }

    obj_set_light(objectListNode->obj, proto->lightDistance, proto->lightIntensity, NULL);

    if ((proto->flags & 0x08) != 0) {
        obj_toggle_flat(objectListNode->obj, NULL);
    }

    if ((proto->flags & 0x10) != 0) {
        objectListNode->obj->flags |= OBJECT_NO_BLOCK;
    }

    if ((proto->flags & 0x800) != 0) {
        objectListNode->obj->flags |= OBJECT_MULTIHEX;
    }

    if ((proto->flags & 0x8000) != 0) {
        objectListNode->obj->flags |= OBJECT_TRANS_NONE;
    } else {
        if ((proto->flags & 0x10000) != 0) {
            objectListNode->obj->flags |= OBJECT_TRANS_WALL;
        } else if ((proto->flags & 0x20000) != 0) {
            objectListNode->obj->flags |= OBJECT_TRANS_GLASS;
        } else if ((proto->flags & 0x40000) != 0) {
            objectListNode->obj->flags |= OBJECT_TRANS_STEAM;
        } else if ((proto->flags & 0x80000) != 0) {
            objectListNode->obj->flags |= OBJECT_TRANS_ENERGY;
        } else if ((proto->flags & 0x4000) != 0) {
            objectListNode->obj->flags |= OBJECT_TRANS_RED;
        }
    }

    if ((proto->flags & 0x20000000) != 0) {
        objectListNode->obj->flags |= OBJECT_LIGHT_THRU;
    }

    if ((proto->flags & 0x80000000) != 0) {
        objectListNode->obj->flags |= OBJECT_SHOOT_THRU;
    }

    if ((proto->flags & 0x10000000) != 0) {
        objectListNode->obj->flags |= OBJECT_WALL_TRANS_END;
    }

    if ((proto->flags & 0x1000) != 0) {
        objectListNode->obj->flags |= OBJECT_NO_HIGHLIGHT;
    }

    obj_new_sid(objectListNode->obj, &(objectListNode->obj->sid));

    return 0;
}

// 0x47BA90
int obj_pid_new(Object** objectPtr, int pid)
{
    Proto* proto;

    *objectPtr = NULL;

    if (proto_ptr(pid, &proto) == -1) {
        return -1;
    }

    return obj_new(objectPtr, proto->fid, pid);
}

// 0x47BAC0
int obj_copy(Object** a1, Object* a2)
{
    if (a2 == NULL) {
        return -1;
    }

    ObjectListNode* objectListNode;

    // NOTE: Uninline.
    if (obj_create_object_node(&objectListNode) == -1) {
        return -1;
    }

    if (obj_create_object(&(objectListNode->obj)) == -1) {
        // NOTE: Uninline.
        obj_destroy_object_node(&objectListNode);
        return -1;
    }

    clear_pupdate_data(objectListNode->obj);

    memcpy(objectListNode->obj, a2, sizeof(Object));

    if (a1 != NULL) {
        *a1 = objectListNode->obj;
    }

    obj_insert(objectListNode);

    objectListNode->obj->id = new_obj_id();

    if (objectListNode->obj->sid != -1) {
        objectListNode->obj->sid = -1;
        obj_new_sid(objectListNode->obj, &(objectListNode->obj->sid));
    }

    return 0;
}

// 0x47BB90
int obj_connect(Object* object, int tile, int elevation, Rect* rect)
{
    if (object == NULL) {
        return -1;
    }

    if (!hexGridTileIsValid(tile)) {
        return -1;
    }

    if (!elevationIsValid(elevation)) {
        return -1;
    }

    ObjectListNode* objectListNode;

    // NOTE: Uninline.
    if (obj_create_object_node(&objectListNode) == -1) {
        return -1;
    }

    objectListNode->obj = object;

    return obj_connect_to_tile(objectListNode, tile, elevation, rect);
}

// 0x47BC00
int obj_disconnect(Object* obj, Rect* rect)
{
    if (obj == NULL) {
        return -1;
    }

    ObjectListNode* node;
    ObjectListNode* prev_node;
    if (obj_node_ptr(obj, &node, &prev_node) != 0) {
        return -1;
    }

    if (obj_adjust_light(obj, 1, rect) == -1) {
        if (rect != NULL) {
            obj_bound(obj, rect);
        }
    }

    if (prev_node != NULL) {
        prev_node->next = node->next;
    } else {
        int tile = node->obj->tile;
        if (tile == -1) {
            floatingObjects = floatingObjects->next;
        } else {
            objectTable[tile] = objectTable[tile]->next;
        }
    }

    if (node != NULL) {
        mem_free(node);
    }

    obj->tile = -1;

    return 0;
}

// 0x47BCC4
int obj_offset(Object* obj, int x, int y, Rect* rect)
{
    if (obj == NULL) {
        return -1;
    }

    ObjectListNode* node = NULL;
    ObjectListNode* previousNode = NULL;
    if (obj_node_ptr(obj, &node, &previousNode) == -1) {
        return -1;
    }

    if (obj == obj_dude) {
        if (rect != NULL) {
            Rect eggRect;
            obj_bound(obj_egg, &eggRect);
            rectCopy(rect, &eggRect);

            if (previousNode != NULL) {
                previousNode->next = node->next;
            } else {
                int tile = node->obj->tile;
                if (tile == -1) {
                    floatingObjects = floatingObjects->next;
                } else {
                    objectTable[tile] = objectTable[tile]->next;
                }
            }

            obj->x += x;
            obj->sx += x;

            obj->y += y;
            obj->sy += y;

            obj_insert(node);

            rectOffset(&eggRect, x, y);

            obj_offset(obj_egg, x, y, NULL);
            rect_min_bound(rect, &eggRect, rect);
        } else {
            if (previousNode != NULL) {
                previousNode->next = node->next;
            } else {
                int tile = node->obj->tile;
                if (tile == -1) {
                    floatingObjects = floatingObjects->next;
                } else {
                    objectTable[tile] = objectTable[tile]->next;
                }
            }

            obj->x += x;
            obj->sx += x;

            obj->y += y;
            obj->sy += y;

            obj_insert(node);

            obj_offset(obj_egg, x, y, NULL);
        }
    } else {
        if (rect != NULL) {
            obj_bound(obj, rect);

            if (previousNode != NULL) {
                previousNode->next = node->next;
            } else {
                int tile = node->obj->tile;
                if (tile == -1) {
                    floatingObjects = floatingObjects->next;
                } else {
                    objectTable[tile] = objectTable[tile]->next;
                }
            }

            obj->x += x;
            obj->sx += x;

            obj->y += y;
            obj->sy += y;

            obj_insert(node);

            Rect objectRect;
            rectCopy(&objectRect, rect);

            rectOffset(&objectRect, x, y);

            rect_min_bound(rect, &objectRect, rect);
        } else {
            if (previousNode != NULL) {
                previousNode->next = node->next;
            } else {
                int tile = node->obj->tile;
                if (tile == -1) {
                    floatingObjects = floatingObjects->next;
                } else {
                    objectTable[tile] = objectTable[tile]->next;
                }
            }

            obj->x += x;
            obj->sx += x;

            obj->y += y;
            obj->sy += y;

            obj_insert(node);
        }
    }

    return 0;
}

// 0x47BFF0
int obj_move(Object* a1, int a2, int a3, int elevation, Rect* a5)
{
    if (a1 == NULL) {
        return -1;
    }

    // TODO: Get rid of initialization.
    ObjectListNode* node = NULL;
    ObjectListNode* previousNode;
    int v22 = 0;

    int tile = a1->tile;
    if (hexGridTileIsValid(tile)) {
        if (obj_node_ptr(a1, &node, &previousNode) == -1) {
            return -1;
        }

        if (obj_adjust_light(a1, 1, a5) == -1) {
            if (a5 != NULL) {
                obj_bound(a1, a5);
            }
        }

        if (previousNode != NULL) {
            previousNode->next = node->next;
        } else {
            int tile = node->obj->tile;
            if (tile == -1) {
                floatingObjects = floatingObjects->next;
            } else {
                objectTable[tile] = objectTable[tile]->next;
            }
        }

        a1->tile = -1;
        a1->elevation = elevation;
        v22 = 1;
    } else {
        if (elevation == a1->elevation) {
            if (a5 != NULL) {
                obj_bound(a1, a5);
            }
        } else {
            if (obj_node_ptr(a1, &node, &previousNode) == -1) {
                return -1;
            }

            if (a5 != NULL) {
                obj_bound(a1, a5);
            }

            if (previousNode != NULL) {
                previousNode->next = node->next;
            } else {
                int tile = node->obj->tile;
                if (tile != -1) {
                    objectTable[tile] = objectTable[tile]->next;
                } else {
                    floatingObjects = floatingObjects->next;
                }
            }

            a1->elevation = elevation;
            v22 = 1;
        }
    }

    CacheEntry* cacheHandle;
    Art* art = art_ptr_lock(a1->fid, &cacheHandle);
    if (art != NULL) {
        a1->sx = a2 - art_frame_width(art, a1->frame, a1->rotation) / 2;
        a1->sy = a3 - (art_frame_length(art, a1->frame, a1->rotation) - 1);
        art_ptr_unlock(cacheHandle);
    }

    if (v22) {
        obj_insert(node);
    }

    if (a5 != NULL) {
        Rect rect;
        obj_bound(a1, &rect);
        rect_min_bound(a5, &rect, a5);
    }

    if (a1 == obj_dude) {
        if (a1 != NULL) {
            Rect rect;
            obj_move(obj_egg, a2, a3, elevation, &rect);
            rect_min_bound(a5, &rect, a5);
        } else {
            obj_move(obj_egg, a2, a3, elevation, NULL);
        }
    }

    return 0;
}

// 0x47C228
int obj_move_to_tile(Object* obj, int tile, int elevation, Rect* rect)
{
    if (obj == NULL) {
        return -1;
    }

    if (!hexGridTileIsValid(tile)) {
        return -1;
    }

    if (!elevationIsValid(elevation)) {
        return -1;
    }

    ObjectListNode* node;
    ObjectListNode* prevNode;
    if (obj_node_ptr(obj, &node, &prevNode) == -1) {
        return -1;
    }

    Rect v23;
    int v5 = obj_adjust_light(obj, 1, rect);
    if (rect != NULL) {
        if (v5 == -1) {
            obj_bound(obj, rect);
        }

        rectCopy(&v23, rect);
    }

    int oldElevation = obj->elevation;
    if (prevNode != NULL) {
        prevNode->next = node->next;
    } else {
        int tileIndex = node->obj->tile;
        if (tileIndex == -1) {
            floatingObjects = floatingObjects->next;
        } else {
            objectTable[tileIndex] = objectTable[tileIndex]->next;
        }
    }

    if (obj_connect_to_tile(node, tile, elevation, rect) == -1) {
        return -1;
    }

    if (rect != NULL) {
        rect_min_bound(rect, &v23, rect);
    }

    if (obj == obj_dude) {
        ObjectListNode* objectListNode = objectTable[tile];
        while (objectListNode != NULL) {
            Object* obj = objectListNode->obj;
            int elev = obj->elevation;
            if (elevation < elev) {
                break;
            }

            if (elevation == elev) {
                if (FID_TYPE(obj->fid) == OBJ_TYPE_MISC) {
                    if (obj->pid >= 0x5000010 && obj->pid <= 0x5000017) {
                        ObjectData* data = &(obj->data);

                        MapTransition transition;
                        memset(&transition, 0, sizeof(transition));

                        transition.map = data->misc.map;
                        transition.tile = data->misc.tile;
                        transition.elevation = data->misc.elevation;
                        transition.rotation = data->misc.rotation;
                        map_leave_map(&transition);
                    }
                }
            }

            objectListNode = objectListNode->next;
        }

        // NOTE: Uninline.
        obj_set_seen(tile);

        int roofX = tile % 200 / 2;
        int roofY = tile / 200 / 2;
        if (roofX != obj_last_roof_x || roofY != obj_last_roof_y || elevation != obj_last_elev) {
            int currentSquare = square[elevation]->field_0[roofX + 100 * roofY];
            int currentSquareFid = art_id(OBJ_TYPE_TILE, (currentSquare >> 16) & 0xFFF, 0, 0, 0);
            // CE: Add additional checks for -1 to prevent array lookup at index -101.
            int previousSquare = obj_last_roof_x != -1 && obj_last_roof_y != -1
                ? square[elevation]->field_0[obj_last_roof_x + 100 * obj_last_roof_y]
                : 0;
            bool isEmpty = art_id(OBJ_TYPE_TILE, 1, 0, 0, 0) == currentSquareFid;

            if (isEmpty != obj_last_is_empty || (((currentSquare >> 16) & 0xF000) >> 12) != (((previousSquare >> 16) & 0xF000) >> 12)) {
                if (!obj_last_is_empty) {
                    tile_fill_roof(obj_last_roof_x, obj_last_roof_y, elevation, true);
                }

                if (!isEmpty) {
                    tile_fill_roof(roofX, roofY, elevation, false);
                }

                if (rect != NULL) {
                    rect_min_bound(rect, &scr_size, rect);
                }
            }

            obj_last_roof_x = roofX;
            obj_last_roof_y = roofY;
            obj_last_elev = elevation;
            obj_last_is_empty = isEmpty;
        }

        if (rect != NULL) {
            Rect r;
            obj_move_to_tile(obj_egg, tile, elevation, &r);
            rect_min_bound(rect, &r, rect);
        } else {
            obj_move_to_tile(obj_egg, tile, elevation, 0);
        }

        if (elevation != oldElevation) {
            map_set_elevation(elevation);
            tile_set_center(tile, TILE_SET_CENTER_REFRESH_WINDOW | TILE_SET_CENTER_FLAG_IGNORE_SCROLL_RESTRICTIONS);
            if (isInCombat()) {
                game_user_wants_to_quit = 1;
            }
        }
    } else {
        if (elevation != obj_last_elev && PID_TYPE(obj->pid) == OBJ_TYPE_CRITTER) {
            combat_delete_critter(obj);
        }
    }

    return 0;
}

// 0x47C588
int obj_reset_roof()
{
    int fid = art_id(OBJ_TYPE_TILE, (square[obj_dude->elevation]->field_0[obj_last_roof_x + 100 * obj_last_roof_y] >> 16) & 0xFFF, 0, 0, 0);
    if (fid != art_id(OBJ_TYPE_TILE, 1, 0, 0, 0)) {
        tile_fill_roof(obj_last_roof_x, obj_last_roof_y, obj_dude->elevation, 1);
    }
    return 0;
}

// Sets object fid.
//
// 0x47C624
int obj_change_fid(Object* obj, int fid, Rect* dirtyRect)
{
    Rect new_rect;

    if (obj == NULL) {
        return -1;
    }

    if (dirtyRect != NULL) {
        obj_bound(obj, dirtyRect);

        obj->fid = fid;

        obj_bound(obj, &new_rect);
        rect_min_bound(dirtyRect, &new_rect, dirtyRect);
    } else {
        obj->fid = fid;
    }

    return 0;
}

// Sets object frame.
//
// 0x47C66C
int obj_set_frame(Object* obj, int frame, Rect* rect)
{
    Rect new_rect;
    Art* art;
    CacheEntry* cache_entry;
    int framesPerDirection;

    if (obj == NULL) {
        return -1;
    }

    art = art_ptr_lock(obj->fid, &cache_entry);
    if (art == NULL) {
        return -1;
    }

    framesPerDirection = art->frameCount;

    art_ptr_unlock(cache_entry);

    if (frame >= framesPerDirection) {
        return -1;
    }

    if (rect != NULL) {
        obj_bound(obj, rect);
        obj->frame = frame;
        obj_bound(obj, &new_rect);
        rect_min_bound(rect, &new_rect, rect);
    } else {
        obj->frame = frame;
    }

    return 0;
}

// 0x47C6D8
int obj_inc_frame(Object* obj, Rect* dirtyRect)
{
    Art* art;
    CacheEntry* cache_entry;
    int framesPerDirection;
    int nextFrame;

    if (obj == NULL) {
        return -1;
    }

    art = art_ptr_lock(obj->fid, &cache_entry);
    if (art == NULL) {
        return -1;
    }

    framesPerDirection = art->frameCount;

    art_ptr_unlock(cache_entry);

    nextFrame = obj->frame + 1;
    if (nextFrame >= framesPerDirection) {
        nextFrame = 0;
    }

    if (dirtyRect != NULL) {

        obj_bound(obj, dirtyRect);

        obj->frame = nextFrame;

        Rect updatedRect;
        obj_bound(obj, &updatedRect);
        rect_min_bound(dirtyRect, &updatedRect, dirtyRect);
    } else {
        obj->frame = nextFrame;
    }

    return 0;
}

// 0x47C748
//
int obj_dec_frame(Object* obj, Rect* dirtyRect)
{
    Art* art;
    CacheEntry* cache_entry;
    int framesPerDirection;
    int prevFrame;
    Rect newRect;

    if (obj == NULL) {
        return -1;
    }

    art = art_ptr_lock(obj->fid, &cache_entry);
    if (art == NULL) {
        return -1;
    }

    framesPerDirection = art->frameCount;

    art_ptr_unlock(cache_entry);

    prevFrame = obj->frame - 1;
    if (prevFrame < 0) {
        prevFrame = framesPerDirection - 1;
    }

    if (dirtyRect != NULL) {
        obj_bound(obj, dirtyRect);
        obj->frame = prevFrame;
        obj_bound(obj, &newRect);
        rect_min_bound(dirtyRect, &newRect, dirtyRect);
    } else {
        obj->frame = prevFrame;
    }

    return 0;
}

// 0x47C7BC
int obj_set_rotation(Object* obj, int direction, Rect* dirtyRect)
{
    if (obj == NULL) {
        return -1;
    }

    if (direction >= ROTATION_COUNT) {
        return -1;
    }

    if (dirtyRect != NULL) {
        obj_bound(obj, dirtyRect);
        obj->rotation = direction;

        Rect newRect;
        obj_bound(obj, &newRect);
        rect_min_bound(dirtyRect, &newRect, dirtyRect);
    } else {
        obj->rotation = direction;
    }

    return 0;
}

// 0x47C808
int obj_inc_rotation(Object* obj, Rect* dirtyRect)
{
    int rotation = obj->rotation + 1;
    if (rotation >= ROTATION_COUNT) {
        rotation = ROTATION_NE;
    }

    return obj_set_rotation(obj, rotation, dirtyRect);
}

// 0x47C820
int obj_dec_rotation(Object* obj, Rect* dirtyRect)
{
    int rotation = obj->rotation - 1;
    if (rotation < 0) {
        rotation = ROTATION_NW;
    }

    return obj_set_rotation(obj, rotation, dirtyRect);
}

// 0x47C83C
void obj_rebuild_all_light()
{
    light_reset_tiles();

    for (int tile = 0; tile < HEX_GRID_SIZE; tile++) {
        ObjectListNode* objectListNode = objectTable[tile];
        while (objectListNode != NULL) {
            obj_adjust_light(objectListNode->obj, 0, NULL);
            objectListNode = objectListNode->next;
        }
    }
}

// 0x47C878
int obj_set_light(Object* obj, int lightDistance, int lightIntensity, Rect* rect)
{
    int v7;
    Rect new_rect;

    if (obj == NULL) {
        return -1;
    }

    v7 = obj_turn_off_light(obj, rect);
    if (lightIntensity > 0) {
        if (lightDistance >= 8) {
            lightDistance = 8;
        }

        obj->lightIntensity = lightIntensity;
        obj->lightDistance = lightDistance;

        if (rect != NULL) {
            v7 = obj_turn_on_light(obj, &new_rect);
            rect_min_bound(rect, &new_rect, rect);
        } else {
            v7 = obj_turn_on_light(obj, NULL);
        }
    } else {
        obj->lightIntensity = 0;
        obj->lightDistance = 0;
    }

    return v7;
}

// 0x47C8EC
int obj_get_visible_light(Object* obj)
{
    int lightLevel = light_get_ambient();
    int lightIntensity = light_get_tile_true(obj->elevation, obj->tile);

    if (obj == obj_dude) {
        lightIntensity -= obj_dude->lightIntensity;
    }

    if (lightIntensity >= lightLevel) {
        if (lightIntensity > LIGHT_LEVEL_MAX) {
            lightIntensity = LIGHT_LEVEL_MAX;
        }
    } else {
        lightIntensity = lightLevel;
    }

    return lightIntensity;
}

// 0x47C930
int obj_turn_on_light(Object* obj, Rect* rect)
{
    if (obj == NULL) {
        return -1;
    }

    if (obj->lightIntensity <= 0) {
        obj->flags &= ~OBJECT_LIGHTING;
        return -1;
    }

    if ((obj->flags & OBJECT_LIGHTING) == 0) {
        obj->flags |= OBJECT_LIGHTING;

        if (obj_adjust_light(obj, 0, rect) == -1) {
            if (rect != NULL) {
                obj_bound(obj, rect);
            }
        }
    }

    return 0;
}

// 0x47C984
int obj_turn_off_light(Object* obj, Rect* rect)
{
    if (obj == NULL) {
        return -1;
    }

    if (obj->lightIntensity <= 0) {
        obj->flags &= ~OBJECT_LIGHTING;
        return -1;
    }

    if ((obj->flags & OBJECT_LIGHTING) != 0) {
        if (obj_adjust_light(obj, 1, rect) == -1) {
            if (rect != NULL) {
                obj_bound(obj, rect);
            }
        }

        obj->flags &= ~OBJECT_LIGHTING;
    }

    return 0;
}

// 0x47C9D8
int obj_turn_on(Object* obj, Rect* rect)
{
    if (obj == NULL) {
        return -1;
    }

    if ((obj->flags & OBJECT_HIDDEN) == 0) {
        return -1;
    }

    obj->flags &= ~OBJECT_HIDDEN;
    obj->outline &= ~OUTLINE_DISABLED;

    if (obj_adjust_light(obj, 0, rect) == -1) {
        if (rect != NULL) {
            obj_bound(obj, rect);
        }
    }

    if (obj == obj_dude) {
        if (rect != NULL) {
            Rect eggRect;
            obj_bound(obj_egg, &eggRect);
            rect_min_bound(rect, &eggRect, rect);
        }
    }

    return 0;
}

// 0x47CA50
int obj_turn_off(Object* object, Rect* rect)
{
    if (object == NULL) {
        return -1;
    }

    if ((object->flags & OBJECT_HIDDEN) != 0) {
        return -1;
    }

    if (obj_adjust_light(object, 1, rect) == -1) {
        if (rect != NULL) {
            obj_bound(object, rect);
        }
    }

    object->flags |= OBJECT_HIDDEN;

    if ((object->outline & OUTLINE_TYPE_MASK) != 0) {
        object->outline |= OUTLINE_DISABLED;
    }

    if (object == obj_dude) {
        if (rect != NULL) {
            Rect eggRect;
            obj_bound(obj_egg, &eggRect);
            rect_min_bound(rect, &eggRect, rect);
        }
    }

    return 0;
}

// 0x47CACC
int obj_turn_on_outline(Object* object, Rect* rect)
{
    if (object == NULL) {
        return -1;
    }

    object->outline &= ~OUTLINE_DISABLED;

    if (rect != NULL) {
        obj_bound(object, rect);
    }

    return 0;
}

// 0x47CAE8
int obj_turn_off_outline(Object* object, Rect* rect)
{
    if (object == NULL) {
        return -1;
    }

    if ((object->outline & OUTLINE_TYPE_MASK) != 0) {
        object->outline |= OUTLINE_DISABLED;
    }

    if (rect != NULL) {
        obj_bound(object, rect);
    }

    return 0;
}

// 0x47CB14
int obj_toggle_flat(Object* object, Rect* rect)
{
    Rect v1;

    if (object == NULL) {
        return -1;
    }

    ObjectListNode* node;
    ObjectListNode* previousNode;
    if (obj_node_ptr(object, &node, &previousNode) == -1) {
        return -1;
    }

    if (rect != NULL) {
        obj_bound(object, rect);

        if (previousNode != NULL) {
            previousNode->next = node->next;
        } else {
            int tile_index = node->obj->tile;
            if (tile_index == -1) {
                floatingObjects = floatingObjects->next;
            } else {
                objectTable[tile_index] = objectTable[tile_index]->next;
            }
        }

        object->flags ^= OBJECT_FLAT;

        obj_insert(node);
        obj_bound(object, &v1);
        rect_min_bound(rect, &v1, rect);
    } else {
        if (previousNode != NULL) {
            previousNode->next = node->next;
        } else {
            int tile = node->obj->tile;
            if (tile == -1) {
                floatingObjects = floatingObjects->next;
            } else {
                objectTable[tile] = objectTable[tile]->next;
            }
        }

        object->flags ^= OBJECT_FLAT;

        obj_insert(node);
    }

    return 0;
}

// 0x47CCE4
int obj_erase_object(Object* object, Rect* rect)
{
    if (object == NULL) {
        return -1;
    }

    gmouse_remove_item_outline(object);

    ObjectListNode* node;
    ObjectListNode* previousNode;
    if (obj_node_ptr(object, &node, &previousNode) == 0) {
        if (obj_adjust_light(object, 1, rect) == -1) {
            if (rect != NULL) {
                obj_bound(object, rect);
            }
        }

        if (obj_remove(node, previousNode) != 0) {
            return -1;
        }

        return 0;
    }

    // NOTE: Uninline.
    if (obj_create_object_node(&node) == -1) {
        return -1;
    }

    node->obj = object;

    if (obj_remove(node, node) == -1) {
        return -1;
    }

    return 0;
}

// 0x47CD98
int obj_inven_free(Inventory* inventory)
{
    for (int index = 0; index < inventory->length; index++) {
        InventoryItem* inventoryItem = &(inventory->items[index]);

        ObjectListNode* node;
        // NOTE: Uninline.
        obj_create_object_node(&node);

        node->obj = inventoryItem->item;
        node->obj->flags &= ~OBJECT_NO_REMOVE;
        obj_remove(node, node);

        inventoryItem->item = NULL;
    }

    if (inventory->items != NULL) {
        mem_free(inventory->items);
        inventory->items = NULL;
        inventory->capacity = 0;
        inventory->length = 0;
    }

    return 0;
}

// 0x47CE34
bool obj_action_can_talk_to(Object* obj)
{
    return proto_action_can_talk_to(obj->pid) && (PID_TYPE(obj->pid) == OBJ_TYPE_CRITTER) && critter_is_active(obj);
}

// Returns root owner of given object.
//
// 0x47CE64
Object* obj_top_environment(Object* object)
{
    Object* owner = object->owner;
    if (owner == NULL) {
        return NULL;
    }

    while (owner->owner != NULL) {
        owner = owner->owner;
    }

    return owner;
}

// 0x47CE78
void obj_remove_all()
{
    ObjectListNode* node;
    ObjectListNode* prev;
    ObjectListNode* next;

    scr_remove_all();

    for (int tile = 0; tile < HEX_GRID_SIZE; tile++) {
        node = objectTable[tile];
        prev = NULL;

        while (node != NULL) {
            next = node->next;
            if (obj_remove(node, prev) == -1) {
                prev = node;
            }
            node = next;
        }
    }

    node = floatingObjects;
    prev = NULL;

    while (node != NULL) {
        next = node->next;
        if (obj_remove(node, prev) == -1) {
            prev = node;
        }
        node = next;
    }

    obj_last_roof_y = -1;
    obj_last_elev = -1;
    obj_last_is_empty = true;
    obj_last_roof_x = -1;
}

// 0x47CF08
Object* obj_find_first()
{
    find_elev = 0;

    ObjectListNode* objectListNode;
    for (find_tile = 0; find_tile < HEX_GRID_SIZE; find_tile++) {
        objectListNode = objectTable[find_tile];
        if (objectListNode) {
            break;
        }
    }

    if (find_tile == HEX_GRID_SIZE) {
        find_ptr = NULL;
        return NULL;
    }

    while (objectListNode != NULL) {
        if (art_get_disable(FID_TYPE(objectListNode->obj->fid)) == 0) {
            find_ptr = objectListNode;
            return objectListNode->obj;
        }
        objectListNode = objectListNode->next;
    }

    find_ptr = NULL;
    return NULL;
}

// 0x47CF7C
Object* obj_find_next()
{
    if (find_ptr == NULL) {
        return NULL;
    }

    ObjectListNode* objectListNode = find_ptr->next;

    while (find_tile < HEX_GRID_SIZE) {
        if (objectListNode == NULL) {
            objectListNode = objectTable[find_tile++];
        }

        while (objectListNode != NULL) {
            Object* object = objectListNode->obj;
            if (!art_get_disable(FID_TYPE(object->fid))) {
                find_ptr = objectListNode;
                return object;
            }
            objectListNode = objectListNode->next;
        }
    }

    find_ptr = NULL;
    return NULL;
}

// 0x47CFEC
Object* obj_find_first_at(int elevation)
{
    find_elev = elevation;
    find_tile = 0;

    for (find_tile = 0; find_tile < HEX_GRID_SIZE; find_tile++) {
        ObjectListNode* objectListNode = objectTable[find_tile];
        while (objectListNode != NULL) {
            Object* object = objectListNode->obj;
            if (object->elevation == elevation) {
                if (!art_get_disable(FID_TYPE(object->fid))) {
                    find_ptr = objectListNode;
                    return object;
                }
            }
            objectListNode = objectListNode->next;
        }
    }

    find_ptr = NULL;
    return NULL;
}

// 0x47D070
Object* obj_find_next_at()
{
    if (find_ptr == NULL) {
        return NULL;
    }

    ObjectListNode* objectListNode = find_ptr->next;

    while (find_tile < HEX_GRID_SIZE) {
        if (objectListNode == NULL) {
            objectListNode = objectTable[find_tile++];
        }

        while (objectListNode != NULL) {
            Object* object = objectListNode->obj;
            if (object->elevation == find_elev) {
                if (!art_get_disable(FID_TYPE(object->fid))) {
                    find_ptr = objectListNode;
                    return object;
                }
            }
            objectListNode = objectListNode->next;
        }
    }

    find_ptr = NULL;
    return NULL;
}

// 0x47D108
void obj_bound(Object* obj, Rect* rect)
{
    if (obj == NULL) {
        return;
    }

    if (rect == NULL) {
        return;
    }

    bool isOutlined = false;
    if ((obj->outline & OUTLINE_TYPE_MASK) != 0) {
        isOutlined = true;
    }

    CacheEntry* artHandle;
    Art* art = art_ptr_lock(obj->fid, &artHandle);
    if (art == NULL) {
        rect->ulx = 0;
        rect->uly = 0;
        rect->lrx = 0;
        rect->lry = 0;
        return;
    }

    int width = art_frame_width(art, obj->frame, obj->rotation);
    int height = art_frame_length(art, obj->frame, obj->rotation);

    if (obj->tile == -1) {
        rect->ulx = obj->sx;
        rect->uly = obj->sy;
        rect->lrx = obj->sx + width - 1;
        rect->lry = obj->sy + height - 1;
    } else {
        int tileScreenY;
        int tileScreenX;
        if (tile_coord(obj->tile, &tileScreenX, &tileScreenY, obj->elevation) == 0) {
            tileScreenX += 16;
            tileScreenY += 8;

            tileScreenX += art->xOffsets[obj->rotation];
            tileScreenY += art->yOffsets[obj->rotation];

            tileScreenX += obj->x;
            tileScreenY += obj->y;

            rect->ulx = tileScreenX - width / 2;
            rect->uly = tileScreenY - height + 1;
            rect->lrx = width + rect->ulx - 1;
            rect->lry = tileScreenY;
        } else {
            rect->ulx = 0;
            rect->uly = 0;
            rect->lrx = 0;
            rect->lry = 0;
            isOutlined = false;
        }
    }

    art_ptr_unlock(artHandle);

    if (isOutlined) {
        rect->ulx--;
        rect->uly--;
        rect->lrx++;
        rect->lry++;
    }
}

// 0x47D2A0
bool obj_occupied(int tile, int elevation)
{
    ObjectListNode* objectListNode = objectTable[tile];
    while (objectListNode != NULL) {
        if (objectListNode->obj->elevation == elevation
            && objectListNode->obj != obj_mouse
            && objectListNode->obj != obj_mouse_flat) {
            return true;
        }
        objectListNode = objectListNode->next;
    }

    return false;
}

// 0x47D2F0
Object* obj_blocking_at(Object* a1, int tile, int elev)
{
    ObjectListNode* objectListNode;
    Object* v7;
    int type;

    if (!hexGridTileIsValid(tile)) {
        return NULL;
    }

    objectListNode = objectTable[tile];
    while (objectListNode != NULL) {
        v7 = objectListNode->obj;
        if (v7->elevation == elev) {
            if ((v7->flags & OBJECT_HIDDEN) == 0 && (v7->flags & OBJECT_NO_BLOCK) == 0 && v7 != a1) {
                type = FID_TYPE(v7->fid);
                if (type == OBJ_TYPE_CRITTER
                    || type == OBJ_TYPE_SCENERY
                    || type == OBJ_TYPE_WALL) {
                    return v7;
                }
            }
        }
        objectListNode = objectListNode->next;
    }

    for (int rotation = 0; rotation < ROTATION_COUNT; rotation++) {
        int neighboor = tile_num_in_direction(tile, rotation, 1);
        if (hexGridTileIsValid(neighboor)) {
            objectListNode = objectTable[neighboor];
            while (objectListNode != NULL) {
                v7 = objectListNode->obj;
                if ((v7->flags & OBJECT_MULTIHEX) != 0) {
                    if (v7->elevation == elev) {
                        if ((v7->flags & OBJECT_HIDDEN) == 0 && (v7->flags & OBJECT_NO_BLOCK) == 0 && v7 != a1) {
                            type = FID_TYPE(v7->fid);
                            if (type == OBJ_TYPE_CRITTER
                                || type == OBJ_TYPE_SCENERY
                                || type == OBJ_TYPE_WALL) {
                                return v7;
                            }
                        }
                    }
                }
                objectListNode = objectListNode->next;
            }
        }
    }

    return NULL;
}

// 0x47D3D8
int obj_scroll_blocking_at(int tile, int elev)
{
    // TODO: Might be an error - why tile 0 is excluded?
    if (tile <= 0 || tile >= 40000) {
        return -1;
    }

    ObjectListNode* objectListNode = objectTable[tile];
    while (objectListNode != NULL) {
        if (elev < objectListNode->obj->elevation) {
            break;
        }

        if (objectListNode->obj->elevation == elev && objectListNode->obj->pid == 0x500000C) {
            return 0;
        }

        objectListNode = objectListNode->next;
    }

    return -1;
}

// 0x47D41C
Object* obj_sight_blocking_at(Object* a1, int tile, int elevation)
{
    ObjectListNode* objectListNode = objectTable[tile];
    while (objectListNode != NULL) {
        Object* object = objectListNode->obj;
        if (object->elevation == elevation
            && (object->flags & OBJECT_HIDDEN) == 0
            && (object->flags & OBJECT_LIGHT_THRU) == 0
            && object != a1) {
            int objectType = FID_TYPE(object->fid);
            if (objectType == OBJ_TYPE_SCENERY || objectType == OBJ_TYPE_WALL) {
                return object;
            }
        }
        objectListNode = objectListNode->next;
    }

    return NULL;
}

// 0x47D468
int obj_dist(Object* object1, Object* object2)
{
    int distance = tile_dist(object1->tile, object2->tile);

    if ((object1->flags & OBJECT_MULTIHEX) != 0) {
        distance -= 1;
    }

    if ((object2->flags & OBJECT_MULTIHEX) != 0) {
        distance -= 1;
    }

    if (distance < 0) {
        distance = 0;
    }

    return distance;
}

// 0x47D494
int obj_create_list(int tile, int elevation, int objectType, Object*** objectListPtr)
{
    if (objectListPtr == NULL) {
        return -1;
    }

    int count = 0;
    if (tile == -1) {
        for (int index = 0; index < HEX_GRID_SIZE; index++) {
            ObjectListNode* objectListNode = objectTable[index];
            while (objectListNode != NULL) {
                Object* obj = objectListNode->obj;
                if ((obj->flags & OBJECT_HIDDEN) == 0
                    && obj->elevation == elevation
                    && FID_TYPE(obj->fid) == objectType) {
                    count++;
                }
                objectListNode = objectListNode->next;
            }
        }
    } else {
        ObjectListNode* objectListNode = objectTable[tile];
        while (objectListNode != NULL) {
            Object* obj = objectListNode->obj;
            if ((obj->flags & OBJECT_HIDDEN) == 0
                && obj->elevation == elevation
                && FID_TYPE(objectListNode->obj->fid) == objectType) {
                count++;
            }
            objectListNode = objectListNode->next;
        }
    }

    if (count == 0) {
        return 0;
    }

    Object** objects = *objectListPtr = (Object**)mem_malloc(sizeof(*objects) * count);
    if (objects == NULL) {
        return -1;
    }

    if (tile == -1) {
        for (int index = 0; index < HEX_GRID_SIZE; index++) {
            ObjectListNode* objectListNode = objectTable[index];
            while (objectListNode) {
                Object* obj = objectListNode->obj;
                if ((obj->flags & OBJECT_HIDDEN) == 0
                    && obj->elevation == elevation
                    && FID_TYPE(obj->fid) == objectType) {
                    *objects++ = obj;
                }
                objectListNode = objectListNode->next;
            }
        }
    } else {
        ObjectListNode* objectListNode = objectTable[tile];
        while (objectListNode != NULL) {
            Object* obj = objectListNode->obj;
            if ((obj->flags & OBJECT_HIDDEN) == 0
                && obj->elevation == elevation
                && FID_TYPE(obj->fid) == objectType) {
                *objects++ = obj;
            }
            objectListNode = objectListNode->next;
        }
    }

    return count;
}

// 0x47D628
void obj_delete_list(Object** objectList)
{
    if (objectList != NULL) {
        mem_free(objectList);
    }
}

// 0x47D634
void translucent_trans_buf_to_buf(unsigned char* src, int srcWidth, int srcHeight, int srcPitch, unsigned char* dest, int destX, int destY, int destPitch, unsigned char* a9, unsigned char* a10)
{
    dest += destPitch * destY + destX;
    int srcStep = srcPitch - srcWidth;
    int destStep = destPitch - srcWidth;

    for (int y = 0; y < srcHeight; y++) {
        for (int x = 0; x < srcWidth; x++) {
            // TODO: Probably wrong.
            unsigned char v1 = a10[*src];
            unsigned char* v2 = a9 + (v1 << 8);
            unsigned char v3 = *dest;

            *dest = v2[v3];

            src++;
            dest++;
        }

        src += srcStep;
        dest += destStep;
    }
}

// 0x47D758
void dark_trans_buf_to_buf(unsigned char* src, int srcWidth, int srcHeight, int srcPitch, unsigned char* dest, int destX, int destY, int destPitch, int light)
{
    unsigned char* sp = src;
    unsigned char* dp = dest + destPitch * destY + destX;

    int srcStep = srcPitch - srcWidth;
    int destStep = destPitch - srcWidth;
    // TODO: Name might be confusing.
    int lightModifier = light >> 9;

    for (int y = 0; y < srcHeight; y++) {
        for (int x = 0; x < srcWidth; x++) {
            unsigned char b = *sp;
            if (b != 0) {
                if (b < 0xE5) {
                    b = intensityColorTable[b][lightModifier];
                }

                *dp = b;
            }

            sp++;
            dp++;
        }

        sp += srcStep;
        dp += destStep;
    }
}

// 0x47D7E4
void dark_translucent_trans_buf_to_buf(unsigned char* src, int srcWidth, int srcHeight, int srcPitch, unsigned char* dest, int destX, int destY, int destPitch, int light, unsigned char* a10, unsigned char* a11)
{
    int srcStep = srcPitch - srcWidth;
    int destStep = destPitch - srcWidth;
    int lightModifier = light >> 9;

    dest += destPitch * destY + destX;

    for (int y = 0; y < srcHeight; y++) {
        for (int x = 0; x < srcWidth; x++) {
            unsigned char srcByte = *src;
            if (srcByte != 0) {
                unsigned char destByte = *dest;
                unsigned int index = a11[srcByte] << 8;
                index = a10[index + destByte];
                *dest = intensityColorTable[index][lightModifier];
            }

            src++;
            dest++;
        }

        src += srcStep;
        dest += destStep;
    }
}

// 0x47D898
void intensity_mask_buf_to_buf(unsigned char* src, int srcWidth, int srcHeight, int srcPitch, unsigned char* dest, int destPitch, unsigned char* mask, int maskPitch, int light)
{
    int srcStep = srcPitch - srcWidth;
    int destStep = destPitch - srcWidth;
    int maskStep = maskPitch - srcWidth;
    light >>= 9;

    for (int y = 0; y < srcHeight; y++) {
        for (int x = 0; x < srcWidth; x++) {
            unsigned char b = *src;
            if (b != 0) {
                b = intensityColorTable[b][light];
                unsigned char m = *mask;
                if (m != 0) {
                    unsigned char d = *dest;
                    int q = intensityColorTable[d][128 - m];
                    m = intensityColorTable[b][m];
                    b = colorMixAddTable[m][q];
                }
                *dest = b;
            }

            src++;
            dest++;
            mask++;
        }

        src += srcStep;
        dest += destStep;
        mask += maskStep;
    }
}

// 0x47D9A4
int obj_outline_object(Object* obj, int outlineType, Rect* rect)
{
    if (obj == NULL) {
        return -1;
    }

    if ((obj->outline & OUTLINE_TYPE_MASK) != 0) {
        return -1;
    }

    if ((obj->flags & OBJECT_NO_HIGHLIGHT) != 0) {
        return -1;
    }

    obj->outline = outlineType;

    if ((obj->flags & OBJECT_HIDDEN) != 0) {
        obj->outline |= OUTLINE_DISABLED;
    }

    if (rect != NULL) {
        obj_bound(obj, rect);
    }

    return 0;
}

// 0x47D9E0
int obj_remove_outline(Object* object, Rect* rect)
{
    if (object == NULL) {
        return -1;
    }

    if (rect != NULL) {
        obj_bound(object, rect);
    }

    object->outline = 0;

    return 0;
}

// 0x47DA30
int obj_intersects_with(Object* object, int x, int y)
{
    int flags = 0;

    if (object == obj_egg || (object->flags & OBJECT_HIDDEN) == 0) {
        CacheEntry* handle;
        Art* art = art_ptr_lock(object->fid, &handle);
        if (art != NULL) {
            int width = art_frame_width(art, object->frame, object->rotation);
            int height = art_frame_length(art, object->frame, object->rotation);

            int minX;
            int minY;
            int maxX;
            int maxY;
            if (object->tile == -1) {
                minX = object->sx;
                minY = object->sy;
                maxX = minX + width - 1;
                maxY = minY + height - 1;
            } else {
                int tileScreenX;
                int tileScreenY;
                tile_coord(object->tile, &tileScreenX, &tileScreenY, object->elevation);
                tileScreenX += 16;
                tileScreenY += 8;

                tileScreenX += art->xOffsets[object->rotation];
                tileScreenY += art->yOffsets[object->rotation];

                tileScreenX += object->x;
                tileScreenY += object->y;

                minX = tileScreenX - width / 2;
                maxX = minX + width - 1;

                minY = tileScreenY - height + 1;
                maxY = tileScreenY;
            }

            if (x >= minX && x <= maxX && y >= minY && y <= maxY) {
                unsigned char* data = art_frame_data(art, object->frame, object->rotation);
                if (data != NULL) {
                    if (data[width * (y - minY) + x - minX] != 0) {
                        flags |= 0x01;

                        if ((object->flags & OBJECT_FLAG_0xFC000) != 0) {
                            if ((object->flags & OBJECT_TRANS_NONE) == 0) {
                                flags &= ~0x03;
                                flags |= 0x02;
                            }
                        } else {
                            int type = FID_TYPE(object->fid);
                            if (type == OBJ_TYPE_SCENERY || type == OBJ_TYPE_WALL) {
                                Proto* proto;
                                proto_ptr(object->pid, &proto);

                                bool v20;
                                int extendedFlags = proto->scenery.extendedFlags;
                                if ((extendedFlags & 0x8000000) != 0 || (extendedFlags & 0x80000000) != 0) {
                                    v20 = tile_in_front_of(object->tile, obj_dude->tile);
                                } else if ((extendedFlags & 0x10000000) != 0) {
                                    // NOTE: Original code uses bitwise or, but given the fact that these functions return
                                    // bools, logical or is more suitable.
                                    v20 = tile_in_front_of(object->tile, obj_dude->tile) || tile_to_right_of(obj_dude->tile, object->tile);
                                } else if ((extendedFlags & 0x20000000) != 0) {
                                    v20 = tile_in_front_of(object->tile, obj_dude->tile) && tile_to_right_of(obj_dude->tile, object->tile);
                                } else {
                                    v20 = tile_to_right_of(obj_dude->tile, object->tile);
                                }

                                if (v20) {
                                    if (obj_intersects_with(obj_egg, x, y) != 0) {
                                        flags |= 0x04;
                                    }
                                }
                            }
                        }
                    }
                }
            }

            art_ptr_unlock(handle);
        }
    }

    return flags;
}

// 0x47DCC0
int obj_create_intersect_list(int x, int y, int elevation, int objectType, ObjectWithFlags** entriesPtr)
{
    int upperLeftTile = tile_num(x - 320, y - 240, elevation, true);
    *entriesPtr = NULL;

    if (updateHexArea <= 0) {
        return 0;
    }

    int count = 0;

    int parity = tile_center_tile & 1;
    for (int index = 0; index < updateHexArea; index++) {
        int offsetIndex = orderTable[parity][index];
        if (offsetDivTable[offsetIndex] < 30 && offsetModTable[offsetIndex] < 20) {
            int tile = offsetTable[parity][offsetIndex] + upperLeftTile;
            ObjectListNode* objectListNode = hexGridTileIsValid(tile)
                ? objectTable[tile]
                : NULL;
            while (objectListNode != NULL) {
                Object* object = objectListNode->obj;
                if (object->elevation > elevation) {
                    break;
                }

                if (object->elevation == elevation
                    && (objectType == -1 || FID_TYPE(object->fid) == objectType)
                    && object != obj_egg) {
                    int flags = obj_intersects_with(object, x, y);
                    if (flags != 0) {
                        ObjectWithFlags* entries = (ObjectWithFlags*)mem_realloc(*entriesPtr, sizeof(*entries) * (count + 1));
                        if (entries != NULL) {
                            *entriesPtr = entries;
                            entries[count].object = object;
                            entries[count].flags = flags;
                            count++;
                        }
                    }
                }

                objectListNode = objectListNode->next;
            }
        }
    }

    return count;
}

// 0x47DE48
void obj_delete_intersect_list(ObjectWithFlags** entriesPtr)
{
    if (entriesPtr != NULL && *entriesPtr != NULL) {
        mem_free(*entriesPtr);
        *entriesPtr = NULL;
    }
}

// 0x47DE68
void obj_set_seen(int tile)
{
    obj_seen[tile >> 3] |= 1 << (tile & 7);
}

// 0x47DE84
void obj_process_seen()
{
    int i;
    int v7;
    int v8;
    int v5;
    int v0;
    int v3;
    ObjectListNode* obj_entry;

    memset(obj_seen_check, 0, 5001);

    v0 = 400;
    for (i = 0; i < 5001; i++) {
        if (obj_seen[i] != 0) {
            for (v3 = i - 400; v3 != v0; v3 += 25) {
                if (v3 >= 0 && v3 < 5001) {
                    obj_seen_check[v3] = -1;
                    if (v3 > 0) {
                        obj_seen_check[v3 - 1] = -1;
                    }
                    if (v3 < 5000) {
                        obj_seen_check[v3 + 1] = -1;
                    }
                    if (v3 > 1) {
                        obj_seen_check[v3 - 2] = -1;
                    }
                    if (v3 < 4999) {
                        obj_seen_check[v3 + 2] = -1;
                    }
                }
            }
        }
        v0++;
    }

    v7 = 0;
    for (i = 0; i < 5001; i++) {
        if (obj_seen_check[i] != 0) {
            v8 = 1;
            for (v5 = v7; v5 < v7 + 8; v5++) {
                if (v8 & obj_seen_check[i]) {
                    if (v5 < 40000) {
                        for (obj_entry = objectTable[v5]; obj_entry != NULL; obj_entry = obj_entry->next) {
                            if (obj_entry->obj->elevation == obj_dude->elevation) {
                                obj_entry->obj->flags |= OBJECT_SEEN;
                            }
                        }
                    }
                }
                v8 *= 2;
            }
        }
        v7 += 8;
    }

    memset(obj_seen, 0, 5001);
}

// 0x47DFC8
char* object_name(Object* obj)
{
    int objectType = FID_TYPE(obj->fid);
    switch (objectType) {
    case OBJ_TYPE_ITEM:
        return item_name(obj);
    case OBJ_TYPE_CRITTER:
        return critter_name(obj);
    default:
        return proto_name(obj->pid);
    }
}

// 0x47DFF8
char* object_description(Object* obj)
{
    if (FID_TYPE(obj->fid) == OBJ_TYPE_ITEM) {
        return item_description(obj);
    }

    return proto_description(obj->pid);
}

// 0x47E01C
void obj_preload_art_cache(int flags)
{
    if (preload_list == NULL) {
        return;
    }

    unsigned char arr[4096];
    memset(arr, 0, sizeof(arr));

    if ((flags & 0x02) == 0) {
        for (int i = 0; i < SQUARE_GRID_SIZE; i++) {
            int v3 = square[0]->field_0[i];
            arr[v3 & 0xFFF] = 1;
            arr[(v3 >> 16) & 0xFFF] = 1;
        }
    }

    if ((flags & 0x04) == 0) {
        for (int i = 0; i < SQUARE_GRID_SIZE; i++) {
            int v3 = square[1]->field_0[i];
            arr[v3 & 0xFFF] = 1;
            arr[(v3 >> 16) & 0xFFF] = 1;
        }
    }

    if ((flags & 0x08) == 0) {
        for (int i = 0; i < SQUARE_GRID_SIZE; i++) {
            int v3 = square[2]->field_0[i];
            arr[v3 & 0xFFF] = 1;
            arr[(v3 >> 16) & 0xFFF] = 1;
        }
    }

    qsort(preload_list, preload_list_index, sizeof(*preload_list), obj_preload_sort);

    int v11 = preload_list_index;
    int v12 = preload_list_index;

    if (FID_TYPE(preload_list[v12 - 1]) == OBJ_TYPE_WALL) {
        int objectType = OBJ_TYPE_ITEM;
        do {
            v11--;
            objectType = FID_TYPE(preload_list[v12 - 1]);
            v12--;
        } while (objectType == OBJ_TYPE_WALL);
        v11++;
    }

    CacheEntry* cache_handle;
    if (art_ptr_lock(*preload_list, &cache_handle) != NULL) {
        art_ptr_unlock(cache_handle);
    }

    for (int i = 1; i < v11; i++) {
        if (preload_list[i - 1] != preload_list[i]) {
            if (art_ptr_lock(preload_list[i], &cache_handle) != NULL) {
                art_ptr_unlock(cache_handle);
            }
        }
    }

    for (int i = 0; i < 4096; i++) {
        if (arr[i] != 0) {
            int fid = art_id(OBJ_TYPE_TILE, i, 0, 0, 0);
            if (art_ptr_lock(fid, &cache_handle) != NULL) {
                art_ptr_unlock(cache_handle);
            }
        }
    }

    for (int i = v11; i < preload_list_index; i++) {
        if (preload_list[i - 1] != preload_list[i]) {
            if (art_ptr_lock(preload_list[i], &cache_handle) != NULL) {
                art_ptr_unlock(cache_handle);
            }
        }
    }

    mem_free(preload_list);
    preload_list = NULL;

    preload_list_index = 0;
}

// 0x47E250
static int obj_object_table_init()
{
    int tile;

    for (tile = 0; tile < HEX_GRID_SIZE; tile++) {
        objectTable[tile] = NULL;
    }

    return 0;
}

// 0x47E26C
static int obj_offset_table_init()
{
    int i;

    if (offsetTable[0] != NULL) {
        return -1;
    }

    if (offsetTable[1] != NULL) {
        return -1;
    }

    offsetTable[0] = (int*)mem_malloc(sizeof(int) * updateHexArea);
    if (offsetTable[0] == NULL) {
        goto err;
    }

    offsetTable[1] = (int*)mem_malloc(sizeof(int) * updateHexArea);
    if (offsetTable[1] == NULL) {
        goto err;
    }

    for (int parity = 0; parity < 2; parity++) {
        int originTile = tile_num(updateAreaPixelBounds.ulx, updateAreaPixelBounds.uly, 0);
        if (originTile != -1) {
            int* offsets = offsetTable[tile_center_tile & 1];
            int originTileX;
            int originTileY;
            tile_coord(originTile, &originTileX, &originTileY, 0);

            int parityShift = 16;
            originTileX += 16;
            originTileY += 8;
            if (originTileX > updateAreaPixelBounds.ulx) {
                parityShift = -parityShift;
            }

            int tileX = originTileX;
            for (int y = 0; y < updateHexHeight; y++) {
                for (int x = 0; x < updateHexWidth; x++) {
                    int tile = tile_num(tileX, originTileY, 0);
                    if (tile == -1) {
                        goto err;
                    }

                    tileX += 32;
                    *offsets++ = tile - originTile;
                }

                tileX = parityShift + originTileX;
                originTileY += 12;
                parityShift = -parityShift;
            }
        }

        if (tile_set_center(tile_center_tile + 1, TILE_SET_CENTER_FLAG_IGNORE_SCROLL_RESTRICTIONS) == -1) {
            goto err;
        }
    }

    offsetDivTable = (int*)mem_malloc(sizeof(int) * updateHexArea);
    if (offsetDivTable == NULL) {
        goto err;
    }

    for (i = 0; i < updateHexArea; i++) {
        offsetDivTable[i] = i / updateHexWidth;
    }

    offsetModTable = (int*)mem_malloc(sizeof(int) * updateHexArea);
    if (offsetModTable == NULL) {
        goto err;
    }

    for (i = 0; i < updateHexArea; i++) {
        offsetModTable[i] = i % updateHexWidth;
    }

    return 0;

err:
    obj_offset_table_exit();

    return -1;
}

// 0x47E484
static void obj_offset_table_exit()
{
    if (offsetModTable != NULL) {
        mem_free(offsetModTable);
        offsetModTable = NULL;
    }

    if (offsetDivTable != NULL) {
        mem_free(offsetDivTable);
        offsetDivTable = NULL;
    }

    if (offsetTable[1] != NULL) {
        mem_free(offsetTable[1]);
        offsetTable[1] = NULL;
    }

    if (offsetTable[0] != NULL) {
        mem_free(offsetTable[0]);
        offsetTable[0] = NULL;
    }
}

// 0x47E4F4
static int obj_order_table_init()
{
    if (orderTable[0] != NULL || orderTable[1] != NULL) {
        return -1;
    }

    orderTable[0] = (int*)mem_malloc(sizeof(int) * updateHexArea);
    if (orderTable[0] == NULL) {
        goto err;
    }

    orderTable[1] = (int*)mem_malloc(sizeof(int) * updateHexArea);
    if (orderTable[1] == NULL) {
        goto err;
    }

    for (int index = 0; index < updateHexArea; index++) {
        orderTable[0][index] = index;
        orderTable[1][index] = index;
    }

    qsort(orderTable[0], updateHexArea, sizeof(int), obj_order_comp_func_even);
    qsort(orderTable[1], updateHexArea, sizeof(int), obj_order_comp_func_odd);

    return 0;

err:

    // NOTE: Uninline.
    obj_order_table_exit();

    return -1;
}

// 0x47E604
static int obj_order_comp_func_even(const void* a1, const void* a2)
{
    int v1 = *(int*)a1;
    int v2 = *(int*)a2;
    return offsetTable[0][v1] - offsetTable[0][v2];
}

// 0x47E61C
static int obj_order_comp_func_odd(const void* a1, const void* a2)
{
    int v1 = *(int*)a1;
    int v2 = *(int*)a2;
    return offsetTable[1][v1] - offsetTable[1][v2];
}

// 0x47E634
static void obj_order_table_exit()
{
    if (orderTable[1] != NULL) {
        mem_free(orderTable[1]);
        orderTable[1] = NULL;
    }

    if (orderTable[0] != NULL) {
        mem_free(orderTable[0]);
        orderTable[0] = NULL;
    }
}

// 0x47E670
static int obj_render_table_init()
{
    if (renderTable != NULL) {
        return -1;
    }

    renderTable = (ObjectListNode**)mem_malloc(sizeof(*renderTable) * updateHexArea);
    if (renderTable == NULL) {
        return -1;
    }

    for (int index = 0; index < updateHexArea; index++) {
        renderTable[index] = NULL;
    }

    return 0;
}

// 0x47E6E4
static void obj_render_table_exit()
{
    if (renderTable != NULL) {
        mem_free(renderTable);
        renderTable = NULL;
    }
}

// 0x47E704
static void obj_light_table_init()
{
    for (int s = 0; s < 2; s++) {
        int v4 = tile_center_tile + s;
        for (int i = 0; i < ROTATION_COUNT; i++) {
            int v15 = 8;
            int* p = light_offsets[v4 & 1][i];
            for (int j = 0; j < 8; j++) {
                int tile = tile_num_in_direction(v4, (i + 1) % ROTATION_COUNT, j);

                for (int m = 0; m < v15; m++) {
                    *p++ = tile_num_in_direction(tile, i, m + 1) - v4;
                }

                v15--;
            }
        }
    }
}

// 0x47E8C8
static void obj_blend_table_init()
{
    for (int index = 0; index < 256; index++) {
        int r = (Color2RGB(index) & 0x7C00) >> 10;
        int g = (Color2RGB(index) & 0x3E0) >> 5;
        int b = Color2RGB(index) & 0x1F;
        glassGrayTable[index] = ((r + 5 * g + 4 * b) / 10) >> 2;
        commonGrayTable[index] = ((b + 3 * r + 6 * g) / 10) >> 2;
    }

    glassGrayTable[0] = 0;
    commonGrayTable[0] = 0;

    wallBlendTable = getColorBlendTable(colorTable[25439]);
    glassBlendTable = getColorBlendTable(colorTable[10239]);
    steamBlendTable = getColorBlendTable(colorTable[32767]);
    energyBlendTable = getColorBlendTable(colorTable[30689]);
    redBlendTable = getColorBlendTable(colorTable[31744]);
}

// 0x47E9CC
static void obj_blend_table_exit()
{
    freeColorBlendTable(colorTable[25439]);
    freeColorBlendTable(colorTable[10239]);
    freeColorBlendTable(colorTable[32767]);
    freeColorBlendTable(colorTable[30689]);
    freeColorBlendTable(colorTable[31744]);
}

// 0x47EA08
static void obj_misc_table_init()
{
    centerToUpperLeft = tile_num(updateAreaPixelBounds.ulx, updateAreaPixelBounds.uly, 0) - tile_center_tile;
}

// 0x47EA2C
int obj_save_obj(DB_FILE* stream, Object* object)
{
    if ((object->flags & OBJECT_NO_SAVE) != 0) {
        return 0;
    }

    CritterCombatData* combatData = NULL;
    Object* whoHitMe = NULL;
    if (PID_TYPE(object->pid) == OBJ_TYPE_CRITTER) {
        combatData = &(object->data.critter.combat);
        whoHitMe = combatData->whoHitMe;
        if (whoHitMe != 0) {
            if (combatData->whoHitMeCid != -1) {
                combatData->whoHitMeCid = whoHitMe->cid;
            }
        } else {
            combatData->whoHitMeCid = -1;
        }
    }

    if (obj_write_obj(object, stream) == -1) {
        return -1;
    }

    if (PID_TYPE(object->pid) == OBJ_TYPE_CRITTER) {
        combatData->whoHitMe = whoHitMe;
    }

    Inventory* inventory = &(object->data.inventory);
    for (int index = 0; index < inventory->length; index++) {
        InventoryItem* inventoryItem = &(inventory->items[index]);

        if (db_fwriteInt(stream, inventoryItem->quantity) == -1) {
            return -1;
        }

        if (obj_save_obj(stream, inventoryItem->item) == -1) {
            return -1;
        }

        if ((inventoryItem->item->flags & OBJECT_NO_SAVE) != 0) {
            return -1;
        }
    }

    return 0;
}

// 0x47EAF8
int obj_load_obj(DB_FILE* stream, Object** objectPtr, int elevation, Object* owner)
{
    Object* obj;

    if (obj_create_object(&obj) == -1) {
        *objectPtr = NULL;
        return -1;
    }

    if (obj_read_obj(obj, stream) != 0) {
        *objectPtr = NULL;
        return -1;
    }

    if (obj->sid != -1) {
        Script* script;
        if (scr_ptr(obj->sid, &script) == -1) {
            obj->sid = -1;
        } else {
            script->owner = obj;
        }
    }

    obj_fix_violence_settings(&(obj->fid));

    if (!art_fid_valid(obj->fid)) {
        debug_printf("\nError: invalid object art fid: %u\n", obj->fid);
        // NOTE: Uninline.
        obj_destroy_object(&obj);
        return -2;
    }

    if (elevation == -1) {
        elevation = obj->elevation;
    } else {
        obj->elevation = elevation;
    }

    obj->owner = owner;

    Inventory* inventory = &(obj->data.inventory);
    if (inventory->length <= 0) {
        inventory->capacity = 0;
        inventory->items = NULL;
        *objectPtr = obj;
        return 0;
    }

    InventoryItem* inventoryItems = inventory->items = (InventoryItem*)mem_malloc(sizeof(*inventoryItems) * inventory->capacity);
    if (inventoryItems == NULL) {
        return -1;
    }

    for (int inventoryItemIndex = 0; inventoryItemIndex < inventory->length; inventoryItemIndex++) {
        InventoryItem* inventoryItem = &(inventoryItems[inventoryItemIndex]);
        if (db_freadInt(stream, &(inventoryItem->quantity)) != 0) {
            return -1;
        }

        if (obj_load_obj(stream, &(inventoryItem->item), elevation, obj) != 0) {
            return -1;
        }
    }

    *objectPtr = obj;

    return 0;
}

// 0x47EC80
int obj_save_dude(DB_FILE* stream)
{
    int field_78 = obj_dude->sid;

    obj_dude->flags &= ~OBJECT_NO_SAVE;
    obj_dude->sid = -1;

    int rc = obj_save_obj(stream, obj_dude);

    obj_dude->sid = field_78;
    obj_dude->flags |= OBJECT_NO_SAVE;

    if (db_fwriteInt(stream, tile_center_tile) == -1) {
        db_fclose(stream);
        return -1;
    }

    return rc;
}

// 0x47ECE4
int obj_load_dude(DB_FILE* stream)
{
    int savedTile = obj_dude->tile;
    int savedElevation = obj_dude->elevation;
    int savedRotation = obj_dude->rotation;
    int savedOid = obj_dude->id;

    scr_clear_dude_script();

    Object* temp;
    int rc = obj_load_obj(stream, &temp, -1, NULL);

    memcpy(obj_dude, temp, sizeof(*obj_dude));

    obj_dude->flags |= OBJECT_NO_SAVE;

    scr_clear_dude_script();

    obj_dude->id = savedOid;

    scr_set_dude_script();

    int newTile = obj_dude->tile;
    obj_dude->tile = savedTile;

    int newElevation = obj_dude->elevation;
    obj_dude->elevation = savedElevation;

    int newRotation = obj_dude->rotation;
    obj_dude->rotation = newRotation;

    scr_set_dude_script();

    if (rc != -1) {
        obj_move_to_tile(obj_dude, newTile, newElevation, NULL);
        obj_set_rotation(obj_dude, newRotation, NULL);
    }

    // Set ownership of inventory items from temporary instance to dude.
    Inventory* inventory = &(obj_dude->data.inventory);
    for (int index = 0; index < inventory->length; index++) {
        InventoryItem* inventoryItem = &(inventory->items[index]);
        inventoryItem->item->owner = obj_dude;
    }

    obj_fix_combat_cid_for_dude();

    // Dude has claimed ownership of items in temporary instance's inventory.
    // We don't need object's dealloc routine to remove these items from the
    // game, so simply nullify temporary inventory as if nothing was there.
    Inventory* tempInventory = &(temp->data.inventory);
    tempInventory->length = 0;
    tempInventory->capacity = 0;
    tempInventory->items = NULL;

    temp->flags &= ~OBJECT_NO_REMOVE;

    if (obj_erase_object(temp, NULL) == -1) {
        debug_printf("\nError: obj_load_dude: Can't destroy temp object!\n");
    }

    inven_reset_dude();

    int tile;
    if (db_freadInt(stream, &tile) == -1) {
        db_fclose(stream);
        return -1;
    }

    tile_set_center(tile, TILE_SET_CENTER_REFRESH_WINDOW | TILE_SET_CENTER_FLAG_IGNORE_SCROLL_RESTRICTIONS);

    return rc;
}

// 0x47EE5C
static int obj_create_object(Object** objectPtr)
{
    if (objectPtr == NULL) {
        return -1;
    }

    Object* object = *objectPtr = (Object*)mem_malloc(sizeof(Object));
    if (object == NULL) {
        return -1;
    }

    memset(object, 0, sizeof(Object));

    object->id = -1;
    object->tile = -1;
    object->cid = -1;
    object->outline = 0;
    object->pid = -1;
    object->sid = -1;
    object->owner = NULL;
    object->field_80 = -1;

    return 0;
}

// 0x47EEDC
static void obj_destroy_object(Object** objectPtr)
{
    if (objectPtr == NULL) {
        return;
    }

    if (*objectPtr == NULL) {
        return;
    }

    mem_free(*objectPtr);

    *objectPtr = NULL;
}

// 0x47EEFC
static int obj_create_object_node(ObjectListNode** nodePtr)
{
    if (nodePtr == NULL) {
        return -1;
    }

    ObjectListNode* node = *nodePtr = (ObjectListNode*)mem_malloc(sizeof(*node));
    if (node == NULL) {
        return -1;
    }

    node->obj = NULL;
    node->next = NULL;

    return 0;
}

// 0x47EF30
static void obj_destroy_object_node(ObjectListNode** nodePtr)
{
    if (nodePtr == NULL) {
        return;
    }

    if (*nodePtr == NULL) {
        return;
    }

    mem_free(*nodePtr);

    *nodePtr = NULL;
}

// 0x47EF50
static int obj_node_ptr(Object* object, ObjectListNode** nodePtr, ObjectListNode** previousNodePtr)
{
    if (object == NULL) {
        return -1;
    }

    if (nodePtr == NULL) {
        return -1;
    }

    int tile = object->tile;
    if (tile != -1) {
        *nodePtr = objectTable[tile];
    } else {
        *nodePtr = floatingObjects;
    }

    if (previousNodePtr != NULL) {
        *previousNodePtr = NULL;
        while (*nodePtr != NULL) {
            if (object == (*nodePtr)->obj) {
                break;
            }

            *previousNodePtr = *nodePtr;

            *nodePtr = (*nodePtr)->next;
        }
    } else {
        while (*nodePtr != NULL) {
            if (object == (*nodePtr)->obj) {
                break;
            }

            *nodePtr = (*nodePtr)->next;
        }
    }

    if (*nodePtr != NULL) {
        return 0;
    }

    return -1;
}

// 0x47EFCC
static void obj_insert(ObjectListNode* objectListNode)
{
    ObjectListNode** objectListNodePtr;

    if (objectListNode == NULL) {
        return;
    }

    if (objectListNode->obj->tile == -1) {
        objectListNodePtr = &floatingObjects;
    } else {
        Art* art = NULL;
        CacheEntry* cacheHandle = NULL;

        objectListNodePtr = &(objectTable[objectListNode->obj->tile]);

        while (*objectListNodePtr != NULL) {
            Object* obj = (*objectListNodePtr)->obj;
            if (obj->elevation > objectListNode->obj->elevation) {
                break;
            }

            if (obj->elevation == objectListNode->obj->elevation) {
                if ((obj->flags & OBJECT_FLAT) == 0 && (objectListNode->obj->flags & OBJECT_FLAT) != 0) {
                    break;
                }

                if ((obj->flags & OBJECT_FLAT) == (objectListNode->obj->flags & OBJECT_FLAT)) {
                    bool v11 = false;
                    CacheEntry* a2;
                    Art* v12 = art_ptr_lock(obj->fid, &a2);
                    if (v12 != NULL) {

                        if (art == NULL) {
                            art = art_ptr_lock(objectListNode->obj->fid, &cacheHandle);
                        }

                        // TODO: Incomplete.

                        art_ptr_unlock(a2);

                        if (v11) {
                            break;
                        }
                    }
                }
            }

            objectListNodePtr = &((*objectListNodePtr)->next);
        }

        if (art != NULL) {
            art_ptr_unlock(cacheHandle);
        }
    }

    objectListNode->next = *objectListNodePtr;
    *objectListNodePtr = objectListNode;
}

// 0x47F13C
static int obj_remove(ObjectListNode* a1, ObjectListNode* a2)
{
    if (a1->obj == NULL) {
        return -1;
    }

    if ((a1->obj->flags & OBJECT_NO_REMOVE) != 0) {
        return -1;
    }

    obj_inven_free(&(a1->obj->data.inventory));

    if (a1->obj->sid != -1) {
        exec_script_proc(a1->obj->sid, SCRIPT_PROC_DESTROY);
        scr_remove(a1->obj->sid);
    }

    if (a1 != a2) {
        if (a2 != NULL) {
            a2->next = a1->next;
        } else {
            int tile = a1->obj->tile;
            if (tile == -1) {
                floatingObjects = floatingObjects->next;
            } else {
                objectTable[tile] = objectTable[tile]->next;
            }
        }
    }

    // NOTE: Uninline.
    obj_destroy_object(&(a1->obj));

    // NOTE: Uninline.
    obj_destroy_object_node(&a1);

    return 0;
}

// 0x47F20C
static int obj_connect_to_tile(ObjectListNode* node, int tile, int elevation, Rect* rect)
{
    if (node == NULL) {
        return -1;
    }

    if (!hexGridTileIsValid(tile)) {
        return -1;
    }

    if (!elevationIsValid(elevation)) {
        return -1;
    }

    node->obj->tile = tile;
    node->obj->elevation = elevation;
    node->obj->x = 0;
    node->obj->y = 0;
    node->obj->owner = 0;

    obj_insert(node);

    if (obj_adjust_light(node->obj, 0, rect) == -1) {
        if (rect != NULL) {
            obj_bound(node->obj, rect);
        }
    }

    return 0;
}

// 0x47F30C
static int obj_adjust_light(Object* obj, int a2, Rect* rect)
{
    if (obj == NULL) {
        return -1;
    }

    if (obj->lightIntensity <= 0) {
        return -1;
    }

    if ((obj->flags & OBJECT_HIDDEN) != 0) {
        return -1;
    }

    if ((obj->flags & OBJECT_LIGHTING) == 0) {
        return -1;
    }

    if (!hexGridTileIsValid(obj->tile)) {
        return -1;
    }

    AdjustLightIntensityProc* adjustLightIntensity = a2 ? light_subtract_from_tile : light_add_to_tile;
    adjustLightIntensity(obj->elevation, obj->tile, obj->lightIntensity);

    Rect objectRect;
    obj_bound(obj, &objectRect);

    if (obj->lightDistance > 8) {
        obj->lightDistance = 8;
    }

    if (obj->lightIntensity > 65536) {
        obj->lightIntensity = 65536;
    }

    int(*v70)[36] = light_offsets[obj->tile & 1];
    int v7 = (obj->lightIntensity - 655) / (obj->lightDistance + 1);
    int v28[36];
    v28[0] = obj->lightIntensity - v7;
    v28[1] = v28[0] - v7;
    v28[8] = v28[0] - v7;
    v28[2] = v28[0] - v7 - v7;
    v28[9] = v28[2];
    v28[15] = v28[0] - v7 - v7;
    v28[3] = v28[2] - v7;
    v28[10] = v28[2] - v7;
    v28[16] = v28[2] - v7;
    v28[21] = v28[2] - v7;
    v28[4] = v28[2] - v7 - v7;
    v28[11] = v28[4];
    v28[17] = v28[2] - v7 - v7;
    v28[22] = v28[2] - v7 - v7;
    v28[26] = v28[2] - v7 - v7;
    v28[5] = v28[4] - v7;
    v28[12] = v28[4] - v7;
    v28[18] = v28[4] - v7;
    v28[23] = v28[4] - v7;
    v28[27] = v28[4] - v7;
    v28[30] = v28[4] - v7;
    v28[6] = v28[4] - v7 - v7;
    v28[13] = v28[6];
    v28[19] = v28[4] - v7 - v7;
    v28[24] = v28[4] - v7 - v7;
    v28[28] = v28[4] - v7 - v7;
    v28[31] = v28[4] - v7 - v7;
    v28[33] = v28[4] - v7 - v7;
    v28[7] = v28[6] - v7;
    v28[14] = v28[6] - v7;
    v28[20] = v28[6] - v7;
    v28[25] = v28[6] - v7;
    v28[29] = v28[6] - v7;
    v28[32] = v28[6] - v7;
    v28[34] = v28[6] - v7;
    v28[35] = v28[6] - v7;

    for (int index = 0; index < 36; index++) {
        if (obj->lightDistance >= light_distance[index]) {
            for (int rotation = 0; rotation < ROTATION_COUNT; rotation++) {
                int v14;
                int nextRotation = (rotation + 1) % ROTATION_COUNT;
                int eax;
                int edx;
                int ebx;
                int esi;
                int edi;
                switch (index) {
                case 0:
                    v14 = 0;
                    break;
                case 1:
                    v14 = light_blocked[rotation][0];
                    break;
                case 2:
                    v14 = light_blocked[rotation][1];
                    break;
                case 3:
                    v14 = light_blocked[rotation][2];
                    break;
                case 4:
                    v14 = light_blocked[rotation][3];
                    break;
                case 5:
                    v14 = light_blocked[rotation][4];
                    break;
                case 6:
                    v14 = light_blocked[rotation][5];
                    break;
                case 7:
                    v14 = light_blocked[rotation][6];
                    break;
                case 8:
                    v14 = light_blocked[rotation][0] & light_blocked[nextRotation][0];
                    break;
                case 9:
                    v14 = light_blocked[rotation][1] & light_blocked[rotation][8];
                    break;
                case 10:
                    v14 = light_blocked[rotation][2] & light_blocked[rotation][9];
                    break;
                case 11:
                    v14 = light_blocked[rotation][3] & light_blocked[rotation][10];
                    break;
                case 12:
                    v14 = light_blocked[rotation][4] & light_blocked[rotation][11];
                    break;
                case 13:
                    v14 = light_blocked[rotation][5] & light_blocked[rotation][12];
                    break;
                case 14:
                    v14 = light_blocked[rotation][6] & light_blocked[rotation][13];
                    break;
                case 15:
                    v14 = light_blocked[rotation][8] & light_blocked[nextRotation][1];
                    break;
                case 16:
                    v14 = light_blocked[rotation][8] | (light_blocked[rotation][9] & light_blocked[rotation][15]);
                    break;
                case 17:
                    edx = light_blocked[rotation][9];
                    edx |= light_blocked[rotation][10];
                    ebx = light_blocked[rotation][8];
                    esi = light_blocked[rotation][16];
                    ebx &= edx;
                    edx &= esi;
                    edi = light_blocked[rotation][15];
                    ebx |= edx;
                    edx = light_blocked[rotation][10];
                    eax = light_blocked[rotation][9];
                    edx |= edi;
                    eax &= edx;
                    v14 = ebx | eax;
                    break;
                case 18:
                    edx = light_blocked[rotation][0];
                    ebx = light_blocked[rotation][9];
                    esi = light_blocked[rotation][10];
                    edx |= ebx;
                    edi = light_blocked[rotation][11];
                    edx |= esi;
                    ebx = light_blocked[rotation][17];
                    edx |= edi;
                    ebx &= edx;
                    edx = esi;
                    esi = light_blocked[rotation][16];
                    edi = light_blocked[rotation][9];
                    edx &= esi;
                    edx |= edi;
                    edx |= ebx;
                    v14 = edx;
                    break;
                case 19:
                    edx = light_blocked[rotation][17];
                    edi = light_blocked[rotation][18];
                    ebx = light_blocked[rotation][11];
                    edx |= edi;
                    esi = light_blocked[rotation][10];
                    ebx &= edx;
                    edx = light_blocked[rotation][9];
                    edx |= esi;
                    ebx |= edx;
                    edx = light_blocked[rotation][12];
                    edx &= edi;
                    ebx |= edx;
                    v14 = ebx;
                    break;
                case 20:
                    edx = light_blocked[rotation][2];
                    esi = light_blocked[rotation][11];
                    edi = light_blocked[rotation][12];
                    ebx = light_blocked[rotation][8];
                    edx |= esi;
                    esi = light_blocked[rotation][9];
                    edx |= edi;
                    edi = light_blocked[rotation][10];
                    ebx &= edx;
                    edx &= esi;
                    esi = light_blocked[rotation][17];
                    ebx |= edx;
                    edx = light_blocked[rotation][16];
                    ebx |= edi;
                    edi = light_blocked[rotation][18];
                    edx |= esi;
                    esi = light_blocked[rotation][19];
                    edx |= edi;
                    eax = light_blocked[rotation][11];
                    edx |= esi;
                    eax &= edx;
                    ebx |= eax;
                    v14 = ebx;
                    break;
                case 21:
                    v14 = (light_blocked[rotation][8] & light_blocked[nextRotation][1])
                        | (light_blocked[rotation][15] & light_blocked[nextRotation][2]);
                    break;
                case 22:
                    edx = light_blocked[nextRotation][1];
                    ebx = light_blocked[rotation][15];
                    esi = light_blocked[rotation][21];
                    edx |= ebx;
                    ebx = light_blocked[rotation][8];
                    edx |= esi;
                    ebx &= edx;
                    edx = light_blocked[rotation][9];
                    edi = esi;
                    edx |= esi;
                    esi = light_blocked[rotation][15];
                    edx &= esi;
                    ebx |= edx;
                    edx = esi;
                    esi = light_blocked[rotation][16];
                    edx |= edi;
                    edx &= esi;
                    ebx |= edx;
                    v14 = ebx;
                    break;
                case 23:
                    edx = light_blocked[rotation][3];
                    ebx = light_blocked[rotation][16];
                    esi = light_blocked[rotation][15];
                    ebx |= edx;
                    edx = light_blocked[rotation][9];
                    edx &= esi;
                    edi = light_blocked[rotation][22];
                    ebx |= edx;
                    edx = light_blocked[rotation][17];
                    edx &= edi;
                    ebx |= edx;
                    v14 = ebx;
                    break;
                case 24:
                    edx = light_blocked[rotation][0];
                    edi = light_blocked[rotation][9];
                    ebx = light_blocked[rotation][10];
                    edx |= edi;
                    esi = light_blocked[rotation][17];
                    edx |= ebx;
                    edi = light_blocked[rotation][18];
                    edx |= esi;
                    ebx = light_blocked[rotation][16];
                    edx |= edi;
                    esi = light_blocked[rotation][16];
                    ebx &= edx;
                    edx = light_blocked[rotation][15];
                    edi = light_blocked[rotation][23];
                    edx |= esi;
                    esi = light_blocked[rotation][9];
                    edx |= edi;
                    edi = light_blocked[rotation][8];
                    edx &= esi;
                    edx |= edi;
                    esi = light_blocked[rotation][22];
                    ebx |= edx;
                    edx = light_blocked[rotation][15];
                    edi = light_blocked[rotation][23];
                    edx |= esi;
                    esi = light_blocked[rotation][17];
                    edx |= edi;
                    edx &= esi;
                    ebx |= edx;
                    edx = light_blocked[rotation][18];
                    edx &= edi;
                    ebx |= edx;
                    v14 = ebx;
                    break;
                case 25:
                    edx = light_blocked[rotation][8];
                    edi = light_blocked[rotation][15];
                    ebx = light_blocked[rotation][16];
                    edx |= edi;
                    esi = light_blocked[rotation][23];
                    edx |= ebx;
                    edi = light_blocked[rotation][24];
                    edx |= esi;
                    ebx = light_blocked[rotation][9];
                    edx |= edi;
                    esi = light_blocked[rotation][1];
                    ebx &= edx;
                    edx = light_blocked[rotation][8];
                    edx &= esi;
                    edi = light_blocked[rotation][16];
                    ebx |= edx;
                    edx = light_blocked[rotation][8];
                    esi = light_blocked[rotation][17];
                    edx |= edi;
                    edi = light_blocked[rotation][24];
                    esi |= edx;
                    esi |= edi;
                    esi &= light_blocked[rotation][10];
                    edi = light_blocked[rotation][23];
                    ebx |= esi;
                    esi = light_blocked[rotation][17];
                    edx |= edi;
                    ebx |= esi;
                    esi = light_blocked[rotation][24];
                    edi = light_blocked[rotation][18];
                    edx |= esi;
                    edx &= edi;
                    esi = light_blocked[rotation][19];
                    ebx |= edx;
                    edx = light_blocked[rotation][0];
                    eax = light_blocked[rotation][24];
                    edx |= esi;
                    eax &= edx;
                    ebx |= eax;
                    v14 = ebx;
                    break;
                case 26:
                    ebx = light_blocked[rotation][8];
                    esi = light_blocked[nextRotation][1];
                    edi = light_blocked[nextRotation][2];
                    esi &= ebx;
                    ebx = light_blocked[rotation][15];
                    ebx &= edi;
                    eax = light_blocked[rotation][21];
                    ebx |= esi;
                    eax &= light_blocked[nextRotation][3];
                    ebx |= eax;
                    v14 = ebx;
                    break;
                case 27:
                    edx = light_blocked[nextRotation][0];
                    edi = light_blocked[rotation][15];
                    esi = light_blocked[rotation][21];
                    edx |= edi;
                    edi = light_blocked[rotation][26];
                    edx |= esi;
                    esi = light_blocked[rotation][22];
                    edx |= edi;
                    edi = light_blocked[nextRotation][1];
                    esi &= edx;
                    edx = light_blocked[rotation][8];
                    ebx = light_blocked[rotation][15];
                    edx &= edi;
                    edx |= ebx;
                    edi = light_blocked[rotation][16];
                    esi |= edx;
                    edx = light_blocked[rotation][8];
                    eax = light_blocked[rotation][21];
                    edx |= edi;
                    eax &= edx;
                    esi |= eax;
                    v14 = esi;
                    break;
                case 28:
                    ebx = light_blocked[rotation][9];
                    edi = light_blocked[rotation][16];
                    esi = light_blocked[rotation][23];
                    edx = light_blocked[nextRotation][0];
                    ebx |= edi;
                    edi = light_blocked[rotation][15];
                    ebx |= esi;
                    esi = light_blocked[rotation][8];
                    ebx &= edi;
                    edi = light_blocked[rotation][21];
                    ebx |= esi;
                    esi = light_blocked[rotation][22];
                    edx |= edi;
                    edi = light_blocked[rotation][27];
                    edx |= esi;
                    esi = light_blocked[rotation][16];
                    edx |= edi;
                    edx &= esi;
                    edi = light_blocked[rotation][17];
                    ebx |= edx;
                    edx = light_blocked[rotation][9];
                    esi = light_blocked[rotation][23];
                    edx |= edi;
                    edi = light_blocked[rotation][22];
                    edx |= esi;
                    edx &= edi;
                    ebx |= edx;
                    edx = esi;
                    edx &= light_blocked[rotation][27];
                    ebx |= edx;
                    v14 = ebx;
                    break;
                case 29:
                    edx = light_blocked[rotation][8];
                    edi = light_blocked[rotation][16];
                    ebx = light_blocked[rotation][23];
                    edx |= edi;
                    esi = light_blocked[rotation][15];
                    ebx |= edx;
                    edx = light_blocked[rotation][9];
                    edx &= esi;
                    edi = light_blocked[rotation][22];
                    ebx |= edx;
                    edx = light_blocked[rotation][17];
                    edx &= edi;
                    esi = light_blocked[rotation][28];
                    ebx |= edx;
                    edx = light_blocked[rotation][24];
                    edx &= esi;
                    ebx |= edx;
                    v14 = ebx;
                    break;
                case 30:
                    ebx = light_blocked[rotation][8];
                    esi = light_blocked[nextRotation][1];
                    edi = light_blocked[nextRotation][2];
                    esi &= ebx;
                    ebx = light_blocked[rotation][15];
                    ebx &= edi;
                    edi = light_blocked[nextRotation][3];
                    esi |= ebx;
                    ebx = light_blocked[rotation][21];
                    ebx &= edi;
                    eax = light_blocked[rotation][26];
                    ebx |= esi;
                    eax &= light_blocked[nextRotation][4];
                    ebx |= eax;
                    v14 = ebx;
                    break;
                case 31:
                    edx = light_blocked[rotation][8];
                    esi = light_blocked[nextRotation][1];
                    edi = light_blocked[rotation][15];
                    edx &= esi;
                    ebx = light_blocked[rotation][21];
                    edx |= edi;
                    esi = light_blocked[rotation][22];
                    ebx |= edx;
                    edx = light_blocked[rotation][8];
                    edi = light_blocked[rotation][27];
                    edx |= esi;
                    esi = light_blocked[rotation][26];
                    edx |= edi;
                    edx &= esi;
                    ebx |= edx;
                    edx = edi;
                    edx &= light_blocked[rotation][30];
                    ebx |= edx;
                    v14 = ebx;
                    break;
                case 32:
                    ebx = light_blocked[rotation][8];
                    edi = light_blocked[rotation][9];
                    esi = light_blocked[rotation][16];
                    ebx |= edi;
                    edi = light_blocked[rotation][23];
                    ebx |= esi;
                    esi = light_blocked[rotation][28];
                    ebx |= edi;
                    ebx |= esi;
                    esi = light_blocked[rotation][15];
                    esi &= ebx;
                    edx = light_blocked[rotation][8];
                    edx &= light_blocked[nextRotation][1];
                    ebx = light_blocked[rotation][16];
                    esi |= edx;
                    edx = light_blocked[rotation][8];
                    edx |= ebx;
                    ebx = light_blocked[rotation][28];
                    edi = light_blocked[rotation][21];
                    ebx |= edx;
                    ebx &= edi;
                    edi = light_blocked[rotation][23];
                    ebx |= esi;
                    esi = light_blocked[rotation][22];
                    edx |= edi;
                    ebx |= esi;
                    esi = light_blocked[rotation][28];
                    edi = light_blocked[rotation][27];
                    edx |= esi;
                    edx &= edi;
                    esi = light_blocked[rotation][31];
                    ebx |= edx;
                    edx = light_blocked[rotation][0];
                    edi = light_blocked[rotation][28];
                    edx |= esi;
                    edx &= edi;
                    ebx |= edx;
                    v14 = ebx;
                    break;
                case 33:
                    esi = light_blocked[rotation][8];
                    edi = light_blocked[nextRotation][1];
                    ebx = light_blocked[rotation][15];
                    esi &= edi;
                    ebx &= light_blocked[nextRotation][2];
                    edi = light_blocked[nextRotation][3];
                    esi |= ebx;
                    ebx = light_blocked[rotation][21];
                    ebx &= edi;
                    edi = light_blocked[nextRotation][4];
                    esi |= ebx;
                    ebx = light_blocked[rotation][26];
                    ebx &= edi;
                    eax = light_blocked[rotation][30];
                    ebx |= esi;
                    eax &= light_blocked[nextRotation][5];
                    ebx |= eax;
                    v14 = ebx;
                    break;
                case 34:
                    edx = light_blocked[nextRotation][2];
                    edi = light_blocked[rotation][26];
                    ebx = light_blocked[rotation][30];
                    edx |= edi;
                    esi = light_blocked[rotation][15];
                    edx |= ebx;
                    ebx = light_blocked[rotation][8];
                    edi = light_blocked[rotation][21];
                    ebx &= edx;
                    edx &= esi;
                    esi = light_blocked[rotation][22];
                    ebx |= edx;
                    edx = light_blocked[rotation][16];
                    ebx |= edi;
                    edi = light_blocked[rotation][27];
                    edx |= esi;
                    esi = light_blocked[rotation][31];
                    edx |= edi;
                    eax = light_blocked[rotation][26];
                    edx |= esi;
                    eax &= edx;
                    ebx |= eax;
                    v14 = ebx;
                    break;
                case 35:
                    ebx = light_blocked[rotation][8];
                    esi = light_blocked[nextRotation][1];
                    edi = light_blocked[nextRotation][2];
                    esi &= ebx;
                    ebx = light_blocked[rotation][15];
                    ebx &= edi;
                    edi = light_blocked[nextRotation][3];
                    esi |= ebx;
                    ebx = light_blocked[rotation][21];
                    ebx &= edi;
                    edi = light_blocked[nextRotation][4];
                    esi |= ebx;
                    ebx = light_blocked[rotation][26];
                    ebx &= edi;
                    edi = light_blocked[nextRotation][5];
                    esi |= ebx;
                    ebx = light_blocked[rotation][30];
                    ebx &= edi;
                    eax = light_blocked[rotation][33];
                    ebx |= esi;
                    eax &= light_blocked[nextRotation][6];
                    ebx |= eax;
                    v14 = ebx;
                    break;
                default:
                    assert(false && "Should be unreachable");
                }

                if (v14 == 0) {
                    // TODO: Check.
                    int tile = obj->tile + v70[rotation][index];
                    if (hexGridTileIsValid(tile)) {
                        bool v12 = true;

                        ObjectListNode* objectListNode = objectTable[tile];
                        while (objectListNode != NULL) {
                            if ((objectListNode->obj->flags & OBJECT_HIDDEN) == 0) {
                                if (objectListNode->obj->elevation > obj->elevation) {
                                    break;
                                }

                                if (objectListNode->obj->elevation == obj->elevation) {
                                    Rect v29;
                                    obj_bound(objectListNode->obj, &v29);
                                    rect_min_bound(&objectRect, &v29, &objectRect);

                                    v14 = (objectListNode->obj->flags & OBJECT_LIGHT_THRU) == 0;

                                    if (FID_TYPE(objectListNode->obj->fid) == OBJ_TYPE_WALL) {
                                        if ((objectListNode->obj->flags & OBJECT_FLAT) == 0) {
                                            Proto* proto;
                                            proto_ptr(objectListNode->obj->pid, &proto);
                                            if ((proto->wall.extendedFlags & 0x8000000) != 0 || (proto->wall.extendedFlags & 0x40000000) != 0) {
                                                if (rotation != ROTATION_W
                                                    && rotation != ROTATION_NW
                                                    && (rotation != ROTATION_NE || index >= 8)
                                                    && (rotation != ROTATION_SW || index <= 15)) {
                                                    v12 = false;
                                                }
                                            } else if ((proto->wall.extendedFlags & 0x10000000) != 0) {
                                                if (rotation != ROTATION_NE && rotation != ROTATION_NW) {
                                                    v12 = false;
                                                }
                                            } else if ((proto->wall.extendedFlags & 0x20000000) != 0) {
                                                if (rotation != ROTATION_NE
                                                    && rotation != ROTATION_E
                                                    && rotation != ROTATION_W
                                                    && rotation != ROTATION_NW
                                                    && (rotation != ROTATION_SW || index <= 15)) {
                                                    v12 = false;
                                                }
                                            } else {
                                                if (rotation != ROTATION_NE
                                                    && rotation != ROTATION_E
                                                    && (rotation != ROTATION_NW || index <= 7)) {
                                                    v12 = false;
                                                }
                                            }
                                        }
                                    } else {
                                        if (v14 && rotation >= ROTATION_E && rotation <= ROTATION_SW) {
                                            v12 = false;
                                        }
                                    }

                                    if (v14) {
                                        break;
                                    }
                                }
                            }
                            objectListNode = objectListNode->next;
                        }

                        if (v12) {
                            adjustLightIntensity(obj->elevation, tile, v28[index]);
                        }
                    }
                }

                light_blocked[rotation][index] = v14;
            }
        }
    }

    if (rect != NULL) {
        Rect* lightDistanceRect = &(light_rect[obj->lightDistance]);
        memcpy(rect, lightDistanceRect, sizeof(*lightDistanceRect));

        int x;
        int y;
        tile_coord(obj->tile, &x, &y, obj->elevation);
        x += 16;
        y += 8;

        x -= rect->lrx / 2;
        y -= rect->lry / 2;

        rectOffset(rect, x, y);
        rect_min_bound(rect, &objectRect, rect);
    }

    return 0;
}

// 0x4801A0
static void obj_render_outline(Object* object, Rect* rect)
{
    CacheEntry* cacheEntry;
    Art* art = art_ptr_lock(object->fid, &cacheEntry);
    if (art == NULL) {
        return;
    }

    int frameWidth = art_frame_width(art, object->frame, object->rotation);
    int frameHeight = art_frame_length(art, object->frame, object->rotation);

    Rect v49;
    v49.ulx = 0;
    v49.uly = 0;
    v49.lrx = frameWidth - 1;

    // FIXME: I'm not sure why it ignores frameHeight and makes separate call
    // to obtain height.
    v49.lry = art_frame_length(art, object->frame, object->rotation) - 1;

    Rect objectRect;
    if (object->tile == -1) {
        objectRect.ulx = object->sx;
        objectRect.uly = object->sy;
        objectRect.lrx = object->sx + frameWidth - 1;
        objectRect.lry = object->sy + frameHeight - 1;
    } else {
        int x;
        int y;
        tile_coord(object->tile, &x, &y, object->elevation);
        x += 16;
        y += 8;

        x += art->xOffsets[object->rotation];
        y += art->yOffsets[object->rotation];

        x += object->x;
        y += object->y;

        objectRect.ulx = x - frameWidth / 2;
        objectRect.uly = y - (frameHeight - 1);
        objectRect.lrx = objectRect.ulx + frameWidth - 1;
        objectRect.lry = y;

        object->sx = objectRect.ulx;
        object->sy = objectRect.uly;
    }

    Rect v32;
    rectCopy(&v32, rect);

    v32.ulx--;
    v32.uly--;
    v32.lrx++;
    v32.lry++;

    rect_inside_bound(&v32, &buf_rect, &v32);

    if (rect_inside_bound(&objectRect, &v32, &objectRect) == 0) {
        v49.ulx += objectRect.ulx - object->sx;
        v49.uly += objectRect.uly - object->sy;
        v49.lrx = v49.ulx + (objectRect.lrx - objectRect.ulx);
        v49.lry = v49.uly + (objectRect.lry - objectRect.uly);

        unsigned char* src = art_frame_data(art, object->frame, object->rotation);

        unsigned char* dest = back_buf + buf_full * object->sy + object->sx;
        int destStep = buf_full - frameWidth;

        unsigned char color;
        unsigned char* v47 = NULL;
        unsigned char* v48 = NULL;
        int v53 = object->outline & OUTLINE_PALETTED;
        int outlineType = object->outline & OUTLINE_TYPE_MASK;
        int v43;
        int v44;

        switch (outlineType) {
        case OUTLINE_TYPE_HOSTILE:
            color = 243;
            v53 = 0;
            v43 = 5;
            v44 = frameHeight / 5;
            break;
        case OUTLINE_TYPE_2:
            color = colorTable[31744];
            v44 = 0;
            if (v53 != 0) {
                v47 = commonGrayTable;
                v48 = redBlendTable;
            }
            break;
        case OUTLINE_TYPE_4:
            color = colorTable[15855];
            v44 = 0;
            if (v53 != 0) {
                v47 = commonGrayTable;
                v48 = wallBlendTable;
            }
            break;
        case OUTLINE_TYPE_FRIENDLY:
            v43 = 4;
            v44 = frameHeight / 4;
            color = 229;
            v53 = 0;
            break;
        case OUTLINE_TYPE_ITEM:
            v44 = 0;
            color = colorTable[30632];
            if (v53 != 0) {
                v47 = commonGrayTable;
                v48 = redBlendTable;
            }
            break;
        default:
            color = colorTable[31775];
            v53 = 0;
            v44 = 0;
            break;
        }

        unsigned char v54 = color;
        unsigned char* dest14 = dest;
        unsigned char* src15 = src;
        for (int y = 0; y < frameHeight; y++) {
            bool cycle = true;
            if (v44 != 0) {
                if (y % v44 == 0) {
                    v54++;
                }

                if (v54 > v43 + color - 1) {
                    v54 = color;
                }
            }

            int v22 = dest14 - back_buf;
            for (int x = 0; x < frameWidth; x++) {
                v22 = dest14 - back_buf;
                if (*src15 != 0 && cycle) {
                    if (x >= v49.ulx && x <= v49.lrx && y >= v49.uly && y <= v49.lry && v22 > 0 && v22 % buf_full != 0) {
                        unsigned char v20;
                        if (v53 != 0) {
                            v20 = v48[(v47[v54] << 8) + *(dest14 - 1)];
                        } else {
                            v20 = v54;
                        }
                        *(dest14 - 1) = v20;
                    }
                    cycle = false;
                } else if (*src15 == 0 && !cycle) {
                    if (x >= v49.ulx && x <= v49.lrx && y >= v49.uly && y <= v49.lry) {
                        int v21;
                        if (v53 != 0) {
                            v21 = v48[(v47[v54] << 8) + *dest14];
                        } else {
                            v21 = v54;
                        }
                        *dest14 = v21 & 0xFF;
                    }
                    cycle = true;
                }
                dest14++;
                src15++;
            }

            if (*(src15 - 1) != 0) {
                if (v22 < buf_size) {
                    int v23 = frameWidth - 1;
                    if (v23 >= v49.ulx && v23 <= v49.lrx && y >= v49.uly && y <= v49.lry) {
                        if (v53 != 0) {
                            *dest14 = v48[(v47[v54] << 8) + *dest14];
                        } else {
                            *dest14 = v54;
                        }
                    }
                }
            }

            dest14 += destStep;
        }

        for (int x = 0; x < frameWidth; x++) {
            bool cycle = true;
            unsigned char v28 = color;
            unsigned char* dest27 = dest + x;
            unsigned char* src27 = src + x;
            for (int y = 0; y < frameHeight; y++) {
                if (v44 != 0) {
                    if (y % v44 == 0) {
                        v28++;
                    }

                    if (v28 > color + v43 - 1) {
                        v28 = color;
                    }
                }

                if (*src27 != 0 && cycle) {
                    if (x >= v49.ulx && x <= v49.lrx && y >= v49.uly && y <= v49.lry) {
                        unsigned char* v29 = dest27 - buf_full;
                        if (v29 >= back_buf) {
                            if (v53) {
                                *v29 = v48[(v47[v28] << 8) + *v29];
                            } else {
                                *v29 = v28;
                            }
                        }
                    }
                    cycle = false;
                } else if (*src27 == 0 && !cycle) {
                    if (x >= v49.ulx && x <= v49.lrx && y >= v49.uly && y <= v49.lry) {
                        if (v53) {
                            *dest27 = v48[(v47[v28] << 8) + *dest27];
                        } else {
                            *dest27 = v28;
                        }
                    }
                    cycle = true;
                }

                dest27 += buf_full;
                src27 += frameWidth;
            }

            if (src27[-frameWidth] != 0) {
                if (dest27 - back_buf < buf_size) {
                    int y = frameHeight - 1;
                    if (x >= v49.ulx && x <= v49.lrx && y >= v49.uly && y <= v49.lry) {
                        if (v53) {
                            *dest27 = v48[(v47[v28] << 8) + *dest27];
                        } else {
                            *dest27 = v28;
                        }
                    }
                }
            }
        }
    }

    art_ptr_unlock(cacheEntry);
}

// 0x480868
static void obj_render_object(Object* object, Rect* rect, int light)
{
    int type = FID_TYPE(object->fid);
    if (art_get_disable(type)) {
        return;
    }

    CacheEntry* cacheEntry;
    Art* art = art_ptr_lock(object->fid, &cacheEntry);
    if (art == NULL) {
        return;
    }

    int frameWidth = art_frame_width(art, object->frame, object->rotation);
    int frameHeight = art_frame_length(art, object->frame, object->rotation);

    Rect objectRect;
    if (object->tile == -1) {
        objectRect.ulx = object->sx;
        objectRect.uly = object->sy;
        objectRect.lrx = object->sx + frameWidth - 1;
        objectRect.lry = object->sy + frameHeight - 1;
    } else {
        int objectScreenX;
        int objectScreenY;
        tile_coord(object->tile, &objectScreenX, &objectScreenY, object->elevation);
        objectScreenX += 16;
        objectScreenY += 8;

        objectScreenX += art->xOffsets[object->rotation];
        objectScreenY += art->yOffsets[object->rotation];

        objectScreenX += object->x;
        objectScreenY += object->y;

        objectRect.ulx = objectScreenX - frameWidth / 2;
        objectRect.uly = objectScreenY - (frameHeight - 1);
        objectRect.lrx = objectRect.ulx + frameWidth - 1;
        objectRect.lry = objectScreenY;

        object->sx = objectRect.ulx;
        object->sy = objectRect.uly;
    }

    if (rect_inside_bound(&objectRect, rect, &objectRect) != 0) {
        art_ptr_unlock(cacheEntry);
        return;
    }

    unsigned char* src = art_frame_data(art, object->frame, object->rotation);
    unsigned char* src2 = src;
    int v50 = objectRect.ulx - object->sx;
    int v49 = objectRect.uly - object->sy;
    src += frameWidth * v49 + v50;
    int objectWidth = objectRect.lrx - objectRect.ulx + 1;
    int objectHeight = objectRect.lry - objectRect.uly + 1;

    if (type == 6) {
        trans_buf_to_buf(src,
            objectWidth,
            objectHeight,
            frameWidth,
            back_buf + buf_full * objectRect.uly + objectRect.ulx,
            buf_full);
        art_ptr_unlock(cacheEntry);
        return;
    }

    if (type == 2 || type == 3) {
        if ((obj_dude->flags & OBJECT_HIDDEN) == 0 && (object->flags & OBJECT_FLAG_0xFC000) == 0) {
            Proto* proto;
            proto_ptr(object->pid, &proto);

            bool v17;
            int extendedFlags = proto->critter.extendedFlags;
            if ((extendedFlags & 0x8000000) != 0 || (extendedFlags & 0x80000000) != 0) {
                // TODO: Probably wrong.
                v17 = tile_in_front_of(object->tile, obj_dude->tile);
                if (!v17
                    || !tile_to_right_of(object->tile, obj_dude->tile)
                    || (object->flags & OBJECT_WALL_TRANS_END) == 0) {
                    // nothing
                } else {
                    v17 = false;
                }
            } else if ((extendedFlags & 0x10000000) != 0) {
                // NOTE: Uses bitwise OR, so both functions are evaluated.
                v17 = tile_in_front_of(object->tile, obj_dude->tile)
                    || tile_to_right_of(obj_dude->tile, object->tile);
            } else if ((extendedFlags & 0x20000000) != 0) {
                v17 = tile_in_front_of(object->tile, obj_dude->tile)
                    && tile_to_right_of(obj_dude->tile, object->tile);
            } else {
                v17 = tile_to_right_of(obj_dude->tile, object->tile);
                if (v17
                    && tile_in_front_of(obj_dude->tile, object->tile)
                    && (object->flags & OBJECT_WALL_TRANS_END) != 0) {
                    v17 = 0;
                }
            }

            if (v17) {
                CacheEntry* eggHandle;
                Art* egg = art_ptr_lock(obj_egg->fid, &eggHandle);
                if (egg == NULL) {
                    return;
                }

                int eggWidth;
                int eggHeight;
                art_frame_width_length(egg, 0, 0, &eggWidth, &eggHeight);

                int eggScreenX;
                int eggScreenY;
                tile_coord(obj_egg->tile, &eggScreenX, &eggScreenY, obj_egg->elevation);
                eggScreenX += 16;
                eggScreenY += 8;

                eggScreenX += egg->xOffsets[0];
                eggScreenY += egg->yOffsets[0];

                eggScreenX += obj_egg->x;
                eggScreenY += obj_egg->y;

                Rect eggRect;
                eggRect.ulx = eggScreenX - eggWidth / 2;
                eggRect.uly = eggScreenY - (eggHeight - 1);
                eggRect.lrx = eggRect.ulx + eggWidth - 1;
                eggRect.lry = eggScreenY;

                obj_egg->sx = eggRect.ulx;
                obj_egg->sy = eggRect.uly;

                Rect updatedEggRect;
                if (rect_inside_bound(&eggRect, &objectRect, &updatedEggRect) == 0) {
                    Rect rects[4];

                    rects[0].ulx = objectRect.ulx;
                    rects[0].uly = objectRect.uly;
                    rects[0].lrx = objectRect.lrx;
                    rects[0].lry = updatedEggRect.uly - 1;

                    rects[1].ulx = objectRect.ulx;
                    rects[1].uly = updatedEggRect.uly;
                    rects[1].lrx = updatedEggRect.ulx - 1;
                    rects[1].lry = updatedEggRect.lry;

                    rects[2].ulx = updatedEggRect.lrx + 1;
                    rects[2].uly = updatedEggRect.uly;
                    rects[2].lrx = objectRect.lrx;
                    rects[2].lry = updatedEggRect.lry;

                    rects[3].ulx = objectRect.ulx;
                    rects[3].uly = updatedEggRect.lry + 1;
                    rects[3].lrx = objectRect.lrx;
                    rects[3].lry = objectRect.lry;

                    for (int i = 0; i < 4; i++) {
                        Rect* v21 = &(rects[i]);
                        if (v21->ulx <= v21->lrx && v21->uly <= v21->lry) {
                            unsigned char* sp = src + frameWidth * (v21->uly - objectRect.uly) + (v21->ulx - objectRect.ulx);
                            dark_trans_buf_to_buf(sp, v21->lrx - v21->ulx + 1, v21->lry - v21->uly + 1, frameWidth, back_buf, v21->ulx, v21->uly, buf_full, light);
                        }
                    }

                    unsigned char* mask = art_frame_data(egg, 0, 0);
                    intensity_mask_buf_to_buf(
                        src + frameWidth * (updatedEggRect.uly - objectRect.uly) + (updatedEggRect.ulx - objectRect.ulx),
                        updatedEggRect.lrx - updatedEggRect.ulx + 1,
                        updatedEggRect.lry - updatedEggRect.uly + 1,
                        frameWidth,
                        back_buf + buf_full * updatedEggRect.uly + updatedEggRect.ulx,
                        buf_full,
                        mask + eggWidth * (updatedEggRect.uly - eggRect.uly) + (updatedEggRect.ulx - eggRect.ulx),
                        eggWidth,
                        light);
                    art_ptr_unlock(eggHandle);
                    art_ptr_unlock(cacheEntry);
                    return;
                }

                art_ptr_unlock(eggHandle);
            }
        }
    }

    switch (object->flags & OBJECT_FLAG_0xFC000) {
    case OBJECT_TRANS_RED:
        dark_translucent_trans_buf_to_buf(src, objectWidth, objectHeight, frameWidth, back_buf, objectRect.ulx, objectRect.uly, buf_full, light, redBlendTable, commonGrayTable);
        break;
    case OBJECT_TRANS_WALL:
        dark_translucent_trans_buf_to_buf(src, objectWidth, objectHeight, frameWidth, back_buf, objectRect.ulx, objectRect.uly, buf_full, 0x10000, wallBlendTable, commonGrayTable);
        break;
    case OBJECT_TRANS_GLASS:
        dark_translucent_trans_buf_to_buf(src, objectWidth, objectHeight, frameWidth, back_buf, objectRect.ulx, objectRect.uly, buf_full, light, glassBlendTable, glassGrayTable);
        break;
    case OBJECT_TRANS_STEAM:
        dark_translucent_trans_buf_to_buf(src, objectWidth, objectHeight, frameWidth, back_buf, objectRect.ulx, objectRect.uly, buf_full, light, steamBlendTable, commonGrayTable);
        break;
    case OBJECT_TRANS_ENERGY:
        dark_translucent_trans_buf_to_buf(src, objectWidth, objectHeight, frameWidth, back_buf, objectRect.ulx, objectRect.uly, buf_full, light, energyBlendTable, commonGrayTable);
        break;
    default:
        dark_trans_buf_to_buf(src, objectWidth, objectHeight, frameWidth, back_buf, objectRect.ulx, objectRect.uly, buf_full, light);
        break;
    }

    art_ptr_unlock(cacheEntry);
}

// Updates fid according to current violence level.
//
// 0x4810EC
void obj_fix_violence_settings(int* fid)
{
    if (FID_TYPE(*fid) != OBJ_TYPE_CRITTER) {
        return;
    }

    bool shouldResetViolenceLevel = false;
    if (fix_violence_level == -1) {
        if (!config_get_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_VIOLENCE_LEVEL_KEY, &fix_violence_level)) {
            fix_violence_level = VIOLENCE_LEVEL_MAXIMUM_BLOOD;
        }
        shouldResetViolenceLevel = true;
    }

    int start;
    int end;

    switch (fix_violence_level) {
    case VIOLENCE_LEVEL_NONE:
        start = ANIM_BIG_HOLE_SF;
        end = ANIM_FALL_FRONT_BLOOD_SF;
        break;
    case VIOLENCE_LEVEL_MINIMAL:
        start = ANIM_BIG_HOLE_SF;
        end = ANIM_FIRE_DANCE_SF;
        break;
    case VIOLENCE_LEVEL_NORMAL:
        start = ANIM_BIG_HOLE_SF;
        end = ANIM_SLICED_IN_HALF_SF;
        break;
    default:
        // Do not replace anything.
        start = ANIM_COUNT + 1;
        end = ANIM_COUNT + 1;
        break;
    }

    int anim = FID_ANIM_TYPE(*fid);
    if (anim >= start && anim <= end) {
        anim = (anim == ANIM_FALL_BACK_BLOOD_SF)
            ? ANIM_FALL_BACK_SF
            : ANIM_FALL_FRONT_SF;
        *fid = art_id(OBJ_TYPE_CRITTER, *fid & 0xFFF, anim, (*fid & 0xF000) >> 12, (*fid & 0x70000000) >> 28);
    }

    if (shouldResetViolenceLevel) {
        fix_violence_level = -1;
    }
}

// 0x4811E0
static int obj_preload_sort(const void* a1, const void* a2)
{
    // 0x505D0C
    static int cd_order[9] = {
        1,
        0,
        3,
        5,
        4,
        2,
        0,
        0,
        0,
    };

    int v1 = *(int*)a1;
    int v2 = *(int*)a2;

    int v3 = cd_order[FID_TYPE(v1)];
    int v4 = cd_order[FID_TYPE(v2)];

    int cmp = v3 - v4;
    if (cmp != 0) {
        return cmp;
    }

    cmp = (v1 & 0xFFF) - (v2 & 0xFFF);
    if (cmp != 0) {
        return cmp;
    }

    cmp = ((v1 & 0xF000) >> 12) - (((v2 & 0xF000) >> 12));
    if (cmp != 0) {
        return cmp;
    }

    cmp = ((v1 & 0xFF0000) >> 16) - (((v2 & 0xFF0000) >> 16));
    return cmp;
}

} // namespace fallout
