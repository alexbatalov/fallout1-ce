#include "game/wordwrap.h"

#include <ctype.h>
#include <stddef.h>
#include <string.h>

#include "plib/gnw/text.h"

namespace fallout {

// 0x4A91C0
int word_wrap(const char* string, int width, short* breakpoints, short* breakpointsLengthPtr)
{
    breakpoints[0] = 0;
    *breakpointsLengthPtr = 1;

    for (int index = 1; index < WORD_WRAP_MAX_COUNT; index++) {
        breakpoints[index] = -1;
    }

    if (text_max() > width) {
        return -1;
    }

    if (text_width(string) < width) {
        breakpoints[*breakpointsLengthPtr] = (short)strlen(string);
        *breakpointsLengthPtr += 1;
        return 0;
    }

    int gap = text_spacing();

    int accum = 0;
    const char* prevSpaceOrHyphen = NULL;
    const char* pch = string;
    while (*pch != '\0') {
        accum += gap + text_char_width(*pch & 0xFF);
        if (accum <= width) {
            // NOTE: quests.txt #807 uses extended ascii.
            if (isspace(*pch & 0xFF) || *pch == '-') {
                prevSpaceOrHyphen = pch;
            }
        } else {
            if (*breakpointsLengthPtr == WORD_WRAP_MAX_COUNT) {
                return -1;
            }

            if (prevSpaceOrHyphen != NULL) {
                // Word wrap.
                breakpoints[*breakpointsLengthPtr] = prevSpaceOrHyphen - string + 1;
                *breakpointsLengthPtr += 1;

                pch = prevSpaceOrHyphen;
            } else {
                // Character wrap.
                breakpoints[*breakpointsLengthPtr] = pch - string;
                *breakpointsLengthPtr += 1;

                pch--;
            }

            prevSpaceOrHyphen = NULL;
            accum = 0;
        }
        pch++;
    }

    if (*breakpointsLengthPtr == WORD_WRAP_MAX_COUNT) {
        return -1;
    }

    breakpoints[*breakpointsLengthPtr] = pch - string + 1;
    *breakpointsLengthPtr += 1;

    return 0;
}

} // namespace fallout
