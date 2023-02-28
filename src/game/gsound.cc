#include "game/gsound.h"

#include <stdio.h>
#include <string.h>

#include "game/anim.h"
#include "game/combat.h"
#include "game/gconfig.h"
#include "game/item.h"
#include "game/map.h"
#include "game/object.h"
#include "game/proto.h"
#include "game/queue.h"
#include "game/roll.h"
#include "game/sfxcache.h"
#include "game/stat.h"
#include "game/worldmap.h"
#include "int/audio.h"
#include "int/audiof.h"
#include "int/movie.h"
#include "platform_compat.h"
#include "pointer_registry.h"
#include "plib/db/db.h"
#include "plib/gnw/debug.h"
#include "plib/gnw/gnw.h"
#include "plib/gnw/input.h"
#include "plib/gnw/memory.h"

namespace fallout {

static void gsound_bkg_proc();
static int gsound_open(const char* fname, int flags);
static long gsound_compressed_tell(int handle);
static int gsound_write(int handle, const void* buf, unsigned int size);
static int gsound_close(int handle);
static int gsound_read(int handle, void* buf, unsigned int size);
static long gsound_seek(int handle, long offset, int origin);
static long gsound_tell(int handle);
static long gsound_filesize(int handle);
static bool gsound_compressed_query(char* filePath);
static void gsound_internal_speech_callback(void* userData, int a2);
static void gsound_internal_background_callback(void* userData, int a2);
static void gsound_internal_effect_callback(void* userData, int a2);
static int gsound_background_allocate(Sound** out_s, int a2, int a3);
static int gsound_background_find_with_copy(char* dest, const char* src);
static int gsound_background_find_dont_copy(char* dest, const char* src);
static int gsound_speech_find_dont_copy(char* dest, const char* src);
static void gsound_background_remove_last_copy();
static int gsound_background_start();
static int gsound_speech_start();
static int gsound_get_music_path(char** out_value, const char* key);
static void gsound_check_active_effects();
static Sound* gsound_get_sound_ready_for_effect();
static bool gsound_file_exists_f(const char* fname);
static int gsound_file_exists_db(const char* path);
static int gsound_setup_paths();

// TODO: Remove.
// 0x4F2C54
char _aSoundSfx[] = "sound\\sfx\\";

// TODO: Remove.
// 0x4F2C60
char _aSoundMusic_0[] = "sound\\music\\";

// TODO: Remove.
// 0x4F2C70
char _aSoundSpeech_0[] = "sound\\speech\\";

// 0x505434
static bool gsound_initialized = false;

// 0x505438
static bool gsound_debug = false;

// 0x50543C
static bool gsound_background_enabled = false;

// 0x505440
static int gsound_background_df_vol = 0;

// 0x505444
static int gsound_background_fade = 0;

// 0x505448
static bool gsound_speech_enabled = false;

// 0x50544C
static bool gsound_sfx_enabled = false;

// Number of active effects (max 4).
//
// 0x505450
static int gsound_active_effect_counter;

// 0x505454
static Sound* gsound_background_tag = NULL;

// 0x505458
static Sound* gsound_speech_tag = NULL;

// 0x50545C
static SoundEndCallback* gsound_background_callback_fp = NULL;

// 0x505460
static SoundEndCallback* gsound_speech_callback_fp = NULL;

// 0x505464
static char snd_lookup_weapon_type[WEAPON_SOUND_EFFECT_COUNT] = {
    'R', // Ready
    'A', // Attack
    'O', // Out of ammo
    'F', // Firing
    'H', // Hit
};

// 0x505469
static char snd_lookup_scenery_action[SCENERY_SOUND_EFFECT_COUNT] = {
    'O', // Open
    'C', // Close
    'L', // Lock
    'N', // Unlock
    'U', // Use
};

// 0x505470
static int background_storage_requested = -1;

// 0x505474
static int background_loop_requested = -1;

// 0x505478
static char* sound_sfx_path = _aSoundSfx;

// 0x50547C
static char* sound_music_path1 = _aSoundMusic_0;

// 0x505480
static char* sound_music_path2 = _aSoundMusic_0;

// 0x505484
static char* sound_speech_path = _aSoundSpeech_0;

// 0x505488
static int master_volume = VOLUME_MAX;

// 0x50548C
static int background_volume = VOLUME_MAX;

// 0x505490
static int speech_volume = VOLUME_MAX;

// 0x505494
static int sndfx_volume = VOLUME_MAX;

// 0x505498
static int detectDevices = -1;

// 0x595450
static char background_fname_copied[COMPAT_MAX_PATH];

// 0x595555
static char sfx_file_name[13];

// 0x595562
static char background_fname_requested[COMPAT_MAX_PATH];

// 0x4475A0
int gsound_init()
{
    if (gsound_initialized) {
        if (gsound_debug) {
            debug_printf("Trying to initialize gsound twice.\n");
        }
        return -1;
    }

    bool initialize;
    configGetBool(&game_config, GAME_CONFIG_SOUND_KEY, GAME_CONFIG_INITIALIZE_KEY, &initialize);
    if (!initialize) {
        return 0;
    }

    configGetBool(&game_config, GAME_CONFIG_SOUND_KEY, GAME_CONFIG_DEBUG_KEY, &gsound_debug);

    if (gsound_debug) {
        debug_printf("Initializing sound system...");
    }

    if (gsound_get_music_path(&sound_music_path1, GAME_CONFIG_MUSIC_PATH1_KEY) != 0) {
        return -1;
    }

    if (gsound_get_music_path(&sound_music_path2, GAME_CONFIG_MUSIC_PATH2_KEY) != 0) {
        return -1;
    }

    if (strlen(sound_music_path1) > 247 || strlen(sound_music_path2) > 247) {
        if (gsound_debug) {
            debug_printf("Music paths way too long.\n");
        }
        return -1;
    }

    // gsound_setup_paths
    if (gsound_setup_paths() != 0) {
        return -1;
    }

    soundRegisterAlloc(mem_malloc, mem_realloc, mem_free);

    // initialize direct sound
    if (soundInit(detectDevices, 24, 0x8000, 0x8000, 22050) != 0) {
        if (gsound_debug) {
            debug_printf("failed!\n");
        }

        return -1;
    }

    if (gsound_debug) {
        debug_printf("success.\n");
    }

    initAudiof(gsound_compressed_query);
    initAudio(gsound_compressed_query);

    int cacheSize;
    config_get_value(&game_config, GAME_CONFIG_SOUND_KEY, GAME_CONFIG_CACHE_SIZE_KEY, &cacheSize);
    if (cacheSize >= 0x40000) {
        debug_printf("\n!!! Config file needs adustment.  Please remove the ");
        debug_printf("cache_size line and run fallout again.  This will reset ");
        debug_printf("cache_size to the new default, which is expressed in K.\n");
        return -1;
    }

    if (sfxc_init(cacheSize << 10, sound_sfx_path) != 0) {
        if (gsound_debug) {
            debug_printf("Unable to initialize sound effects cache.\n");
        }
    }

    if (soundSetDefaultFileIO(gsound_open, gsound_close, gsound_read, gsound_write, gsound_seek, gsound_tell, gsound_filesize) != 0) {
        if (gsound_debug) {
            debug_printf("Failure setting sound I/O calls.\n");
        }
        return -1;
    }

    add_bk_process(gsound_bkg_proc);
    gsound_initialized = true;

    // SOUNDS
    bool sounds = 0;
    configGetBool(&game_config, GAME_CONFIG_SOUND_KEY, GAME_CONFIG_SOUNDS_KEY, &sounds);

    if (gsound_debug) {
        debug_printf("Sounds are ");
    }

    if (sounds) {
        // NOTE: Uninline.
        gsound_sfx_enable();
    } else {
        if (gsound_debug) {
            debug_printf(" not ");
        }
    }

    if (gsound_debug) {
        debug_printf("on.\n");
    }

    // MUSIC
    bool music = 0;
    configGetBool(&game_config, GAME_CONFIG_SOUND_KEY, GAME_CONFIG_MUSIC_KEY, &music);

    if (gsound_debug) {
        debug_printf("Music is ");
    }

    if (music) {
        // NOTE: Uninline.
        gsound_background_enable();
    } else {
        if (gsound_debug) {
            debug_printf(" not ");
        }
    }

    if (gsound_debug) {
        debug_printf("on.\n");
    }

    // SPEEECH
    bool speech = 0;
    configGetBool(&game_config, GAME_CONFIG_SOUND_KEY, GAME_CONFIG_SPEECH_KEY, &speech);

    if (gsound_debug) {
        debug_printf("Speech is ");
    }

    if (speech) {
        // NOTE: Uninline.
        gsound_speech_enable();
    } else {
        if (gsound_debug) {
            debug_printf(" not ");
        }
    }

    if (gsound_debug) {
        debug_printf("on.\n");
    }

    config_get_value(&game_config, GAME_CONFIG_SOUND_KEY, GAME_CONFIG_MASTER_VOLUME_KEY, &master_volume);
    gsound_set_master_volume(master_volume);

    config_get_value(&game_config, GAME_CONFIG_SOUND_KEY, GAME_CONFIG_MUSIC_VOLUME_KEY, &background_volume);
    gsound_background_volume_set(background_volume);

    config_get_value(&game_config, GAME_CONFIG_SOUND_KEY, GAME_CONFIG_SNDFX_VOLUME_KEY, &sndfx_volume);
    gsound_set_sfx_volume(sndfx_volume);

    config_get_value(&game_config, GAME_CONFIG_SOUND_KEY, GAME_CONFIG_SPEECH_VOLUME_KEY, &speech_volume);
    gsound_speech_volume_set(speech_volume);

    // NOTE: Uninline.
    gsound_background_fade_set(0);
    background_fname_requested[0] = '\0';

    return 0;
}

// 0x447A94
void gsound_reset()
{
    if (!gsound_initialized) {
        return;
    }

    if (gsound_debug) {
        debug_printf("Resetting sound system...");
    }

    // NOTE: Uninline.
    gsound_speech_stop();

    if (gsound_background_df_vol) {
        // NOTE: Uninline.
        gsound_background_enable();
    }

    gsound_background_stop();

    // NOTE: Uninline.
    gsound_background_fade_set(0);

    soundFlushAllSounds();

    sfxc_flush();

    gsound_active_effect_counter = 0;

    if (gsound_debug) {
        debug_printf("done.\n");
    }

    return;
}

// 0x447B74
int gsound_exit()
{
    if (!gsound_initialized) {
        return -1;
    }

    remove_bk_process(gsound_bkg_proc);

    // NOTE: Uninline.
    gsound_speech_stop();

    gsound_background_stop();
    gsound_background_remove_last_copy();
    soundClose();
    sfxc_exit();
    audiofClose();
    audioClose();

    gsound_initialized = false;

    return 0;
}

// 0x447BEC
void gsound_sfx_enable()
{
    if (gsound_initialized) {
        gsound_sfx_enabled = true;
    }
}

// 0x447C00
void gsound_sfx_disable()
{
    if (gsound_initialized) {
        gsound_sfx_enabled = false;
    }
}

// 0x447C14
int gsound_sfx_is_enabled()
{
    return gsound_sfx_enabled;
}

// 0x447C1C
int gsound_set_master_volume(int volume)
{
    if (!gsound_initialized) {
        return -1;
    }

    if (volume < VOLUME_MIN && volume > VOLUME_MAX) {
        if (gsound_debug) {
            debug_printf("Requested master volume out of range.\n");
        }
        return -1;
    }

    if (gsound_background_df_vol && volume != 0 && gsound_background_volume_get() != 0) {
        // NOTE: Uninline.
        gsound_background_enable();
        gsound_background_df_vol = 0;
    }

    if (soundSetMasterVolume(volume) != 0) {
        if (gsound_debug) {
            debug_printf("Error setting master sound volume.\n");
        }
        return -1;
    }

    master_volume = volume;
    if (gsound_background_enabled && volume == 0) {
        // NOTE: Uninline.
        gsound_background_disable();
        gsound_background_df_vol = 1;
    }

    return 0;
}

// 0x447D40
int gsound_get_master_volume()
{
    return master_volume;
}

// 0x447D48
int gsound_set_sfx_volume(int volume)
{
    if (!gsound_initialized || volume < VOLUME_MIN || volume > VOLUME_MAX) {
        if (gsound_debug) {
            debug_printf("Error setting sfx volume.\n");
        }
        return -1;
    }

    sndfx_volume = volume;

    return 0;
}

// 0x447D84
int gsound_get_sfx_volume()
{
    return sndfx_volume;
}

// 0x447D8C
void gsound_background_disable()
{
    if (gsound_initialized) {
        if (gsound_background_enabled) {
            gsound_background_stop();
            movieSetVolume(0);
            gsound_background_enabled = false;
        }
    }
}

// 0x447DB8
void gsound_background_enable()
{
    if (gsound_initialized) {
        if (!gsound_background_enabled) {
            movieSetVolume((int)(background_volume * 0.94));
            gsound_background_enabled = true;
            gsound_background_restart_last(12);
        }
    }
}

// 0x447E04
int gsound_background_is_enabled()
{
    return gsound_background_enabled;
}

// 0x447E0C
void gsound_background_volume_set(int volume)
{
    if (!gsound_initialized) {
        return;
    }

    if (volume < VOLUME_MIN || volume > VOLUME_MAX) {
        if (gsound_debug) {
            debug_printf("Requested background volume out of range.\n");
        }
        return;
    }

    background_volume = volume;

    if (gsound_background_df_vol) {
        // NOTE: Uninline.
        gsound_background_enable();
        gsound_background_df_vol = 0;
    }

    if (gsound_background_enabled) {
        movieSetVolume((int)(volume * 0.94));
    }

    if (gsound_background_enabled) {
        if (gsound_background_tag != NULL) {
            soundVolume(gsound_background_tag, (int)(background_volume * 0.94));
        }
    }

    if (gsound_background_enabled) {
        if (volume == 0 || gsound_get_master_volume() == 0) {
            // NOTE: Uninline.
            gsound_background_disable();
            gsound_background_df_vol = 1;
        }
    }
}

// 0x447F48
int gsound_background_volume_get()
{
    return background_volume;
}

// 0x447F50
int gsound_background_volume_get_set(int volume)
{
    int oldMusicVolume;

    // NOTE: Uninline.
    oldMusicVolume = gsound_background_volume_get();
    gsound_background_volume_set(volume);

    return oldMusicVolume;
}

// 0x447F60
void gsound_background_fade_set(int value)
{
    gsound_background_fade = value;
}

// 0x447F68
int gsound_background_fade_get()
{
    return gsound_background_fade;
}

// 0x447F70
int gsound_background_fade_get_set(int value)
{
    int oldValue;

    // NOTE: Uninline.
    oldValue = gsound_background_fade_get();

    // NOTE: Uninline.
    gsound_background_fade_set(value);

    return oldValue;
}

// 0x447F80
void gsound_background_callback_set(SoundEndCallback* callback)
{
    gsound_background_callback_fp = callback;
}

// 0x447F88
SoundEndCallback* gsound_background_callback_get()
{
    return gsound_background_callback_fp;
}

// 0x447F90
SoundEndCallback* gsound_background_callback_get_set(SoundEndCallback* callback)
{
    SoundEndCallback* oldCallback;

    // NOTE: Uninline.
    oldCallback = gsound_background_callback_get();

    // NOTE: Uninline.
    gsound_background_callback_set(callback);

    return oldCallback;
}

// 0x447FA0
int gsound_background_length_get()
{
    return soundLength(gsound_background_tag);
}

// [fileName] is base file name, without path and extension.
//
// 0x447FAC
int gsound_background_play(const char* fileName, int a2, int a3, int a4)
{
    int rc;

    background_storage_requested = a3;
    background_loop_requested = a4;

    strcpy(background_fname_requested, fileName);

    if (!gsound_initialized) {
        return -1;
    }

    if (!gsound_background_enabled) {
        return -1;
    }

    if (gsound_debug) {
        debug_printf("Loading background sound file %s%s...", fileName, ".acm");
    }

    gsound_background_stop();

    rc = gsound_background_allocate(&gsound_background_tag, a3, a4);
    if (rc != 0) {
        if (gsound_debug) {
            debug_printf("failed because sound could not be allocated.\n");
        }

        gsound_background_tag = NULL;
        return -1;
    }

    rc = soundSetFileIO(gsound_background_tag, audiofOpen, audiofCloseFile, audiofRead, NULL, audiofSeek, gsound_compressed_tell, audiofFileSize);
    if (rc != 0) {
        if (gsound_debug) {
            debug_printf("failed because file IO could not be set for compression.\n");
        }

        soundDelete(gsound_background_tag);
        gsound_background_tag = NULL;

        return -1;
    }

    rc = soundSetChannel(gsound_background_tag, 3);
    if (rc != 0) {
        if (gsound_debug) {
            debug_printf("failed because the channel could not be set.\n");
        }

        soundDelete(gsound_background_tag);
        gsound_background_tag = NULL;

        return -1;
    }

    char path[COMPAT_MAX_PATH + 1];
    if (a3 == 13) {
        rc = gsound_background_find_dont_copy(path, fileName);
    } else if (a3 == 14) {
        rc = gsound_background_find_with_copy(path, fileName);
    }

    if (rc != SOUND_NO_ERROR) {
        if (gsound_debug) {
            debug_printf("'failed because the file could not be found.\n");
        }

        soundDelete(gsound_background_tag);
        gsound_background_tag = NULL;

        return -1;
    }

    if (a4 == 16) {
        rc = soundLoop(gsound_background_tag, 0xFFFF);
        if (rc != SOUND_NO_ERROR) {
            if (gsound_debug) {
                debug_printf("failed because looping could not be set.\n");
            }

            soundDelete(gsound_background_tag);
            gsound_background_tag = NULL;

            return -1;
        }
    }

    rc = soundSetCallback(gsound_background_tag, gsound_internal_background_callback, NULL);
    if (rc != SOUND_NO_ERROR) {
        if (gsound_debug) {
            debug_printf("soundSetCallback failed for background sound\n");
        }
    }

    if (a2 == 11) {
        rc = soundSetReadLimit(gsound_background_tag, 0x40000);
        if (rc != SOUND_NO_ERROR) {
            if (gsound_debug) {
                debug_printf("unable to set read limit ");
            }
        }
    }

    rc = soundLoad(gsound_background_tag, path);
    if (rc != SOUND_NO_ERROR) {
        if (gsound_debug) {
            debug_printf("failed on call to soundLoad.\n");
        }

        soundDelete(gsound_background_tag);
        gsound_background_tag = NULL;

        return -1;
    }

    if (a2 != 11) {
        rc = soundSetReadLimit(gsound_background_tag, 0x40000);
        if (rc != 0) {
            if (gsound_debug) {
                debug_printf("unable to set read limit ");
            }
        }
    }

    if (a2 == 10) {
        return 0;
    }

    rc = gsound_background_start();
    if (rc != 0) {
        if (gsound_debug) {
            debug_printf("failed starting to play.\n");
        }

        soundDelete(gsound_background_tag);
        gsound_background_tag = NULL;

        return -1;
    }

    if (gsound_debug) {
        debug_printf("succeeded.\n");
    }

    return 0;
}

// 0x448338
int gsound_background_play_level_music(const char* a1, int a2)
{
    return gsound_background_play(a1, a2, 14, 16);
}

// 0x44834C
int gsound_background_play_preloaded()
{
    if (!gsound_initialized) {
        return -1;
    }

    if (!gsound_background_enabled) {
        return -1;
    }

    if (gsound_background_tag == NULL) {
        return -1;
    }

    if (soundPlaying(gsound_background_tag)) {
        return -1;
    }

    if (soundPaused(gsound_background_tag)) {
        return -1;
    }

    if (soundDone(gsound_background_tag)) {
        return -1;
    }

    if (gsound_background_start() != 0) {
        soundDelete(gsound_background_tag);
        gsound_background_tag = NULL;
        return -1;
    }

    return 0;
}

// 0x4483E4
void gsound_background_stop()
{
    if (gsound_initialized && gsound_background_enabled && gsound_background_tag) {
        if (gsound_background_fade) {
            if (soundFade(gsound_background_tag, 2000, 0) == 0) {
                gsound_background_tag = NULL;
                return;
            }
        }

        soundDelete(gsound_background_tag);
        gsound_background_tag = NULL;
    }
}

// 0x44843C
void gsound_background_restart_last(int value)
{
    if (background_fname_requested[0] != '\0') {
        if (gsound_background_play(background_fname_requested, value, background_storage_requested, background_loop_requested) != 0) {
            if (gsound_debug)
                debug_printf(" background restart failed ");
        }
    }
}

// 0x448480
void gsound_background_pause()
{
    if (gsound_background_tag != NULL) {
        soundPause(gsound_background_tag);
    }
}

// 0x448494
void gsound_background_unpause()
{
    if (gsound_background_tag != NULL) {
        soundUnpause(gsound_background_tag);
    }
}

// 0x4484A8
void gsound_speech_disable()
{
    if (gsound_initialized) {
        if (gsound_speech_enabled) {
            gsound_speech_stop();
            gsound_speech_enabled = false;
        }
    }
}

// 0x4484F0
void gsound_speech_enable()
{
    if (gsound_initialized) {
        if (!gsound_speech_enabled) {
            gsound_speech_enabled = true;
        }
    }
}

// 0x448510
int gsound_speech_is_enabled()
{
    return gsound_speech_enabled;
}

// 0x448518
void gsound_speech_volume_set(int volume)
{
    if (!gsound_initialized) {
        return;
    }

    if (volume < VOLUME_MIN || volume > VOLUME_MAX) {
        if (gsound_debug) {
            debug_printf("Requested speech volume out of range.\n");
        }
        return;
    }

    speech_volume = volume;

    if (gsound_speech_enabled) {
        if (gsound_speech_tag != NULL) {
            soundVolume(gsound_speech_tag, (int)(volume * 0.69));
        }
    }
}

// 0x44858C
int gsound_speech_volume_get()
{
    return speech_volume;
}

// 0x448594
int gsound_speech_volume_get_set(int volume)
{
    int oldVolume = speech_volume;
    gsound_speech_volume_set(volume);
    return oldVolume;
}

// 0x4485A4
void gsound_speech_callback_set(SoundEndCallback* callback)
{
    gsound_speech_callback_fp = callback;
}

// 0x4485AC
SoundEndCallback* gsound_speech_callback_get()
{
    return gsound_speech_callback_fp;
}

// 0x4485B4
SoundEndCallback* gsound_speech_callback_get_set(SoundEndCallback* callback)
{
    SoundEndCallback* oldCallback;

    // NOTE: Uninline.
    oldCallback = gsound_speech_callback_get();

    // NOTE: Uninline.
    gsound_speech_callback_set(callback);

    return oldCallback;
}

// 0x4485C4
int gsound_speech_length_get()
{
    return soundLength(gsound_speech_tag);
}

// 0x4485D0
int gsound_speech_play(const char* fname, int a2, int a3, int a4)
{
    char path[COMPAT_MAX_PATH + 1];

    if (!gsound_initialized) {
        return -1;
    }

    if (!gsound_speech_enabled) {
        return -1;
    }

    if (gsound_debug) {
        debug_printf("Loading speech sound file %s%s...", fname, ".ACM");
    }

    // uninline
    gsound_speech_stop();

    if (gsound_background_allocate(&gsound_speech_tag, a3, a4)) {
        if (gsound_debug) {
            debug_printf("failed because sound could not be allocated.\n");
        }
        gsound_speech_tag = NULL;
        return -1;
    }

    if (soundSetFileIO(gsound_speech_tag, audioOpen, audioCloseFile, audioRead, NULL, audioSeek, gsound_compressed_tell, audioFileSize)) {
        if (gsound_debug) {
            debug_printf("failed because file IO could not be set for compression.\n");
        }
        soundDelete(gsound_speech_tag);
        gsound_speech_tag = NULL;
        return -1;
    }

    if (gsound_speech_find_dont_copy(path, fname)) {
        if (gsound_debug) {
            debug_printf("failed because the file could not be found.\n");
        }
        soundDelete(gsound_speech_tag);
        gsound_speech_tag = NULL;
        return -1;
    }

    if (a4 == 16) {
        if (soundLoop(gsound_speech_tag, 0xFFFF)) {
            if (gsound_debug) {
                debug_printf("failed because looping could not be set.\n");
            }
            soundDelete(gsound_speech_tag);
            gsound_speech_tag = NULL;
            return -1;
        }
    }

    if (soundSetCallback(gsound_speech_tag, gsound_internal_speech_callback, NULL)) {
        if (gsound_debug) {
            debug_printf("soundSetCallback failed for speech sound\n");
        }
    }

    if (a2 == 11) {
        if (soundSetReadLimit(gsound_speech_tag, 0x40000)) {
            if (gsound_debug) {
                debug_printf("unable to set read limit ");
            }
        }
    }

    if (soundLoad(gsound_speech_tag, path)) {
        if (gsound_debug) {
            debug_printf("failed on call to soundLoad.\n");
        }
        soundDelete(gsound_speech_tag);
        gsound_speech_tag = NULL;
        return -1;
    }

    if (a2 != 11) {
        if (soundSetReadLimit(gsound_speech_tag, 0x40000)) {
            if (gsound_debug) {
                debug_printf("unable to set read limit ");
            }
        }
    }

    if (a2 == 10) {
        return 0;
    }

    if (gsound_speech_start()) {
        if (gsound_debug) {
            debug_printf("failed starting to play.\n");
        }
        soundDelete(gsound_speech_tag);
        gsound_speech_tag = NULL;
        return -1;
    }

    if (gsound_debug) {
        debug_printf("succeeded.\n");
    }

    return 0;
}

// 0x4488BC
int gsound_speech_play_preloaded()
{
    if (!gsound_initialized) {
        return -1;
    }

    if (!gsound_speech_enabled) {
        return -1;
    }

    if (gsound_speech_tag == NULL) {
        return -1;
    }

    if (soundPlaying(gsound_speech_tag)) {
        return -1;
    }

    if (soundPaused(gsound_speech_tag)) {
        return -1;
    }

    if (soundDone(gsound_speech_tag)) {
        return -1;
    }

    if (gsound_speech_start() != 0) {
        soundDelete(gsound_speech_tag);
        gsound_speech_tag = NULL;

        return -1;
    }

    return 0;
}

// 0x448954
void gsound_speech_stop()
{
    if (gsound_initialized && gsound_speech_enabled) {
        if (gsound_speech_tag != NULL) {
            soundDelete(gsound_speech_tag);
            gsound_speech_tag = NULL;
        }
    }
}

// 0x448984
void gsound_speech_pause()
{
    if (gsound_speech_tag != NULL) {
        soundPause(gsound_speech_tag);
    }
}

// 0x448998
void gsound_speech_unpause()
{
    if (gsound_speech_tag != NULL) {
        soundUnpause(gsound_speech_tag);
    }
}

// 0x4489BC
int gsound_play_sfx_file_volume(const char* a1, int a2)
{
    Sound* v1;

    if (!gsound_initialized) {
        return -1;
    }

    if (!gsound_sfx_enabled) {
        return -1;
    }

    v1 = gsound_load_sound_volume(a1, NULL, a2);
    if (v1 == NULL) {
        return -1;
    }

    soundPlay(v1);

    return 0;
}

// 0x448A0C
Sound* gsound_load_sound(const char* name, Object* object)
{
    if (!gsound_initialized) {
        return NULL;
    }

    if (!gsound_sfx_enabled) {
        return NULL;
    }

    if (gsound_debug) {
        debug_printf("Loading sound file %s%s...", name, ".ACM");
    }

    if (gsound_active_effect_counter >= SOUND_EFFECTS_MAX_COUNT) {
        if (gsound_debug) {
            debug_printf("failed because there are already %d active effects.\n", gsound_active_effect_counter);
        }

        return NULL;
    }

    Sound* sound = gsound_get_sound_ready_for_effect();
    if (sound == NULL) {
        if (gsound_debug) {
            debug_printf("failed.\n");
        }

        return NULL;
    }

    ++gsound_active_effect_counter;

    char path[COMPAT_MAX_PATH];
    snprintf(path, sizeof(path), "%s%s%s", sound_sfx_path, name, ".ACM");

    if (soundLoad(sound, path) == 0) {
        if (gsound_debug) {
            debug_printf("succeeded.\n");
        }

        return sound;
    }

    if (object != NULL) {
        if (FID_TYPE(object->fid) == OBJ_TYPE_CRITTER && (name[0] == 'H' || name[0] == 'N')) {
            char v9 = name[1];
            if (v9 == 'A' || v9 == 'F' || v9 == 'M') {
                if (v9 == 'A') {
                    if (stat_level(object, STAT_GENDER)) {
                        v9 = 'F';
                    } else {
                        v9 = 'M';
                    }
                }
            }

            snprintf(path, sizeof(path), "%sH%cXXXX%s%s", sound_sfx_path, v9, name + 6, ".ACM");

            if (gsound_debug) {
                debug_printf("tyring %s ", path + strlen(sound_sfx_path));
            }

            if (soundLoad(sound, path) == 0) {
                if (gsound_debug) {
                    debug_printf("succeeded (with alias).\n");
                }

                return sound;
            }

            if (v9 == 'F') {
                snprintf(path, sizeof(path), "%sHMXXXX%s%s", sound_sfx_path, name + 6, ".ACM");

                if (gsound_debug) {
                    debug_printf("tyring %s ", path + strlen(sound_sfx_path));
                }

                if (soundLoad(sound, path) == 0) {
                    if (gsound_debug) {
                        debug_printf("succeeded (with male alias).\n");
                    }

                    return sound;
                }
            }
        }
    }

    if (strncmp(name, "MALIEU", 6) == 0 || strncmp(name, "MAMTN2", 6) == 0) {
        snprintf(path, sizeof(path), "%sMAMTNT%s%s", sound_sfx_path, name + 6, ".ACM");

        if (gsound_debug) {
            debug_printf("tyring %s ", path + strlen(sound_sfx_path));
        }

        if (soundLoad(sound, path) == 0) {
            if (gsound_debug) {
                debug_printf("succeeded (with alias).\n");
            }

            return sound;
        }
    }

    --gsound_active_effect_counter;

    soundDelete(sound);

    if (gsound_debug) {
        debug_printf("failed.\n");
    }

    return NULL;
}

// 0x448D8C
Sound* gsound_load_sound_volume(const char* name, Object* object, int volume)
{
    Sound* sound = gsound_load_sound(name, object);

    if (sound != NULL) {
        soundVolume(sound, (volume * sndfx_volume) / VOLUME_MAX);
    }

    return sound;
}

// 0x448DBC
void gsound_delete_sfx(Sound* sound)
{
    if (!gsound_initialized) {
        return;
    }

    if (!gsound_sfx_enabled) {
        return;
    }

    if (soundPlaying(sound)) {
        if (gsound_debug) {
            debug_printf("Trying to manually delete a sound effect after it has started playing.\n");
        }
        return;
    }

    if (soundDelete(sound) != 0) {
        if (gsound_debug) {
            debug_printf("Unable to delete sound effect -- active effect counter may get out of sync.\n");
        }
        return;
    }

    --gsound_active_effect_counter;
}

// 0x448E20
int gsnd_anim_sound(Sound* sound, void* a2)
{
    if (!gsound_initialized) {
        return 0;
    }

    if (!gsound_sfx_enabled) {
        return 0;
    }

    if (sound == NULL) {
        return 0;
    }

    soundPlay(sound);

    return 0;
}

// 0x448E40
int gsound_play_sound(Sound* sound)
{
    if (!gsound_initialized) {
        return -1;
    }

    if (!gsound_sfx_enabled) {
        return -1;
    }

    if (sound == NULL) {
        return -1;
    }

    soundPlay(sound);

    return 0;
}

// 0x448E64
int gsound_compute_relative_volume(Object* obj)
{
    int type;
    int v3;
    Object* v7;
    Rect v12;
    Rect v14;
    Rect iso_win_rect;
    int distance;
    int perception;

    v3 = 0x7FFF;

    if (obj) {
        type = FID_TYPE(obj->fid);
        if (type == 0 || type == 1 || type == 2) {
            v7 = obj_top_environment(obj);
            if (!v7) {
                v7 = obj;
            }

            obj_bound(v7, &v14);

            win_get_rect(display_win, &iso_win_rect);

            if (rect_inside_bound(&v14, &iso_win_rect, &v12) == -1) {
                distance = obj_dist(v7, obj_dude);
                perception = stat_level(obj_dude, STAT_PERCEPTION);
                if (distance > perception) {
                    if (distance < 2 * perception) {
                        v3 = 0x7FFF - 0x5554 * (distance - perception) / perception;
                    } else {
                        v3 = 0x2AAA;
                    }
                } else {
                    v3 = 0x7FFF;
                }
            }
        }
    }

    return v3;
}

// 0x448F34
char* gsnd_build_character_sfx_name(Object* a1, int anim, int extra)
{
    char v7[13];
    char v8;
    char v9;

    if (art_get_base_name(FID_TYPE(a1->fid), a1->fid & 0xFFF, v7) == -1) {
        return NULL;
    }

    if (anim == ANIM_TAKE_OUT) {
        if (art_get_code(anim, extra, &v8, &v9) == -1) {
            return NULL;
        }
    } else {
        if (art_get_code(anim, (a1->fid & 0xF000) >> 12, &v8, &v9) == -1) {
            return NULL;
        }
    }

    // TODO: Check.
    if (anim == ANIM_FALL_FRONT || anim == ANIM_FALL_BACK) {
        if (extra == CHARACTER_SOUND_EFFECT_PASS_OUT) {
            v8 = 'Y';
        } else if (extra == CHARACTER_SOUND_EFFECT_DIE) {
            v8 = 'Z';
        }
    } else if ((anim == ANIM_THROW_PUNCH || anim == ANIM_KICK_LEG) && extra == CHARACTER_SOUND_EFFECT_CONTACT) {
        v8 = 'Z';
    }

    snprintf(sfx_file_name, sizeof(sfx_file_name), "%s%c%c", v7, v8, v9);
    compat_strupr(sfx_file_name);
    return sfx_file_name;
}

// 0x449020
char* gsnd_build_ambient_sfx_name(const char* a1)
{
    snprintf(sfx_file_name, sizeof(sfx_file_name), "A%6s%1d", a1, 1);
    compat_strupr(sfx_file_name);
    return sfx_file_name;
}

// 0x449048
char* gsnd_build_interface_sfx_name(const char* a1)
{
    snprintf(sfx_file_name, sizeof(sfx_file_name), "N%6s%1d", a1, 1);
    compat_strupr(sfx_file_name);
    return sfx_file_name;
}

// 0x449090
char* gsnd_build_weapon_sfx_name(int effectType, Object* weapon, int hitMode, Object* target)
{
    int v6;
    char weaponSoundCode;
    char effectTypeCode;
    char materialCode;
    Proto* proto;
    int damage_type;

    weaponSoundCode = item_w_sound_id(weapon);
    effectTypeCode = snd_lookup_weapon_type[effectType];

    if (effectType != WEAPON_SOUND_EFFECT_READY
        && effectType != WEAPON_SOUND_EFFECT_OUT_OF_AMMO) {
        if (hitMode != HIT_MODE_LEFT_WEAPON_PRIMARY
            && hitMode != HIT_MODE_RIGHT_WEAPON_PRIMARY
            && hitMode != HIT_MODE_PUNCH) {
            v6 = 2;
        } else {
            v6 = 1;
        }
    } else {
        v6 = 1;
    }

    damage_type = item_w_damage_type(weapon);
    if (effectTypeCode != 'H' || target == NULL || damage_type == DAMAGE_TYPE_EXPLOSION || damage_type == DAMAGE_TYPE_PLASMA || damage_type == DAMAGE_TYPE_EMP) {
        materialCode = 'X';
    } else {
        const int type = FID_TYPE(target->fid);
        int material;
        switch (type) {
        case OBJ_TYPE_ITEM:
            proto_ptr(target->pid, &proto);
            material = proto->item.material;
            break;
        case OBJ_TYPE_SCENERY:
            proto_ptr(target->pid, &proto);
            material = proto->scenery.material;
            break;
        case OBJ_TYPE_WALL:
            proto_ptr(target->pid, &proto);
            material = proto->wall.material;
            break;
        default:
            material = -1;
            break;
        }

        switch (material) {
        case MATERIAL_TYPE_GLASS:
        case MATERIAL_TYPE_METAL:
        case MATERIAL_TYPE_PLASTIC:
            materialCode = 'M';
            break;
        case MATERIAL_TYPE_WOOD:
            materialCode = 'W';
            break;
        case MATERIAL_TYPE_DIRT:
        case MATERIAL_TYPE_STONE:
        case MATERIAL_TYPE_CEMENT:
            materialCode = 'S';
            break;
        default:
            materialCode = 'F';
            break;
        }
    }

    snprintf(sfx_file_name, sizeof(sfx_file_name), "W%c%c%1d%cXX%1d", effectTypeCode, weaponSoundCode, v6, materialCode, 1);
    compat_strupr(sfx_file_name);
    return sfx_file_name;
}

// 0x4491C4
char* gsnd_build_scenery_sfx_name(int actionType, int action, const char* name)
{
    char actionTypeCode = actionType == SOUND_EFFECT_ACTION_TYPE_PASSIVE ? 'P' : 'A';
    char actionCode = snd_lookup_scenery_action[action];

    snprintf(sfx_file_name, sizeof(sfx_file_name), "S%c%c%4s%1d", actionTypeCode, actionCode, name, 1);
    compat_strupr(sfx_file_name);

    return sfx_file_name;
}

// 0x449208
char* gsnd_build_open_sfx_name(Object* object, int action)
{
    if (FID_TYPE(object->fid) == OBJ_TYPE_SCENERY) {
        char scenerySoundId;
        Proto* proto;
        if (proto_ptr(object->pid, &proto) != -1) {
            scenerySoundId = proto->scenery.field_34;
        } else {
            scenerySoundId = 'A';
        }
        snprintf(sfx_file_name, sizeof(sfx_file_name), "S%cDOORS%c", snd_lookup_scenery_action[action], scenerySoundId);
    } else {
        Proto* proto;
        proto_ptr(object->pid, &proto);
        snprintf(sfx_file_name, sizeof(sfx_file_name), "I%cCNTNR%c", snd_lookup_scenery_action[action], proto->item.field_80);
    }
    compat_strupr(sfx_file_name);
    return sfx_file_name;
}

// 0x44929C
void gsound_red_butt_press(int btn, int keyCode)
{
    gsound_play_sfx_file("ib1p1xx1");
}

// 0x4492A4
void gsound_red_butt_release(int btn, int keyCode)
{
    gsound_play_sfx_file("ib1lu1x1");
}

// 0x4492AC
void gsound_toggle_butt_press(int btn, int keyCode)
{
    gsound_play_sfx_file("toggle");
}

// 0x4492AC
void gsound_toggle_butt_release(int btn, int keyCode)
{
    gsound_play_sfx_file("toggle");
}

// 0x4492B4
void gsound_med_butt_press(int btn, int keyCode)
{
    gsound_play_sfx_file("ib2p1xx1");
}

// 0x4492BC
void gsound_med_butt_release(int btn, int keyCode)
{
    gsound_play_sfx_file("ib2lu1x1");
}

// 0x4492C4
void gsound_lrg_butt_press(int btn, int keyCode)
{
    gsound_play_sfx_file("ib3p1xx1");
}

// 0x4492CC
void gsound_lrg_butt_release(int btn, int keyCode)
{
    gsound_play_sfx_file("ib3lu1x1");
}

// 0x4492D4
int gsound_play_sfx_file(const char* name)
{
    if (!gsound_initialized) {
        return -1;
    }

    if (!gsound_sfx_enabled) {
        return -1;
    }

    Sound* sound = gsound_load_sound(name, NULL);
    if (sound == NULL) {
        return -1;
    }

    soundPlay(sound);

    return 0;
}

// 0x44932C
static void gsound_bkg_proc()
{
    soundUpdate();
}

// 0x449334
static int gsound_open(const char* fname, int flags)
{
    if ((flags & 2) != 0) {
        return -1;
    }

    DB_FILE* stream = db_fopen(fname, "rb");
    if (stream == NULL) {
        return -1;
    }

    return ptrToInt(stream);
}

// 0x449348
static long gsound_compressed_tell(int fileHandle)
{
    return -1;
}

// 0x449348
static int gsound_write(int fileHandle, const void* buf, unsigned int size)
{
    return -1;
}

// 0x449350
static int gsound_close(int fileHandle)
{
    if (fileHandle == -1) {
        return -1;
    }

    return db_fclose((DB_FILE*)intToPtr(fileHandle));
}

// 0x44935C
static int gsound_read(int fileHandle, void* buffer, unsigned int size)
{
    if (fileHandle == -1) {
        return -1;
    }

    return db_fread(buffer, 1, size, (DB_FILE*)intToPtr(fileHandle));
}

// 0x449378
static long gsound_seek(int fileHandle, long offset, int origin)
{
    if (fileHandle == -1) {
        return -1;
    }

    if (db_fseek((DB_FILE*)intToPtr(fileHandle), offset, origin) != 0) {
        return -1;
    }

    return db_ftell((DB_FILE*)intToPtr(fileHandle));
}

// 0x44939C
static long gsound_tell(int handle)
{
    if (handle == -1) {
        return -1;
    }

    return db_ftell((DB_FILE*)intToPtr(handle));
}

// 0x4493A8
static long gsound_filesize(int handle)
{
    if (handle == -1) {
        return -1;
    }

    return db_filelength((DB_FILE*)intToPtr(handle));
}

// 0x4493B4
static bool gsound_compressed_query(char* filePath)
{
    return true;
}

// 0x4493BC
static void gsound_internal_speech_callback(void* userData, int a2)
{
    if (a2 == 1) {
        gsound_speech_tag = NULL;

        if (gsound_speech_callback_fp) {
            gsound_speech_callback_fp();
        }
    }
}

// 0x4493DC
static void gsound_internal_background_callback(void* userData, int a2)
{
    if (a2 == 1) {
        gsound_background_tag = NULL;

        if (gsound_background_callback_fp) {
            gsound_background_callback_fp();
        }
    }
}

// 0x4493FC
static void gsound_internal_effect_callback(void* userData, int a2)
{
    if (a2 == 1) {
        --gsound_active_effect_counter;
    }
}

// 0x449408
static int gsound_background_allocate(Sound** soundPtr, int a2, int a3)
{
    int v5 = 10;
    int v6 = 0;
    if (a2 == 13) {
        v6 |= 0x01;
    } else if (a2 == 14) {
        v6 |= 0x02;
    }

    if (a3 == 15) {
        v6 |= 0x04;
    } else if (a3 == 16) {
        v5 = 42;
    }

    Sound* sound = soundAllocate(v6, v5);
    if (sound == NULL) {
        return -1;
    }

    *soundPtr = sound;

    return 0;
}

// 0x44945C
static int gsound_background_find_with_copy(char* dest, const char* src)
{
    size_t len = strlen(src) + strlen(".ACM");
    if (strlen(sound_music_path1) + len > COMPAT_MAX_PATH || strlen(sound_music_path2) + len > COMPAT_MAX_PATH) {
        if (gsound_debug) {
            debug_printf("Full background path too long.\n");
        }

        return -1;
    }

    if (gsound_debug) {
        debug_printf(" finding background sound ");
    }

    char outPath[COMPAT_MAX_PATH];
    snprintf(outPath, sizeof(outPath), "%s%s%s", sound_music_path1, src, ".ACM");
    if (gsound_file_exists_f(outPath)) {
        strncpy(dest, outPath, COMPAT_MAX_PATH);
        dest[COMPAT_MAX_PATH] = '\0';
        return 0;
    }

    if (gsound_debug) {
        debug_printf("by copy ");
    }

    gsound_background_remove_last_copy();

    char inPath[COMPAT_MAX_PATH];
    snprintf(inPath, sizeof(inPath), "%s%s%s", sound_music_path2, src, ".ACM");

    FILE* inStream = compat_fopen(inPath, "rb");
    if (inStream == NULL) {
        if (gsound_debug) {
            debug_printf("Unable to find music file %s to copy down.\n", src);
        }

        return -1;
    }

    FILE* outStream = compat_fopen(outPath, "wb");
    if (outStream == NULL) {
        if (gsound_debug) {
            debug_printf("Unable to open music file %s for copying to.", src);
        }

        fclose(inStream);

        return -1;
    }

    void* buffer = mem_malloc(0x2000);
    if (buffer == NULL) {
        if (gsound_debug) {
            debug_printf("Out of memory in gsound_background_find_with_copy.\n", src);
        }

        fclose(outStream);
        fclose(inStream);

        return -1;
    }

    bool err = false;
    while (!feof(inStream)) {
        size_t bytesRead = fread(buffer, 1, 0x2000, inStream);
        if (bytesRead == 0) {
            break;
        }

        if (fwrite(buffer, 1, bytesRead, outStream) != bytesRead) {
            err = true;
            break;
        }
    }

    mem_free(buffer);
    fclose(outStream);
    fclose(inStream);

    if (err) {
        if (gsound_debug) {
            debug_printf("Background sound file copy failed on write -- ");
            debug_printf("likely out of disc space.\n");
        }

        return -1;
    }

    strcpy(background_fname_copied, src);

    strncpy(dest, outPath, COMPAT_MAX_PATH);
    dest[COMPAT_MAX_PATH] = '\0';

    return 0;
}

// 0x449758
static int gsound_background_find_dont_copy(char* dest, const char* src)
{
    char path[COMPAT_MAX_PATH];
    int len;

    len = strlen(src) + strlen(".ACM");
    if (strlen(sound_music_path1) + len > COMPAT_MAX_PATH || strlen(sound_music_path2) + len > COMPAT_MAX_PATH) {
        if (gsound_debug) {
            debug_printf("Full background path too long.\n");
        }

        return -1;
    }

    if (gsound_debug) {
        debug_printf(" finding background sound ");
    }

    snprintf(path, sizeof(path), "%s%s%s", sound_music_path1, src, ".ACM");
    if (gsound_file_exists_f(path)) {
        strncpy(dest, path, COMPAT_MAX_PATH);
        dest[COMPAT_MAX_PATH] = '\0';
        return 0;
    }

    if (gsound_debug) {
        debug_printf("in 2nd path ");
    }

    snprintf(path, sizeof(path), "%s%s%s", sound_music_path2, src, ".ACM");
    if (gsound_file_exists_f(path)) {
        strncpy(dest, path, COMPAT_MAX_PATH);
        dest[COMPAT_MAX_PATH] = '\0';
        return 0;
    }

    if (gsound_debug) {
        debug_printf("-- find failed ");
    }

    return -1;
}

// 0x4498C0
static int gsound_speech_find_dont_copy(char* dest, const char* src)
{
    char path[COMPAT_MAX_PATH];

    if (strlen(sound_speech_path) + strlen(".acm") > COMPAT_MAX_PATH) {
        if (gsound_debug) {
            // FIXME: The message is wrong (notes background path, but here
            // we're dealing with speech path).
            debug_printf("Full background path too long.\n");
        }

        return -1;
    }

    if (gsound_debug) {
        debug_printf(" finding speech sound ");
    }

    snprintf(path, sizeof(path), "%s%s%s", sound_speech_path, src, ".ACM");

    // NOTE: Uninline.
    if (!gsound_file_exists_db(path)) {
        if (gsound_debug) {
            debug_printf("-- find failed ");
        }

        return -1;
    }

    strncpy(dest, path, COMPAT_MAX_PATH);
    dest[COMPAT_MAX_PATH] = '\0';

    return 0;
}

// 0x4499B4
static void gsound_background_remove_last_copy()
{
    if (background_fname_copied[0] != '\0') {
        char path[COMPAT_MAX_PATH];
        snprintf(path, sizeof(path), "%s%s%s", "sound\\music\\", background_fname_copied, ".ACM");
        if (compat_remove(path)) {
            if (gsound_debug) {
                debug_printf("Deleting old music file failed.\n");
            }
        }

        background_fname_copied[0] = '\0';
    }
}

// 0x449A18
static int gsound_background_start()
{
    int result;

    if (gsound_debug) {
        debug_printf(" playing ");
    }

    if (gsound_background_fade) {
        soundVolume(gsound_background_tag, 1);
        result = soundFade(gsound_background_tag, 2000, (int)(background_volume * 0.94));
    } else {
        soundVolume(gsound_background_tag, (int)(background_volume * 0.94));
        result = soundPlay(gsound_background_tag);
    }

    if (result != 0) {
        if (gsound_debug) {
            debug_printf("Unable to play background sound.\n");
        }

        result = -1;
    }

    return result;
}

// 0x449AC8
static int gsound_speech_start()
{
    if (gsound_debug) {
        debug_printf(" playing ");
    }

    soundVolume(gsound_speech_tag, (int)(speech_volume * 0.69));

    if (soundPlay(gsound_speech_tag) != 0) {
        if (gsound_debug) {
            debug_printf("Unable to play speech sound.\n");
        }

        return -1;
    }

    return 0;
}

// 0x449B34
static int gsound_get_music_path(char** out_value, const char* key)
{
    int len;
    char* copy;
    char* value;

    config_get_string(&game_config, GAME_CONFIG_SOUND_KEY, key, out_value);

    value = *out_value;
    len = strlen(value);

    if (value[len - 1] == '\\' || value[len - 1] == '/') {
        return 0;
    }

    copy = (char*)mem_malloc(len + 2);
    if (copy == NULL) {
        if (gsound_debug) {
            debug_printf("Out of memory in gsound_get_music_path.\n");
        }
        return -1;
    }

    strcpy(copy, value);
    copy[len] = '\\';
    copy[len + 1] = '\0';

    if (config_set_string(&game_config, GAME_CONFIG_SOUND_KEY, key, copy) != 1) {
        if (gsound_debug) {
            debug_printf("config_set_string failed in gsound_music_path.\n");
        }

        return -1;
    }

    if (config_get_string(&game_config, GAME_CONFIG_SOUND_KEY, key, out_value)) {
        mem_free(copy);
        return 0;
    }

    if (gsound_debug) {
        debug_printf("config_get_string failed in gsound_music_path.\n");
    }

    return -1;
}

// 0x449C7C
static void gsound_check_active_effects()
{
    if (gsound_active_effect_counter < 0 || gsound_active_effect_counter > 4) {
        if (gsound_debug) {
            debug_printf("WARNING: %d active effects.\n", gsound_active_effect_counter);
        }
    }
}

// 0x449CA4
static Sound* gsound_get_sound_ready_for_effect()
{
    int rc;

    Sound* sound = soundAllocate(5, 10);
    if (sound == NULL) {
        if (gsound_debug) {
            debug_printf(" Can't allocate sound for effect. ");
        }

        if (gsound_debug) {
            debug_printf("soundAllocate returned: %d, %s\n", 0, soundError(0));
        }

        return NULL;
    }

    if (sfxc_is_initialized()) {
        rc = soundSetFileIO(sound, sfxc_cached_open, sfxc_cached_close, sfxc_cached_read, sfxc_cached_write, sfxc_cached_seek, sfxc_cached_tell, sfxc_cached_file_size);
    } else {
        rc = soundSetFileIO(sound, audioOpen, audioCloseFile, audioRead, NULL, audioSeek, gsound_compressed_tell, audioFileSize);
    }

    if (rc != 0) {
        if (gsound_debug) {
            debug_printf("Can't set file IO on sound effect.\n");
        }

        if (gsound_debug) {
            debug_printf("soundSetFileIO returned: %d, %s\n", rc, soundError(rc));
        }

        soundDelete(sound);

        return NULL;
    }

    rc = soundSetCallback(sound, gsound_internal_effect_callback, NULL);
    if (rc != 0) {
        if (gsound_debug) {
            debug_printf("failed because the callback could not be set.\n");
        }

        if (gsound_debug) {
            debug_printf("soundSetCallback returned: %d, %s\n", rc, soundError(rc));
        }

        soundDelete(sound);

        return NULL;
    }

    soundVolume(sound, sndfx_volume);

    return sound;
}

// 0x449E08
static bool gsound_file_exists_f(const char* fname)
{
    FILE* f = compat_fopen(fname, "rb");
    if (f == NULL) {
        return false;
    }

    fclose(f);

    return true;
}

// 0x449E24
static int gsound_file_exists_db(const char* path)
{
    dir_entry de;
    return db_dir_entry(path, &de) == 0;
}

// 0x449E40
static int gsound_setup_paths()
{
    // TODO: Incomplete.

    return 0;
}

} // namespace fallout
