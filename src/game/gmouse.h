#ifndef FALLOUT_GAME_GMOUSE_H_
#define FALLOUT_GAME_GMOUSE_H_

#include "game/object_types.h"

namespace fallout {

typedef enum GameMouseMode {
    GAME_MOUSE_MODE_MOVE,
    GAME_MOUSE_MODE_ARROW,
    GAME_MOUSE_MODE_CROSSHAIR,
    GAME_MOUSE_MODE_USE_CROSSHAIR,
    GAME_MOUSE_MODE_USE_FIRST_AID,
    GAME_MOUSE_MODE_USE_DOCTOR,
    GAME_MOUSE_MODE_USE_LOCKPICK,
    GAME_MOUSE_MODE_USE_STEAL,
    GAME_MOUSE_MODE_USE_TRAPS,
    GAME_MOUSE_MODE_USE_SCIENCE,
    GAME_MOUSE_MODE_USE_REPAIR,
    GAME_MOUSE_MODE_COUNT,
    FIRST_GAME_MOUSE_MODE_SKILL = GAME_MOUSE_MODE_USE_FIRST_AID,
    GAME_MOUSE_MODE_SKILL_COUNT = GAME_MOUSE_MODE_COUNT - FIRST_GAME_MOUSE_MODE_SKILL,
} GameMouseMode;

typedef enum GameMouseActionMenuItem {
    GAME_MOUSE_ACTION_MENU_ITEM_CANCEL = 0,
    GAME_MOUSE_ACTION_MENU_ITEM_DROP = 1,
    GAME_MOUSE_ACTION_MENU_ITEM_INVENTORY = 2,
    GAME_MOUSE_ACTION_MENU_ITEM_LOOK = 3,
    GAME_MOUSE_ACTION_MENU_ITEM_ROTATE = 4,
    GAME_MOUSE_ACTION_MENU_ITEM_TALK = 5,
    GAME_MOUSE_ACTION_MENU_ITEM_USE = 6,
    GAME_MOUSE_ACTION_MENU_ITEM_UNLOAD = 7,
    GAME_MOUSE_ACTION_MENU_ITEM_USE_SKILL = 8,
    GAME_MOUSE_ACTION_MENU_ITEM_COUNT,
} GameMouseActionMenuItem;

typedef enum MouseCursorType {
    MOUSE_CURSOR_NONE,
    MOUSE_CURSOR_ARROW,
    MOUSE_CURSOR_SMALL_ARROW_UP,
    MOUSE_CURSOR_SMALL_ARROW_DOWN,
    MOUSE_CURSOR_SCROLL_NW,
    MOUSE_CURSOR_SCROLL_N,
    MOUSE_CURSOR_SCROLL_NE,
    MOUSE_CURSOR_SCROLL_E,
    MOUSE_CURSOR_SCROLL_SE,
    MOUSE_CURSOR_SCROLL_S,
    MOUSE_CURSOR_SCROLL_SW,
    MOUSE_CURSOR_SCROLL_W,
    MOUSE_CURSOR_SCROLL_NW_INVALID,
    MOUSE_CURSOR_SCROLL_N_INVALID,
    MOUSE_CURSOR_SCROLL_NE_INVALID,
    MOUSE_CURSOR_SCROLL_E_INVALID,
    MOUSE_CURSOR_SCROLL_SE_INVALID,
    MOUSE_CURSOR_SCROLL_S_INVALID,
    MOUSE_CURSOR_SCROLL_SW_INVALID,
    MOUSE_CURSOR_SCROLL_W_INVALID,
    MOUSE_CURSOR_CROSSHAIR,
    MOUSE_CURSOR_PLUS,
    MOUSE_CURSOR_DESTROY,
    MOUSE_CURSOR_USE_CROSSHAIR,
    MOUSE_CURSOR_WATCH,
    MOUSE_CURSOR_WAIT_PLANET,
    MOUSE_CURSOR_WAIT_WATCH,
    MOUSE_CURSOR_TYPE_COUNT,
    FIRST_GAME_MOUSE_ANIMATED_CURSOR = MOUSE_CURSOR_WAIT_PLANET,
} MouseCursorType;

extern bool gmouse_clicked_on_edge;

extern Object* obj_mouse;
extern Object* obj_mouse_flat;

int gmouse_init();
int gmouse_reset();
void gmouse_exit();
void gmouse_enable();
void gmouse_disable(int a1);
int gmouse_is_enabled();
void gmouse_enable_scrolling();
void gmouse_disable_scrolling();
int gmouse_scrolling_is_enabled();
void gmouse_set_click_to_scroll(int a1);
int gmouse_get_click_to_scroll();
int gmouse_is_scrolling();
void gmouse_bk_process();
void gmouse_handle_event(int mouseX, int mouseY, int mouseState);
int gmouse_set_cursor(int cursor);
int gmouse_get_cursor();
void gmouse_set_mapper_mode(int mode);
void gmouse_3d_enable_modes();
void gmouse_3d_disable_modes();
int gmouse_3d_modes_are_enabled();
void gmouse_3d_set_mode(int a1);
int gmouse_3d_get_mode();
void gmouse_3d_toggle_mode();
void gmouse_3d_refresh();
int gmouse_3d_set_fid(int fid);
int gmouse_3d_get_fid();
void gmouse_3d_reset_fid();
void gmouse_3d_on();
void gmouse_3d_off();
bool gmouse_3d_is_on();
Object* object_under_mouse(int objectType, bool a2, int elevation);
int gmouse_3d_build_pick_frame(int x, int y, int menuItem, int width, int height);
int gmouse_3d_pick_frame_hot(int* a1, int* a2);
int gmouse_3d_build_menu_frame(int x, int y, const int* menuItems, int menuItemsCount, int width, int height);
int gmouse_3d_menu_frame_hot(int* x, int* y);
int gmouse_3d_highlight_menu_frame(int menuItemIndex);
int gmouse_3d_build_to_hit_frame(const char* string, int color);
int gmouse_3d_build_hex_frame(const char* string, int color);
void gmouse_3d_synch_item_highlight();
void gmouse_remove_item_outline(Object* object);

void gameMouseRefreshImmediately();

} // namespace fallout

#endif /* FALLOUT_GAME_GMOUSE_H_ */
