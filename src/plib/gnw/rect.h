#ifndef FALLOUT_PLIB_GNW_RECT_H_
#define FALLOUT_PLIB_GNW_RECT_H_

namespace fallout {

// TODO: Remove.
typedef struct Point {
    int x;
    int y;
} Point;

// TODO: Remove.
typedef struct Size {
    int width;
    int height;
} Size;

typedef struct Rect {
    int ulx;
    int uly;
    int lrx;
    int lry;
} Rect;

// TODO: Check.
typedef struct rectdata {
    Rect rect;
    struct rectdata* next;
} rectdata;

typedef rectdata* RectPtr;

void GNW_rect_exit();
void rect_clip_list(RectPtr* pCur, Rect* bound);
RectPtr rect_clip(Rect* b, Rect* t);
RectPtr rect_malloc();
void rect_free(RectPtr ptr);
void rect_min_bound(const Rect* r1, const Rect* r2, Rect* min_bound);
int rect_inside_bound(const Rect* r1, const Rect* bound, Rect* r2);

// TODO: Remove.
static inline void rectCopy(Rect* dest, const Rect* src)
{
    dest->ulx = src->ulx;
    dest->uly = src->uly;
    dest->lrx = src->lrx;
    dest->lry = src->lry;
}

// TODO: Remove.
static inline int rectGetWidth(const Rect* rect)
{
    return rect->lrx - rect->ulx + 1;
}

// TODO: Remove.
static inline int rectGetHeight(const Rect* rect)
{
    return rect->lry - rect->uly + 1;
}

// TODO: Remove.
static inline void rectOffset(Rect* rect, int dx, int dy)
{
    rect->ulx += dx;
    rect->uly += dy;
    rect->lrx += dx;
    rect->lry += dy;
}

} // namespace fallout

#endif /* FALLOUT_PLIB_GNW_RECT_H_ */
