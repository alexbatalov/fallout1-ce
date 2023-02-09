#ifndef FALLOUT_INT_REGION_H_
#define FALLOUT_INT_REGION_H_

#include "int/intrpret.h"
#include "plib/gnw/rect.h"

namespace fallout {

#define REGION_NAME_LENGTH 32

typedef struct Region Region;

typedef void RegionMouseEventCallback(Region* region, void* userData, int event);

typedef struct Region {
    char name[REGION_NAME_LENGTH];
    Point* points;
    int minX;
    int minY;
    int maxX;
    int maxY;
    int centerX;
    int centerY;
    int pointsLength;
    int pointsCapacity;
    Program* program;
    int procs[4];
    int rightProcs[4];
    int field_68;
    int field_6C;
    int field_70;
    int flags;
    RegionMouseEventCallback* mouseEventCallback;
    RegionMouseEventCallback* rightMouseEventCallback;
    void* mouseEventCallbackUserData;
    void* rightMouseEventCallbackUserData;
    void* userData;
} Region;

void regionSetBound(Region* region);
bool pointInRegion(Region* region, int x, int y);
Region* allocateRegion(int initialCapacity);
void regionAddPoint(Region* region, int x, int y);
void regionDelete(Region* region);
void regionAddName(Region* region, const char* src);
const char* regionGetName(Region* region);
void* regionGetUserData(Region* region);
void regionSetUserData(Region* region, void* data);
void regionSetFlag(Region* region, int value);
int regionGetFlag(Region* region);

} // namespace fallout

#endif /* FALLOUT_INT_REGION_H_ */
