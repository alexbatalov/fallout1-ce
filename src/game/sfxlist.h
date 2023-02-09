#ifndef FALLOUT_GAME_SFXLIST_H_
#define FALLOUT_GAME_SFXLIST_H_

namespace fallout {

#define SFXL_OK 0
#define SFXL_ERR 1
#define SFXL_ERR_TAG_INVALID 2

bool sfxl_tag_is_legal(int tag);
int sfxl_init(const char* soundEffectsPath, int compression, int debugLevel);
void sfxl_exit();
int sfxl_name_to_tag(char* name, int* tagPtr);
int sfxl_name(int tag, char** pathPtr);
int sfxl_size_full(int tag, int* sizePtr);
int sfxl_size_cached(int tag, int* sizePtr);

} // namespace fallout

#endif /* FALLOUT_GAME_SFXLIST_H_ */
