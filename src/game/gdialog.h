#ifndef FALLOUT_GAME_GDIALOG_H_
#define FALLOUT_GAME_GDIALOG_H_

#include "game/art.h"
#include "game/object_types.h"
#include "int/intrpret.h"

namespace fallout {

extern unsigned char* light_BlendTable;
extern unsigned char* dark_BlendTable;
extern Object* dialog_target;
extern bool dialog_target_is_party;
extern int dialogue_head;
extern int dialogue_scr_id;

extern unsigned char light_GrayTable[256];
extern unsigned char dark_GrayTable[256];

int gdialog_init();
int gdialog_reset();
int gdialog_exit();
bool dialog_active();
void gdialog_enter(Object* target, int a2);
void dialogue_system_enter();
void gdialog_setup_speech(const char* audioFileName);
void gdialog_free_speech();
int gDialogEnableBK();
int gDialogDisableBK();
int scr_dialogue_init(int headFid, int reaction);
int scr_dialogue_exit();
void gdialog_set_background(int a1);
void gdialog_display_msg(char* msg);
int gDialogStart();
int gDialogSayMessage();
int gDialogOption(int messageListId, int messageId, const char* proc, int reaction);
int gDialogOptionStr(int messageListId, const char* text, const char* proc, int reaction);
int gDialogOptionProc(int messageListId, int messageId, int proc, int reaction);
int gDialogOptionProcStr(int messageListId, const char* text, int proc, int reaction);
int gDialogReply(Program* program, int messageListId, int messageId);
int gDialogReplyStr(Program* program, int messageListId, const char* text);
int gDialogGo();
void talk_to_critter_reacts(int a1);
void gdialogSetBarterMod(int modifier);
int gdActivateBarter(int modifier);
void barter_end_to_talk_to();

} // namespace fallout

#endif /* FALLOUT_GAME_GDIALOG_H_ */
