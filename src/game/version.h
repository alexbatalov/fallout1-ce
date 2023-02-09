#ifndef FALLOUT_GAME_VERSION_H_
#define FALLOUT_GAME_VERSION_H_

#include <stddef.h>

namespace fallout {

// The size of buffer for version string.
#define VERSION_MAX 32

#define VERSION_MAJOR 1
#define VERSION_MINOR 1
#define VERSION_RELEASE 'R'

#define VERSION_BUILD_TIME "Nov 11 1997 14:59:39"

char* getverstr(char* dest, size_t size);

} // namespace fallout

#endif /* FALLOUT_GAME_VERSION_H_ */
