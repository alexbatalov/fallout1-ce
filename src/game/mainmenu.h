#ifndef FALLOUT_GAME_MAINMENU_H_
#define FALLOUT_GAME_MAINMENU_H_

namespace fallout {

typedef enum MainMenuOption {
    MAIN_MENU_INTRO,
    MAIN_MENU_NEW_GAME,
    MAIN_MENU_LOAD_GAME,
    MAIN_MENU_SCREENSAVER,
    MAIN_MENU_TIMEOUT,
    MAIN_MENU_CREDITS,
    MAIN_MENU_QUOTES,
    MAIN_MENU_EXIT,
    MAIN_MENU_SELFRUN,
    MAIN_MENU_OPTIONS,
} MainMenuOption;

extern bool in_main_menu;

int main_menu_create();
void main_menu_destroy();
void main_menu_hide(bool animate);
void main_menu_show(bool animate);
int main_menu_is_shown();
int main_menu_is_enabled();
void main_menu_set_timeout(unsigned int timeout);
unsigned int main_menu_get_timeout();
int main_menu_loop();

} // namespace fallout

#endif /* FALLOUT_GAME_MAINMENU_H_ */
