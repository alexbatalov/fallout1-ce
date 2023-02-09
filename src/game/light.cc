#include "game/light.h"

#include "game/map_defs.h"
#include "game/object.h"
#include "game/perk.h"
#include "game/tile.h"

namespace fallout {

// 0x5057E0
static int ambient_light = LIGHT_LEVEL_MAX;

// 0x59CF1C
static int tile_intensity[ELEVATION_COUNT][HEX_GRID_SIZE];

// 0x46CA70
int light_init()
{
    light_reset_tiles();
    return 0;
}

// 0x46CA70
void light_reset()
{
    light_reset_tiles();
}

// 0x46CA70
void light_exit()
{
    light_reset_tiles();
}

// 0x46CA78
int light_get_ambient()
{
    return ambient_light;
}

// 0x46CA88
void light_set_ambient(int new_ambient_light, bool refresh_screen)
{
    int normalized;
    int old_ambient_light;

    normalized = new_ambient_light + perk_level(PERK_NIGHT_VISION) * LIGHT_LEVEL_NIGHT_VISION_BONUS;

    if (normalized < LIGHT_LEVEL_MIN) {
        normalized = LIGHT_LEVEL_MIN;
    }

    if (normalized > LIGHT_LEVEL_MAX) {
        normalized = LIGHT_LEVEL_MAX;
    }

    old_ambient_light = ambient_light;
    ambient_light = normalized;

    if (refresh_screen) {
        if (old_ambient_light != normalized) {
            tile_refresh_display();
        }
    }
}

// 0x46CA80
void light_increase_ambient(int value, bool refresh_screen)
{
    light_set_ambient(ambient_light + value, refresh_screen);
}

// 0x46CAD4
void light_decrease_ambient(int value, bool refresh_screen)
{
    light_set_ambient(ambient_light - value, refresh_screen);
}

// 0x46CAE8
int light_get_tile(int elevation, int tile)
{
    int intensity;

    if (!elevationIsValid(elevation)) {
        return 0;
    }

    if (!hexGridTileIsValid(tile)) {
        return 0;
    }

    intensity = tile_intensity[elevation][tile];
    if (intensity >= LIGHT_LEVEL_MAX) {
        intensity = LIGHT_LEVEL_MAX;
    }

    return intensity;
}

// 0x46CB2C
int light_get_tile_true(int elevation, int tile)
{
    if (!elevationIsValid(elevation)) {
        return 0;
    }

    if (!hexGridTileIsValid(tile)) {
        return 0;
    }

    return tile_intensity[elevation][tile];
}

// 0x46CB54
void light_set_tile(int elevation, int tile, int lightIntensity)
{
    if (!elevationIsValid(elevation)) {
        return;
    }

    if (!hexGridTileIsValid(tile)) {
        return;
    }

    tile_intensity[elevation][tile] = lightIntensity;
}

// 0x46CB78
void light_add_to_tile(int elevation, int tile, int lightIntensity)
{
    if (!elevationIsValid(elevation)) {
        return;
    }

    if (!hexGridTileIsValid(tile)) {
        return;
    }

    tile_intensity[elevation][tile] += lightIntensity;
}

// 0x46CBB0
void light_subtract_from_tile(int elevation, int tile, int lightIntensity)
{
    if (!elevationIsValid(elevation)) {
        return;
    }

    if (!hexGridTileIsValid(tile)) {
        return;
    }

    tile_intensity[elevation][tile] -= lightIntensity;
}

// 0x46CBEC
void light_reset_tiles()
{
    int elevation;
    int tile;

    for (elevation = 0; elevation < ELEVATION_COUNT; elevation++) {
        for (tile = 0; tile < HEX_GRID_SIZE; tile++) {
            tile_intensity[elevation][tile] = 655;
        }
    }
}

} // namespace fallout
