#ifndef FALLOUT_GAME_EDITOR_H_
#define FALLOUT_GAME_EDITOR_H_

#include "plib/db/db.h"

namespace fallout {

extern int character_points;

int editor_design(bool isCreationMode);
void CharEditInit();
int get_input_str(int win, int cancelKeyCode, char* text, int maxLength, int x, int y, int textColor, int backgroundColor, int flags);
bool isdoschar(int ch);
char* strmfe(char* dest, const char* name, const char* ext);
bool db_access(const char* fname);
char* AddSpaces(char* string, int length);
char* itostndn(int value, char* dest);
int editor_save(DB_FILE* stream);
int editor_load(DB_FILE* stream);
void editor_reset();
void RedrwDMPrk();

} // namespace fallout

#endif /* FALLOUT_GAME_EDITOR_H_ */
