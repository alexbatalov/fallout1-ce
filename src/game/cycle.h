#ifndef FALLOUT_GAME_CYCLE_H_
#define FALLOUT_GAME_CYCLE_H_

namespace fallout {

extern unsigned char slime[12];
extern unsigned char shoreline[18];
extern unsigned char fire_slow[15];
extern unsigned char fire_fast[15];
extern unsigned char monitors[15];

void cycle_init();
void cycle_reset();
void cycle_exit();
void cycle_disable();
void cycle_enable();
bool cycle_is_enabled();
void change_cycle_speed(int value);
int get_cycle_speed();

} // namespace fallout

#endif /* FALLOUT_GAME_CYCLE_H_ */
