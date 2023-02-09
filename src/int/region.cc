#include "int/region.h"

#include <limits.h>
#include <string.h>

#include "int/memdbg.h"
#include "plib/gnw/debug.h"

namespace fallout {

// 0x490F60
void regionSetBound(Region* region)
{
    int minX = INT_MAX;
    int maxX = INT_MIN;
    int minY = INT_MAX;
    int maxY = INT_MIN;
    int numPoints = 0;
    int totalX = 0;
    int totalY = 0;

    for (int index = 0; index < region->pointsLength; index++) {
        Point* point = &(region->points[index]);
        if (minX >= point->x) minX = point->x;
        if (minY >= point->y) minY = point->y;
        if (maxX <= point->x) maxX = point->x;
        if (maxY <= point->y) maxY = point->y;
        totalX += point->x;
        totalY += point->y;
        numPoints++;
    }

    region->minY = minY;
    region->maxX = maxX;
    region->maxY = maxY;
    region->minX = minX;

    if (numPoints != 0) {
        region->centerX = totalX / numPoints;
        region->centerY = totalY / numPoints;
    }
}

// 0x491024
bool pointInRegion(Region* region, int x, int y)
{
    if (region == NULL) {
        return false;
    }

    if (x < region->minX || x > region->maxX || y < region->minY || y > region->maxY) {
        return false;
    }

    int v1;

    Point* prev = &(region->points[0]);
    if (x >= prev->x) {
        if (y >= prev->y) {
            v1 = 2;
        } else {
            v1 = 1;
        }
    } else {
        if (y >= prev->y) {
            v1 = 3;
        } else {
            v1 = 0;
        }
    }

    int v4 = 0;
    for (int index = 0; index < region->pointsLength; index++) {
        int v2;

        Point* point = &(region->points[index + 1]);
        if (x >= point->x) {
            if (y >= point->y) {
                v2 = 2;
            } else {
                v2 = 1;
            }
        } else {
            if (y >= point->y) {
                v2 = 3;
            } else {
                v2 = 0;
            }
        }

        int v3 = v2 - v1;
        switch (v3) {
        case -3:
            v3 = 1;
            break;
        case -2:
        case 2:
            if ((double)x < ((double)point->x - (double)(prev->x - point->x) / (double)(prev->y - point->y) * (double)(point->y - y))) {
                v3 = -v3;
            }
            break;
        case 3:
            v3 = -1;
            break;
        }

        prev = point;
        v1 = v2;

        v4 += v3;
    }

    if (v4 == 4 || v4 == -4) {
        return true;
    }

    return false;
}

// 0x491188
Region* allocateRegion(int initialCapacity)
{
    Region* region = (Region*)mymalloc(sizeof(*region), __FILE__, __LINE__); // "..\int\REGION.C", 142
    memset(region, 0, sizeof(*region));

    if (initialCapacity != 0) {
        region->points = (Point*)mymalloc(sizeof(*region->points) * (initialCapacity + 1), __FILE__, __LINE__); // "..\int\REGION.C", 147
        region->pointsCapacity = initialCapacity + 1;
    } else {
        region->points = NULL;
        region->pointsCapacity = 0;
    }

    region->name[0] = '\0';
    region->flags = 0;
    region->minY = INT_MIN;
    region->maxY = INT_MAX;
    region->procs[3] = 0;
    region->rightProcs[1] = 0;
    region->rightProcs[3] = 0;
    region->field_68 = 0;
    region->rightProcs[0] = 0;
    region->field_70 = 0;
    region->rightProcs[2] = 0;
    region->mouseEventCallback = NULL;
    region->rightMouseEventCallback = NULL;
    region->mouseEventCallbackUserData = 0;
    region->rightMouseEventCallbackUserData = 0;
    region->pointsLength = 0;
    region->minX = region->minY;
    region->maxX = region->maxY;
    region->procs[2] = 0;
    region->procs[1] = 0;
    region->procs[0] = 0;
    region->rightProcs[0] = 0;

    return region;
}

// 0x491274
void regionAddPoint(Region* region, int x, int y)
{
    if (region == NULL) {
        debug_printf("regionAddPoint(): null region ptr\n");
        return;
    }

    if (region->points != NULL) {
        if (region->pointsCapacity - 1 == region->pointsLength) {
            region->points = (Point*)myrealloc(region->points, sizeof(*region->points) * (region->pointsCapacity + 1), __FILE__, __LINE__); // "..\int\REGION.C", 190
            region->pointsCapacity++;
        }
    } else {
        region->pointsCapacity = 2;
        region->pointsLength = 0;
        region->points = (Point*)mymalloc(sizeof(*region->points) * 2, __FILE__, __LINE__); // "..\int\REGION.C", 185
    }

    int pointIndex = region->pointsLength;
    region->pointsLength++;

    Point* point = &(region->points[pointIndex]);
    point->x = x;
    point->y = y;

    Point* end = &(region->points[pointIndex + 1]);
    end->x = region->points->x;
    end->y = region->points->y;
}

// 0x491318
void regionDelete(Region* region)
{
    if (region == NULL) {
        debug_printf("regionDelete(): null region ptr\n");
        return;
    }

    if (region->points != NULL) {
        myfree(region->points, __FILE__, __LINE__); // "..\int\REGION.C", 206
    }

    myfree(region, __FILE__, __LINE__); // "..\int\REGION.C", 207
}

// 0x4A2F54
void regionAddName(Region* region, const char* name)
{
    if (region == NULL) {
        debug_printf("regionAddName(): null region ptr\n");
        return;
    }

    if (name == NULL) {
        region->name[0] = '\0';
        return;
    }

    strncpy(region->name, name, REGION_NAME_LENGTH - 1);
}

// 0x49138C
const char* regionGetName(Region* region)
{
    if (region == NULL) {
        debug_printf("regionGetName(): null region ptr\n");
        return "<null>";
    }

    return region->name;
}

// 0x4913A4
void* regionGetUserData(Region* region)
{
    if (region == NULL) {
        debug_printf("regionGetUserData(): null region ptr\n");
        return NULL;
    }

    return region->userData;
}

// 0x4913C0
void regionSetUserData(Region* region, void* data)
{
    if (region == NULL) {
        debug_printf("regionSetUserData(): null region ptr\n");
        return;
    }

    region->userData = data;
}

// 0x4913DC
void regionSetFlag(Region* region, int value)
{
    region->flags |= value;
}

// 0x4913E0
int regionGetFlag(Region* region)
{
    return region->flags;
}

} // namespace fallout
