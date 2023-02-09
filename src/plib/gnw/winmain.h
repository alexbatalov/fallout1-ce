#ifndef FALLOUT_PLIB_GNW_WINMAIN_H_
#define FALLOUT_PLIB_GNW_WINMAIN_H_

#ifdef _WIN32
#include <windows.h>
#endif

namespace fallout {

extern bool GNW95_isActive;

#ifdef _WIN32
extern HANDLE GNW95_mutex;
#endif

extern char GNW95_title[256];

} // namespace fallout

#endif /* FALLOUT_PLIB_GNW_WINMAIN_H_ */
