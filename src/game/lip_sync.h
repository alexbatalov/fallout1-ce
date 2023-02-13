#ifndef FALLOUT_GAME_LIP_SYNC_H_
#define FALLOUT_GAME_LIP_SYNC_H_

#include <stddef.h>

#include "int/sound.h"
#include "plib/db/db.h"

namespace fallout {

#define PHONEME_COUNT (42)

typedef enum LipsFlags {
    LIPS_FLAG_0x01 = 0x01,
    LIPS_FLAG_0x02 = 0x02,
} LipsFlags;

typedef struct SpeechMarker {
    int marker;
    int position;
} SpeechMarker;

typedef struct LipsData {
    int version;
    int field_4;
    int flags;
    Sound* sound;
    int field_10;
    void* field_14;
    unsigned char* phonemes;
    int field_1C;
    int field_20;
    int phoneme_count;
    int field_28;
    int marker_count;
    SpeechMarker* markers;
    int field_34;
    int field_38;
    int field_3C;
    int field_40;
    int field_44;
    int field_48;
    int field_4C;
    char file_name[8];
    char field_58[4];
    char field_5C[4];
    char field_60[4];
    char field_64[260];
} LipsData;

extern unsigned char head_phoneme_current;
extern unsigned char head_phoneme_drawn;
extern int head_marker_current;
extern bool lips_draw_head;
extern LipsData lip_info;
extern int speechStartTime;

void lips_bkg_proc();
int lips_play_speech();
int lips_load_file(const char* audioFileName, const char* headFileName);
int lips_free_speech();

} // namespace fallout

#endif /* FALLOUT_GAME_LIP_SYNC_H_ */
