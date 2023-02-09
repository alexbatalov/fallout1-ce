#include "game/amutex.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace fallout {

#ifdef _WIN32
// 0x540010
static HANDLE autorun_mutex;
#endif

// 0x413450
bool autorun_mutex_create()
{
#ifdef _WIN32
    autorun_mutex = CreateMutexA(NULL, FALSE, "InterplayGenericAutorunMutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(autorun_mutex);
        return false;
    }
#endif

    return true;
}

// 0x413490
void autorun_mutex_destroy()
{
#ifdef _WIN32
    if (autorun_mutex != NULL) {
        CloseHandle(autorun_mutex);
    }
#endif
}

} // namespace fallout
