#ifndef FALLOUT_INT_NEVS_H_
#define FALLOUT_INT_NEVS_H_

#include "int/intrpret.h"

namespace fallout {

typedef void(NevsCallback)(const char* name);

typedef enum NevsType {
    NEVS_TYPE_EVENT = 0,
    NEVS_TYPE_HANDLER = 1,
} NevsType;

void nevs_close();
void nevs_initonce();
int nevs_addevent(const char* name, Program* program, int proc, int type);
int nevs_addCevent(const char* name, NevsCallback* callback, int type);
int nevs_clearevent(const char* name);
int nevs_signal(const char* name);
void nevs_update();

} // namespace fallout

#endif /* FALLOUT_INT_NEVS_H_ */
