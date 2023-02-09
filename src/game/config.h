#ifndef FALLOUT_GAME_CONFIG_H_
#define FALLOUT_GAME_CONFIG_H_

#include "plib/assoc/assoc.h"

namespace fallout {

// A representation of .INI file.
//
// It's implemented as a [assoc_array] whos keys are section names of .INI file,
// and it's values are [ConfigSection] structs.
typedef assoc_array Config;

// Representation of .INI section.
//
// It's implemented as a [assoc_array] whos keys are names of .INI file
// key-pair values, and it's values are pointers to strings (char**).
typedef assoc_array ConfigSection;

bool config_init(Config* config);
void config_exit(Config* config);
bool config_cmd_line_parse(Config* config, int argc, char** argv);
bool config_get_string(Config* config, const char* sectionKey, const char* key, char** valuePtr);
bool config_set_string(Config* config, const char* sectionKey, const char* key, const char* value);
bool config_get_value(Config* config, const char* sectionKey, const char* key, int* valuePtr);
bool config_get_values(Config* config, const char* section, const char* key, int* arr, int count);
bool config_set_value(Config* config, const char* sectionKey, const char* key, int value);
bool config_load(Config* config, const char* filePath, bool isDb);
bool config_save(Config* config, const char* filePath, bool isDb);
bool config_get_double(Config* config, const char* sectionKey, const char* key, double* valuePtr);
bool config_set_double(Config* config, const char* sectionKey, const char* key, double value);

// TODO: Remove.
bool configGetBool(Config* config, const char* sectionKey, const char* key, bool* valuePtr);
bool configSetBool(Config* config, const char* sectionKey, const char* key, bool value);

} // namespace fallout

#endif /* FALLOUT_GAME_CONFIG_H_ */
