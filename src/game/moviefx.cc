#include "game/moviefx.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game/config.h"
#include "game/palette.h"
#include "int/movie.h"
#include "platform_compat.h"
#include "plib/gnw/debug.h"
#include "plib/gnw/memory.h"

namespace fallout {

typedef enum MovieEffectType {
    MOVIE_EFFECT_TYPE_NONE = 0,
    MOVIE_EFFECT_TYPE_FADE_IN = 1,
    MOVIE_EFFECT_TYPE_FADE_OUT = 2,
} MovieEffectFadeType;

typedef struct MovieEffect {
    int startFrame;
    int endFrame;
    int steps;
    unsigned char fadeType;
    // range 0-63
    unsigned char r;
    // range 0-63
    unsigned char g;
    // range 0-63
    unsigned char b;
    struct MovieEffect* next;
} MovieEffect;

static void moviefx_callback_func(int frame);
static void moviefx_palette_func(unsigned char* palette, int start, int end);
static void moviefx_add(MovieEffect* movie_effect);
static void moviefx_remove_all();

// 0x505B68
static bool moviefx_initialized = false;

// 0x505B6C
static MovieEffect* moviefx_effects_list = NULL;

// 0x637424
static unsigned char source_palette[768];

// 0x637724
static bool inside_fade;

// 0x479AD0
int moviefx_init()
{
    if (moviefx_initialized) {
        return -1;
    }

    memset(source_palette, 0, sizeof(source_palette));

    moviefx_initialized = true;

    return 0;
}

// 0x479B04
void moviefx_reset()
{
    if (!moviefx_initialized) {
        return;
    }

    movieSetCallback(NULL);
    movieSetPaletteFunc(NULL);
    moviefx_remove_all();

    inside_fade = false;

    memset(source_palette, 0, sizeof(source_palette));
}

// 0x479B40
void moviefx_exit()
{
    if (!moviefx_initialized) {
        return;
    }

    movieSetCallback(NULL);
    movieSetPaletteFunc(NULL);
    moviefx_remove_all();

    inside_fade = false;

    memset(source_palette, 0, sizeof(source_palette));
}

// 0x479B8C
int moviefx_start(const char* filePath)
{
    if (!moviefx_initialized) {
        return -1;
    }

    movieSetCallback(NULL);
    movieSetPaletteFunc(NULL);
    moviefx_remove_all();
    inside_fade = false;
    memset(source_palette, 0, sizeof(source_palette));

    if (filePath == NULL) {
        return -1;
    }

    Config config;
    if (!config_init(&config)) {
        return -1;
    }

    int rc = -1;

    char path[COMPAT_MAX_PATH];
    strcpy(path, filePath);

    char* pch = strrchr(path, '.');
    if (pch != NULL) {
        *pch = '\0';
    }

    strcpy(path + strlen(path), ".cfg");

    int* movieEffectFrameList;

    if (!config_load(&config, path, true)) {
        goto out;
    }

    int movieEffectsLength;
    if (!config_get_value(&config, "info", "total_effects", &movieEffectsLength)) {
        goto out;
    }

    movieEffectFrameList = (int*)mem_malloc(sizeof(*movieEffectFrameList) * movieEffectsLength);
    if (movieEffectFrameList == NULL) {
        goto out;
    }

    bool frameListRead;
    if (movieEffectsLength >= 2) {
        frameListRead = config_get_values(&config, "info", "effect_frames", movieEffectFrameList, movieEffectsLength);
    } else {
        frameListRead = config_get_value(&config, "info", "effect_frames", &(movieEffectFrameList[0]));
    }

    if (frameListRead) {
        int movieEffectsCreated = 0;
        for (int index = 0; index < movieEffectsLength; index++) {
            char section[20];
            compat_itoa(movieEffectFrameList[index], section, 10);

            char* fadeTypeString;
            if (!config_get_string(&config, section, "fade_type", &fadeTypeString)) {
                continue;
            }

            int fadeType = MOVIE_EFFECT_TYPE_NONE;
            if (compat_stricmp(fadeTypeString, "in") == 0) {
                fadeType = MOVIE_EFFECT_TYPE_FADE_IN;
            } else if (compat_stricmp(fadeTypeString, "out") == 0) {
                fadeType = MOVIE_EFFECT_TYPE_FADE_OUT;
            }

            if (fadeType == MOVIE_EFFECT_TYPE_NONE) {
                continue;
            }

            int fadeColor[3];
            if (!config_get_values(&config, section, "fade_color", fadeColor, 3)) {
                continue;
            }

            int steps;
            if (!config_get_value(&config, section, "fade_steps", &steps)) {
                continue;
            }

            MovieEffect* movieEffect = (MovieEffect*)mem_malloc(sizeof(*movieEffect));
            if (movieEffect == NULL) {
                continue;
            }

            memset(movieEffect, 0, sizeof(*movieEffect));
            movieEffect->startFrame = movieEffectFrameList[index];
            movieEffect->endFrame = movieEffect->startFrame + steps - 1;
            movieEffect->steps = steps;
            movieEffect->fadeType = fadeType & 0xFF;
            movieEffect->r = fadeColor[0] & 0xFF;
            movieEffect->g = fadeColor[1] & 0xFF;
            movieEffect->b = fadeColor[2] & 0xFF;

            if (movieEffect->startFrame <= 1) {
                inside_fade = true;
            }

            // NOTE: Uninline.
            moviefx_add(movieEffect);

            movieEffectsCreated++;
        }

        if (movieEffectsCreated != 0) {
            movieSetCallback(moviefx_callback_func);
            movieSetPaletteFunc(moviefx_palette_func);
            rc = 0;
        }
    }

    mem_free(movieEffectFrameList);

out:

    config_exit(&config);

    return rc;
}

// 0x479F00
void moviefx_stop()
{
    if (!moviefx_initialized) {
        return;
    }

    movieSetCallback(NULL);
    movieSetPaletteFunc(NULL);

    moviefx_remove_all();

    inside_fade = false;
    memset(source_palette, 0, sizeof(source_palette));
}

// 0x479F54
static void moviefx_callback_func(int frame)
{
    MovieEffect* movieEffect = moviefx_effects_list;
    while (movieEffect != NULL) {
        if (frame >= movieEffect->startFrame && frame <= movieEffect->endFrame) {
            break;
        }
        movieEffect = movieEffect->next;
    }

    if (movieEffect != NULL) {
        unsigned char palette[768];
        int step = frame - movieEffect->startFrame + 1;

        if (movieEffect->fadeType == MOVIE_EFFECT_TYPE_FADE_IN) {
            for (int index = 0; index < 256; index++) {
                palette[index * 3] = movieEffect->r - (step * (movieEffect->r - source_palette[index * 3]) / movieEffect->steps);
                palette[index * 3 + 1] = movieEffect->g - (step * (movieEffect->g - source_palette[index * 3 + 1]) / movieEffect->steps);
                palette[index * 3 + 2] = movieEffect->b - (step * (movieEffect->b - source_palette[index * 3 + 2]) / movieEffect->steps);
            }
        } else {
            for (int index = 0; index < 256; index++) {
                palette[index * 3] = source_palette[index * 3] - (step * (source_palette[index * 3] - movieEffect->r) / movieEffect->steps);
                palette[index * 3 + 1] = source_palette[index * 3 + 1] - (step * (source_palette[index * 3 + 1] - movieEffect->g) / movieEffect->steps);
                palette[index * 3 + 2] = source_palette[index * 3 + 2] - (step * (source_palette[index * 3 + 2] - movieEffect->b) / movieEffect->steps);
            }
        }

        palette_set_to(palette);
    }

    inside_fade = movieEffect != NULL;
}

// 0x47A0BC
static void moviefx_palette_func(unsigned char* palette, int start, int end)
{
    memcpy(source_palette + 3 * start, palette, 3 * (end - start + 1));

    if (!inside_fade) {
        palette_set_entries(palette, start, end);
    }
}

// 0x47A10C
static void moviefx_add(MovieEffect* movie_effect)
{
    movie_effect->next = moviefx_effects_list;
    moviefx_effects_list = movie_effect;
}

// 0x47A120
static void moviefx_remove_all()
{
    MovieEffect* movieEffect = moviefx_effects_list;
    while (movieEffect != NULL) {
        MovieEffect* next = movieEffect->next;
        mem_free(movieEffect);
        movieEffect = next;
    }

    moviefx_effects_list = NULL;
}

} // namespace fallout
