#ifndef FALLOUT_GAME_MAP_DEFS_H_
#define FALLOUT_GAME_MAP_DEFS_H_

namespace fallout {

#define ELEVATION_COUNT (3)

#define SQUARE_GRID_WIDTH (100)
#define SQUARE_GRID_HEIGHT (100)
#define SQUARE_GRID_SIZE (SQUARE_GRID_WIDTH * SQUARE_GRID_HEIGHT)

#define HEX_GRID_WIDTH (200)
#define HEX_GRID_HEIGHT (200)
#define HEX_GRID_SIZE (HEX_GRID_WIDTH * HEX_GRID_HEIGHT)

static inline bool elevationIsValid(int elevation)
{
    return elevation >= 0 && elevation < ELEVATION_COUNT;
}

static inline bool squareGridTileIsValid(int tile)
{
    return tile >= 0 && tile < SQUARE_GRID_SIZE;
}

static inline bool hexGridTileIsValid(int tile)
{
    return tile >= 0 && tile < HEX_GRID_SIZE;
}

} // namespace fallout

#endif /* FALLOUT_GAME_MAP_DEFS_H_ */
