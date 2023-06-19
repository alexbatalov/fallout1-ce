#include "game/gconfig.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#include <shlwapi.h>
#else
#include <libgen.h>
#include <unistd.h>
#endif

#include "platform_compat.h"
#include "plib/gnw/debug.h"

namespace fallout {

// A flag indicating if `game_config` was initialized.
//
// 0x504FD8
static bool gconfig_initialized = false;

// fallout.cfg
//
// 0x58CC20
Config game_config;

// NOTE: There are additional 4 bytes following this array at 0x58EA7C, which
// probably means it's size is 264 bytes.
//
// 0x58CC48
static char gconfig_file_name[COMPAT_MAX_PATH];

// Inits main game config.
//
// `isMapper` is a flag indicating whether we're initing config for a main
// game, or a mapper. This value is `false` for the game itself.
//
// `argc` and `argv` are command line arguments. The engine assumes there is
// at least 1 element which is executable path at index 0. There is no
// additional check for `argc`, so it will crash if you pass NULL, or an empty
// array into `argv`.
//
// The executable path from `argv` is used resolve path to `fallout.cfg`,
// which should be in the same folder. This function provide defaults if
// `fallout.cfg` is not present, or cannot be read for any reason.
//
// Finally, this function merges key-value pairs from `argv` if any, see
// `config_cmd_line_parse` for expected format.
//
// 0x43D690
bool gconfig_init(bool isMapper, int argc, char** argv)
{
    char* sep;

    if (gconfig_initialized) {
        return false;
    }

    if (!config_init(&game_config)) {
        return false;
    }

    // Initialize defaults.
    config_set_string(&game_config, GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_EXECUTABLE_KEY, "game");
    config_set_string(&game_config, GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_MASTER_DAT_KEY, "master.dat");
    config_set_string(&game_config, GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_MASTER_PATCHES_KEY, "data");
    config_set_string(&game_config, GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_CRITTER_DAT_KEY, "critter.dat");
    config_set_string(&game_config, GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_CRITTER_PATCHES_KEY, "data");
    config_set_string(&game_config, GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_LANGUAGE_KEY, ENGLISH);
    config_set_value(&game_config, GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_SCROLL_LOCK_KEY, 0);
    config_set_value(&game_config, GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_INTERRUPT_WALK_KEY, 1);
    config_set_value(&game_config, GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_ART_CACHE_SIZE_KEY, 8);
    config_set_value(&game_config, GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_COLOR_CYCLING_KEY, 1);
    config_set_value(&game_config, GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_HASHING_KEY, 1);
    config_set_value(&game_config, GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_SPLASH_KEY, 0);
    config_set_value(&game_config, GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_FREE_SPACE_KEY, 20480);
    config_set_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_GAME_DIFFICULTY_KEY, 1);
    config_set_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_COMBAT_DIFFICULTY_KEY, 1);
    config_set_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_VIOLENCE_LEVEL_KEY, 3);
    config_set_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_TARGET_HIGHLIGHT_KEY, 2);
    config_set_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_ITEM_HIGHLIGHT_KEY, 1);
    config_set_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_RUNNING_BURNING_GUY_KEY, 1);
    config_set_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_COMBAT_MESSAGES_KEY, 1);
    config_set_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_COMBAT_TAUNTS_KEY, 1);
    config_set_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_LANGUAGE_FILTER_KEY, 0);
    config_set_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_RUNNING_KEY, 0);
    config_set_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_SUBTITLES_KEY, 0);
    config_set_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_COMBAT_SPEED_KEY, 0);
    config_set_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_PLAYER_SPEED_KEY, 0);
    config_set_double(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_TEXT_BASE_DELAY_KEY, 3.5);
    config_set_double(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_TEXT_LINE_DELAY_KEY, 1.399994);
    config_set_double(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_BRIGHTNESS_KEY, 1.0);
    config_set_double(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_MOUSE_SENSITIVITY_KEY, 1.0);
    config_set_value(&game_config, GAME_CONFIG_SOUND_KEY, GAME_CONFIG_INITIALIZE_KEY, 1);
    config_set_value(&game_config, GAME_CONFIG_SOUND_KEY, GAME_CONFIG_DEVICE_KEY, -1);
    config_set_value(&game_config, GAME_CONFIG_SOUND_KEY, GAME_CONFIG_PORT_KEY, -1);
    config_set_value(&game_config, GAME_CONFIG_SOUND_KEY, GAME_CONFIG_IRQ_KEY, -1);
    config_set_value(&game_config, GAME_CONFIG_SOUND_KEY, GAME_CONFIG_DMA_KEY, -1);
    config_set_value(&game_config, GAME_CONFIG_SOUND_KEY, GAME_CONFIG_SOUNDS_KEY, 1);
    config_set_value(&game_config, GAME_CONFIG_SOUND_KEY, GAME_CONFIG_MUSIC_KEY, 1);
    config_set_value(&game_config, GAME_CONFIG_SOUND_KEY, GAME_CONFIG_SPEECH_KEY, 1);
    config_set_value(&game_config, GAME_CONFIG_SOUND_KEY, GAME_CONFIG_MASTER_VOLUME_KEY, 22281);
    config_set_value(&game_config, GAME_CONFIG_SOUND_KEY, GAME_CONFIG_MUSIC_VOLUME_KEY, 22281);
    config_set_value(&game_config, GAME_CONFIG_SOUND_KEY, GAME_CONFIG_SNDFX_VOLUME_KEY, 22281);
    config_set_value(&game_config, GAME_CONFIG_SOUND_KEY, GAME_CONFIG_SPEECH_VOLUME_KEY, 22281);
    config_set_value(&game_config, GAME_CONFIG_SOUND_KEY, GAME_CONFIG_CACHE_SIZE_KEY, 448);
    config_set_string(&game_config, GAME_CONFIG_SOUND_KEY, GAME_CONFIG_MUSIC_PATH1_KEY, "sound\\music\\");
    config_set_string(&game_config, GAME_CONFIG_SOUND_KEY, GAME_CONFIG_MUSIC_PATH2_KEY, "sound\\music\\");
    config_set_string(&game_config, GAME_CONFIG_DEBUG_KEY, GAME_CONFIG_MODE_KEY, "environment");
    config_set_value(&game_config, GAME_CONFIG_DEBUG_KEY, GAME_CONFIG_SHOW_TILE_NUM_KEY, 0);
    config_set_value(&game_config, GAME_CONFIG_DEBUG_KEY, GAME_CONFIG_SHOW_SCRIPT_MESSAGES_KEY, 0);
    config_set_value(&game_config, GAME_CONFIG_DEBUG_KEY, GAME_CONFIG_SHOW_LOAD_INFO_KEY, 0);
    config_set_value(&game_config, GAME_CONFIG_DEBUG_KEY, GAME_CONFIG_OUTPUT_MAP_DATA_INFO_KEY, 0);

    if (isMapper) {
        config_set_string(&game_config, GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_EXECUTABLE_KEY, "mapper");
        config_set_value(&game_config, GAME_CONFIG_MAPPER_KEY, GAME_CONFIG_OVERRIDE_LIBRARIAN_KEY, 0);
        config_set_value(&game_config, GAME_CONFIG_MAPPER_KEY, GAME_CONFIG_USE_ART_NOT_PROTOS_KEY, 0);
        config_set_value(&game_config, GAME_CONFIG_MAPPER_KEY, GAME_CONFIG_REBUILD_PROTOS_KEY, 0);
        config_set_value(&game_config, GAME_CONFIG_MAPPER_KEY, GAME_CONFIG_FIX_MAP_OBJECTS_KEY, 0);
        config_set_value(&game_config, GAME_CONFIG_MAPPER_KEY, GAME_CONFIG_FIX_MAP_INVENTORY_KEY, 0);
        config_set_value(&game_config, GAME_CONFIG_MAPPER_KEY, GAME_CONFIG_IGNORE_REBUILD_ERRORS_KEY, 0);
        config_set_value(&game_config, GAME_CONFIG_MAPPER_KEY, GAME_CONFIG_SHOW_PID_NUMBERS_KEY, 0);
        config_set_value(&game_config, GAME_CONFIG_MAPPER_KEY, GAME_CONFIG_SAVE_TEXT_MAPS_KEY, 0);
        config_set_value(&game_config, GAME_CONFIG_MAPPER_KEY, GAME_CONFIG_RUN_MAPPER_AS_GAME_KEY, 0);
        config_set_value(&game_config, GAME_CONFIG_MAPPER_KEY, GAME_CONFIG_DEFAULT_F8_AS_GAME_KEY, 1);
    }

    char* gamedir = NULL;
    for (int i = 1; i < argc; i++) {
        if (strlen(argv[i]) < 2 || argv[i][0] != '-') {
            if (gamedir == NULL && strchr(argv[i], '[') == NULL && strchr(argv[i], ']') == NULL)
                gamedir = argv[i];
            continue;
        }

        switch (argv[i][1]) {
        case 'v':
            do_debug = true;
            break;
        case 'h':
            printf("usage: %s [-v] [game_dir] [option...]\n"
                   "\n"
                   "Flags:\n"
                   "  -v           Print more information.\n\n"
                   "  game_dir     Fallout installation directory; defaults to directory binary\n"
                   "               is in.\n\n"
                   "  option       Any list of option to override from fallout.cfg\n"
                   "               as \"[section]key=value\" Note that new value will be written\n"
                   "               to fallout.cfg! Overriding values from f1_res.ini is not\n"
                   "               supported.\n\n",
                argv[0]);
            exit(0);
        }
    }
    if (gamedir == NULL) {
#if _WIN32
        gamedir = (char*)calloc(strlen(argv[0]), sizeof(char));
        strcpy(gamedir, argv[0]);
        PathRemoveFileSpecA(gamedir);
#else
        gamedir = dirname(argv[0]);
#endif
        if (gamedir == NULL) {
            perror("resolving argv[0]");
            exit(1);
        }
    }
    debug_printf("game directory: %s\n", gamedir);
    if (chdir(gamedir) == -1) {
        perror("changing to game directory");
        exit(1);
    }

    // Make `fallout.cfg` file path.
    sep = strrchr(argv[0], '\\');
    if (sep != NULL) {
        *sep = '\0';
        snprintf(gconfig_file_name, sizeof(gconfig_file_name), "%s\\%s", argv[0], GAME_CONFIG_FILE_NAME);
        *sep = '\\';
    } else {
        strcpy(gconfig_file_name, GAME_CONFIG_FILE_NAME);
    }

    // Read contents of `fallout.cfg` into config. The values from the file
    // will override the defaults above.
    config_load(&game_config, gconfig_file_name, false);

    // Add key-values from command line, which overrides both defaults and
    // whatever was loaded from `fallout.cfg`.
    config_cmd_line_parse(&game_config, argc, argv);

    gconfig_initialized = true;

    return true;
}

// Saves game config into `fallout.cfg`.
//
// 0x43DD08
bool gconfig_save()
{
    if (!gconfig_initialized) {
        return false;
    }

    if (!config_save(&game_config, gconfig_file_name, false)) {
        return false;
    }

    return true;
}

// Frees game config, optionally saving it.
//
// 0x43DD30
bool gconfig_exit(bool shouldSave)
{
    if (!gconfig_initialized) {
        return false;
    }

    bool result = true;

    if (shouldSave) {
        if (!config_save(&game_config, gconfig_file_name, false)) {
            result = false;
        }
    }

    config_exit(&game_config);

    gconfig_initialized = false;

    return result;
}

} // namespace fallout
