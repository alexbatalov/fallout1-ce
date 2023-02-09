#ifndef FALLOUT_GAME_GCONFIG_H_
#define FALLOUT_GAME_GCONFIG_H_

#include "game/config.h"

namespace fallout {

// The file name of the main config file.
#define GAME_CONFIG_FILE_NAME "fallout.cfg"

#define GAME_CONFIG_SYSTEM_KEY "system"
#define GAME_CONFIG_PREFERENCES_KEY "preferences"
#define GAME_CONFIG_SOUND_KEY "sound"
#define GAME_CONFIG_MAPPER_KEY "mapper"
#define GAME_CONFIG_DEBUG_KEY "debug"

#define GAME_CONFIG_EXECUTABLE_KEY "executable"
#define GAME_CONFIG_MASTER_DAT_KEY "master_dat"
#define GAME_CONFIG_MASTER_PATCHES_KEY "master_patches"
#define GAME_CONFIG_CRITTER_DAT_KEY "critter_dat"
#define GAME_CONFIG_CRITTER_PATCHES_KEY "critter_patches"
#define GAME_CONFIG_PATCHES_KEY "patches"
#define GAME_CONFIG_LANGUAGE_KEY "language"
#define GAME_CONFIG_SCROLL_LOCK_KEY "scroll_lock"
#define GAME_CONFIG_INTERRUPT_WALK_KEY "interrupt_walk"
#define GAME_CONFIG_ART_CACHE_SIZE_KEY "art_cache_size"
#define GAME_CONFIG_COLOR_CYCLING_KEY "color_cycling"
#define GAME_CONFIG_CYCLE_SPEED_FACTOR_KEY "cycle_speed_factor"
#define GAME_CONFIG_HASHING_KEY "hashing"
#define GAME_CONFIG_SPLASH_KEY "splash"
#define GAME_CONFIG_FREE_SPACE_KEY "free_space"
#define GAME_CONFIG_TIMES_RUN_KEY "times_run"
#define GAME_CONFIG_GAME_DIFFICULTY_KEY "game_difficulty"
#define GAME_CONFIG_RUNNING_BURNING_GUY_KEY "running_burning_guy"
#define GAME_CONFIG_COMBAT_DIFFICULTY_KEY "combat_difficulty"
#define GAME_CONFIG_VIOLENCE_LEVEL_KEY "violence_level"
#define GAME_CONFIG_TARGET_HIGHLIGHT_KEY "target_highlight"
#define GAME_CONFIG_ITEM_HIGHLIGHT_KEY "item_highlight"
#define GAME_CONFIG_COMBAT_LOOKS_KEY "combat_looks"
#define GAME_CONFIG_COMBAT_MESSAGES_KEY "combat_messages"
#define GAME_CONFIG_COMBAT_TAUNTS_KEY "combat_taunts"
#define GAME_CONFIG_LANGUAGE_FILTER_KEY "language_filter"
#define GAME_CONFIG_RUNNING_KEY "running"
#define GAME_CONFIG_SUBTITLES_KEY "subtitles"
#define GAME_CONFIG_COMBAT_SPEED_KEY "combat_speed"
#define GAME_CONFIG_PLAYER_SPEED_KEY "player_speed"
#define GAME_CONFIG_TEXT_BASE_DELAY_KEY "text_base_delay"
#define GAME_CONFIG_TEXT_LINE_DELAY_KEY "text_line_delay"
#define GAME_CONFIG_BRIGHTNESS_KEY "brightness"
#define GAME_CONFIG_MOUSE_SENSITIVITY_KEY "mouse_sensitivity"
#define GAME_CONFIG_INITIALIZE_KEY "initialize"
#define GAME_CONFIG_DEVICE_KEY "device"
#define GAME_CONFIG_PORT_KEY "port"
#define GAME_CONFIG_IRQ_KEY "irq"
#define GAME_CONFIG_DMA_KEY "dma"
#define GAME_CONFIG_SOUNDS_KEY "sounds"
#define GAME_CONFIG_MUSIC_KEY "music"
#define GAME_CONFIG_SPEECH_KEY "speech"
#define GAME_CONFIG_MASTER_VOLUME_KEY "master_volume"
#define GAME_CONFIG_MUSIC_VOLUME_KEY "music_volume"
#define GAME_CONFIG_SNDFX_VOLUME_KEY "sndfx_volume"
#define GAME_CONFIG_SPEECH_VOLUME_KEY "speech_volume"
#define GAME_CONFIG_CACHE_SIZE_KEY "cache_size"
#define GAME_CONFIG_MUSIC_PATH1_KEY "music_path1"
#define GAME_CONFIG_MUSIC_PATH2_KEY "music_path2"
#define GAME_CONFIG_DEBUG_SFXC_KEY "debug_sfxc"
#define GAME_CONFIG_MODE_KEY "mode"
#define GAME_CONFIG_SHOW_TILE_NUM_KEY "show_tile_num"
#define GAME_CONFIG_SHOW_SCRIPT_MESSAGES_KEY "show_script_messages"
#define GAME_CONFIG_SHOW_LOAD_INFO_KEY "show_load_info"
#define GAME_CONFIG_OUTPUT_MAP_DATA_INFO_KEY "output_map_data_info"
#define GAME_CONFIG_EXECUTABLE_KEY "executable"
#define GAME_CONFIG_OVERRIDE_LIBRARIAN_KEY "override_librarian"
#define GAME_CONFIG_USE_ART_NOT_PROTOS_KEY "use_art_not_protos"
#define GAME_CONFIG_REBUILD_PROTOS_KEY "rebuild_protos"
#define GAME_CONFIG_FIX_MAP_OBJECTS_KEY "fix_map_objects"
#define GAME_CONFIG_FIX_MAP_INVENTORY_KEY "fix_map_inventory"
#define GAME_CONFIG_IGNORE_REBUILD_ERRORS_KEY "ignore_rebuild_errors"
#define GAME_CONFIG_SHOW_PID_NUMBERS_KEY "show_pid_numbers"
#define GAME_CONFIG_SAVE_TEXT_MAPS_KEY "save_text_maps"
#define GAME_CONFIG_RUN_MAPPER_AS_GAME_KEY "run_mapper_as_game"
#define GAME_CONFIG_DEFAULT_F8_AS_GAME_KEY "default_f8_as_game"
#define GAME_CONFIG_PLAYER_SPEEDUP_KEY "player_speedup"

#define ENGLISH "english"
#define FRENCH "french"
#define GERMAN "german"
#define ITALIAN "italian"
#define SPANISH "spanish"

typedef enum GameDifficulty {
    GAME_DIFFICULTY_EASY,
    GAME_DIFFICULTY_NORMAL,
    GAME_DIFFICULTY_HARD,
} GameDifficulty;

typedef enum CombatDifficulty {
    COMBAT_DIFFICULTY_EASY,
    COMBAT_DIFFICULTY_NORMAL,
    COMBAT_DIFFICULTY_HARD,
} CombatDifficulty;

typedef enum ViolenceLevel {
    VIOLENCE_LEVEL_NONE,
    VIOLENCE_LEVEL_MINIMAL,
    VIOLENCE_LEVEL_NORMAL,
    VIOLENCE_LEVEL_MAXIMUM_BLOOD,
} ViolenceLevel;

typedef enum TargetHighlight {
    TARGET_HIGHLIGHT_OFF,
    TARGET_HIGHLIGHT_ON,
    TARGET_HIGHLIGHT_TARGETING_ONLY,
} TargetHighlight;

extern Config game_config;

bool gconfig_init(bool isMapper, int argc, char** argv);
bool gconfig_save();
bool gconfig_exit(bool shouldSave);

} // namespace fallout

#endif /* FALLOUT_GAME_GCONFIG_H_ */
