#include "game/version.h"

#include <stdio.h>

namespace fallout {

// 0x4A10C0
char* getverstr(char* dest, size_t size)
{
    snprintf(dest, size, "FALLOUT %d.%d", VERSION_MAJOR, VERSION_MINOR);
    return dest;
}

} // namespace fallout
