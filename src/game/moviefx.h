#ifndef FALLOUT_GAME_MOVIEFX_H_
#define FALLOUT_GAME_MOVIEFX_H_

namespace fallout {

int moviefx_init();
void moviefx_reset();
void moviefx_exit();
int moviefx_start(const char* fileName);
void moviefx_stop();

} // namespace fallout

#endif /* FALLOUT_GAME_MOVIEFX_H_ */
