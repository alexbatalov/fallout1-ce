#ifndef FALLOUT_GAME_SFXCACHE_H_
#define FALLOUT_GAME_SFXCACHE_H_

namespace fallout {

// The maximum number of sound effects that can be loaded and played
// simultaneously.
#define SOUND_EFFECTS_MAX_COUNT 4

int sfxc_init(int cache_size, const char* effectsPath);
void sfxc_exit();
int sfxc_is_initialized();
void sfxc_flush();
int sfxc_cached_open(const char* fname, int mode);
int sfxc_cached_close(int handle);
int sfxc_cached_read(int handle, void* buf, unsigned int size);
int sfxc_cached_write(int handle, const void* buf, unsigned int size);
long sfxc_cached_seek(int handle, long offset, int origin);
long sfxc_cached_tell(int handle);
long sfxc_cached_file_size(int handle);

} // namespace fallout

#endif /* FALLOUT_GAME_SFXCACHE_H_ */
