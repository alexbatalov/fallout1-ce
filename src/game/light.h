#ifndef FALLOUT_GAME_LIGHT_H_
#define FALLOUT_GAME_LIGHT_H_

namespace fallout {

#define LIGHT_LEVEL_MAX 65536
#define LIGHT_LEVEL_MIN (LIGHT_LEVEL_MAX / 4)

// 10% of max light per "Night Vision" rank
#define LIGHT_LEVEL_NIGHT_VISION_BONUS (LIGHT_LEVEL_MAX / 10)

typedef void(AdjustLightIntensityProc)(int elevation, int tile, int intensity);

int light_init();
void light_reset();
void light_exit();
int light_get_ambient();
void light_set_ambient(int new_ambient_light, bool refresh_screen);
void light_increase_ambient(int value, bool refresh_screen);
void light_decrease_ambient(int value, bool refresh_screen);
int light_get_tile(int elevation, int tile);
int light_get_tile_true(int elevation, int tile);
void light_set_tile(int elevation, int tile, int intensity);
void light_add_to_tile(int elevation, int tile, int intensity);
void light_subtract_from_tile(int elevation, int tile, int intensity);
void light_reset_tiles();

} // namespace fallout

#endif /* FALLOUT_GAME_LIGHT_H_ */
