#include "pointer_registry.h"

namespace fallout {

PointerRegistry* PointerRegistry::shared()
{
    static PointerRegistry* shared = new PointerRegistry();
    return shared;
}

PointerRegistry::PointerRegistry()
{
    // 0 is reserved for nullptr, so start with 1.
    _next = 1;
}

int PointerRegistry::store(void* ptr)
{
    if (ptr == nullptr) return 0;
    int ref = _next++;
    _map[ref] = ptr;
    return ref;
}

void* PointerRegistry::fetch(int ref, bool remove)
{
    if (ref == 0) return nullptr;
    void* ptr = _map[ref];
    if (remove) {
        _map.erase(ref);
    }
    return ptr;
}

int ptrToInt(void* ptr)
{
    return PointerRegistry::shared()->store(ptr);
}

void* intToPtr(int ref, bool remove)
{
    return PointerRegistry::shared()->fetch(ref, remove);
}

} // namespace fallout
