#ifndef FALLOUT_GAME_GSOUND_H_
#define FALLOUT_GAME_GSOUND_H_

#include "game/object_types.h"
#include "int/sound.h"

namespace fallout {

typedef enum WeaponSoundEffect {
    WEAPON_SOUND_EFFECT_READY,
    WEAPON_SOUND_EFFECT_ATTACK,
    WEAPON_SOUND_EFFECT_OUT_OF_AMMO,
    WEAPON_SOUND_EFFECT_AMMO_FLYING,
    WEAPON_SOUND_EFFECT_HIT,
    WEAPON_SOUND_EFFECT_COUNT,
} WeaponSoundEffect;

typedef enum SoundEffectActionType {
    SOUND_EFFECT_ACTION_TYPE_ACTIVE,
    SOUND_EFFECT_ACTION_TYPE_PASSIVE,
} SoundEffectActionType;

typedef enum ScenerySoundEffect {
    SCENERY_SOUND_EFFECT_OPEN,
    SCENERY_SOUND_EFFECT_CLOSED,
    SCENERY_SOUND_EFFECT_LOCKED,
    SCENERY_SOUND_EFFECT_UNLOCKED,
    SCENERY_SOUND_EFFECT_USED,
    SCENERY_SOUND_EFFECT_COUNT,
} ScenerySoundEffect;

typedef enum CharacterSoundEffect {
    CHARACTER_SOUND_EFFECT_UNUSED,
    CHARACTER_SOUND_EFFECT_KNOCKDOWN,
    CHARACTER_SOUND_EFFECT_PASS_OUT,
    CHARACTER_SOUND_EFFECT_DIE,
    CHARACTER_SOUND_EFFECT_CONTACT,
} CharacterSoundEffect;

typedef void(SoundEndCallback)();

int gsound_init();
void gsound_reset();
int gsound_exit();
void gsound_sfx_enable();
void gsound_sfx_disable();
int gsound_sfx_is_enabled();
int gsound_set_master_volume(int value);
int gsound_get_master_volume();
int gsound_set_sfx_volume(int value);
int gsound_get_sfx_volume();
void gsound_background_disable();
void gsound_background_enable();
int gsound_background_is_enabled();
void gsound_background_volume_set(int value);
int gsound_background_volume_get();
int gsound_background_volume_get_set(int a1);
void gsound_background_fade_set(int value);
int gsound_background_fade_get();
int gsound_background_fade_get_set(int value);
void gsound_background_callback_set(SoundEndCallback* callback);
SoundEndCallback* gsound_background_callback_get();
SoundEndCallback* gsound_background_callback_get_set(SoundEndCallback* callback);
int gsound_background_length_get();
int gsound_background_play(const char* fileName, int a2, int a3, int a4);
int gsound_background_play_level_music(const char* a1, int a2);
int gsound_background_play_preloaded();
void gsound_background_stop();
void gsound_background_restart_last(int value);
void gsound_background_pause();
void gsound_background_unpause();
void gsound_speech_disable();
void gsound_speech_enable();
int gsound_speech_is_enabled();
void gsound_speech_volume_set(int value);
int gsound_speech_volume_get();
int gsound_speech_volume_get_set(int volume);
void gsound_speech_callback_set(SoundEndCallback* callback);
SoundEndCallback* gsound_speech_callback_get();
SoundEndCallback* gsound_speech_callback_get_set(SoundEndCallback* callback);
int gsound_speech_length_get();
int gsound_speech_play(const char* fname, int a2, int a3, int a4);
int gsound_speech_play_preloaded();
void gsound_speech_stop();
void gsound_speech_pause();
void gsound_speech_unpause();
int gsound_play_sfx_file_volume(const char* a1, int a2);
Sound* gsound_load_sound(const char* name, Object* a2);
Sound* gsound_load_sound_volume(const char* a1, Object* a2, int a3);
void gsound_delete_sfx(Sound* a1);
int gsnd_anim_sound(Sound* sound, void* a2);
int gsound_play_sound(Sound* a1);
int gsound_compute_relative_volume(Object* obj);
char* gsnd_build_character_sfx_name(Object* a1, int anim, int extra);
char* gsnd_build_ambient_sfx_name(const char* a1);
char* gsnd_build_interface_sfx_name(const char* a1);
char* gsnd_build_weapon_sfx_name(int effectType, Object* weapon, int hitMode, Object* target);
char* gsnd_build_scenery_sfx_name(int actionType, int action, const char* name);
char* gsnd_build_open_sfx_name(Object* a1, int a2);
void gsound_red_butt_press(int btn, int keyCode);
void gsound_red_butt_release(int btn, int keyCode);
void gsound_toggle_butt_press(int btn, int keyCode);
void gsound_toggle_butt_release(int btn, int keyCode);
void gsound_med_butt_press(int btn, int keyCode);
void gsound_med_butt_release(int btn, int keyCode);
void gsound_lrg_butt_press(int btn, int keyCode);
void gsound_lrg_butt_release(int btn, int keyCode);
int gsound_play_sfx_file(const char* name);

} // namespace fallout

#endif /* FALLOUT_GAME_GSOUND_H_ */
