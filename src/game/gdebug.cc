#include "game/gdebug.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "plib/gnw/debug.h"
#include "plib/gnw/gnw.h"

namespace fallout {

// 0x43DD80
void fatal_error(const char* format, const char* message, const char* file, int line)
{
    char stringBuffer[260];

    debug_printf("\n");
    debug_printf(format, message, file, line);

    win_exit();

    printf("\n\n\n\n\n   ");
    printf(format, message, file, line);
    printf("\n\n\n\n\n");

    snprintf(stringBuffer, sizeof(stringBuffer), format, message, file, line);
    GNWSystemError(stringBuffer);

    exit(1);
}

} // namespace fallout
