#ifndef FALLOUT_INT_MOUSEMGR_H_
#define FALLOUT_INT_MOUSEMGR_H_

namespace fallout {

typedef char*(MouseManagerNameMangler)(char* fileName);
typedef int(MouseManagerRateProvider)();
typedef int(MouseManagerTimeProvider)();

void mousemgrSetNameMangler(MouseManagerNameMangler* func);
void mousemgrSetTimeCallback(MouseManagerRateProvider* rateFunc, MouseManagerTimeProvider* currentTimeFunc);
void initMousemgr();
void mousemgrClose();
void mousemgrUpdate();
int mouseSetFrame(char* fileName, int a2);
bool mouseSetMouseShape(char* fileName, int a2, int a3);
bool mouseSetMousePointer(char* fileName);
void mousemgrResetMouse();
void mouseHide();
void mouseShow();

} // namespace fallout

#endif /* FALLOUT_INT_MOUSEMGR_H_ */
