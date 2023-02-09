#ifndef FALLOUT_GAME_SELFRUN_H_
#define FALLOUT_GAME_SELFRUN_H_

namespace fallout {

#define SELFRUN_RECORDING_FILE_NAME_LENGTH 13
#define SELFRUN_MAP_FILE_NAME_LENGTH 13

typedef struct SelfrunData {
    char recordingFileName[SELFRUN_RECORDING_FILE_NAME_LENGTH];
    char mapFileName[SELFRUN_MAP_FILE_NAME_LENGTH];
    int stopKeyCode;
} SelfrunData;

int selfrun_get_list(char*** fileListPtr, int* fileListLengthPtr);
int selfrun_free_list(char*** fileListPtr);
int selfrun_prep_playback(const char* fileName, SelfrunData* selfrunData);
void selfrun_playback_loop(SelfrunData* selfrunData);
int selfrun_prep_recording(const char* recordingName, const char* mapFileName, SelfrunData* selfrunData);
void selfrun_recording_loop(SelfrunData* selfrunData);

} // namespace fallout

#endif /* FALLOUT_GAME_SELFRUN_H_ */
