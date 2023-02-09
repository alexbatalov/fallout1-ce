#ifndef FALLOUT_PLIB_GNW_VCR_H_
#define FALLOUT_PLIB_GNW_VCR_H_

#include "plib/db/db.h"

namespace fallout {

#define VCR_BUFFER_CAPACITY 4096

typedef enum VcrState {
    VCR_STATE_RECORDING,
    VCR_STATE_PLAYING,
    VCR_STATE_TURNED_OFF,
} VcrState;

#define VCR_STATE_STOP_REQUESTED 0x80000000

typedef enum VcrTerminationFlags {
    // Specifies that VCR playback should stop if any key is pressed.
    VCR_TERMINATE_ON_KEY_PRESS = 0x01,

    // Specifies that VCR playback should stop if mouse is mouved.
    VCR_TERMINATE_ON_MOUSE_MOVE = 0x02,

    // Specifies that VCR playback should stop if any mouse button is pressed.
    VCR_TERMINATE_ON_MOUSE_PRESS = 0x04,
} VcrTerminationFlags;

typedef enum VcrPlaybackCompletionReason {
    VCR_PLAYBACK_COMPLETION_REASON_NONE = 0,

    // Indicates that VCR playback completed normally.
    VCR_PLAYBACK_COMPLETION_REASON_COMPLETED = 1,

    // Indicates that VCR playback terminated according to termination flags.
    VCR_PLAYBACK_COMPLETION_REASON_TERMINATED = 2,
} VcrPlaybackCompletionReason;

typedef enum VcrEntryType {
    VCR_ENTRY_TYPE_NONE = 0,
    VCR_ENTRY_TYPE_INITIAL_STATE = 1,
    VCR_ENTRY_TYPE_KEYBOARD_EVENT = 2,
    VCR_ENTRY_TYPE_MOUSE_EVENT = 3,
} VcrEntryType;

typedef struct VcrEntry {
    unsigned int type;
    unsigned int time;
    unsigned int counter;
    union {
        struct {
            int mouseX;
            int mouseY;
            int keyboardLayout;
        } initial;
        struct {
            short key;
        } keyboardEvent;
        struct {
            int dx;
            int dy;
            int buttons;
        } mouseEvent;
    };
} VcrEntry;

typedef void(VcrPlaybackCompletionCallback)(int reason);

extern VcrEntry* vcr_buffer;
extern int vcr_buffer_index;
extern unsigned int vcr_state;
extern unsigned int vcr_time;
extern unsigned int vcr_counter;
extern unsigned int vcr_terminate_flags;
extern int vcr_terminated_condition;

bool vcr_record(const char* fileName);
bool vcr_play(const char* fileName, unsigned int terminationFlags, VcrPlaybackCompletionCallback* callback);
void vcr_stop();
int vcr_status();
int vcr_update();
bool vcr_dump_buffer();
bool vcr_save_record(VcrEntry* ptr, DB_FILE* stream);
bool vcr_load_record(VcrEntry* ptr, DB_FILE* stream);

} // namespace fallout

#endif /* FALLOUT_PLIB_GNW_VCR_H_ */
