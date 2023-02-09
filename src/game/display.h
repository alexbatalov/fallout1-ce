#ifndef FALLOUT_GAME_DISPLAY_H_
#define FALLOUT_GAME_DISPLAY_H_

namespace fallout {

int display_init();
int display_reset();
void display_exit();
void display_print(char* string);
void display_clear();
void display_redraw();
void display_scroll_up(int btn, int keyCode);
void display_scroll_down(int btn, int keyCode);
void display_arrow_up(int btn, int keyCode);
void display_arrow_down(int btn, int keyCode);
void display_arrow_restore(int btn, int keyCode);
void display_disable();
void display_enable();

} // namespace fallout

#endif /* FALLOUT_GAME_DISPLAY_H_ */
