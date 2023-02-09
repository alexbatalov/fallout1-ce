#ifndef FALLOUT_GAME_GMOVIE_H_
#define FALLOUT_GAME_GMOVIE_H_

#include "plib/db/db.h"

namespace fallout {

typedef enum GameMovieFlags {
    GAME_MOVIE_FADE_IN = 0x01,
    GAME_MOVIE_FADE_OUT = 0x02,
    GAME_MOVIE_STOP_MUSIC = 0x04,
    GAME_MOVIE_PAUSE_MUSIC = 0x08,
} GameMovieFlags;

typedef enum GameMovie {
    MOVIE_IPLOGO,
    MOVIE_MPLOGO,
    MOVIE_INTRO,
    MOVIE_VEXPLD,
    MOVIE_CATHEXP,
    MOVIE_OVRINTRO,
    MOVIE_BOIL3,
    MOVIE_OVRRUN,
    MOVIE_WALKM,
    MOVIE_WALKW,
    MOVIE_DIPEDV,
    MOVIE_BOIL1,
    MOVIE_BOIL2,
    MOVIE_RAEKILLS,
    MOVIE_COUNT,
} GameMovie;

int gmovie_init();
void gmovie_reset();
void gmovie_exit();
int gmovie_load(DB_FILE* stream);
int gmovie_save(DB_FILE* stream);
int gmovie_play(int game_movie, int game_movie_flags);
bool gmovie_has_been_played(int game_movie);

} // namespace fallout

#endif /* FALLOUT_GAME_GMOVIE_H_ */
