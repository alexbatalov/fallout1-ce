#ifndef FALLOUT_GAME_INVENTRY_H_
#define FALLOUT_GAME_INVENTRY_H_

#include "game/art.h"
#include "game/object_types.h"

namespace fallout {

// TODO: Convert to enum.
#define OFF_59E7BC_COUNT 12

typedef enum InventoryWindowCursor {
    INVENTORY_WINDOW_CURSOR_HAND,
    INVENTORY_WINDOW_CURSOR_ARROW,
    INVENTORY_WINDOW_CURSOR_PICK,
    INVENTORY_WINDOW_CURSOR_MENU,
    INVENTORY_WINDOW_CURSOR_BLANK,
    INVENTORY_WINDOW_CURSOR_COUNT,
} InventoryWindowCursor;

typedef enum InventoryWindowType {
    // Normal inventory window with quick character sheet.
    INVENTORY_WINDOW_TYPE_NORMAL,

    // Narrow inventory window with just an item scroller that's shown when
    // a "Use item on" is selected from context menu.
    INVENTORY_WINDOW_TYPE_USE_ITEM_ON,

    // Looting/strealing interface.
    INVENTORY_WINDOW_TYPE_LOOT,

    // Barter interface.
    INVENTORY_WINDOW_TYPE_TRADE,

    // Supplementary "Move items" window. Used to set quantity of items when
    // moving items between inventories.
    INVENTORY_WINDOW_TYPE_MOVE_ITEMS,

    // Supplementary "Set timer" window. Internally it's implemented as "Move
    // items" window but with timer overlay and slightly different adjustment
    // mechanics.
    INVENTORY_WINDOW_TYPE_SET_TIMER,

    INVENTORY_WINDOW_TYPE_COUNT,
} InventoryWindowType;

extern CacheEntry* ikey[OFF_59E7BC_COUNT];

void inven_set_dude(Object* obj, int pid);
void inven_reset_dude();
void handle_inventory();
bool setup_inventory(int inventoryWindowType);
void exit_inventory(bool a1);
void display_inventory(int first_item_index, int selected_index, int inventoryWindowType, int selected_quantity = 1);
void display_target_inventory(int first_item_index, int selected_index, Inventory* inventory, int inventoryWindowType, int selected_quantity = 1);
void display_body(int fid, int inventoryWindowType);
int inven_init();
void inven_exit();
void inven_set_mouse(int cursor);
void inven_hover_on(int btn, int keyCode);
void inven_hover_off(int btn, int keyCode);
void inven_pickup(int keyCode, int a2);
void switch_hand(Object* a1, Object** a2, Object** a3, int a4);
void adjust_ac(Object* critter, Object* oldArmor, Object* newArmor);
void adjust_fid();
void use_inventory_on(Object* a1);
Object* inven_right_hand(Object* obj);
Object* inven_left_hand(Object* obj);
Object* inven_worn(Object* obj);
int inven_pid_is_carried(Object* obj, int pid);
Object* inven_pid_is_carried_ptr(Object* obj, int pid);
int inven_pid_quantity_carried(Object* obj, int pid);
void display_stats();
Object* inven_find_type(Object* obj, int a2, int* inout_a3);
Object* inven_find_id(Object* obj, int a2);
Object* inven_index_ptr(Object* obj, int a2);
int inven_wield(Object* critter, Object* item, int a3);
int inven_unwield(Object* critter, int a2);
int inven_from_button(int input, Object** a2, Object*** a3, Object** a4);
void inven_display_msg(char* string);
void inven_obj_examine_func(Object* critter, Object* item);
void inven_action_cursor(int eventCode, int inventoryWindowType);
int loot_container(Object* a1, Object* a2);
int inven_steal_container(Object* a1, Object* a2);
int move_inventory(InventoryItem* inventoryItem, int itemIndex, Object* target, bool isPlanting, bool skipDrag, bool moveAll);
void barter_inventory(int win, Object* target, Object* peon_table, Object* barterer_table, int mod);
void container_enter(int a1, int a2);
void container_exit(int keyCode, int inventoryWindowType);
int drop_into_container(Object* a1, Object* a2, int a3, Object** a4, int quantity);
int drop_ammo_into_weapon(Object* weapon, Object* ammo, Object** a3, int quantity, int keyCode);
void draw_amount(int value, int inventoryWindowType);
int inven_set_timer(Object* a1);

} // namespace fallout

#endif /* FALLOUT_GAME_INVENTRY_H_ */
