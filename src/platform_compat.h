#ifndef FALLOUT_PLATFORM_COMPAT_H_
#define FALLOUT_PLATFORM_COMPAT_H_

#include <stddef.h>
#include <stdio.h>

namespace fallout {

// TODO: This is compatibility cross-platform layer. Designed to have minimal
// impact on the codebase. Remove once it's no longer needed.

// A naive cross-platform MAX_PATH/PATH_MAX/MAX_PATH drop-in replacement.
//
// TODO: Remove when we migrate to use std::filesystem::path or std::string to
// represent paths across the codebase.
#define COMPAT_MAX_PATH 260

#define COMPAT_MAX_DRIVE 3
#define COMPAT_MAX_DIR 256
#define COMPAT_MAX_FNAME 256
#define COMPAT_MAX_EXT 256

int compat_stricmp(const char* string1, const char* string2);
int compat_strnicmp(const char* string1, const char* string2, size_t size);
char* compat_strupr(char* string);
char* compat_strlwr(char* string);
char* compat_itoa(int value, char* buffer, int radix);
void compat_splitpath(const char* path, char* drive, char* dir, char* fname, char* ext);
void compat_makepath(char* path, const char* drive, const char* dir, const char* fname, const char* ext);
int compat_read(int fileHandle, void* buf, unsigned int size);
int compat_write(int fileHandle, const void* buf, unsigned int size);
long compat_lseek(int fileHandle, long offset, int origin);
long compat_tell(int fileHandle);
long compat_filelength(int fd);
int compat_mkdir(const char* path);
unsigned int compat_timeGetTime();
FILE* compat_fopen(const char* path, const char* mode);
int compat_remove(const char* path);
int compat_rename(const char* oldFileName, const char* newFileName);
void compat_windows_path_to_native(char* path);
void compat_resolve_path(char* path);
char* compat_strdup(const char* string);
long getFileSize(FILE* stream);

} // namespace fallout

#endif /* FALLOUT_PLATFORM_COMPAT_H_ */
